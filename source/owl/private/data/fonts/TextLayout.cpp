/**
 * @file TextLayout.cpp
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "data/fonts/TextLayout.h"

#include "core/utils/Utf8.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <tuple>

namespace owl::data::fonts {

namespace {

constexpr size_t g_noBreak = static_cast<size_t>(-1);
// Relative slack of the fit tests, so a text measured at exactly the box size still fits.
constexpr float g_fitTolerance = 1.e-4f;
constexpr int g_shrinkIterations = 20;

constexpr auto isBreakSpace(const char32_t iCodePoint) noexcept -> bool {
	return iCodePoint == U' ' || iCodePoint == U'\t';
}

// Code point after the one ending at iOffset, for the kerning pair; a line end pairs with a space.
auto peekNext(const std::string_view iText, const size_t iOffset) noexcept -> char32_t {
	if (iOffset >= iText.size())
		return U' ';
	size_t peek = iOffset;
	const char32_t next = drawnCodePoint(core::utils::decodeNextCodePoint(iText, peek));
	return next == U'\n' || next == U'\r' ? U' ' : next;
}

struct BlockExtent {
	uint32_t lines = 0;
	float width = 0.f;
};

auto measureLines(const Font& iFont, const std::string_view iText, const float iKerning, const float iWrapWidth)
		-> BlockExtent {
	BlockExtent extent;
	LineBreaker breaker(iFont, iText, iKerning, iWrapWidth);
	while (const auto line = breaker.next()) {
		++extent.lines;
		extent.width = std::max(extent.width, line->width);
	}
	return extent;
}

auto blockHeight(const Font& iFont, const uint32_t iLines, const float iLineSpacing) -> float {
	if (iLines == 0)
		return 0.f;
	return iFont.getAscender() - iFont.getDescender() +
		   static_cast<float>(iLines - 1) * (iFont.getScaledLineHeight() + iLineSpacing);
}

auto blockTop(const TextLayout& iLayout, const float iHeight, const float iAscender) -> float {
	const float boxHeight = iLayout.boxSize.y();
	switch (iLayout.vertical) {
		case VerticalAlignment::Top:
			return boxHeight > 0.f ? 0.5f * boxHeight : 0.f;
		case VerticalAlignment::Middle:
			return 0.5f * iHeight;
		case VerticalAlignment::Bottom:
			return boxHeight > 0.f ? -0.5f * boxHeight + iHeight : iHeight;
		case VerticalAlignment::Baseline:
			return iAscender;
	}
	return 0.f;
}

}// namespace

LineBreaker::LineBreaker(const Font& iFont, const std::string_view iText, const float iKerning,
						 const float iWrapWidth) noexcept
	: mp_font{&iFont}, m_text{iText}, m_kerning{iKerning}, m_wrapWidth{iWrapWidth}, m_done{iText.empty()} {}

auto LineBreaker::next() -> std::optional<TextLine> {
	if (m_done)
		return std::nullopt;
	const size_t lineBegin = m_position;
	float width = 0.f;
	float penX = 0.f;
	bool hasGlyph = false;
	bool inSpaces = false;
	size_t breakEnd = g_noBreak;
	size_t breakResume = g_noBreak;
	float breakWidth = 0.f;
	size_t offset = m_position;
	while (offset < m_text.size()) {
		const size_t glyphBegin = offset;
		const char32_t current = drawnCodePoint(core::utils::decodeNextCodePoint(m_text, offset));
		if (current == U'\r')
			continue;
		if (current == U'\n') {
			m_position = offset;
			return TextLine{.begin = lineBegin, .end = glyphBegin, .width = width};
		}
		const float advance = mp_font->getAdvance(current, peekNext(m_text, offset)) + m_kerning;
		if (isBreakSpace(current)) {
			if (hasGlyph && !inSpaces) {
				breakEnd = glyphBegin;
				breakWidth = width;
			}
			inSpaces = true;
			penX += advance;
			continue;
		}
		if (inSpaces && breakEnd != g_noBreak)
			breakResume = glyphBegin;
		inSpaces = false;
		const float right = penX + mp_font->getAdvance(current, U' ');
		if (m_wrapWidth > 0.f && hasGlyph && right > m_wrapWidth) {
			if (breakEnd != g_noBreak && breakResume != g_noBreak) {
				m_position = breakResume;
				return TextLine{.begin = lineBegin, .end = breakEnd, .width = breakWidth};
			}
			m_position = glyphBegin;
			return TextLine{.begin = lineBegin, .end = glyphBegin, .width = width};
		}
		width = right;
		penX += advance;
		hasGlyph = true;
	}
	m_done = true;
	m_position = m_text.size();
	return TextLine{.begin = lineBegin, .end = m_text.size(), .width = width};
}

auto computeTextLayout(const Font& iFont, const std::string_view iText, const TextLayout& iLayout)
		-> TextLayoutMetrics {
	TextLayoutMetrics metrics;
	float size = std::max(iLayout.fontSize, 0.f);
	metrics.fontSize = size;
	if (size <= 0.f || iText.empty())
		return metrics;
	const math::vec2& box = iLayout.boxSize;
	const bool wrap = iLayout.wrap && box.x() > 0.f;
	BlockExtent block;
	const auto fits = [&](const float iSize) -> bool {
		block = measureLines(iFont, iText, iLayout.kerning, wrap ? box.x() / iSize : 0.f);
		const float slack = 1.f + g_fitTolerance;
		return (box.x() <= 0.f || block.width * iSize <= box.x() * slack) &&
			   (box.y() <= 0.f || blockHeight(iFont, block.lines, iLayout.lineSpacing) * iSize <= box.y() * slack);
	};
	if (!fits(size) && iLayout.fit == TextFit::Shrink) {
		const float minSize = std::clamp(iLayout.minFontSize, size * g_fitTolerance, size);
		if (wrap) {
			float low = minSize;
			float high = size;
			for (int i = 0; i < g_shrinkIterations; ++i) {
				const float mid = 0.5f * (low + high);
				if (fits(mid))
					low = mid;
				else
					high = mid;
			}
			size = low;
			std::ignore = fits(size);
		} else {
			if (box.x() > 0.f && block.width > 0.f)
				size = std::min(size, box.x() / block.width);
			if (const float height = blockHeight(iFont, block.lines, iLayout.lineSpacing);
				box.y() > 0.f && height > 0.f)
				size = std::min(size, box.y() / height);
			size = std::max(size, minSize);
		}
	}
	metrics.fontSize = size;
	metrics.lineCount = block.lines;
	metrics.width = block.width * size;
	metrics.height = blockHeight(iFont, block.lines, iLayout.lineSpacing) * size;
	metrics.wrapWidth = wrap ? box.x() / size : 0.f;
	const float ascender = iFont.getAscender() * size;
	metrics.firstBaseline = blockTop(iLayout, metrics.height, ascender) - ascender;
	return metrics;
}

auto measureTextLayout(const Font& iFont, const std::string_view iText, const TextLayout& iLayout) -> math::box2f {
	const TextLayoutMetrics metrics = computeTextLayout(iFont, iText, iLayout);
	if (metrics.lineCount == 0)
		return {};
	float left = 0.f;
	float right = 0.f;
	bool first = true;
	LineBreaker breaker(iFont, iText, iLayout.kerning, metrics.wrapWidth);
	while (const auto line = breaker.next()) {
		const float width = line->width * metrics.fontSize;
		const float start = lineStart(iLayout, width);
		left = first ? start : std::min(left, start);
		right = first ? start + width : std::max(right, start + width);
		first = false;
	}
	const float top = metrics.firstBaseline + iFont.getAscender() * metrics.fontSize;
	return {math::vec2{left, top - metrics.height}, math::vec2{right, top}};
}

auto lineStart(const TextLayout& iLayout, const float iLineWidth) noexcept -> float {
	const float boxWidth = iLayout.boxSize.x();
	switch (iLayout.horizontal) {
		case HorizontalAlignment::Left:
			return boxWidth > 0.f ? -0.5f * boxWidth : 0.f;
		case HorizontalAlignment::Center:
			return -0.5f * iLineWidth;
		case HorizontalAlignment::Right:
			return boxWidth > 0.f ? 0.5f * boxWidth - iLineWidth : -iLineWidth;
	}
	return 0.f;
}

auto clipGlyph(Font::GlyphMetrics& ioGlyph, const math::box2f& iClip) noexcept -> bool {
	for (size_t axis = 0; axis < 2; ++axis) {
		const float quadMin = ioGlyph.quad.min()[axis];
		const float quadMax = ioGlyph.quad.max()[axis];
		const float low = std::max(quadMin, iClip.min()[axis]);
		const float high = std::min(quadMax, iClip.max()[axis]);
		if (high <= low)
			return false;
		const float uvMin = ioGlyph.uv.min()[axis];
		const float uvPerUnit = (ioGlyph.uv.max()[axis] - uvMin) / (quadMax - quadMin);
		ioGlyph.uv.min()[axis] = uvMin + (low - quadMin) * uvPerUnit;
		ioGlyph.uv.max()[axis] = uvMin + (high - quadMin) * uvPerUnit;
		ioGlyph.quad.min()[axis] = low;
		ioGlyph.quad.max()[axis] = high;
	}
	return true;
}

}// namespace owl::data::fonts
