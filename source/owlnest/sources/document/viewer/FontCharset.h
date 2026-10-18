/**
 * @file FontCharset.h
 * @author Silmaen
 * @date 17/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <data/fonts/FontSettings.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace owl::nest::viewer {

/**
 * @brief
 *  A slice of the charset of a font that lies in one Unicode block.
 */
struct CharsetBlock {
	/// Name of the Unicode block ("Other" for a code point outside the known blocks).
	std::string_view name;
	/// The covered code points, a part of the block.
	data::fonts::CodePointRange range;
	/// Number of code points of the slice.
	uint32_t count = 0;
};

/**
 * @brief
 *  The charset of a font, told by Unicode block.
 */
struct CharsetSummary {
	/// The slices, in code point order.
	std::vector<CharsetBlock> blocks;
	/// Total number of code points of the charset (not every one has a glyph in the font file).
	size_t codePoints = 0;
};

/**
 * @brief
 *  Name the Unicode block of a code point.
 * @param[in] iCodePoint The code point.
 * @return The block name, "Other" when the block is not in the table.
 */
[[nodiscard]] auto unicodeBlockName(char32_t iCodePoint) -> std::string_view;

/**
 * @brief
 *  Summarize the charset a font is loaded with: default ranges plus the extra ones, merged, cut at the Unicode blocks.
 * @param[in] iSettings The import settings of the font.
 * @return The slices and the code point total.
 */
[[nodiscard]] auto summarizeCharset(const data::fonts::FontSettings& iSettings) -> CharsetSummary;

/**
 * @brief
 *  Place of one sample line in the sample picture of a font.
 */
struct SampleRow {
	/// Font size of the line, in pixels.
	float fontSize = 0.f;
	/// Distance from the picture top to the line top, in pixels.
	float top = 0.f;
	/// Height of the line (text and box margin), in pixels.
	float height = 0.f;
};

/**
 * @brief
 *  Stack the sample lines of a font from top to bottom.
 * @param[in] iSizes Font sizes in pixels, one line each.
 * @param[in] iLineHeight Line height of the font, in font sizes (`Font::getScaledLineHeight`).
 * @param[in] iMargin Space around a line, in font sizes (room for the outline and the box).
 * @param[in] iGap Space between two lines, in pixels.
 * @return The rows; the picture height is `top + height` of the last one.
 */
[[nodiscard]] auto layoutSampleRows(std::span<const float> iSizes, float iLineHeight, float iMargin, float iGap)
		-> std::vector<SampleRow>;

}// namespace owl::nest::viewer
