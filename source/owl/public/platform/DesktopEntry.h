/**
 * @file DesktopEntry.h
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include "core/Core.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace owl::platform {

/**
 * @brief
 *  Freedesktop desktop entry describing a running application.
 *
 * Wayland compositors do not take the window icon from the application: they look up the desktop entry whose
 * file name matches the window `app_id` and display its `Icon`. The entry written by installDesktopEntry() is hidden
 * from the application menus (`NoDisplay=true`).
 */
struct OWL_API DesktopEntry {
	/// Application identifier: the file is named `<appId>.desktop` and must match the Wayland `app_id`.
	std::string appId;
	/// Human-readable application name.
	std::string name;
	/// Absolute path of the executable.
	std::filesystem::path executable;
	/// Absolute path of the icon image (PNG or SVG).
	std::filesystem::path icon;
};

/**
 * @brief
 *  Build the text of a desktop entry file.
 * @param[in] iEntry The entry description.
 * @return The `.desktop` file content.
 */
OWL_API auto desktopEntryText(const DesktopEntry& iEntry) -> std::string;

/**
 * @brief
 *  Location of the user desktop entry for an application (`$XDG_DATA_HOME/applications/<appId>.desktop`, falling
 *  back to `$HOME/.local/share`).
 * @param[in] iAppId The application identifier.
 * @return The path, or `std::nullopt` off Linux or when neither variable is set.
 */
OWL_API auto desktopEntryPath(std::string_view iAppId) -> std::optional<std::filesystem::path>;

/**
 * @brief
 *  Write the user desktop entry of an application, unless an identical one already exists.
 * @param[in] iEntry The entry description.
 * @return True when the entry is in place and up to date.
 */
OWL_API auto installDesktopEntry(const DesktopEntry& iEntry) -> bool;

}// namespace owl::platform
