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

#include <atomic>
#include <condition_variable>
#include <cstdlib>
#include <exception>
#include <format>
#include <mutex>
#include <stop_token>
#include <thread>

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
};

namespace {

constexpr uint32_t g_WatchdogTickMs = 10;

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

void overdueHook(lua_State* iState, [[maybe_unused]] lua_Debug* iDebug) {
	auto* quota = quotaOf(iState);
	if (!quota->enforced)
		return;
	quota->timeExceeded = true;
	// The hook stays armed on every instruction, so a script catching the error with pcall cannot loop on.
	luaL_error(iState, "time budget exceeded (%d ms per call)", static_cast<int>(quota->timeBudgetMs));
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

	~Watchdog() = default;

	static auto instance() -> Watchdog& {
		static Watchdog watchdog;
		return watchdog;
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
		pthread_kill(ioSlot.thread, signalNumber());
#else
		// No thread-directed signal here: set the hook from this thread, as the reference interpreter does.
		interruptIfOverdue(ioSlot);
#endif
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

constexpr const char* g_WrapSource = "local create, resume, pack, unpack, err = coroutine.create, coroutine.resume, "
									 "table.pack, table.unpack, error\n"
									 "coroutine.wrap = function(f)\n"
									 "  local co = create(f)\n"
									 "  return function(...)\n"
									 "    local r = pack(resume(co, ...))\n"
									 "    if not r[1] then err(r[2], 0) end\n"
									 "    return unpack(r, 2, r.n)\n"
									 "  end\n"
									 "end\n";

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
	auto* quota = quotaOf(iState);
	const bool enforced = quota->enforced;
	quota->enforced = false;
	const auto function = lua_tocfunction(iState, lua_upvalueindex(1));
	bool failed = false;
	int results = 0;
	try {
		results = function(iState);
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
	lua_pop(iState, 2);
	if (luaL_loadbufferx(iState, g_WrapSource, std::char_traits<char>::length(g_WrapSource), "=sandbox", "t") !=
				LUA_OK ||
		lua_pcall(iState, 0, 0, 0) != LUA_OK) {
		OWL_CORE_ERROR("LuaEngine: Failed to install coroutine.wrap: {}.", lua_tostring(iState, -1))
		lua_pop(iState, 1);
	}

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
	for (const auto* reg = iFunctions; reg->name != nullptr; ++reg) {// NOLINT(*-pointer-arithmetic) luaL_Reg list.
		lua_pushcfunction(iState, reg->func);
		lua_pushcclosure(iState, guardedBinding, 1);
		lua_setfield(iState, -2, reg->name);
	}
	lua_setfield(iState, -2, iTableName);
	lua_pop(iState, 1);
}

auto LuaEngine::protectedCall(const int iArgCount, const std::string& iWhat) const -> bool {
	const int base = lua_gettop(mp_state) - iArgCount;
	lua_pushcfunction(mp_state, messageHandler);
	lua_insert(mp_state, base);
	mp_quota->memoryExceeded = false;
	mp_quota->timeExceeded = false;
	if (lua_gethook(mp_state) != nullptr)
		lua_sethook(mp_state, nullptr, 0, 0);
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
	const int result = lua_pcall(mp_state, iArgCount, 0, base);
	mp_quota->enforced = outerEnforced;
	slot.state.store(outerState, std::memory_order_relaxed);
	slot.running.store(outerRunning, std::memory_order_relaxed);
	slot.budgetMs.store(outerBudget, std::memory_order_relaxed);
	if (result == LUA_OK) {
		lua_pop(mp_state, 1);
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
	OWL_CORE_ERROR("LuaEngine: Error in {}: {}.", iWhat, message != nullptr ? message : "(no message)")
	lua_pop(mp_state, 2);
	return false;
}

auto LuaEngine::runLoadedChunk(const int iLoadResult, const std::string& iName) const -> bool {
	if (iLoadResult != LUA_OK) {
		mp_quota->lastStatus = iLoadResult == LUA_ERRMEM ? LuaStatus::MemoryQuota : LuaStatus::LoadError;
		const char* message = lua_tostring(mp_state, -1);
		OWL_CORE_ERROR("LuaEngine: Error loading '{}': {}.", iName, message != nullptr ? message : "(no message)")
		lua_pop(mp_state, 1);
		return false;
	}
	return protectedCall(0, std::format("chunk '{}'", iName));
}

auto LuaEngine::loadScript(const std::filesystem::path& iPath) const -> bool {
	OWL_PROFILE_FUNCTION()

	if (mp_state == nullptr) {
		OWL_CORE_ERROR("LuaEngine: Cannot load script, Lua state is invalid.")
		return false;
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
	const auto* data = reinterpret_cast<const char*>(iData.data());
	mp_quota->enforced = true;
	const int result = luaL_loadbufferx(mp_state, data, iData.size(), iName.c_str(), "t");
	mp_quota->enforced = false;
	return runLoadedChunk(result, iName);
}

void LuaEngine::pushRawGlobal(const std::string& iName) const {
	lua_pushglobaltable(mp_state);
	lua_pushlstring(mp_state, iName.data(), iName.size());
	lua_rawget(mp_state, -2);
	lua_remove(mp_state, -2);
}

void LuaEngine::popRawGlobal(const std::string& iName) const {
	lua_pushglobaltable(mp_state);
	lua_pushlstring(mp_state, iName.data(), iName.size());
	lua_rotate(mp_state, -3, -1);
	lua_rawset(mp_state, -3);
	lua_pop(mp_state, 1);
}

auto LuaEngine::pushGlobalFunction(const std::string& iName) const -> bool {
	pushRawGlobal(iName);
	if (lua_isfunction(mp_state, -1) != 0)
		return true;
	lua_pop(mp_state, 1);
	mp_quota->lastStatus = LuaStatus::Missing;
	return false;
}

auto LuaEngine::hasFunction(const std::string& iName) const -> bool {
	if (mp_state == nullptr)
		return false;
	pushRawGlobal(iName);
	const bool isFunc = lua_isfunction(mp_state, -1) != 0;
	lua_pop(mp_state, 1);
	return isFunc;
}

auto LuaEngine::callFunction(const std::string& iName) const -> bool {
	OWL_PROFILE_FUNCTION()

	if (mp_state == nullptr || !pushGlobalFunction(iName))
		return false;
	return protectedCall(0, iName);
}

auto LuaEngine::callFunction(const std::string& iName, const float iArg) const -> bool {
	OWL_PROFILE_FUNCTION()

	if (mp_state == nullptr || !pushGlobalFunction(iName))
		return false;
	lua_pushnumber(mp_state, static_cast<lua_Number>(iArg));
	return protectedCall(1, iName);
}

auto LuaEngine::callFunction(const std::string& iName, const uint64_t iArg) const -> bool {
	OWL_PROFILE_FUNCTION()

	if (mp_state == nullptr || !pushGlobalFunction(iName))
		return false;
	lua_pushinteger(mp_state, static_cast<lua_Integer>(iArg));
	return protectedCall(1, iName);
}

// ---- Global variable getters ----
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

auto LuaEngine::getState() const -> lua_State* { return mp_state; }

}// namespace owl::script
