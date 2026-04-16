/**
 * @file SettingsManager.h
 * @author Silmaen
 * @date 14/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace owl::scene {

/**
 * @brief Persistent game settings with defaults + user overrides.
 *
 * Two-layer key-value store: game defaults (from game_settings.yml in assets)
 * overlaid with user overrides (settings.yml in user directory). Settings
 * persist across scene transitions and game launches.
 *
 * Built-in keys (resolution, fullscreen, volumes) can be auto-applied to
 * engine subsystems via applyBuiltins().
 */
class OWL_API SettingsManager final {
public:
	SettingsManager() = delete;

	/// Value type (same as GameState::Value).
	using Value = std::variant<int64_t, float, std::string, bool>;

	/**
	 * @brief Set the game name (for user directory path).
	 * @param[in] iGameName The game name.
	 */
	static void setGameName(const std::string& iGameName);

	/**
	 * @brief Get the game's user directory (parent of saves/).
	 * @return The user directory path.
	 */
	[[nodiscard]] static auto getUserDirectory() -> std::filesystem::path;

	/**
	 * @brief Get the user settings file path.
	 * @return Path to settings.yml in the user directory.
	 */
	[[nodiscard]] static auto getSettingsPath() -> std::filesystem::path;

	/**
	 * @brief Load game defaults from a YAML file (game_settings.yml in assets).
	 * @param[in] iPath Path to the game defaults file.
	 */
	static void loadDefaults(const std::filesystem::path& iPath);

	/**
	 * @brief Load game defaults from a YAML string (for pack-based loading).
	 * @param[in] iContent The YAML content as a string.
	 */
	static void loadDefaultsFromString(const std::string& iContent);

	/**
	 * @brief Load user overrides from settings.yml.
	 */
	static void loadUserSettings();

	/**
	 * @brief Save user overrides to settings.yml.
	 */
	static void saveUserSettings();

	/**
	 * @brief Set a default value for a key.
	 * @param[in] iKey The key.
	 * @param[in] iValue The default value.
	 */
	static void setDefault(const std::string& iKey, Value iValue);

	/**
	 * @brief Set a user override for a key.
	 * @param[in] iKey The key.
	 * @param[in] iValue The value.
	 */
	static void set(const std::string& iKey, Value iValue);

	/**
	 * @brief Get a setting (user override > default > nullopt).
	 * @param[in] iKey The key.
	 * @return The value, or nullopt if not found.
	 */
	[[nodiscard]] static auto get(const std::string& iKey) -> std::optional<Value>;

	/**
	 * @brief Get a setting with a fallback.
	 * @param[in] iKey The key.
	 * @param[in] iDefault Fallback if key not found.
	 * @return The value.
	 */
	[[nodiscard]] static auto get(const std::string& iKey, const Value& iDefault) -> Value;

	/**
	 * @brief Typed getter convenience.
	 * @tparam T The expected type.
	 * @param[in] iKey The key.
	 * @return The value if found and matching type, or nullopt.
	 */
	template<typename T>
	[[nodiscard]] static auto getAs(const std::string& iKey) -> std::optional<T> {
		if (const auto val = get(iKey); val.has_value()) {
			if (const auto* ptr = std::get_if<T>(&val.value()))
				return *ptr;
		}
		return std::nullopt;
	}

	/**
	 * @brief Remove a user override (reverts to default).
	 * @param[in] iKey The key.
	 */
	static void resetToDefault(const std::string& iKey);

	/// Reset all user overrides to defaults.
	static void resetAllToDefaults();

	/// Check if a user override exists for a key.
	[[nodiscard]] static auto hasOverride(const std::string& iKey) -> bool;

	/// Check if a key exists (in overrides or defaults).
	[[nodiscard]] static auto has(const std::string& iKey) -> bool;

	/// Get all keys (union of defaults and overrides).
	[[nodiscard]] static auto keys() -> std::vector<std::string>;

	/**
	 * @brief Apply built-in settings to engine subsystems (window, sound).
	 *
	 * Applies resolution, fullscreen, resizable to the window, and
	 * master volume to the sound listener.
	 */
	static void applyBuiltins();

	/// Clear all data (defaults + overrides). Used for testing.
	static void clear();

	/// Built-in setting key constants.
	static constexpr auto KeyResolutionWidth = "resolution_width";
	/// Built-in key for window height.
	static constexpr auto KeyResolutionHeight = "resolution_height";
	/// Built-in key for fullscreen mode.
	static constexpr auto KeyFullscreen = "fullscreen";
	/// Built-in key for resizable window.
	static constexpr auto KeyResizable = "resizable";
	/// Built-in key for master volume.
	static constexpr auto KeyVolumeMaster = "volume_master";
	/// Built-in key for music volume.
	static constexpr auto KeyVolumeMusic = "volume_music";
	/// Built-in key for SFX volume.
	static constexpr auto KeyVolumeSfx = "volume_sfx";

private:
	/// Game name for directory path.
	static std::string s_gameName;
	/// Game defaults (from game_settings.yml).
	static std::unordered_map<std::string, Value> s_defaults;
	/// User overrides (from settings.yml).
	static std::unordered_map<std::string, Value> s_overrides;
};

}// namespace owl::scene
