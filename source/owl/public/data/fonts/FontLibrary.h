/**
 * @file FontLibrary.h
 * @author Silmaen
 * @date 10/21/24
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "data/fonts/Font.h"
#include <array>
#include <cstddef>
#include <filesystem>
#include <list>
#include <string>
#include <string_view>
#include <unordered_map>

/**
 * @brief
 *  Namespace for font management.
 */
namespace owl::data::fonts {

/// Name of the default font, always loaded and packed with a game.
constexpr std::string_view g_DefaultFontName{"OpenSans-Regular"};

/// Font files of the ImGui overlay (`gui::UiLayer`: body, strong, code, icons), relative to an asset directory.
constexpr std::array<std::string_view, 4> g_UiFontFiles{"fonts/Inter-Regular.ttf", "fonts/Inter-SemiBold.ttf",
														"fonts/JetBrainsMono-Regular.ttf", "fonts/lucide.ttf"};

/**
 * @brief
 *  Class for managing the fonts.
 */
class OWL_API FontLibrary final {
public:
	/**
	 * @brief
	 *  Default constructor.
	 */
	FontLibrary();

	/**
	 * @brief
	 *  Default destructor.
	 */
	~FontLibrary();

	FontLibrary(const FontLibrary&) = delete;

	FontLibrary(FontLibrary&&) = delete;

	auto operator=(const FontLibrary&) -> FontLibrary& = delete;

	auto operator=(FontLibrary&&) -> FontLibrary& = delete;

	/**
	 * @brief
	 *  Initialize the library.
	 */
	void init();

	/**
	 * @brief
	 *  Destroy the library.
	 */
	void destroy();

	/**
	 * @brief
	 *  Get the default font.
	 * @return The default font.
	 */
	[[nodiscard]] auto getDefaultFont() const -> const shared<Font>&;

	/**
	 * @brief
	 *  Get or load the font.
	 * @param[in] iName The font's name.
	 * @return The font, or nullptr if no matching font could be loaded.
	 */
	[[nodiscard]] auto getFont(const std::string& iName) -> const shared<Font>&;

	/**
	 * @brief
	 *  Rebuild a loaded font in place with new import settings (editor, after saving its `.owlfont`).
	 * @param[in] iName The font's name.
	 * @param[in] iSettings The new settings.
	 * @return True when the font was loaded and has been rebuilt.
	 */
	auto reloadFont(const std::string& iName, const FontSettings& iSettings) -> bool;

	/**
	 * @brief
	 *  Forget every loaded font whose file lies in a directory (a project being closed); the default font stays.
	 * @param[in] iDirectory The directory.
	 * @return The number of fonts forgotten.
	 */
	auto unloadFontsFrom(const std::filesystem::path& iDirectory) -> size_t;

	/**
	 * @brief
	 *  Get the list of already loaded fonts.
	 * @return The list of loaded fonts.
	 */
	[[nodiscard]] auto getLoadedFontNames() const -> std::list<std::string>;

	/**
	 * @brief
	 *  Get a list of possible fonts.
	 * @return List of font files found.
	 */
	[[nodiscard]] static auto getFoundFontNames() -> std::list<std::string>;

private:
	/// Default font name for faster search.
	std::string m_defaultFontName;
	/// list of fonts.
	std::unordered_map<std::string, shared<Font>> m_fonts;

	/**
	 * @brief
	 *  Load a font.
	 * @param[in] iName The font's name.
	 */
	void loadFont(const std::string& iName);
};

}// namespace owl::data::fonts
