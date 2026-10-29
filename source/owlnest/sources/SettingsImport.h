/**
 * @file SettingsImport.h
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include <core/expected.h>

#include <cstdint>
#include <filesystem>
#include <optional>

/**
 * @brief
 *  Import of the settings of a previous Owl Nest installation.
 *
 * The files are staged in `<user directory>/import_pending/` and moved in place at the next start, before anything
 * reads them, so the running session cannot write its own settings over them when it quits.
 */
namespace owl::nest::settings_import {

/**
 * @brief
 *  Why an import could not be staged.
 */
enum struct ImportError : uint8_t {
	NoSettings,///< The folder holds no Owl Nest settings.
	SameDirectory,///< The folder is the current user directory.
	CopyFailed,///< A file could not be copied.
};

/**
 * @brief
 *  Find the settings of an installation.
 * @param[in] iFolder The installation folder, or its user directory.
 * @return `<iFolder>/user` or `iFolder` when it holds `OwlNest_settings.yml` or `config.yml`, nothing otherwise.
 */
[[nodiscard]] auto findSettingsSource(const std::filesystem::path& iFolder) -> std::optional<std::filesystem::path>;

/**
 * @brief
 *  Copy the settings of a previous installation to the staging folder of a user directory.
 *
 * Copied: `config.yml`, `OwlNest_settings.yml`, every `*_imgui.ini` and the `games/` folder (settings and saves of
 * the games played in the editor). Caches, logs and autosaves stay behind.
 * @param[in] iFolder The previous installation folder, or its user directory.
 * @param[in] iUserDirectory The current user directory.
 * @return The number of files staged.
 */
[[nodiscard]] auto stageImport(const std::filesystem::path& iFolder, const std::filesystem::path& iUserDirectory)
		-> expected<uint32_t, ImportError>;

/**
 * @brief
 *  Move the staged settings of a user directory in place, replacing the current ones.
 * @param[in] iUserDirectory The current user directory.
 * @return The number of files moved; 0 when nothing was staged.
 */
auto applyPendingImport(const std::filesystem::path& iUserDirectory) -> uint32_t;

}// namespace owl::nest::settings_import
