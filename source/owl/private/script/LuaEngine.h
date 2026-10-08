/**
 * @file LuaEngine.h
 * @author Silmaen
 * @date 09/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "script/ScriptEngine.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct lua_State;
struct luaL_Reg;

namespace owl::script {
/**
 * @brief
 *  Outcome of the last host call into Lua.
 */
enum struct LuaStatus : uint8_t {
	Ok,///< The call completed.
	Missing,///< The requested global function does not exist.
	LoadError,///< The chunk could not be read or compiled (syntax error, binary chunk, missing file).
	RuntimeError,///< The script raised an error (`error()`, bad argument, C++ exception in a binding).
	MemoryQuota,///< An allocation was refused because the state reached its memory ceiling.
	TimeQuota,///< The call ran longer than its time budget.
	Invalid,///< The Lua state could not be created.
};

/**
 * @brief
 *  What the user can do about a failed load or call.
 * @param[in] iStatus The status of the failed load or call.
 * @return A short imperative sentence fragment, logged after `Fix:`.
 */
[[nodiscard]] auto fixHint(LuaStatus iStatus) -> std::string_view;

/**
 * @brief
 *  Low-level wrapper around a Lua state.
 *
 * Owns a single lua_State*, provides sandboxed script loading and execution:
 * source chunks only (no bytecode), a memory ceiling enforced by a custom allocator, a time
 * budget per host call enforced by a watchdog thread, and every call into Lua made in protected mode.
 *
 * The watchdog costs nothing while scripts behave: each protected call only bumps a per-thread sequence
 * number. When the same call is still running after its budget, the watchdog signals the script thread
 * (`pthread_kill` on Linux; elsewhere it sets the hook directly, as the reference `lua` interpreter does),
 * and the signal handler arms a count hook that raises an error on the next instruction of the running
 * Lua thread. `coroutine.resume` is wrapped so that the hook follows the running coroutine.
 * This class is engine-private and never exposed in public headers.
 */
class OWL_API LuaEngine final {
public:
	/**
	 * @brief
	 *  Default constructor — creates a new Lua state with sandboxed standard libraries and the default quotas.
	 */
	LuaEngine();

	/**
	 * @brief
	 *  Constructor — creates a new Lua state with sandboxed standard libraries and explicit quotas.
	 * @param[in] iQuotas Memory and time limits of the state.
	 */
	explicit LuaEngine(const ScriptQuotas& iQuotas);

	/**
	 * @brief
	 *  Destructor — closes the Lua state.
	 */
	~LuaEngine();

	LuaEngine(const LuaEngine&) = delete;

	LuaEngine(LuaEngine&&) = delete;

	auto operator=(const LuaEngine&) -> LuaEngine& = delete;

	auto operator=(LuaEngine&&) -> LuaEngine& = delete;

	/**
	 * @brief
	 *  Check whether the Lua state is valid.
	 * @return True if the state was created successfully.
	 */
	[[nodiscard]] auto isValid() const -> bool;

	/**
	 * @brief
	 *  Change the limits of this state; they apply from the next host call.
	 * @param[in] iQuotas Memory and time limits.
	 */
	void setQuotas(const ScriptQuotas& iQuotas) const;

	/**
	 * @brief
	 *  Get the limits of this state.
	 * @return The memory and time limits.
	 */
	[[nodiscard]] auto getQuotas() const -> ScriptQuotas;

	/**
	 * @brief
	 *  Get the number of bytes currently allocated by the state.
	 * @return The heap size of the state, in bytes.
	 */
	[[nodiscard]] auto getMemoryUsage() const -> size_t;

	/**
	 * @brief
	 *  Get the outcome of the last load or call.
	 * @return The status of the last host call into Lua.
	 */
	[[nodiscard]] auto getLastStatus() const -> LuaStatus;

	/**
	 * @brief
	 *  Name the owner of this state in every error message (e.g. the script and its entity).
	 * @param[in] iContext Text inserted after the failing callback name; empty to drop it.
	 */
	void setErrorContext(const std::string& iContext) const;

	/**
	 * @brief
	 *  Register C functions as a global table, each wrapped in the exception trampoline.
	 *
	 * The trampoline catches any C++ exception thrown by the function and raises it as a Lua error once
	 * the C++ frames are gone, and lifts the memory ceiling while the function runs, so that only an
	 * argument check (`luaL_check*`, raised before any non-trivial local exists) can unwind its frame.
	 * @param[in] iState The Lua state.
	 * @param[in] iTableName Name of the global table.
	 * @param[in] iFunctions Null-terminated list of functions.
	 */
	static void registerGuardedTable(lua_State* iState, const char* iTableName, const luaL_Reg* iFunctions);

	/**
	 * @brief
	 *  Load and execute a Lua script file.
	 * @param[in] iPath Path to the .lua file.
	 * @return True on success.
	 */
	[[nodiscard]] auto loadScript(const std::filesystem::path& iPath) const -> bool;

	/**
	 * @brief
	 *  Load and execute a Lua script from a memory buffer.
	 * @param[in] iData Buffer containing the Lua source code.
	 * @param[in] iName Chunk name for error messages.
	 * @return True on success.
	 */
	[[nodiscard]] auto loadBuffer(const std::vector<uint8_t>& iData, const std::string& iName) const -> bool;

	/**
	 * @brief
	 *  Check whether a global function exists in the Lua state.
	 * @param[in] iName Function name.
	 * @return True if the global is a function.
	 */
	[[nodiscard]] auto hasFunction(const std::string& iName) const -> bool;

	/**
	 * @brief
	 *  Call a global Lua function with no arguments.
	 * @param[in] iName Function name.
	 * @return True on success.
	 */
	[[nodiscard]] auto callFunction(const std::string& iName) const -> bool;

	/**
	 * @brief
	 *  Call a global Lua function with a single float argument (typically deltaTime).
	 * @param[in] iName Function name.
	 * @param[in] iArg Float argument.
	 * @return True on success.
	 */
	[[nodiscard]] auto callFunction(const std::string& iName, float iArg) const -> bool;

	/**
	 * @brief
	 *  Call a global Lua function with a single uint64_t argument (typically entity ID).
	 * @param[in] iName Function name.
	 * @param[in] iArg Integer argument.
	 * @return True on success.
	 */
	[[nodiscard]] auto callFunction(const std::string& iName, uint64_t iArg) const -> bool;

	// ---- Global variable access ----
	/**
	 * @brief
	 *  Get a global float value.
	 * @param[in] iName Variable name.
	 * @return The value, or std::nullopt if not found or wrong type.
	 */
	[[nodiscard]] auto getGlobalFloat(const std::string& iName) const -> std::optional<float>;

	/**
	 * @brief
	 *  Get a global integer value.
	 * @param[in] iName Variable name.
	 * @return The value, or std::nullopt if not found or wrong type.
	 */
	[[nodiscard]] auto getGlobalInt(const std::string& iName) const -> std::optional<int64_t>;

	/**
	 * @brief
	 *  Get a global string value.
	 * @param[in] iName Variable name.
	 * @return The value, or std::nullopt if not found or wrong type.
	 */
	[[nodiscard]] auto getGlobalString(const std::string& iName) const -> std::optional<std::string>;

	/**
	 * @brief
	 *  Get a global boolean value.
	 * @param[in] iName Variable name.
	 * @return The value, or std::nullopt if not found or wrong type.
	 */
	[[nodiscard]] auto getGlobalBool(const std::string& iName) const -> std::optional<bool>;

	/**
	 * @brief
	 *  Set a global float value.
	 * @param[in] iName Variable name.
	 * @param[in] iValue Value to set.
	 */
	void setGlobal(const std::string& iName, float iValue) const;

	/**
	 * @brief
	 *  Set a global integer value.
	 * @param[in] iName Variable name.
	 * @param[in] iValue Value to set.
	 */
	void setGlobal(const std::string& iName, int64_t iValue) const;

	/**
	 * @brief
	 *  Set a global string value.
	 * @param[in] iName Variable name.
	 * @param[in] iValue Value to set.
	 */
	void setGlobal(const std::string& iName, const std::string& iValue) const;

	/**
	 * @brief
	 *  Set a global boolean value.
	 * @param[in] iName Variable name.
	 * @param[in] iValue Value to set.
	 */
	void setGlobal(const std::string& iName, bool iValue) const;

	/**
	 * @brief
	 *  Get the raw Lua state pointer (for binding registration).
	 * @return The Lua state, or nullptr if invalid.
	 */
	[[nodiscard]] auto getState() const -> lua_State*;

	/// Opaque quota bookkeeping shared with the allocator and the overdue hook.
	struct Quota;

private:
	/**
	 * @brief
	 *  Call the function under its arguments on top of the stack, in protected mode and under the quotas.
	 * @param[in] iArgCount Number of arguments above the function.
	 * @param[in] iWhat Description of the call for error messages.
	 * @return True on success.
	 */
	[[nodiscard]] auto protectedCall(int iArgCount, const std::string& iWhat) const -> bool;

	/**
	 * @brief
	 *  Run a compiled chunk left on the stack by a load function, or log its load error.
	 * @param[in] iLoadResult Result code of the load function.
	 * @param[in] iName Chunk name for error messages.
	 * @return True on success.
	 */
	[[nodiscard]] auto runLoadedChunk(int iLoadResult, const std::string& iName) const -> bool;

	/**
	 * @brief
	 *  Push the raw value of a global, bypassing any metatable set by the script on `_G`.
	 * @param[in] iName Global name.
	 */
	void pushRawGlobal(const std::string& iName) const;

	/**
	 * @brief
	 *  Pop the value on top of the stack into a raw global, bypassing any metatable on `_G`.
	 * @param[in] iName Global name.
	 */
	void popRawGlobal(const std::string& iName) const;

	/**
	 * @brief
	 *  Push a global function, or record LuaStatus::Missing and push nothing.
	 * @param[in] iName Function name.
	 * @return True if the function was pushed.
	 */
	[[nodiscard]] auto pushGlobalFunction(const std::string& iName) const -> bool;

	/// Quota bookkeeping (stable address, given to the allocator).
	uniq<Quota> mp_quota;
	/// The Lua state.
	lua_State* mp_state = nullptr;
};

}// namespace owl::script
