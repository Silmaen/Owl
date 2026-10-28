/**
 * @file LuaEngine.cpp
 * @author Silmaen
 * @date 09/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "LuaEngine.h"

#include "core/external/lua.h"
#include "platform/FileUtils.h"
#include "script/ScriptDebugger.h"
#include "script/visual/VisualScript.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <format>
#include <functional>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#ifdef OWL_PLATFORM_LINUX
#include <csignal>
#include <pthread.h>
#endif

namespace owl::script {

struct LuaEngine::Quota {
	size_t memoryLimit = 0;
	size_t memoryUsed = 0;
	uint32_t timeBudgetMs = 0;
	bool enforced = false;
	bool memoryExceeded = false;
	bool timeExceeded = false;
	LuaStatus lastStatus = LuaStatus::Ok;
	std::string warning;
	std::function<std::string()> errorContext;
	ModuleReader moduleReader;
	void* hostPointer = nullptr;
	double deltaTime = 0.0;
	// Registry references of the interned names of the functions the host calls, to skip hashing them per call.
	std::vector<std::pair<std::string, int>> nameRefs;
	// The last error logged in full, how many times it came back since the last report, and how many were left out.
	std::string lastError;
	uint32_t repeatsPending = 0;
	size_t repeatsSuppressed = 0;
	std::chrono::steady_clock::time_point lastReport;
};

auto fixHint(const LuaStatus iStatus) -> std::string_view {
	switch (iStatus) {
		case LuaStatus::Ok:
			return "nothing to fix";
		case LuaStatus::Missing:
			return "define the function in the script, or remove the call";
		case LuaStatus::LoadError:
			return "fix the syntax error at the reported line, or check the script path of the LuaScript component";
		case LuaStatus::RuntimeError:
			return "fix the script at the first line of the traceback; the callback runs again on its next call";
		case LuaStatus::MemoryQuota:
			return "stop the script from growing its tables without bound, or raise its memory quota";
		case LuaStatus::TimeQuota:
			return "split the long loop across frames, or raise the time quota";
		case LuaStatus::Invalid:
			return "restart the scene; if it persists, the system is out of memory";
	}
	return "check the script";
}

namespace {

constexpr uint32_t g_WatchdogTickMs = 10;
// Enough for the lifecycle callbacks and a few event callbacks or properties.
constexpr size_t g_MaxCachedNames = 16;

auto errorContextOf(const LuaEngine::Quota& iQuota) -> std::string {
	return iQuota.errorContext ? std::format(" of {}", iQuota.errorContext()) : std::string{};
}

auto quotaOf(lua_State* iState) -> LuaEngine::Quota* {
	void* userData = nullptr;
	lua_getallocf(iState, &userData);
	return static_cast<LuaEngine::Quota*>(userData);
}

auto quotaAlloc(void* iUserData, void* iPtr, const size_t iOldSize, const size_t iNewSize) -> void* {
	auto* quota = static_cast<LuaEngine::Quota*>(iUserData);
	const size_t oldSize = iPtr == nullptr ? 0 : iOldSize;
	if (iNewSize == 0) {
		std::free(iPtr);// NOLINT(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory) lua_Alloc contract.
		quota->memoryUsed -= oldSize;
		return nullptr;
	}
	if (iNewSize > oldSize && quota->enforced && quota->memoryLimit != 0 &&
		quota->memoryUsed - oldSize + iNewSize > quota->memoryLimit) {
		quota->memoryExceeded = true;
		return nullptr;
	}
	// NOLINTNEXTLINE(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory) lua_Alloc contract.
	void* block = std::realloc(iPtr, iNewSize);
	if (block == nullptr)
		return iNewSize <= oldSize ? iPtr : nullptr;
	quota->memoryUsed = quota->memoryUsed - oldSize + iNewSize;
	return block;
}

#ifdef OWL_PLATFORM_LINUX
// TSan holds a thread's asynchronous signals until its next intercepted call, which a running Lua loop never makes.
#if defined(__SANITIZE_THREAD__)
constexpr bool g_threadSanitizer = true;
#elif defined(__has_feature)
#if __has_feature(thread_sanitizer)
constexpr bool g_threadSanitizer = true;
#else
constexpr bool g_threadSanitizer = false;
#endif
#else
constexpr bool g_threadSanitizer = false;
#endif
#endif

void overdueHook(lua_State* iState, [[maybe_unused]] lua_Debug* iDebug) {
	auto* quota = quotaOf(iState);
	if (!quota->enforced)
		return;
	quota->timeExceeded = true;
	// The hook stays armed on every instruction, so a script catching the error with pcall cannot loop on.
	luaL_error(iState, "time budget exceeded (%d ms per call)", static_cast<int>(quota->timeBudgetMs));
}

constexpr int g_maxLocals = 64;
constexpr int g_maxTableItems = 4;
constexpr size_t g_maxValueText = 120;

// NOLINTNEXTLINE(misc-no-recursion) An entry is described once: a nested table is shown as {…}, never entered.
auto shortTable(lua_State* iState, const int iIndex) -> std::string {
	const int table = lua_absindex(iState, iIndex);
	std::string text = "{";
	int count = 0;
	lua_pushnil(iState);
	while (lua_next(iState, table) != 0) {
		if (count < g_maxTableItems) {
			if (count > 0)
				text += ", ";
			if (lua_type(iState, -2) == LUA_TSTRING)
				text += std::format("{}=", lua_tostring(iState, -2));
			text += lua_type(iState, -1) == LUA_TTABLE ? "{…}" : LuaEngine::describeValue(iState, -1).value;
		}
		++count;
		lua_pop(iState, 1);
	}
	if (count > g_maxTableItems)
		return std::format("table ({} entries)", count);
	return text + "}";
}

void breakpointHook(lua_State* iState, lua_Debug* iDebug) {
	if (iDebug->event != LUA_HOOKLINE || lua_getinfo(iState, "S", iDebug) == 0)
		return;
	if (!ScriptDebugger::isBreakpoint(iDebug->source, iDebug->currentline))
		return;
	auto* quota = quotaOf(iState);
	const bool enforced = quota->enforced;
	quota->enforced = false;
	BreakpointHit hit{.script = iDebug->source, .line = iDebug->currentline, .context = {}, .locals = {}};
	if (!hit.script.empty() && (hit.script.front() == '@' || hit.script.front() == '='))
		hit.script.erase(0, 1);
	if (quota->errorContext)
		hit.context = quota->errorContext();
	for (int index = 1; index <= g_maxLocals; ++index) {
		const char* name = lua_getlocal(iState, iDebug, index);
		if (name == nullptr)
			break;
		if (name[0] != '(' && !std::string_view{name}.starts_with("__")) {
			auto value = LuaEngine::describeValue(iState, -1);
			value.name = name;
			hit.locals.push_back(std::move(value));
		}
		lua_pop(iState, 1);
	}
	quota->enforced = enforced;
	ScriptDebugger::reportHit(std::move(hit));
}

// The breakpoint hook stays armed for the whole call; the watchdog replaces it when the call overruns its budget.
void armDebugHook(lua_State* iState) {
	if (ScriptDebugger::hasBreakpoints())
		lua_sethook(iState, breakpointHook, LUA_MASKLINE, 0);
	else if (lua_gethook(iState) != nullptr)
		lua_sethook(iState, nullptr, 0, 0);
}

struct CallSlot {
	std::atomic<lua_State*> state{nullptr};
	std::atomic<lua_State*> running{nullptr};
	std::atomic<uint64_t> sequence{0};
	std::atomic<uint64_t> targetSequence{0};
	std::atomic<uint32_t> budgetMs{0};
#ifdef OWL_PLATFORM_LINUX
	pthread_t thread{pthread_self()};
#endif
	uint64_t lastSeen = 0;
	uint32_t staleMs = 0;
	uint64_t firedSequence = 0;
};

void interruptIfOverdue(CallSlot& ioSlot) {
	if (ioSlot.sequence.load(std::memory_order_relaxed) != ioSlot.targetSequence.load(std::memory_order_relaxed))
		return;
	lua_State* target = ioSlot.running.load(std::memory_order_relaxed);
	if (target == nullptr)
		target = ioSlot.state.load(std::memory_order_relaxed);
	if (target != nullptr)
		lua_sethook(target, overdueHook, LUA_MASKCOUNT, 1);
}

thread_local CallSlot* t_slot = nullptr;

#ifdef OWL_PLATFORM_LINUX
void onWatchdogSignal([[maybe_unused]] int iSignal) {
	if (t_slot != nullptr)
		interruptIfOverdue(*t_slot);
}
#endif

class Watchdog final {
public:
	Watchdog(const Watchdog&) = delete;

	Watchdog(Watchdog&&) = delete;

	auto operator=(const Watchdog&) -> Watchdog& = delete;

	auto operator=(Watchdog&&) -> Watchdog& = delete;

	Watchdog() {
#ifdef OWL_PLATFORM_LINUX
		struct sigaction action{};

		OWL_DIAG_PUSH
		OWL_DIAG_DISABLE_CLANG("-Wdisabled-macro-expansion")
		action.sa_handler = onWatchdogSignal;
		OWL_DIAG_POP

		action.sa_flags = SA_RESTART;
		sigemptyset(&action.sa_mask);
		sigaction(signalNumber(), &action, nullptr);
#endif
		m_thread = std::jthread([this](const std::stop_token& iStop) -> void { run(iStop); });
	}

	~Watchdog() = delete;

	static auto instance() -> Watchdog& {
		// Never destroyed: at exit Windows kills the thread first, and joining it could wait forever.
		static auto* watchdog = new Watchdog();// NOLINT(cppcoreguidelines-owning-memory) intentional leak.
		return *watchdog;
	}

	void add(CallSlot* iSlot) {
		const std::scoped_lock<std::mutex> lock(m_mutex);
		m_slots.push_back(iSlot);
	}

	void remove(CallSlot* iSlot) {
		const std::scoped_lock<std::mutex> lock(m_mutex);
		std::erase(m_slots, iSlot);
	}

#ifdef OWL_PLATFORM_LINUX
	static auto signalNumber() -> int { return SIGRTMIN + 4; }
#endif

private:
	void run(const std::stop_token& iStop) {
		std::unique_lock<std::mutex> lock(m_mutex);
		while (!iStop.stop_requested()) {
			m_wake.wait_for(lock, iStop, std::chrono::milliseconds(g_WatchdogTickMs), []() -> bool { return false; });
			for (auto* slot: m_slots) watch(*slot);
		}
	}

	static void watch(CallSlot& ioSlot) {
		const uint64_t sequence = ioSlot.sequence.load(std::memory_order_relaxed);
		const uint32_t budget = ioSlot.budgetMs.load(std::memory_order_relaxed);
		if (ioSlot.state.load(std::memory_order_relaxed) == nullptr || budget == 0 || sequence != ioSlot.lastSeen) {
			ioSlot.lastSeen = sequence;
			ioSlot.staleMs = 0;
			return;
		}
		ioSlot.staleMs += g_WatchdogTickMs;
		if (ioSlot.staleMs < budget || ioSlot.firedSequence == sequence)
			return;
		ioSlot.firedSequence = sequence;
		ioSlot.targetSequence.store(sequence, std::memory_order_relaxed);
#ifdef OWL_PLATFORM_LINUX
		if constexpr (!g_threadSanitizer) {
			pthread_kill(ioSlot.thread, signalNumber());
			return;
		}
#endif
		// No usable thread signal (absent, or held back by TSan): set the hook from here, like the reference lua.c.
		interruptIfOverdue(ioSlot);
	}

	std::mutex m_mutex;
	std::vector<CallSlot*> m_slots;
	std::condition_variable_any m_wake;
	std::jthread m_thread;
};

struct SlotRegistration {
	SlotRegistration(const SlotRegistration&) = delete;

	SlotRegistration(SlotRegistration&&) = delete;

	auto operator=(const SlotRegistration&) -> SlotRegistration& = delete;

	auto operator=(SlotRegistration&&) -> SlotRegistration& = delete;

	SlotRegistration() {
		Watchdog::instance().add(&slot);
		t_slot = &slot;
	}

	~SlotRegistration() {
		t_slot = nullptr;
		Watchdog::instance().remove(&slot);
	}

	CallSlot slot;
};

auto registerCurrentThread() -> CallSlot& {
	thread_local SlotRegistration registration;
	return registration.slot;
}

auto currentSlot() -> CallSlot& {
	if (t_slot != nullptr) [[likely]]
		return *t_slot;
	return registerCurrentThread();
}

void copyHook(lua_State* iFrom, lua_State* iTo) {
	lua_sethook(iTo, lua_gethook(iFrom), lua_gethookmask(iFrom), lua_gethookcount(iFrom));
}

auto sandboxResume(lua_State* iState) -> int {
	lua_State* coroutine = lua_tothread(iState, 1);
	auto& slot = currentSlot();
	lua_State* outer = slot.running.load(std::memory_order_relaxed);
	if (coroutine != nullptr) {
		copyHook(iState, coroutine);
		slot.running.store(coroutine, std::memory_order_relaxed);
	}
	const int argCount = lua_gettop(iState);
	lua_pushvalue(iState, lua_upvalueindex(1));
	lua_insert(iState, 1);
	auto* quota = quotaOf(iState);
	const bool enforced = quota->enforced;
	const int status = lua_pcall(iState, argCount, LUA_MULTRET, 0);
	quota->enforced = enforced;
	slot.running.store(outer, std::memory_order_relaxed);
	if (coroutine != nullptr && lua_gethook(coroutine) != nullptr)
		copyHook(coroutine, iState);
	if (status != LUA_OK)
		return lua_error(iState);
	return lua_gettop(iState);
}

// Upvalues: the sandboxed coroutine.resume and the coroutine.
auto wrappedResume(lua_State* iState) -> int {
	const int argCount = lua_gettop(iState);
	lua_pushvalue(iState, lua_upvalueindex(1));
	lua_pushvalue(iState, lua_upvalueindex(2));
	lua_rotate(iState, 1, 2);
	lua_call(iState, argCount + 1, LUA_MULTRET);
	if (lua_toboolean(iState, 1) == 0) {
		lua_settop(iState, 2);
		return lua_error(iState);
	}
	return lua_gettop(iState) - 1;
}

// coroutine.wrap on top of the sandboxed coroutine.resume (upvalue), so that the time budget follows the coroutine.
auto sandboxWrap(lua_State* iState) -> int {
	luaL_checktype(iState, 1, LUA_TFUNCTION);
	lua_pushvalue(iState, lua_upvalueindex(1));
	lua_State* coroutine = lua_newthread(iState);
	lua_pushvalue(iState, 1);
	lua_xmove(iState, coroutine, 1);
	lua_pushcclosure(iState, wrappedResume, 2);
	return 1;
}

auto panicHandler(lua_State* iState) -> int {
	const char* message = lua_tostring(iState, -1);
	OWL_CORE_CRITICAL("LuaEngine: Unprotected Lua error: {}.", message != nullptr ? message : "(not a string)")
	return 0;
}

void warningHandler(void* iUserData, const char* iMessage, const int iToBeContinued) {
	auto* quota = static_cast<LuaEngine::Quota*>(iUserData);
	if (quota->warning.empty() && iToBeContinued == 0 && iMessage[0] == '@')
		return;
	try {
		quota->warning += iMessage;
		if (iToBeContinued != 0)
			return;
		OWL_CORE_WARN("LuaEngine: Script warning: {}.", quota->warning)
	} catch (...) { OWL_CORE_WARN("LuaEngine: Script warning dropped.") }
	quota->warning.clear();
}

auto messageHandler(lua_State* iState) -> int {
	auto* quota = quotaOf(iState);
	const bool enforced = quota->enforced;
	quota->enforced = false;
	if (lua_type(iState, 1) == LUA_TSTRING)
		luaL_traceback(iState, iState, lua_tostring(iState, 1), 1);
	else
		lua_pushfstring(iState, "(error object is a %s value)", luaL_typename(iState, 1));
	quota->enforced = enforced;
	return 1;
}

auto guardedBinding(lua_State* iState) -> int {
	return LuaEngine::callGuarded(iState, lua_tocfunction(iState, lua_upvalueindex(1)));
}
// Every place where a script can catch an error restores the memory enforcement a binding lifted.
auto restoringCatcher(lua_State* iState) -> int {
	auto* quota = quotaOf(iState);
	const bool enforced = quota->enforced;
	const int argCount = lua_gettop(iState);
	lua_pushvalue(iState, lua_upvalueindex(1));
	lua_insert(iState, 1);
	lua_call(iState, argCount, LUA_MULTRET);
	quota->enforced = enforced;
	return lua_gettop(iState);
}

auto sandboxLoad(lua_State* iState) -> int {
	const bool hasEnvironment = lua_gettop(iState) >= 4;
	lua_settop(iState, 4);
	lua_pushvalue(iState, lua_upvalueindex(1));
	lua_pushvalue(iState, 1);
	lua_pushvalue(iState, 2);
	lua_pushliteral(iState, "t");
	if (hasEnvironment)
		lua_pushvalue(iState, 4);
	lua_call(iState, hasEnvironment ? 4 : 3, LUA_MULTRET);
	return lua_gettop(iState) - 4;
}

auto sandboxSetMetatable(lua_State* iState) -> int {
	if (lua_type(iState, 2) == LUA_TTABLE) {
		lua_pushliteral(iState, "__gc");
		if (lua_rawget(iState, 2) != LUA_TNIL)
			return luaL_error(iState, "setmetatable: '__gc' metamethods are not allowed in the sandbox");
		lua_pop(iState, 1);
	}
	const int argCount = lua_gettop(iState);
	lua_pushvalue(iState, lua_upvalueindex(1));
	lua_insert(iState, 1);
	lua_call(iState, argCount, LUA_MULTRET);
	return lua_gettop(iState);
}

auto sandboxCollectGarbage(lua_State* iState) -> int {
	if (const char* option = luaL_optstring(iState, 1, "collect");
		std::string_view{option} != "count" && std::string_view{option} != "isrunning")
		return luaL_error(iState, "collectgarbage: option '%s' is not allowed in the sandbox", option);
	const int argCount = lua_gettop(iState);
	lua_pushvalue(iState, lua_upvalueindex(1));
	lua_insert(iState, 1);
	lua_call(iState, argCount, LUA_MULTRET);
	return lua_gettop(iState);
}

// Registry key of the table of the modules `require` loaded (a module being loaded is `false`).
constexpr const char* g_ModulesKey = "owl.modules";
constexpr size_t g_MaxModuleName = 128;

auto isModuleName(const std::string_view iName) -> bool {
	if (iName.empty() || iName.size() > g_MaxModuleName)
		return false;
	bool afterDot = true;
	for (const char character: iName) {
		if (character == '.') {
			if (afterDot)
				return false;
			afterDot = true;
			continue;
		}
		if (std::isalnum(static_cast<unsigned char>(character)) == 0 && character != '_')
			return false;
		afterDot = false;
	}
	return !afterDot;
}

// Reads the module named at index 1 and pushes its compiled chunk; runs under the exception trampoline.
auto loadModule(lua_State* iState) -> int {
	const std::string name = lua_tostring(iState, 1);
	std::string path = name;
	std::ranges::replace(path, '.', '/');
	path += ".lua";
	const auto& reader = quotaOf(iState)->moduleReader;
	if (!reader)
		throw std::runtime_error(std::format("require: no module can be read by this script ('{}')", name));
	const auto data = reader(path);
	LuaEngine::restartCallClock(iState);
	if (!data)
		throw std::runtime_error(std::format("require: no module '{}' (file '{}')", name, path));
	const std::string chunkName = "@" + path;
	if (luaL_loadbufferx(iState, reinterpret_cast<const char*>(data->data()), data->size(), chunkName.c_str(), "t") !=
		LUA_OK) {
		std::string message = lua_tostring(iState, -1) != nullptr ? lua_tostring(iState, -1) : "(no message)";
		lua_pop(iState, 1);
		throw std::runtime_error(std::format("require: cannot load '{}': {}", name, message));
	}
	return 1;
}

auto guardedLoadModule(lua_State* iState) -> int { return LuaEngine::callGuarded(iState, loadModule); }

// The module chunk runs outside the trampoline, under the memory ceiling and the time budget of the script.
auto sandboxRequire(lua_State* iState) -> int {
	const char* name = luaL_checkstring(iState, 1);
	if (!isModuleName(name))
		return luaL_error(iState, "require: '%s' is not a module name (words joined by dots: scripts.game.towers)",
						  name);
	lua_settop(iState, 1);
	lua_getfield(iState, LUA_REGISTRYINDEX, g_ModulesKey);
	if (lua_getfield(iState, 2, name) != LUA_TNIL) {
		if (lua_isboolean(iState, -1) && lua_toboolean(iState, -1) == 0)
			return luaL_error(iState, "require: '%s' requires itself while it loads", name);
		return 1;
	}
	lua_pop(iState, 1);
	lua_pushcfunction(iState, guardedLoadModule);
	lua_pushvalue(iState, 1);
	lua_call(iState, 1, 1);
	lua_pushboolean(iState, 0);
	lua_setfield(iState, 2, name);
	if (lua_pcall(iState, 0, 1, 0) != LUA_OK) {
		lua_pushnil(iState);
		lua_setfield(iState, 2, name);
		return lua_error(iState);
	}
	if (lua_isnil(iState, -1)) {
		lua_pop(iState, 1);
		lua_pushboolean(iState, 1);
	}
	lua_pushvalue(iState, -1);
	lua_setfield(iState, 2, name);
	return 1;
}

void wrapBaseFunction(lua_State* iState, const char* iName, const lua_CFunction iWrapper) {
	lua_getfield(iState, -1, iName);
	lua_pushcclosure(iState, iWrapper, 1);
	lua_setfield(iState, -2, iName);
}

void openSandboxedLibraries(lua_State* iState) {
	luaL_requiref(iState, LUA_GNAME, luaopen_base, 1);
	lua_pop(iState, 1);
	luaL_requiref(iState, LUA_TABLIBNAME, luaopen_table, 1);
	lua_pop(iState, 1);
	luaL_requiref(iState, LUA_STRLIBNAME, luaopen_string, 1);
	lua_pop(iState, 1);
	luaL_requiref(iState, LUA_MATHLIBNAME, luaopen_math, 1);
	lua_pop(iState, 1);
	luaL_requiref(iState, LUA_UTF8LIBNAME, luaopen_utf8, 1);
	lua_pop(iState, 1);
	luaL_requiref(iState, LUA_COLIBNAME, luaopen_coroutine, 1);
	lua_pop(iState, 1);

	lua_pushglobaltable(iState);
	for (const auto* func: {"dofile", "loadfile"}) {
		lua_pushnil(iState);
		lua_setfield(iState, -2, func);
	}
	lua_newtable(iState);
	lua_setfield(iState, LUA_REGISTRYINDEX, g_ModulesKey);
	lua_pushcfunction(iState, sandboxRequire);
	lua_setfield(iState, -2, "require");
	wrapBaseFunction(iState, "load", sandboxLoad);
	wrapBaseFunction(iState, "setmetatable", sandboxSetMetatable);
	wrapBaseFunction(iState, "collectgarbage", sandboxCollectGarbage);
	wrapBaseFunction(iState, "pcall", restoringCatcher);
	wrapBaseFunction(iState, "xpcall", restoringCatcher);
	lua_getfield(iState, -1, LUA_STRLIBNAME);
	lua_pushnil(iState);
	lua_setfield(iState, -2, "dump");
	lua_pop(iState, 1);
	lua_getfield(iState, -1, LUA_COLIBNAME);
	wrapBaseFunction(iState, "resume", sandboxResume);
	wrapBaseFunction(iState, "close", restoringCatcher);
	lua_getfield(iState, -1, "resume");
	lua_pushcclosure(iState, sandboxWrap, 1);
	lua_setfield(iState, -2, "wrap");
	lua_pop(iState, 2);

	// Lock the string metatable: the script cannot reach it through getmetatable("").
	lua_pushliteral(iState, "");
	if (lua_getmetatable(iState, -1) != 0) {
		lua_pushboolean(iState, 0);
		lua_setfield(iState, -2, "__metatable");
		lua_pop(iState, 1);
	}
	lua_pop(iState, 1);
}

}// namespace

LuaEngine::LuaEngine() : LuaEngine(ScriptEngine::getDefaultQuotas()) {}

LuaEngine::LuaEngine(const ScriptQuotas& iQuotas)
	: mp_quota{mkUniq<Quota>()}, mp_state{lua_newstate(quotaAlloc, mp_quota.get(), luaL_makeseed(nullptr))} {
	OWL_PROFILE_FUNCTION()

	setQuotas(iQuotas);
	if (mp_state == nullptr) {
		OWL_CORE_ERROR("LuaEngine: Failed to create Lua state.")
		mp_quota->lastStatus = LuaStatus::Invalid;
		return;
	}
	lua_atpanic(mp_state, panicHandler);
	lua_setwarnf(mp_state, warningHandler, mp_quota.get());
	openSandboxedLibraries(mp_state);
	OWL_CORE_TRACE("LuaEngine: Lua state created successfully.")
}

LuaEngine::~LuaEngine() {
	if (mp_state != nullptr) {
		lua_close(mp_state);
		mp_state = nullptr;
	}
}

auto LuaEngine::isValid() const -> bool { return mp_state != nullptr; }

void LuaEngine::setModuleReader(const ModuleReader& iReader) const { mp_quota->moduleReader = iReader; }

void LuaEngine::setQuotas(const ScriptQuotas& iQuotas) const {
	mp_quota->memoryLimit = iQuotas.memoryBytes;
	mp_quota->timeBudgetMs = iQuotas.timePerCallMs;
}

auto LuaEngine::getQuotas() const -> ScriptQuotas {
	return {.memoryBytes = mp_quota->memoryLimit, .timePerCallMs = mp_quota->timeBudgetMs};
}

auto LuaEngine::getMemoryUsage() const -> size_t { return mp_quota->memoryUsed; }

auto LuaEngine::getLastStatus() const -> LuaStatus { return mp_quota->lastStatus; }

void LuaEngine::registerGuardedTable(lua_State* iState, const char* iTableName, const luaL_Reg* iFunctions) {
	lua_pushglobaltable(iState);
	lua_newtable(iState);
	for (const auto* reg = iFunctions; reg->name != nullptr; ++reg) {
		lua_pushcfunction(iState, reg->func);
		lua_pushcclosure(iState, guardedBinding, 1);
		lua_setfield(iState, -2, reg->name);
	}
	lua_setfield(iState, -2, iTableName);
	lua_pop(iState, 1);
}

void LuaEngine::restartCallClock(lua_State* iState) {
	if (quotaOf(iState)->timeExceeded)
		return;
	auto& slot = currentSlot();
	// A new sequence restarts the watchdog count and makes a signal already on its way stand down.
	slot.sequence.store(slot.sequence.load(std::memory_order_relaxed) + 1, std::memory_order_release);
	for (lua_State* state: {iState, slot.running.load(std::memory_order_relaxed)})
		if (state != nullptr && lua_gethook(state) == overdueHook)
			armDebugHook(state);
}

auto LuaEngine::callGuarded(lua_State* iState, int (*iFunction)(lua_State*)) -> int {
	auto* quota = quotaOf(iState);
	const bool enforced = quota->enforced;
	quota->enforced = false;
	bool failed = false;
	int results = 0;
	try {
		results = iFunction(iState);
	} catch (const std::exception& exception) {
		lua_pushfstring(iState, "C++ exception: %s", exception.what());
		failed = true;
	} catch (...) {
		lua_pushliteral(iState, "unknown C++ exception");
		failed = true;
	}
	quota->enforced = enforced;
	return failed ? lua_error(iState) : results;
}

void LuaEngine::registerTable(lua_State* iState, const char* iTableName, const luaL_Reg* iFunctions, const int iCount) {
	lua_pushglobaltable(iState);
	lua_createtable(iState, 0, iCount);
	luaL_setfuncs(iState, iFunctions, 0);
	lua_setfield(iState, -2, iTableName);
	lua_pop(iState, 1);
}

void LuaEngine::setHostPointer(lua_State* iState, void* iPointer) { quotaOf(iState)->hostPointer = iPointer; }

auto LuaEngine::getHostPointer(lua_State* iState) -> void* { return quotaOf(iState)->hostPointer; }

void LuaEngine::setDeltaTime(const double iDeltaTime) const { mp_quota->deltaTime = iDeltaTime; }

auto LuaEngine::getDeltaTime(lua_State* iState) -> double { return quotaOf(iState)->deltaTime; }

auto LuaEngine::protectedCall(const int iHandler, const int iArgCount, const std::string_view iWhat,
							  ChunkResult* oResult) const -> bool {
	mp_quota->memoryExceeded = false;
	mp_quota->timeExceeded = false;
	armDebugHook(mp_state);
	auto& slot = currentSlot();
	lua_State* const outerState = slot.state.load(std::memory_order_relaxed);
	lua_State* const outerRunning = slot.running.load(std::memory_order_relaxed);
	const uint32_t outerBudget = slot.budgetMs.load(std::memory_order_relaxed);
	slot.budgetMs.store(mp_quota->timeBudgetMs, std::memory_order_relaxed);
	slot.running.store(nullptr, std::memory_order_relaxed);
	slot.state.store(mp_state, std::memory_order_relaxed);
	slot.sequence.store(slot.sequence.load(std::memory_order_relaxed) + 1, std::memory_order_release);
	const bool outerEnforced = mp_quota->enforced;
	mp_quota->enforced = true;
	const int result = lua_pcall(mp_state, iArgCount, oResult == nullptr ? 0 : LUA_MULTRET, iHandler);
	mp_quota->enforced = outerEnforced;
	slot.state.store(outerState, std::memory_order_relaxed);
	slot.running.store(outerRunning, std::memory_order_relaxed);
	slot.budgetMs.store(outerBudget, std::memory_order_relaxed);
	if (result == LUA_OK) {
		if (oResult != nullptr)
			for (int index = iHandler + 1; index <= lua_gettop(mp_state); ++index) {
				size_t size = 0;
				const char* text = luaL_tolstring(mp_state, index, &size);
				oResult->values.emplace_back(text, size);
				lua_pop(mp_state, 1);
			}
		lua_settop(mp_state, iHandler - 1);
		mp_quota->lastStatus = LuaStatus::Ok;
		return true;
	}
	if (mp_quota->timeExceeded)
		mp_quota->lastStatus = LuaStatus::TimeQuota;
	else if (result == LUA_ERRMEM && mp_quota->memoryExceeded)
		mp_quota->lastStatus = LuaStatus::MemoryQuota;
	else
		mp_quota->lastStatus = LuaStatus::RuntimeError;
	const char* message = lua_tostring(mp_state, -1);
	if (oResult != nullptr)
		oResult->message = message != nullptr ? message : "(no message)";
	else
		reportError(iWhat, message != nullptr ? message : "(no message)");
	lua_settop(mp_state, iHandler - 1);
	return false;
}

auto LuaEngine::runChunk(const std::string_view iSource, const std::string& iName) const -> ChunkResult {
	ChunkResult result;
	if (mp_state == nullptr) {
		result.message = "the Lua state is invalid";
		return result;
	}
	mp_quota->enforced = true;
	const int loaded = luaL_loadbufferx(mp_state, iSource.data(), iSource.size(), iName.c_str(), "t");
	mp_quota->enforced = false;
	if (loaded != LUA_OK) {
		const char* message = lua_tostring(mp_state, -1);
		result.message = message != nullptr ? message : "(no message)";
		lua_pop(mp_state, 1);
		return result;
	}
	result.compiled = true;
	lua_pushcfunction(mp_state, messageHandler);
	lua_insert(mp_state, -2);
	result.ok = protectedCall(lua_gettop(mp_state) - 1, 0, iName, &result);
	return result;
}

void LuaEngine::reportError(const std::string_view iWhat, const std::string_view iMessage) const {
	// A callback failing every frame would flood the log: the same error is logged once, then a counter once a second.
	const auto now = std::chrono::steady_clock::now();
	const auto key = std::format("{}{}: {}", iWhat, errorContextOf(*mp_quota), iMessage);
	if (key != mp_quota->lastError) {
		mp_quota->lastError = key;
		mp_quota->repeatsPending = 0;
		mp_quota->lastReport = now;
		OWL_CORE_ERROR("LuaEngine: Error in {}{}: {}. Fix: {}.", iWhat, errorContextOf(*mp_quota), iMessage,
					   fixHint(mp_quota->lastStatus))
		return;
	}
	++mp_quota->repeatsPending;
	++mp_quota->repeatsSuppressed;
	if (now - mp_quota->lastReport >= std::chrono::seconds{1}) {
		OWL_CORE_ERROR("LuaEngine: Error in {}{} repeated {} more times.", iWhat, errorContextOf(*mp_quota),
					   mp_quota->repeatsPending)
		mp_quota->repeatsPending = 0;
		mp_quota->lastReport = now;
	}
}

auto LuaEngine::getSuppressedErrorCount() const -> size_t { return mp_quota->repeatsSuppressed; }

void LuaEngine::setErrorContext(const std::function<std::string()>& iContext) const {
	mp_quota->errorContext = iContext;
}

auto LuaEngine::runLoadedChunk(const int iLoadResult, const std::string& iName) const -> bool {
	if (iLoadResult != LUA_OK) {
		mp_quota->lastStatus = iLoadResult == LUA_ERRMEM ? LuaStatus::MemoryQuota : LuaStatus::LoadError;
		const char* message = lua_tostring(mp_state, -1);
		OWL_CORE_ERROR("LuaEngine: Error loading '{}'{}: {}. Fix: {}.", iName, errorContextOf(*mp_quota),
					   message != nullptr ? message : "(no message)", fixHint(mp_quota->lastStatus))
		lua_pop(mp_state, 1);
		return false;
	}
	lua_pushcfunction(mp_state, messageHandler);
	lua_insert(mp_state, -2);
	return protectedCall(lua_gettop(mp_state) - 1, 0, std::format("chunk '{}'", iName));
}

auto LuaEngine::loadScript(const std::filesystem::path& iPath) const -> bool {
	OWL_PROFILE_FUNCTION()

	if (mp_state == nullptr) {
		OWL_CORE_ERROR("LuaEngine: Cannot load script, Lua state is invalid.")
		return false;
	}
	if (visual::isVisualScriptPath(iPath.string())) {
		const auto text = platform::fileToString(iPath);
		return loadBuffer({text.begin(), text.end()}, iPath.string());
	}
	mp_quota->enforced = true;
	const int result = luaL_loadfilex(mp_state, iPath.string().c_str(), "t");
	mp_quota->enforced = false;
	return runLoadedChunk(result, iPath.string());
}

auto LuaEngine::loadBuffer(const std::vector<uint8_t>& iData, const std::string& iName) const -> bool {
	OWL_PROFILE_FUNCTION()

	if (mp_state == nullptr) {
		OWL_CORE_ERROR("LuaEngine: Cannot load buffer, Lua state is invalid.")
		return false;
	}
	if (visual::isVisualScriptPath(iName)) {
		const auto lua = visual::compileText({reinterpret_cast<const char*>(iData.data()), iData.size()}, iName,
											 ScriptDebugger::isEnabled() ? visual::CompileMode::Debug
																		 : visual::CompileMode::Game);
		if (!lua.has_value()) {
			OWL_CORE_ERROR("LuaEngine: Cannot compile the visual script '{}'.", iName)
			return false;
		}
		mp_quota->enforced = true;
		const int result = luaL_loadbufferx(mp_state, lua->data(), lua->size(), iName.c_str(), "t");
		mp_quota->enforced = false;
		return runLoadedChunk(result, iName);
	}
	const auto* data = reinterpret_cast<const char*>(iData.data());
	mp_quota->enforced = true;
	const int result = luaL_loadbufferx(mp_state, data, iData.size(), iName.c_str(), "t");
	mp_quota->enforced = false;
	return runLoadedChunk(result, iName);
}

void LuaEngine::pushRawGlobal(const std::string& iName) const {
	lua_pushglobaltable(mp_state);
	pushName(iName);
	lua_rawget(mp_state, -2);
	lua_replace(mp_state, -2);
}

void LuaEngine::popRawGlobal(const std::string& iName) const {
	lua_pushglobaltable(mp_state);
	pushName(iName);
	lua_rotate(mp_state, -3, -1);
	lua_rawset(mp_state, -3);
	lua_pop(mp_state, 1);
}

void LuaEngine::pushName(const std::string_view iName) const {
	auto& refs = mp_quota->nameRefs;
	if (const auto cached = std::ranges::find(refs, iName, &std::pair<std::string, int>::first); cached != refs.end()) {
		lua_rawgeti(mp_state, LUA_REGISTRYINDEX, cached->second);
		return;
	}
	lua_pushlstring(mp_state, iName.data(), iName.size());
	if (refs.size() < g_MaxCachedNames) {
		lua_pushvalue(mp_state, -1);
		refs.emplace_back(iName, luaL_ref(mp_state, LUA_REGISTRYINDEX));
	}
}

auto LuaEngine::pushGlobalFunction(const std::string_view iName) const -> int {
	lua_pushcfunction(mp_state, messageHandler);
	const int handler = lua_gettop(mp_state);
	lua_pushglobaltable(mp_state);
	pushName(iName);
	if (lua_rawget(mp_state, -2) == LUA_TFUNCTION)
		return handler;
	lua_settop(mp_state, handler - 1);
	mp_quota->lastStatus = LuaStatus::Missing;
	return 0;
}

auto LuaEngine::hasFunction(const std::string& iName) const -> bool {
	if (mp_state == nullptr)
		return false;
	pushRawGlobal(iName);
	const bool isFunc = lua_isfunction(mp_state, -1) != 0;
	lua_pop(mp_state, 1);
	return isFunc;
}

auto LuaEngine::callFunction(const std::string_view iName) const -> bool {
	OWL_PROFILE_FUNCTION()

	if (mp_state == nullptr)
		return false;
	const int handler = pushGlobalFunction(iName);
	return handler != 0 && protectedCall(handler, 0, iName);
}

auto LuaEngine::callFunction(const std::string_view iName, const float iArg) const -> bool {
	OWL_PROFILE_FUNCTION()

	if (mp_state == nullptr)
		return false;
	const int handler = pushGlobalFunction(iName);
	if (handler == 0)
		return false;
	lua_pushnumber(mp_state, static_cast<lua_Number>(iArg));
	return protectedCall(handler, 1, iName);
}

auto LuaEngine::callFunction(const std::string_view iName, const uint64_t iArg) const -> bool {
	OWL_PROFILE_FUNCTION()

	if (mp_state == nullptr)
		return false;
	const int handler = pushGlobalFunction(iName);
	if (handler == 0)
		return false;
	lua_pushinteger(mp_state, static_cast<lua_Integer>(iArg));
	return protectedCall(handler, 1, iName);
}

// ---- Global variable getters ----
// NOLINTNEXTLINE(misc-no-recursion) A table describes its entries once, without entering a nested table.
auto LuaEngine::describeValue(lua_State* iState, const int iIndex) -> WatchValue {
	WatchValue value{.name = {}, .type = luaL_typename(iState, iIndex), .value = {}};
	switch (lua_type(iState, iIndex)) {
		case LUA_TNIL:
			value.value = "nil";
			break;
		case LUA_TBOOLEAN:
			value.value = lua_toboolean(iState, iIndex) != 0 ? "true" : "false";
			break;
		case LUA_TNUMBER:
			if (lua_isinteger(iState, iIndex) != 0)
				value.value = std::format("{}", lua_tointeger(iState, iIndex));
			else
				value.value = std::format("{:.6g}", lua_tonumber(iState, iIndex));
			break;
		case LUA_TSTRING:
			{
				size_t size = 0;
				const char* text = lua_tolstring(iState, iIndex, &size);
				value.value = std::format("\"{}\"", std::string_view{text, std::min(size, g_maxValueText)});
			}
			break;
		case LUA_TTABLE:
			value.value = shortTable(iState, iIndex);
			break;
		default:
			value.value = value.type;
			break;
	}
	return value;
}

auto LuaEngine::readGlobals() const -> std::vector<WatchValue> {
	std::vector<WatchValue> values;
	if (mp_state == nullptr)
		return values;
	lua_pushglobaltable(mp_state);
	lua_pushnil(mp_state);
	while (lua_next(mp_state, -2) != 0) {
		if (lua_type(mp_state, -2) == LUA_TSTRING && lua_type(mp_state, -1) != LUA_TFUNCTION) {
			auto value = describeValue(mp_state, -1);
			value.name = lua_tostring(mp_state, -2);
			values.push_back(std::move(value));
		}
		lua_pop(mp_state, 1);
	}
	lua_pop(mp_state, 1);
	std::ranges::sort(values, {}, &WatchValue::name);
	return values;
}

auto LuaEngine::readGlobalTable(const std::string& iName) const -> std::vector<WatchValue> {
	std::vector<WatchValue> values;
	if (mp_state == nullptr)
		return values;
	pushRawGlobal(iName);
	if (lua_istable(mp_state, -1)) {
		lua_pushnil(mp_state);
		while (lua_next(mp_state, -2) != 0) {
			if (lua_type(mp_state, -2) == LUA_TSTRING) {
				auto value = describeValue(mp_state, -1);
				value.name = lua_tostring(mp_state, -2);
				values.push_back(std::move(value));
			}
			lua_pop(mp_state, 1);
		}
	}
	lua_pop(mp_state, 1);
	std::ranges::sort(values, {}, &WatchValue::name);
	return values;
}

auto LuaEngine::getGlobalFloat(const std::string& iName) const -> std::optional<float> {
	if (mp_state == nullptr)
		return std::nullopt;
	pushRawGlobal(iName);
	if (lua_isnumber(mp_state, -1) == 0) {
		lua_pop(mp_state, 1);
		return std::nullopt;
	}
	const auto val = static_cast<float>(lua_tonumber(mp_state, -1));
	lua_pop(mp_state, 1);
	return val;
}

auto LuaEngine::getGlobalInt(const std::string& iName) const -> std::optional<int64_t> {
	if (mp_state == nullptr)
		return std::nullopt;
	pushRawGlobal(iName);
	if (lua_isinteger(mp_state, -1) == 0) {
		lua_pop(mp_state, 1);
		return std::nullopt;
	}
	const auto val = static_cast<int64_t>(lua_tointeger(mp_state, -1));
	lua_pop(mp_state, 1);
	return val;
}

auto LuaEngine::getGlobalString(const std::string& iName) const -> std::optional<std::string> {
	if (mp_state == nullptr)
		return std::nullopt;
	pushRawGlobal(iName);
	if (lua_isstring(mp_state, -1) == 0) {
		lua_pop(mp_state, 1);
		return std::nullopt;
	}
	std::string val = lua_tostring(mp_state, -1);
	lua_pop(mp_state, 1);
	return val;
}

auto LuaEngine::getGlobalBool(const std::string& iName) const -> std::optional<bool> {
	if (mp_state == nullptr)
		return std::nullopt;
	pushRawGlobal(iName);
	if (lua_isboolean(mp_state, -1) == 0) {
		lua_pop(mp_state, 1);
		return std::nullopt;
	}
	const bool val = lua_toboolean(mp_state, -1) != 0;
	lua_pop(mp_state, 1);
	return val;
}

// ---- Global variable setters ----
void LuaEngine::setGlobal(const std::string& iName, const float iValue) const {
	if (mp_state == nullptr)
		return;
	lua_pushnumber(mp_state, static_cast<lua_Number>(iValue));
	popRawGlobal(iName);
}

void LuaEngine::setGlobal(const std::string& iName, const int64_t iValue) const {
	if (mp_state == nullptr)
		return;
	lua_pushinteger(mp_state, static_cast<lua_Integer>(iValue));
	popRawGlobal(iName);
}

void LuaEngine::setGlobal(const std::string& iName, const std::string& iValue) const {
	if (mp_state == nullptr)
		return;
	lua_pushlstring(mp_state, iValue.data(), iValue.size());
	popRawGlobal(iName);
}

void LuaEngine::setGlobal(const std::string& iName, const bool iValue) const {
	if (mp_state == nullptr)
		return;
	lua_pushboolean(mp_state, iValue ? 1 : 0);
	popRawGlobal(iName);
}

void LuaEngine::setGlobalNumbers(const std::string& iName, const std::span<const double> iValues,
								 const std::span<const char* const> iKeys) const {
	if (mp_state == nullptr)
		return;
	lua_createtable(mp_state, static_cast<int>(iValues.size()), static_cast<int>(iKeys.size()));
	for (size_t index = 0; index < iValues.size(); ++index) {
		lua_pushnumber(mp_state, static_cast<lua_Number>(iValues[index]));
		if (index < iKeys.size())
			lua_setfield(mp_state, -2, iKeys[index]);
		else
			lua_rawseti(mp_state, -2, static_cast<lua_Integer>(index) + 1);
	}
	popRawGlobal(iName);
}

void LuaEngine::setGlobalStrings(const std::string& iName, const std::span<const std::string> iValues) const {
	if (mp_state == nullptr)
		return;
	lua_createtable(mp_state, static_cast<int>(iValues.size()), 0);
	for (size_t index = 0; index < iValues.size(); ++index) {
		lua_pushlstring(mp_state, iValues[index].data(), iValues[index].size());
		lua_rawseti(mp_state, -2, static_cast<lua_Integer>(index) + 1);
	}
	popRawGlobal(iName);
}

auto LuaEngine::getState() const -> lua_State* { return mp_state; }

}// namespace owl::script
