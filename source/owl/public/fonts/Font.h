/**
 * @file Font.h
 * @author Silmaen
 * @date 10/17/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "renderer/Texture.h"

#include "math/box.h"

namespace owl::fonts {

struct MsdfData;

/**
 * @brief Class describing a font.
 */
class OWL_API Font final {
public:
	/**
	 * @brief Default constructor.
	 */
	explicit Font(const std::filesystem::path& iPath, bool iIsDefault = false);
	/**
	 * @brief Default destructor.
	 */
	~Font();
	/**
	 * @brief Default copy constructor.
	 */
	Font(const Font&) = default;
	/**
	 * @brief Default move constructor.
	 */
	Font(Font&&) = default;
	/**
	 * @brief Default copy affectation operator.
	 */
	auto operator=(const Font&) -> Font& = default;
	/**
	 * @brief Default move affectation operator.
	 */
	auto operator=(Font&&) -> Font& = default;

	/**
	 * @brief Get the atlas texture corresponding to the Font.
	 * @return The atlas texture.
	 */
	[[nodiscard]] auto getAtlasTexture() const -> shared<renderer::Texture2D> { return m_atlasTexture; }

	/**
	 * @brief Metrics of the glyphs.
	 */
	struct GlyphMetrics {
		/// The quad for 3D space.
		math::box2f quad;
		/// The quad for texture space.
		math::box2f uv;
	};
	[[nodiscard]] auto getGlyphBox(const char& iChar) const -> GlyphMetrics;
	/**
	 * @brief Get the line width.
	 * @return The line width.
	 */
	[[nodiscard]] auto getScaledLineHeight() const -> float;
	/**
	 * @brief Compute the position of the next character.
	 * @param iChar The current char.
	 * @param iNextChar The next char.
	 * @return ne position.
	 */
	[[nodiscard]] auto getAdvance(const char& iChar, const char& iNextChar) const -> float;

	/**
	 * @brief get the font's name.
	 * @return The font name.
	 */
	[[nodiscard]] auto getName() const -> const std::string& { return m_name; }

	/**
	 * @brief Look if it is the default font.
	 * @return True if this is the default font.
	 */
	[[nodiscard]] auto isDefault() const -> bool { return m_default; }

private:
	/// pointer to the texture.
	shared<renderer::Texture2D> m_atlasTexture;
	/// The internal atlas data.
	shared<MsdfData> m_data;
	/// The name of the font.
	std::string m_name;
	/// If this font is the default one.
	bool m_default = false;
};

}// namespace owl::fonts
