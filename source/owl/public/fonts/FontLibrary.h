/**
 * @file FontLibrary.h
 * @author Silmaen
 * @date 10/21/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "fonts/Font.h"
#include <list>

/**
 * @brief Namespace for font management.
 */
namespace owl::fonts {

/**
 * @brief Class for managing the fonts.
 */
class OWL_API FontLibrary final {
public:
	/**
	 * @brief Default constructor.
	 */
	FontLibrary();
	/**
	 * @brief Default destructor.
	 */
	~FontLibrary();
	FontLibrary(const FontLibrary&) = delete;
	FontLibrary(FontLibrary&&) = delete;
	auto operator=(const FontLibrary&) -> FontLibrary& = delete;
	auto operator=(FontLibrary&&) -> FontLibrary& = delete;
	/**
	 * @brief Initialize the library.
	 */
	void init();
	/**
	 * @brief Get the default font.
	 * @return The default font.
	 */
	auto getDefaultFont() const -> const shared<Font>&;

	/**
	 * @brief Get or load the font.
	 * @param iName The font's name
	 * @return The font or nullptr.
	 */
	auto getFont(const std::string& iName) -> const shared<Font>&;

	/**
	 * @brief Get the list of already loaded fonts.
	 * @return The list of loaded fonts.
	 */
	auto getLoadedFontNames() const -> std::list<std::string>;
	/**
	 * @brief Get a list of possible fonts.
	 * @return List of font files found.
	 */
	auto getFoundFontNames() const -> std::list<std::string>;

private:
	/// Default font name for faster search.
	std::string m_defaultFontName;
	/// list of fonts.
	std::unordered_map<std::string, shared<Font>> m_fonts;
	/**
	 * @brief Load a font.
	 * @param iName The font's name.
	 */
	void loadFont(const std::string& iName);
};

}// namespace owl::fonts
