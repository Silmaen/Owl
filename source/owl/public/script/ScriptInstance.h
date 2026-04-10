/**
 * @file ScriptInstance.h
 * @author Silmaen
 * @date 09/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "ScriptEngine.h"

#include <optional>
#include <string>

namespace owl::script {

/**
 * @brief Per-entity script instance.
 *
 * Wraps a Lua environment table providing isolated state for one entity.
 * Supports lifecycle callbacks and typed property access.
 */
class OWL_API ScriptInstance final {
public:
	/// @brief Default constructor.
	ScriptInstance();
	/// @brief Destructor.
	~ScriptInstance();
	ScriptInstance(const ScriptInstance&) = delete;
	/// @brief Move constructor.
	ScriptInstance(ScriptInstance&& iOther) noexcept;
	auto operator=(const ScriptInstance&) -> ScriptInstance& = delete;
	/// @brief Move assignment.
	auto operator=(ScriptInstance&& iOther) noexcept -> ScriptInstance&;

	/**
	 * @brief Initialize this instance with a script and entity ID.
	 * @param[in] iScriptPath Path to the .lua script (relative to assets).
	 * @param[in] iEntityId The owning entity's UUID (as uint64_t).
	 * @return True on success.
	 */
	[[nodiscard]] auto create(const std::string& iScriptPath, uint64_t iEntityId) -> bool;

	/**
	 * @brief Initialize this instance from a buffer (pack loading).
	 * @param[in] iData The script source buffer.
	 * @param[in] iName The script name.
	 * @param[in] iEntityId The owning entity's UUID.
	 * @return True on success.
	 */
	[[nodiscard]] auto createFromBuffer(const std::vector<uint8_t>& iData, const std::string& iName,
										uint64_t iEntityId) -> bool;

	/// @brief Check whether the instance is valid and ready.
	[[nodiscard]] auto isValid() const -> bool;

	/// @brief Call the script's on_create callback.
	void onCreate();
	/// @brief Call the script's on_update callback.
	/// @param[in] iDeltaTime Frame delta time in seconds.
	void onUpdate(float iDeltaTime);
	/// @brief Call the script's on_destroy callback.
	void onDestroy();
	/// @brief Call the script's on_collision callback.
	/// @param[in] iOtherEntityId UUID of the other entity.
	void onCollision(uint64_t iOtherEntityId);

	// ---- Property access ----

	/// @brief Set a float property on the instance.
	void setProperty(const std::string& iName, float iValue);
	/// @brief Set an integer property on the instance.
	void setProperty(const std::string& iName, int64_t iValue);
	/// @brief Set a string property on the instance.
	void setProperty(const std::string& iName, const std::string& iValue);
	/// @brief Set a boolean property on the instance.
	void setProperty(const std::string& iName, bool iValue);

	/// @brief Get a float property from the instance.
	[[nodiscard]] auto getPropertyFloat(const std::string& iName) -> std::optional<float>;
	/// @brief Get an integer property from the instance.
	[[nodiscard]] auto getPropertyInt(const std::string& iName) -> std::optional<int64_t>;
	/// @brief Get a string property from the instance.
	[[nodiscard]] auto getPropertyString(const std::string& iName) -> std::optional<std::string>;
	/// @brief Get a boolean property from the instance.
	[[nodiscard]] auto getPropertyBool(const std::string& iName) -> std::optional<bool>;

private:
	/// Forward-declared implementation.
	struct Impl;
	/// The implementation.
	uniq<Impl> mp_impl;
};

}// namespace owl::script
