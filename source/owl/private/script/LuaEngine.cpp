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
#include "script/LuaValues.h"
#include "script/ScriptDebugger.h"
#include "script/visual/VisualScript.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <format>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>


namespace owl::script {

struct LuaEngine::Quota {
	size_t memoryLimit = 0;
	size_t memoryUsed = 0;
	uint32_t timeBudgetMs = 0;
	bool enforced = false;
	bool memoryExceeded = false;
	bool timeExceeded = false;
	LuaStatus lastStatus = LuaStatus::Ok;
	std::function<std::string()> errorContext;
	ModuleReader moduleReader;
	void* hostPointer = nullptr;
	double deltaTime = 0.0;
	// Registry references of the interned names of the functions the host calls, to skip hashing them per call.
	std::vector<std::pair<std::string, int>> nameRefs;
	// Registry reference of the message handler: a Luau C function pushed anew is a new closure to allocate.
	int messageHandlerRef = LUA_NOREF;
	// The last error logged in full, how many times it came back since the last report, and how many were left out.
	std::string lastError;
	uint32_t repeatsPending = 0;
	size_t repeatsSuppressed = 0;
	std::chrono::steady_clock::time_point lastReport;
	// Registry references of the loaded chunks, by chunk name, and the breakpoint lines set in each.
	std::vector<std::pair<std::string, int>> chunks;
	std::map<int, std::vector<int>> breakpointLines;
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

// The entries of a table for the debugger: numeric keys first in order, then the others by name, nested tables short.
auto tableEntries(lua_State* iState, const int iIndex) -> std::vector<WatchEntry> {
	constexpr size_t maxEntries = 64;
	const int table = lua_absindex(iState, iIndex);
	std::vector<std::pair<double, WatchEntry>> numbered;
	std::vector<WatchEntry> named;
	lua_pushnil(iState);
	// At the limit the loop leaves the last key on the stack, popped below.
	while (numbered.size() + named.size() < maxEntries && lua_next(iState, table) != 0) {
		WatchEntry entry{.name = {},
						 .value = lua_type(iState, -1) == LUA_TTABLE ? shortTable(iState, -1)
																	 : LuaEngine::describeValue(iState, -1).value};
		if (lua_type(iState, -2) == LUA_TNUMBER) {
			entry.name = LuaEngine::describeValue(iState, -2).value;
			numbered.emplace_back(lua_tonumber(iState, -2), std::move(entry));
		} else {
			entry.name = lua_type(iState, -2) == LUA_TSTRING ? lua_tostring(iState, -2) : luaL_typename(iState, -2);
			named.push_back(std::move(entry));
		}
		lua_pop(iState, 1);
	}
	if (numbered.size() + named.size() >= maxEntries)
		lua_pop(iState, 1);
	std::ranges::sort(numbered, {}, &std::pair<double, WatchEntry>::first);
	std::ranges::sort(named, {}, &WatchEntry::name);
	std::vector<WatchEntry> entries;
	entries.reserve(numbered.size() + named.size());
	for (auto& [key, entry]: numbered) entries.push_back(std::move(entry));
	for (auto& entry: named) entries.push_back(std::move(entry));
	return entries;
}

// A value for the debugger, the entries of a table with it.
auto watchValue(lua_State* iState, const int iIndex) -> WatchValue {
	auto value = LuaEngine::describeValue(iState, iIndex);
	if (lua_type(iState, iIndex) == LUA_TTABLE)
		value.entries = tableEntries(iState, iIndex);
	return value;
}

// Called on a line holding a breakpoint: the hit is reported and the call goes on, the game pauses after it.
void breakCallback(lua_State* iState, lua_Debug* iDebug) {
	if (lua_getinfo(iState, 0, "sl", iDebug) == 0 || !ScriptDebugger::isBreakpoint(iDebug->source, iDebug->currentline))
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
		const char* name = lua_getlocal(iState, 0, index);
		if (name == nullptr)
			break;
		if (name[0] != '(' && !std::string_view{name}.starts_with("__")) {
			auto value = watchValue(iState, -1);
			value.name = name;
			hit.locals.push_back(std::move(value));
		}
		lua_pop(iState, 1);
	}
	quota->enforced = enforced;
	ScriptDebugger::reportHit(std::move(hit));
}

// Sets in every loaded chunk the breakpoints the debugger holds for it, and clears those it dropped.
void syncBreakpoints(lua_State* iState, LuaEngine::Quota& ioQuota) {
	if (ioQuota.breakpointLines.empty() && !ScriptDebugger::hasBreakpoints())
		return;
	for (const auto& [name, ref]: ioQuota.chunks) {
		auto wanted = ScriptDebugger::getBreakpoints(name);
		std::ranges::sort(wanted);
		auto& set = ioQuota.breakpointLines[ref];
		if (set == wanted)
			continue;
		lua_rawgeti(iState, LUA_REGISTRYINDEX, ref);
		for (const int line: set)
			if (!std::ranges::binary_search(wanted, line))
				lua_breakpoint(iState, -1, line, 0);
		for (const int line: wanted)
			if (!std::ranges::binary_search(set, line))
				lua_breakpoint(iState, -1, line, 1);
		lua_pop(iState, 1);
		set = std::move(wanted);
	}
}

// Compiles a chunk and pushes its function, or an error message; the function is kept for its breakpoints.
auto loadChunk(lua_State* iState, const std::string_view iSource, const std::string& iChunkName) -> int {
	lua_CompileOptions options{};
	options.optimizationLevel = 1;
	options.debugLevel = ScriptDebugger::isEnabled() ? 2 : 1;
	size_t size = 0;
	char* bytecode = luau_compile(iSource.data(), iSource.size(), &options, &size);
	const int result = luau_load(iState, iChunkName.c_str(), bytecode, size, 0);
	std::free(bytecode);// NOLINT(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory) luau_compile contract.
	if (result != 0)
		return LUA_ERRSYNTAX;
	auto* quota = quotaOf(iState);
	std::string name = iChunkName;
	if (!name.empty() && (name.front() == '@' || name.front() == '='))
		name.erase(0, 1);
	quota->chunks.emplace_back(std::move(name), lua_ref(iState, -1));
	syncBreakpoints(iState, *quota);
	return LUA_OK;
}

struct CallSlot {
	std::atomic<lua_State*> state{nullptr};
	std::atomic<lua_State*> running{nullptr};
	std::atomic<uint64_t> sequence{0};
	// The sequence of the call the watchdog found overdue; no call has the initial value.
	std::atomic<uint64_t> targetSequence{~uint64_t{0}};
	std::atomic<uint32_t> budgetMs{0};
	// Set while the watchdog installs the interrupt callback: the call keeps its state open until it is done.
	std::atomic<bool> arming{false};
	uint64_t lastSeen = 0;
	std::chrono::steady_clock::time_point staleSince;
	uint64_t firedSequence = 0;
};

thread_local CallSlot* t_slot = nullptr;

// Installed by the watchdog on an overdue call only (Luau calls it at every loop iteration): raises the error there.
void interruptCallback(lua_State* iState, const int iGc) {
	if (iGc >= 0)
		return;
	auto* quota = quotaOf(iState);
	if (!quota->enforced)
		return;
	if (!quota->timeExceeded) {
		const auto* slot = t_slot;
		if (slot == nullptr ||
			slot->sequence.load(std::memory_order_relaxed) != slot->targetSequence.load(std::memory_order_relaxed))
			return;
		quota->timeExceeded = true;
	}
	// Raised again at every safepoint, so a script catching the error with pcall cannot loop on.
	luaL_error(iState, "time budget exceeded (%d ms per call)", static_cast<int>(quota->timeBudgetMs));
}

class Watchdog final {
public:
	Watchdog(const Watchdog&) = delete;

	Watchdog(Watchdog&&) = delete;

	auto operator=(const Watchdog&) -> Watchdog& = delete;

	auto operator=(Watchdog&&) -> Watchdog& = delete;

	Watchdog() {
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
		const auto now = std::chrono::steady_clock::now();
		if (ioSlot.state.load(std::memory_order_relaxed) == nullptr || budget == 0 || sequence != ioSlot.lastSeen) {
			ioSlot.lastSeen = sequence;
			ioSlot.staleSince = now;
			return;
		}
		// Elapsed time, not a tick count: a late tick (coarse Windows timer, loaded machine) would stretch the budget.
		if (now - ioSlot.staleSince < std::chrono::milliseconds(budget) || ioSlot.firedSequence == sequence)
			return;
		ioSlot.firedSequence = sequence;
		// The script reads it at its next safepoint (interruptCallback).
		ioSlot.targetSequence.store(sequence, std::memory_order_relaxed);
		ioSlot.arming.store(true);
		if (auto* state = ioSlot.state.load(); state != nullptr && ioSlot.sequence.load() == sequence)
			lua_callbacks(state)->interrupt = interruptCallback;
		ioSlot.arming.store(false);
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

void panicHandler(lua_State* iState, [[maybe_unused]] const int iErrorCode) {
	const char* message = lua_tostring(iState, -1);
	OWL_CORE_CRITICAL("Lua: Unprotected Lua error: {}.", message != nullptr ? message : "(not a string)")
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

// Sets the functions of a registration list into the table on top of the stack.
void setFunctions(lua_State* iState, const luaL_Reg* iFunctions) {
	for (const auto* reg = iFunctions; reg->name != nullptr; ++reg) {
		lua_pushcclosurek(iState, reg->func, reg->name, 0, nullptr);
		lua_setfield(iState, -2, reg->name);
	}
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

// load(text [, chunkname [, mode [, env]]]): text only, compiled under the quotas of the script.
auto sandboxLoad(lua_State* iState) -> int {
	size_t size = 0;
	const char* text = luaL_checklstring(iState, 1, &size);
	const std::string name = std::format("={}", luaL_optstring(iState, 2, "load"));
	const std::string_view mode = luaL_optstring(iState, 3, "t");
	if ((size > 0 && text[0] == '\x1b') || mode.find('t') == std::string_view::npos) {
		lua_pushnil(iState);
		lua_pushliteral(iState, "attempt to load a binary chunk: scripts load text only");
		return 2;
	}
	const bool hasEnvironment = lua_istable(iState, 4);
	if (loadChunk(iState, {text, size}, name) != LUA_OK) {
		lua_pushnil(iState);
		lua_insert(iState, -2);
		return 2;
	}
	if (hasEnvironment) {
		lua_pushvalue(iState, 4);
		std::ignore = lua_setfenv(iState, -2);
	}
	return 1;
}

// collectgarbage("count") in kilobytes and collectgarbage("isrunning"); a script never drives the collector.
auto sandboxCollectGarbage(lua_State* iState) -> int {
	const char* name = luaL_optstring(iState, 1, "collect");
	const std::string_view option{name};
	if (option == "count") {
		lua_pushnumber(iState, static_cast<lua_Number>(lua_gc(iState, LUA_GCCOUNT, 0)) +
									   static_cast<lua_Number>(lua_gc(iState, LUA_GCCOUNTB, 0)) / 1024.0);
		return 1;
	}
	if (option == "isrunning") {
		lua_pushboolean(iState, lua_gc(iState, LUA_GCISRUNNING, 0));
		return 1;
	}
	luaL_error(iState, "collectgarbage: option '%s' is not allowed in the sandbox", name);
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
	if (loadChunk(iState, {reinterpret_cast<const char*>(data->data()), data->size()}, chunkName) != LUA_OK) {
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
		luaL_error(iState, "require: '%s' is not a module name (words joined by dots: scripts.game.towers)", name);
	lua_settop(iState, 1);
	lua_getfield(iState, LUA_REGISTRYINDEX, g_ModulesKey);
	if (lua_getfield(iState, 2, name) != LUA_TNIL) {
		if (lua_isboolean(iState, -1) && lua_toboolean(iState, -1) == 0)
			luaL_error(iState, "require: '%s' requires itself while it loads", name);
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
		lua_error(iState);
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
	constexpr std::array<std::pair<const char*, lua_CFunction>, 8> libraries{{{"", luaopen_base},
																			  {LUA_TABLIBNAME, luaopen_table},
																			  {LUA_STRLIBNAME, luaopen_string},
																			  {LUA_MATHLIBNAME, luaopen_math},
																			  {LUA_UTF8LIBNAME, luaopen_utf8},
																			  {LUA_COLIBNAME, luaopen_coroutine},
																			  {LUA_BITLIBNAME, luaopen_bit32},
																			  {LUA_BUFFERLIBNAME, luaopen_buffer}}};
	for (const auto& [name, open]: libraries) {
		lua_pushcfunction(iState, open);
		lua_pushstring(iState, name);
		lua_call(iState, 1, 0);
	}

	lua_pushglobaltable(iState);
	lua_newtable(iState);
	lua_setfield(iState, LUA_REGISTRYINDEX, g_ModulesKey);
	lua_pushcfunction(iState, sandboxRequire);
	lua_setfield(iState, -2, "require");
	lua_pushcfunction(iState, sandboxLoad);
	lua_setfield(iState, -2, "load");
	lua_pushcfunction(iState, sandboxCollectGarbage);
	lua_setfield(iState, -2, "collectgarbage");
	wrapBaseFunction(iState, "pcall", restoringCatcher);
	wrapBaseFunction(iState, "xpcall", restoringCatcher);
	lua_getfield(iState, -1, LUA_COLIBNAME);
	wrapBaseFunction(iState, "resume", restoringCatcher);
	wrapBaseFunction(iState, "close", restoringCatcher);
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

// Lua of a visual script, kept with the graph text it came from: every entity of a scene loads the same few graphs.
struct CompiledGraph {
	std::string source;
	std::string lua;
};

std::mutex g_compiledGraphsMutex;
std::map<std::pair<std::string, visual::CompileMode>, CompiledGraph> g_compiledGraphs;

auto compileGraph(const std::string_view iText, const std::string& iName, const visual::CompileMode iMode)
		-> std::optional<std::string> {
	const std::scoped_lock<std::mutex> lock{g_compiledGraphsMutex};
	if (const auto found = g_compiledGraphs.find({iName, iMode});
		found != g_compiledGraphs.end() && found->second.source == iText)
		return found->second.lua;
	auto lua = visual::compileText(iText, iName, iMode);
	if (lua)
		g_compiledGraphs.insert_or_assign({iName, iMode}, CompiledGraph{.source = std::string{iText}, .lua = *lua});
	return lua;
}

}// namespace

LuaEngine::LuaEngine() : LuaEngine(ScriptEngine::getDefaultQuotas()) {}

LuaEngine::LuaEngine(const ScriptQuotas& iQuotas)
	: mp_quota{mkUniq<Quota>()}, mp_state{lua_newstate(quotaAlloc, mp_quota.get())} {
	OWL_PROFILE_FUNCTION()

	setQuotas(iQuotas);
	if (mp_state == nullptr) {
		OWL_CORE_ERROR("Lua: Failed to create the Lua state.")
		mp_quota->lastStatus = LuaStatus::Invalid;
		return;
	}
	auto* callbacks = lua_callbacks(mp_state);
	callbacks->panic = panicHandler;
	callbacks->debugbreak = breakCallback;
	openSandboxedLibraries(mp_state);
	lua_pushcfunction(mp_state, messageHandler);
	mp_quota->messageHandlerRef = lua_ref(mp_state, -1);
	lua_pop(mp_state, 1);
	OWL_CORE_TRACE("Lua: Lua state created.")
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
		lua_pushcclosurek(iState, reg->func, reg->name, 0, nullptr);
		lua_pushcclosurek(iState, guardedBinding, reg->name, 1, nullptr);
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
	if (failed)
		lua_error(iState);
	return results;
}

void LuaEngine::registerTable(lua_State* iState, const char* iTableName, const luaL_Reg* iFunctions, const int iCount) {
	lua_pushglobaltable(iState);
	if (lua_getfield(iState, -1, iTableName) == LUA_TTABLE) {
		// A standard library table (`math`) keeps its functions, the engine ones replace or extend them.
		setFunctions(iState, iFunctions);
		lua_pop(iState, 2);
		return;
	}
	lua_pop(iState, 1);
	lua_createtable(iState, 0, iCount);
	setFunctions(iState, iFunctions);
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
	syncBreakpoints(mp_state, *mp_quota);
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
	slot.state.store(outerState);
	while (slot.arming.load()) std::this_thread::yield();
	lua_callbacks(mp_state)->interrupt = nullptr;
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
	// Luau collects nothing when an allocation fails: the garbage of the failed call goes now, under the ceiling.
	if (mp_quota->memoryExceeded)
		lua_gc(mp_state, LUA_GCCOLLECT, 0);
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
	const int loaded = loadChunk(mp_state, iSource, "=" + iName);
	mp_quota->enforced = false;
	if (loaded != LUA_OK) {
		const char* message = lua_tostring(mp_state, -1);
		result.message = message != nullptr ? message : "(no message)";
		lua_pop(mp_state, 1);
		return result;
	}
	result.compiled = true;
	lua_rawgeti(mp_state, LUA_REGISTRYINDEX, mp_quota->messageHandlerRef);
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
		OWL_CORE_ERROR("Lua: Error in {}{}: {}. Fix: {}.", iWhat, errorContextOf(*mp_quota), iMessage,
					   fixHint(mp_quota->lastStatus))
		return;
	}
	++mp_quota->repeatsPending;
	++mp_quota->repeatsSuppressed;
	if (now - mp_quota->lastReport >= std::chrono::seconds{1}) {
		OWL_CORE_ERROR("Lua: Error in {}{} repeated {} more times.", iWhat, errorContextOf(*mp_quota),
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
		OWL_CORE_ERROR("Lua: Error loading '{}'{}: {}. Fix: {}.", iName, errorContextOf(*mp_quota),
					   message != nullptr ? message : "(no message)", fixHint(mp_quota->lastStatus))
		lua_pop(mp_state, 1);
		return false;
	}
	lua_rawgeti(mp_state, LUA_REGISTRYINDEX, mp_quota->messageHandlerRef);
	lua_insert(mp_state, -2);
	return protectedCall(lua_gettop(mp_state) - 1, 0, std::format("chunk '{}'", iName));
}

auto LuaEngine::loadScript(const std::filesystem::path& iPath) const -> bool {
	OWL_PROFILE_FUNCTION()

	if (mp_state == nullptr) {
		OWL_CORE_ERROR("Lua: Cannot load the script, the Lua state is invalid.")
		return false;
	}
	if (visual::isVisualScriptPath(iPath.string())) {
		const auto text = platform::fileToString(iPath);
		return loadBuffer({text.begin(), text.end()}, iPath.string());
	}
	std::error_code error;
	if (!std::filesystem::is_regular_file(iPath, error)) {
		mp_quota->lastStatus = LuaStatus::LoadError;
		OWL_CORE_ERROR("Lua: Cannot open '{}'{}. Fix: {}.", iPath.string(), errorContextOf(*mp_quota),
					   fixHint(LuaStatus::LoadError))
		return false;
	}
	const auto text = platform::fileToString(iPath);
	mp_quota->enforced = true;
	const int result = loadChunk(mp_state, text, "@" + iPath.string());
	mp_quota->enforced = false;
	return runLoadedChunk(result, iPath.string());
}

auto LuaEngine::loadBuffer(const std::vector<uint8_t>& iData, const std::string& iName) const -> bool {
	OWL_PROFILE_FUNCTION()

	if (mp_state == nullptr) {
		OWL_CORE_ERROR("Lua: Cannot load the buffer, the Lua state is invalid.")
		return false;
	}
	if (visual::isVisualScriptPath(iName)) {
		const auto lua =
				compileGraph({reinterpret_cast<const char*>(iData.data()), iData.size()}, iName,
							 ScriptDebugger::isEnabled() ? visual::CompileMode::Debug : visual::CompileMode::Game);
		if (!lua.has_value()) {
			OWL_CORE_ERROR("Lua: Cannot compile the visual script '{}'.", iName)
			return false;
		}
		mp_quota->enforced = true;
		const int result = loadChunk(mp_state, *lua, "=" + iName);
		mp_quota->enforced = false;
		return runLoadedChunk(result, iName);
	}
	const auto* data = reinterpret_cast<const char*>(iData.data());
	mp_quota->enforced = true;
	const int result = loadChunk(mp_state, {data, iData.size()}, "=" + iName);
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
	lua_pushvalue(mp_state, -3);
	lua_rawset(mp_state, -3);
	lua_pop(mp_state, 2);
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
		refs.emplace_back(iName, lua_ref(mp_state, -1));
		lua_pop(mp_state, 1);
	}
}

auto LuaEngine::pushGlobalFunction(const std::string_view iName) const -> int {
	lua_rawgeti(mp_state, LUA_REGISTRYINDEX, mp_quota->messageHandlerRef);
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
	pushInteger(mp_state, static_cast<int64_t>(iArg));
	return protectedCall(handler, 1, iName);
}

auto LuaEngine::callFunctionForValue(const std::string_view iName, const uint64_t iEntity, const float iSeconds) const
		-> std::optional<std::string> {
	OWL_PROFILE_FUNCTION()

	if (mp_state == nullptr)
		return std::nullopt;
	const int handler = pushGlobalFunction(iName);
	if (handler == 0)
		return std::nullopt;
	pushInteger(mp_state, static_cast<int64_t>(iEntity));
	lua_pushnumber(mp_state, static_cast<lua_Number>(iSeconds));
	ChunkResult result;
	if (!protectedCall(handler, 2, iName, &result))
		return std::nullopt;
	// The global table the function was read from stays under its results.
	return result.values.size() < 2 ? std::string{"nil"} : std::move(result.values[1]);
}

// NOLINTNEXTLINE(misc-no-recursion) A table describes its entries once, without entering a nested table.
auto LuaEngine::describeValue(lua_State* iState, const int iIndex) -> WatchValue {
	WatchValue value{.name = {}, .type = luaL_typename(iState, iIndex), .value = {}, .entries = {}};
	switch (lua_type(iState, iIndex)) {
		case LUA_TNIL:
			value.value = "nil";
			break;
		case LUA_TBOOLEAN:
			value.value = lua_toboolean(iState, iIndex) != 0 ? "true" : "false";
			break;
		case LUA_TNUMBER:
			if (isInteger(iState, iIndex))
				value.value = std::format("{}", toInteger(iState, iIndex));
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
			auto value = watchValue(mp_state, -1);
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
				auto value = watchValue(mp_state, -1);
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
	if (!isInteger(mp_state, -1)) {
		lua_pop(mp_state, 1);
		return std::nullopt;
	}
	const auto val = static_cast<int64_t>(toInteger(mp_state, -1));
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
	pushInteger(mp_state, static_cast<int64_t>(iValue));
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
			lua_rawseti(mp_state, -2, static_cast<int>(index) + 1);
	}
	popRawGlobal(iName);
}

void LuaEngine::setGlobalStrings(const std::string& iName, const std::span<const std::string> iValues) const {
	if (mp_state == nullptr)
		return;
	lua_createtable(mp_state, static_cast<int>(iValues.size()), 0);
	for (size_t index = 0; index < iValues.size(); ++index) {
		lua_pushlstring(mp_state, iValues[index].data(), iValues[index].size());
		lua_rawseti(mp_state, -2, static_cast<int>(index) + 1);
	}
	popRawGlobal(iName);
}

auto LuaEngine::getState() const -> lua_State* { return mp_state; }

}// namespace owl::script
