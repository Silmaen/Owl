/**
 * @file UserDirectory.h
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include "core/Core.h"

#include <filesystem>
#include <string>

namespace owl::platform {

/**
 * @brief
 *  Directory holding the running executable.
 * @return The directory of `/proc/self/exe` (Linux) or of the module file (Windows), the current directory when the
 *  executable cannot be resolved.
 */
[[nodiscard]] OWL_API auto getExecutableDirectory() -> std::filesystem::path;

/**
 * @brief
 *  Choose the directory where an application writes its settings, saves, logs and caches.
 *
 * `<iAppDirectory>/user` when it can be created and written to; otherwise `<iFallbackName>` under the user profile:
 * `%LOCALAPPDATA%` (Windows), `$XDG_DATA_HOME` or `~/.local/share` (Linux), the temporary directory as a last resort.
 * @param[in] iAppDirectory Directory of the application.
 * @param[in] iFallbackName Folder name used in the user profile.
 * @return The chosen directory, created.
 */
[[nodiscard]] OWL_API auto resolveUserDirectory(const std::filesystem::path& iAppDirectory,
												const std::string& iFallbackName) -> std::filesystem::path;

/**
 * @brief
 *  Directory where the running application writes its settings, saves, logs and caches.
 *
 * In order: the directory given to setUserDirectory(), the `OWL_USER_DIR` environment variable, then
 * resolveUserDirectory() on the executable directory with the executable name as fallback; resolved once.
 * @return The user directory of the process, created.
 */
[[nodiscard]] OWL_API auto getUserDirectory() -> std::filesystem::path;

/**
 * @brief
 *  Force the user directory of the process (tests, command line); an empty path restores the resolution.
 * @param[in] iDirectory The directory to use.
 */
OWL_API void setUserDirectory(const std::filesystem::path& iDirectory);

/**
 * @brief
 *  Directory of a game's settings and saves, inside the user directory.
 * @param[in] iGameName Name of the game; empty gives `OwlGame`.
 * @return `<user directory>/games/<game name>`, not created.
 */
[[nodiscard]] OWL_API auto getGameDirectory(const std::string& iGameName) -> std::filesystem::path;

}// namespace owl::platform
