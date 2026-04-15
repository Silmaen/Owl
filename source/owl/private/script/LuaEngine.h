/**
 * @file LuaEngine.h
 * @author Silmaen
 * @date 09/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

struct lua_State;

namespace owl::script {

/**
 * @brief Low-level wrapper around a Lua state.
 *
 * Owns a single lua_State*, provides sandboxed script loading and execution.
 * This class is engine-private and never exposed in public headers.
 */
class OWL_API LuaEngine final {
public:
	/// @brief Default constructor — creates a new Lua state with sandboxed standard libraries.
	LuaEngine();
	/// @brief Destructor — closes the Lua state.
	~LuaEngine();
	LuaEngine(const LuaEngine&) = delete;
	LuaEngine(LuaEngine&&) = delete;
	auto operator=(const LuaEngine&) -> LuaEngine& = delete;
	auto operator=(LuaEngine&&) -> LuaEngine& = delete;

	/// @brief Check whether the Lua state is valid.
	/// @return True if the state was created successfully.
	[[nodiscard]] auto isValid() const -> bool;

	/**
	 * @brief Load and execute a Lua script file.
	 * @param[in] iPath Path to the .lua file.
	 * @return True on success.
	 */
	[[nodiscard]] auto loadScript(const std::filesystem::path& iPath) const -> bool;

	/**
	 * @brief Load and execute a Lua script from a memory buffer.
	 * @param[in] iData Buffer containing the Lua source code.
	 * @param[in] iName Chunk name for error messages.
	 * @return True on success.
	 */
	[[nodiscard]] auto loadBuffer(const std::vector<uint8_t>& iData, const std::string& iName) const -> bool;

	/**
	 * @brief Check whether a global function exists in the Lua state.
	 * @param[in] iName Function name.
	 * @return True if the global is a function.
	 */
	[[nodiscard]] auto hasFunction(const std::string& iName) const -> bool;

	/**
	 * @brief Call a global Lua function with no arguments.
	 * @param[in] iName Function name.
	 * @return True on success.
	 */
	[[nodiscard]] auto callFunction(const std::string& iName) const -> bool;

	/**
	 * @brief Call a global Lua function with a single float argument (typically deltaTime).
	 * @param[in] iName Function name.
	 * @param[in] iArg Float argument.
	 * @return True on success.
	 */
	[[nodiscard]] auto callFunction(const std::string& iName, float iArg) const -> bool;

	/**
	 * @brief Call a global Lua function with a single uint64_t argument (typically entity ID).
	 * @param[in] iName Function name.
	 * @param[in] iArg Integer argument.
	 * @return True on success.
	 */
	[[nodiscard]] auto callFunction(const std::string& iName, uint64_t iArg) const -> bool;

	// ---- Global variable access ----

	/// @brief Get a global float value.
	/// @param[in] iName Variable name.
	/// @return The value, or std::nullopt if not found or wrong type.
	[[nodiscard]] auto getGlobalFloat(const std::string& iName) const -> std::optional<float>;

	/// @brief Get a global integer value.
	/// @param[in] iName Variable name.
	/// @return The value, or std::nullopt if not found or wrong type.
	[[nodiscard]] auto getGlobalInt(const std::string& iName) const -> std::optional<int64_t>;

	/// @brief Get a global string value.
	/// @param[in] iName Variable name.
	/// @return The value, or std::nullopt if not found or wrong type.
	[[nodiscard]] auto getGlobalString(const std::string& iName) const -> std::optional<std::string>;

	/// @brief Get a global boolean value.
	/// @param[in] iName Variable name.
	/// @return The value, or std::nullopt if not found or wrong type.
	[[nodiscard]] auto getGlobalBool(const std::string& iName) const -> std::optional<bool>;

	/// @brief Set a global float value.
	/// @param[in] iName Variable name.
	/// @param[in] iValue Value to set.
	void setGlobal(const std::string& iName, float iValue) const;

	/// @brief Set a global integer value.
	/// @param[in] iName Variable name.
	/// @param[in] iValue Value to set.
	void setGlobal(const std::string& iName, int64_t iValue) const;

	/// @brief Set a global string value.
	/// @param[in] iName Variable name.
	/// @param[in] iValue Value to set.
	void setGlobal(const std::string& iName, const std::string& iValue) const;

	/// @brief Set a global boolean value.
	/// @param[in] iName Variable name.
	/// @param[in] iValue Value to set.
	void setGlobal(const std::string& iName, bool iValue) const;

	/// @brief Get the raw Lua state pointer (for binding registration).
	/// @return The Lua state, or nullptr if invalid.
	[[nodiscard]] auto getState() const -> lua_State*;

private:
	/// The Lua state.
	lua_State* mp_state = nullptr;
};

}// namespace owl::script
