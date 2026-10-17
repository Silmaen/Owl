/**
 * @file FontCharset.cpp
 * @author Silmaen
 * @date 17/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "FontCharset.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace owl::nest::viewer {

namespace {
using data::fonts::CodePointRange;

struct UnicodeBlock {
	std::string_view name;
	CodePointRange range;
};

constexpr std::array<UnicodeBlock, 26> g_blocks{{
		{.name = "Basic Latin", .range = {.first = 0x0000, .last = 0x007F}},
		{.name = "Latin-1 Supplement", .range = {.first = 0x0080, .last = 0x00FF}},
		{.name = "Latin Extended-A", .range = {.first = 0x0100, .last = 0x017F}},
		{.name = "Latin Extended-B", .range = {.first = 0x0180, .last = 0x024F}},
		{.name = "IPA Extensions", .range = {.first = 0x0250, .last = 0x02AF}},
		{.name = "Spacing Modifier Letters", .range = {.first = 0x02B0, .last = 0x02FF}},
		{.name = "Combining Diacritical Marks", .range = {.first = 0x0300, .last = 0x036F}},
		{.name = "Greek and Coptic", .range = {.first = 0x0370, .last = 0x03FF}},
		{.name = "Cyrillic", .range = {.first = 0x0400, .last = 0x04FF}},
		{.name = "Latin Extended Additional", .range = {.first = 0x1E00, .last = 0x1EFF}},
		{.name = "General Punctuation", .range = {.first = 0x2000, .last = 0x206F}},
		{.name = "Superscripts and Subscripts", .range = {.first = 0x2070, .last = 0x209F}},
		{.name = "Currency Symbols", .range = {.first = 0x20A0, .last = 0x20CF}},
		{.name = "Letterlike Symbols", .range = {.first = 0x2100, .last = 0x214F}},
		{.name = "Number Forms", .range = {.first = 0x2150, .last = 0x218F}},
		{.name = "Arrows", .range = {.first = 0x2190, .last = 0x21FF}},
		{.name = "Mathematical Operators", .range = {.first = 0x2200, .last = 0x22FF}},
		{.name = "Miscellaneous Technical", .range = {.first = 0x2300, .last = 0x23FF}},
		{.name = "Box Drawing", .range = {.first = 0x2500, .last = 0x257F}},
		{.name = "Block Elements", .range = {.first = 0x2580, .last = 0x259F}},
		{.name = "Geometric Shapes", .range = {.first = 0x25A0, .last = 0x25FF}},
		{.name = "Miscellaneous Symbols", .range = {.first = 0x2600, .last = 0x26FF}},
		{.name = "Dingbats", .range = {.first = 0x2700, .last = 0x27BF}},
		{.name = "Hiragana", .range = {.first = 0x3040, .last = 0x309F}},
		{.name = "Katakana", .range = {.first = 0x30A0, .last = 0x30FF}},
		{.name = "CJK Unified Ideographs", .range = {.first = 0x4E00, .last = 0x9FFF}},
}};

constexpr std::string_view g_other = "Other";
}// namespace

auto unicodeBlockName(const char32_t iCodePoint) -> std::string_view {
	const auto* const found = std::ranges::find_if(
			g_blocks, [iCodePoint](const UnicodeBlock& iBlock) -> bool { return iBlock.range.contains(iCodePoint); });
	return found == g_blocks.end() ? g_other : found->name;
}

auto summarizeCharset(const data::fonts::FontSettings& iSettings) -> CharsetSummary {
	CharsetSummary summary;
	auto ranges = iSettings.getCharsetRanges();
	std::ranges::sort(ranges, [](const CodePointRange& iLhs, const CodePointRange& iRhs) -> bool {
		return iLhs.first < iRhs.first;
	});
	std::vector<CodePointRange> merged;
	for (const auto& range: ranges) {
		if (range.last < range.first)
			continue;
		if (!merged.empty() && range.first <= merged.back().last + 1)
			merged.back().last = std::max(merged.back().last, range.last);
		else
			merged.push_back(range);
	}
	for (const auto& range: merged) {
		auto from = range.first;
		while (from <= range.last) {
			const auto name = unicodeBlockName(from);
			// The slice ends with the block, or just before the next known block when the code point is in no block.
			auto to = range.last;
			if (name == g_other) {
				for (const auto& block: g_blocks) {
					if (block.range.first > from)
						to = std::min(to, static_cast<char32_t>(block.range.first - 1));
				}
			} else {
				const auto* const block = std::ranges::find(g_blocks, name, &UnicodeBlock::name);
				to = std::min(to, block->range.last);
			}
			const auto count = static_cast<uint32_t>(to - from + 1);
			summary.blocks.push_back({.name = name, .range = {.first = from, .last = to}, .count = count});
			summary.codePoints += count;
			from = to + 1;
		}
	}
	return summary;
}

auto layoutSampleRows(const std::span<const float> iSizes, const float iLineHeight, const float iMargin,
					  const float iGap) -> std::vector<SampleRow> {
	std::vector<SampleRow> rows;
	float top = iGap;
	for (const float size: iSizes) {
		const float height = size * (iLineHeight + 2.f * iMargin);
		rows.push_back({.fontSize = size, .top = top, .height = height});
		top += height + iGap;
	}
	return rows;
}

}// namespace owl::nest::viewer
