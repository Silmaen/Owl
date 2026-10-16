/**
 * @file TextLayout.h
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "data/fonts/Font.h"
#include "math/box.h"
#include "math/vectors.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

namespace owl::data::fonts {

/**
 * @brief
 *  Horizontal placement of each line in the layout box.
 */
enum struct HorizontalAlignment : uint8_t {
	Left,///< Lines start on the left edge (on the origin without a box width).
	Center,///< Lines are centred.
	Right,///< Lines end on the right edge (on the origin without a box width).
};

/**
 * @brief
 *  Vertical placement of the block of lines in the layout box.
 */
enum struct VerticalAlignment : uint8_t {
	Top,///< The ascender of the first line touches the top edge (the origin without a box height).
	Middle,///< The block is centred.
	Bottom,///< The descender of the last line touches the bottom edge (the origin without a box height).
	Baseline,///< The baseline of the first line lies on the origin, whatever the box.
};

/**
 * @brief
 *  What happens to text larger than its layout box.
 */
enum struct TextFit : uint8_t {
	Overflow,///< The text is drawn whole, out of the box.
	Clip,///< The glyphs are cut at the box edges.
	Shrink,///< The font size decreases until the text fits, down to `TextLayout::minFontSize`.
};

/**
 * @brief
 *  Layout options of a string.
 *
 * The layout frame has its origin at the centre of the box; one unit of the frame is one unit of `fontSize`. Glyphs
 * keep the aspect ratio of the font: the box only drives the wrap width, the alignment and the fit.
 */
struct OWL_API TextLayout {
	/// Ascender-to-descender height of the font, in frame units.
	float fontSize = 1.f;
	/// Size of the layout box, centred on the origin; 0 on an axis leaves it unbounded.
	math::vec2 boxSize{0.f, 0.f};
	/// Horizontal alignment of every line.
	HorizontalAlignment horizontal = HorizontalAlignment::Center;
	/// Vertical alignment of the block.
	VerticalAlignment vertical = VerticalAlignment::Middle;
	/// Break the lines on spaces to keep them within the box width (no effect without one).
	bool wrap = false;
	/// Behaviour of text larger than the box.
	TextFit fit = TextFit::Overflow;
	/// Smallest font size `TextFit::Shrink` may reach, in frame units.
	float minFontSize = 0.f;
	/// Extra advance added between two glyphs, in font units (scaled with the font size).
	float kerning = 0.f;
	/// Extra space added between two lines, in font units (scaled with the font size).
	float lineSpacing = 0.f;
};

/**
 * @brief
 *  One line of a laid out text: a byte range of the UTF-8 string.
 */
struct TextLine {
	/// Offset of the first byte of the line.
	size_t begin = 0;
	/// Offset one past the last byte of the line (trailing break spaces excluded).
	size_t end = 0;
	/// Advance width of the line, in font units.
	float width = 0.f;
};

/**
 * @brief
 *  Splits a UTF-8 string into lines: on line feeds and, with a wrap width, on spaces; a word longer than the wrap
 *  width is broken between two code points. Allocates nothing.
 */
class OWL_API LineBreaker final {
public:
	/**
	 * @brief
	 *  Start breaking a string.
	 * @param[in] iFont The font giving the advances.
	 * @param[in] iText The UTF-8 text; it must outlive the breaker.
	 * @param[in] iKerning Extra advance between two glyphs, in font units.
	 * @param[in] iWrapWidth Maximal line width in font units; 0 or less breaks on line feeds only.
	 * @param[in] iMeasure False to skip the measure of the lines when nothing wraps: their width is then 0.
	 */
	LineBreaker(const Font& iFont, std::string_view iText, float iKerning, float iWrapWidth,
				bool iMeasure = true) noexcept;

	/**
	 * @brief
	 *  Next line of the text.
	 * @return The line, or nothing once the text is exhausted (an empty text has no line, a final line feed opens an
	 *  empty last line).
	 */
	auto next() -> std::optional<TextLine>;

private:
	/// The font giving the advances.
	const Font* mp_font;
	/// The text being broken.
	std::string_view m_text;
	/// Extra advance between two glyphs.
	float m_kerning;
	/// Maximal line width, 0 or less for no wrap.
	float m_wrapWidth;
	/// Byte offset of the next line.
	size_t m_position = 0;
	/// True when the width of the lines is needed.
	bool m_measure;
	/// True once the last line is returned.
	bool m_done;
};

/**
 * @brief
 *  Code point drawn for a decoded code point: control characters other than line feed and carriage return draw
 *  as `?`.
 * @param[in] iCodePoint The decoded code point.
 * @return The code point to draw.
 */
[[nodiscard]] constexpr auto drawnCodePoint(const char32_t iCodePoint) noexcept -> char32_t {
	return iCodePoint < 0x20 && iCodePoint != U'\r' && iCodePoint != U'\n' ? U'?' : iCodePoint;
}

/**
 * @brief
 *  Resolved geometry of a text layout.
 */
struct TextLayoutMetrics {
	/// Font size actually used (smaller than the requested one after a shrink).
	float fontSize = 0.f;
	/// Number of lines.
	uint32_t lineCount = 0;
	/// Width of the widest line, in frame units.
	float width = 0.f;
	/// Height of the block, from the first ascender to the last descender, in frame units.
	float height = 0.f;
	/// Ordinate of the first baseline, in frame units.
	float firstBaseline = 0.f;
	/// Wrap width used by the line breaker, in font units (0 without wrap).
	float wrapWidth = 0.f;
};

/**
 * @brief
 *  Resolve the font size, the line count and the block placement of a text.
 * @param[in] iFont The font.
 * @param[in] iText The UTF-8 text.
 * @param[in] iLayout The layout options.
 * @return The metrics of the layout.
 */
[[nodiscard]] OWL_API auto computeTextLayout(const Font& iFont, std::string_view iText, const TextLayout& iLayout)
		-> TextLayoutMetrics;

/**
 * @brief
 *  Bounding box of the line boxes of a laid out text (advance widths, ascender to descender), in frame units.
 * @param[in] iFont The font.
 * @param[in] iText The UTF-8 text.
 * @param[in] iLayout The layout options.
 * @return The box; empty (both corners on the origin) for an empty text.
 */
[[nodiscard]] OWL_API auto measureTextLayout(const Font& iFont, std::string_view iText, const TextLayout& iLayout)
		-> math::box2f;

/**
 * @brief
 *  Abscissa where a line starts in the layout frame.
 * @param[in] iLayout The layout options.
 * @param[in] iLineWidth Width of the line, in frame units.
 * @return The abscissa of the pen at the start of the line.
 */
[[nodiscard]] OWL_API auto lineStart(const TextLayout& iLayout, float iLineWidth) noexcept -> float;

/**
 * @brief
 *  Cut a glyph to a box, its texture coordinates with it.
 * @param[in,out] ioGlyph The glyph, in frame units.
 * @param[in] iClip The clip box.
 * @return False when nothing of the glyph is left.
 */
OWL_API auto clipGlyph(Font::GlyphMetrics& ioGlyph, const math::box2f& iClip) noexcept -> bool;

/**
 * @brief
 *  Lay out a UTF-8 string and call a function for each visible glyph, in the layout frame. Allocates nothing.
 * @tparam Fn Callable taking `const Font::GlyphMetrics&` (quad in frame units).
 * @param[in] iFont The font.
 * @param[in] iText The UTF-8 text, invalid sequences drawn as U+FFFD (or its fallback).
 * @param[in] iLayout The layout options.
 * @param[in] iOnGlyph The function called per glyph.
 */
template<typename Fn>
void forEachLaidOutGlyph(const Font& iFont, const std::string_view iText, const TextLayout& iLayout, Fn&& iOnGlyph) {
	const TextLayoutMetrics metrics = computeTextLayout(iFont, iText, iLayout);
	if (metrics.lineCount == 0 || metrics.fontSize <= 0.f)
		return;
	const float scale = metrics.fontSize;
	const float lineStep = (iFont.getScaledLineHeight() + iLayout.lineSpacing) * scale;
	const bool clip = iLayout.fit == TextFit::Clip;
	const math::vec2 half = iLayout.boxSize * 0.5f;
	constexpr float unbounded = 1.e30f;
	const math::box2f clipBox{
			{iLayout.boxSize.x() > 0.f ? -half.x() : -unbounded, iLayout.boxSize.y() > 0.f ? -half.y() : -unbounded},
			{iLayout.boxSize.x() > 0.f ? half.x() : unbounded, iLayout.boxSize.y() > 0.f ? half.y() : unbounded}};
	// A left-aligned line is placed from its start: its width is not needed.
	LineBreaker breaker(iFont, iText, iLayout.kerning, metrics.wrapWidth,
						iLayout.horizontal != HorizontalAlignment::Left);
	float baseline = metrics.firstBaseline;
	while (const auto line = breaker.next()) {
		float penX = lineStart(iLayout, line->width * scale) / scale;
		size_t offset = line->begin;
		while (offset < line->end) {
			const char32_t current = drawnCodePoint(core::utils::decodeNextCodePoint(iText, offset));
			if (current == U'\r')
				continue;
			char32_t next = U' ';
			if (offset < line->end) {
				size_t peek = offset;
				next = drawnCodePoint(core::utils::decodeNextCodePoint(iText, peek));
			}
			auto glyph = iFont.getGlyphBox(current);
			const float advance = iFont.getAdvance(current, next) + iLayout.kerning;
			glyph.quad.min() = (glyph.quad.min() + math::vec2{penX, 0.f}) * scale + math::vec2{0.f, baseline};
			glyph.quad.max() = (glyph.quad.max() + math::vec2{penX, 0.f}) * scale + math::vec2{0.f, baseline};
			penX += advance;
			if (glyph.quad.max().x() <= glyph.quad.min().x() || glyph.quad.max().y() <= glyph.quad.min().y())
				continue;
			if (clip && !clipGlyph(glyph, clipBox))
				continue;
			iOnGlyph(std::as_const(glyph));
		}
		baseline -= lineStep;
	}
}

}// namespace owl::data::fonts
