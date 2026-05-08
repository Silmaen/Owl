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
 * @brief
 *  Per-entity script instance.
 *
 * Wraps a Lua environment table providing isolated state for one entity.
 * Supports lifecycle callbacks and typed property access.
 */
class OWL_API ScriptInstance final {
public:
	/**
	 * @brief
	 *  Default constructor.
	 */
	ScriptInstance();

	/**
	 * @brief
	 *  Destructor.
	 */
	~ScriptInstance();

	ScriptInstance(const ScriptInstance&) = delete;

	/**
	 * @brief
	 *  Move constructor.
	 * @param[in,out] iOther Source instance, left in a valid-but-unspecified state.
	 */
	ScriptInstance(ScriptInstance&& iOther) noexcept;

	auto operator=(const ScriptInstance&) -> ScriptInstance& = delete;

	/**
	 * @brief
	 *  Move assignment.
	 * @param[in,out] iOther Source instance, left in a valid-but-unspecified state.
	 * @return A reference to this instance.
	 */
	auto operator=(ScriptInstance&& iOther) noexcept -> ScriptInstance&;

	/**
	 * @brief
	 *  Initialize this instance with a script and entity ID.
	 * @param[in] iScriptPath Path to the .lua script (relative to assets).
	 * @param[in] iEntityId The owning entity's UUID (as uint64_t).
	 * @return True on success.
	 */
	[[nodiscard]] auto create(const std::string& iScriptPath, uint64_t iEntityId) const -> bool;

	/**
	 * @brief
	 *  Initialize this instance from a buffer (pack loading).
	 * @param[in] iData The script source buffer.
	 * @param[in] iName The script name.
	 * @param[in] iEntityId The owning entity's UUID.
	 * @return True on success.
	 */
	[[nodiscard]] auto createFromBuffer(const std::vector<uint8_t>& iData, const std::string& iName,
										uint64_t iEntityId) const -> bool;

	/**
	 * @brief
	 *  Check whether the instance is valid and ready.
	 * @return True when the underlying Lua environment is initialised.
	 */
	[[nodiscard]] auto isValid() const -> bool;

	/**
	 * @brief
	 *  Call the script's `on_create` callback.
	 */
	void onCreate() const;

	/**
	 * @brief
	 *  Call the script's `on_update` callback.
	 * @param[in] iDeltaTime Frame delta time in seconds.
	 */
	void onUpdate(float iDeltaTime) const;

	/**
	 * @brief
	 *  Call the script's `on_destroy` callback.
	 */
	void onDestroy() const;

	/**
	 * @brief
	 *  Call the script's `on_collision` callback.
	 * @param[in] iOtherEntityId UUID of the other entity.
	 */
	void onCollision(uint64_t iOtherEntityId) const;

	/**
	 * @brief
	 *  Call an arbitrary named function in the script.
	 * @param[in] iName The function name.
	 * @return True if the function existed and was called successfully.
	 */
	[[nodiscard]] auto callFunction(const std::string& iName) const -> bool;

	// ---- Property access ----
	/**
	 * @brief
	 *  Set a float property on the instance.
	 * @param[in] iName Property name.
	 * @param[in] iValue Value to assign.
	 */
	void setProperty(const std::string& iName, float iValue) const;

	/**
	 * @brief
	 *  Set an integer property on the instance.
	 * @param[in] iName Property name.
	 * @param[in] iValue Value to assign.
	 */
	void setProperty(const std::string& iName, int64_t iValue) const;

	/**
	 * @brief
	 *  Set a string property on the instance.
	 * @param[in] iName Property name.
	 * @param[in] iValue Value to assign.
	 */
	void setProperty(const std::string& iName, const std::string& iValue) const;

	/**
	 * @brief
	 *  Set a boolean property on the instance.
	 * @param[in] iName Property name.
	 * @param[in] iValue Value to assign.
	 */
	void setProperty(const std::string& iName, bool iValue) const;

	/**
	 * @brief
	 *  Get a float property from the instance.
	 * @param[in] iName Property name.
	 * @return The value, or `std::nullopt` when missing or of the wrong type.
	 */
	[[nodiscard]] auto getPropertyFloat(const std::string& iName) const -> std::optional<float>;

	/**
	 * @brief
	 *  Get an integer property from the instance.
	 * @param[in] iName Property name.
	 * @return The value, or `std::nullopt` when missing or of the wrong type.
	 */
	[[nodiscard]] auto getPropertyInt(const std::string& iName) const -> std::optional<int64_t>;

	/**
	 * @brief
	 *  Get a string property from the instance.
	 * @param[in] iName Property name.
	 * @return The value, or `std::nullopt` when missing or of the wrong type.
	 */
	[[nodiscard]] auto getPropertyString(const std::string& iName) const -> std::optional<std::string>;

	/**
	 * @brief
	 *  Get a boolean property from the instance.
	 * @param[in] iName Property name.
	 * @return The value, or `std::nullopt` when missing or of the wrong type.
	 */
	[[nodiscard]] auto getPropertyBool(const std::string& iName) const -> std::optional<bool>;

private:
	/// Forward-declared implementation.
	struct Impl;
	/// The implementation.
	uniq<Impl> mp_impl;
};

}// namespace owl::script
