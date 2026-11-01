/**
 * @file HudThemeLibrary.h
 * @author Silmaen
 * @date 01/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace owl::scene {
class HudTheme;

/**
 * @brief
 *  The `.owlhudtheme` files read so far, by asset path, shared by every scene and HUD of an application.
 */
class OWL_API HudThemeLibrary final {
public:
	/**
	 * @brief
	 *  Constructor.
	 */
	HudThemeLibrary();

	HudThemeLibrary(const HudThemeLibrary&) = delete;

	HudThemeLibrary(HudThemeLibrary&&) = delete;

	auto operator=(const HudThemeLibrary&) -> HudThemeLibrary& = delete;

	auto operator=(HudThemeLibrary&&) -> HudThemeLibrary& = delete;

	/**
	 * @brief
	 *  Destructor.
	 */
	~HudThemeLibrary();

	/**
	 * @brief
	 *  The theme of a path, read on the first call.
	 * @param[in] iPath Asset path of the theme.
	 * @return The theme, or null for an empty path or a file that cannot be read (logged once).
	 */
	[[nodiscard]] auto get(const std::string& iPath) -> shared<const HudTheme>;

	/**
	 * @brief
	 *  Replace the theme of a path (an editor shows its edits live this way).
	 * @param[in] iPath Asset path of the theme.
	 * @param[in] iTheme The theme.
	 */
	void set(const std::string& iPath, const HudTheme& iTheme);

	/**
	 * @brief
	 *  Forget a theme, read again on the next `get`.
	 * @param[in] iPath Asset path of the theme.
	 */
	void invalidate(const std::string& iPath);

	/**
	 * @brief
	 *  Forget every theme.
	 */
	void clear();

	/**
	 * @brief
	 *  Number of files read.
	 * @return The count.
	 */
	[[nodiscard]] auto getLoadCount() const -> uint64_t { return m_loadCount; }

private:
	/// Themes by asset path (null for a file that failed to load).
	std::unordered_map<std::string, shared<const HudTheme>> m_themes;
	/// Number of files read.
	uint64_t m_loadCount = 0;
};

}// namespace owl::scene
