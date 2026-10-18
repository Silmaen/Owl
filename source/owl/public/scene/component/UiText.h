/**
 * @file UiText.h
 * @author Silmaen
 * @date 10/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Serializer.h"
#include "data/fonts/Font.h"
#include "data/fonts/TextLayout.h"

#include <cstdint>
#include <string>

namespace owl::scene::component {
/**
 * @brief
 *  UI text widget component.
 *
 * Renders text in screen space within a Canvas hierarchy, laid out in the `UiRect` of its entity: the rectangle
 * drives the wrap width, the alignment and the fit, the glyphs keep the aspect ratio of the font.
 */
struct OWL_API UiText {
	/// The text content.
	std::string text;
	/// The font (nullptr = default font).
	shared<data::fonts::Font> font = nullptr;
	/// Text colour.
	math::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
	/// Font size in pixels: height from the ascender to the descender of the font.
	float fontSize = 16.f;
	/// Horizontal alignment of the lines in the rectangle.
	data::fonts::HorizontalAlignment horizontalAlignment = data::fonts::HorizontalAlignment::Left;
	/// Vertical alignment of the block of lines in the rectangle.
	data::fonts::VerticalAlignment verticalAlignment = data::fonts::VerticalAlignment::Middle;
	/// Break the lines on spaces to keep them within the rectangle width.
	bool wrap = false;
	/// Behaviour of text larger than the rectangle.
	data::fonts::TextFit fit = data::fonts::TextFit::Overflow;
	/// Smallest font size, in pixels, reached by `TextFit::Shrink`.
	float minFontSize = 8.f;
	/// Kerning adjustment, in font units.
	float kerning = 0.f;
	/// Line spacing adjustment, in font units.
	float lineSpacing = 0.f;
	/// Colour of the outline around the glyphs.
	math::vec4 outlineColor{0.f, 0.f, 0.f, 1.f};
	/// Reach of the outline outside the glyphs, in reference pixels (0 = no outline).
	float outlineWidth = 0.f;
	/// Colour of the box behind the text; a zero alpha draws no box.
	math::vec4 backgroundColor{0.f, 0.f, 0.f, 0.f};
	/// Margin between the text block and its background box, in reference pixels.
	float backgroundPadding = 4.f;

	/**
	 * @brief
	 *  Layout of this text in a rectangle.
	 * @param[in] iRectSize Size of the rectangle, in pixels.
	 * @return The layout options, in pixels.
	 */
	[[nodiscard]] auto getLayout(const math::vec2& iRectSize) const -> data::fonts::TextLayout;

	/**
	 * @brief
	 *  Get the class title.
	 * @return The display name of the component.
	 */
	static auto name() noexcept -> const char* { return "UI Text"; }

	/**
	 * @brief
	 *  Get the YAML key.
	 * @return The YAML serialization key.
	 */
	static auto key() noexcept -> const char* { return "UiText"; }

	/**
	 * @brief
	 *  Write this component to a YAML context.
	 * @param[in] iOut The serializer used as output.
	 */
	void serialize(const core::Serializer& iOut) const;

	/**
	 * @brief
	 *  Read this component from YAML node.
	 * @param[in] iNode The serializer wrapping the source YAML node.
	 */
	void deserialize(const core::Serializer& iNode);
};

}// namespace owl::scene::component
