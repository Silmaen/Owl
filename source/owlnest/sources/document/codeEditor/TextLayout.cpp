/**
 * @file TextLayout.cpp
 * @author Silmaen
 * @date 10/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "TextLayout.h"

#include <algorithm>
#include <cctype>

namespace owl::nest::codeEditor {

namespace {
// Byte length of the UTF-8 sequence starting with this byte (a stray continuation byte counts as one).
constexpr auto utf8Length(const unsigned char iLead) -> size_t {
	if (iLead < 0xC0)
		return 1;
	if (iLead < 0xE0)
		return 2;
	if (iLead < 0xF0)
		return 3;
	return 4;
}

auto byteOfGlyph(const std::string_view iLine, const size_t iGlyphIndex) -> size_t {
	size_t byte = 0;
	for (size_t glyph = 0; glyph < iGlyphIndex && byte < iLine.size(); ++glyph)
		byte += utf8Length(static_cast<unsigned char>(iLine[byte]));
	return std::min(byte, iLine.size());
}

auto isHexDigit(const char iChar) -> bool { return std::isxdigit(static_cast<unsigned char>(iChar)) != 0; }

// A reference ends at a blank, a quote or the YAML punctuation around it.
auto isReferenceEnd(const char iChar) -> bool {
	return std::isspace(static_cast<unsigned char>(iChar)) != 0 || iChar == '\'' || iChar == '"' || iChar == ',' ||
		   iChar == ']' || iChar == '}';
}
}// namespace

auto visualColumn(const std::string_view iLine, const size_t iGlyphIndex, const size_t iTabSize) -> size_t {
	size_t column = 0;
	size_t byte = 0;
	for (size_t glyph = 0; glyph < iGlyphIndex && byte < iLine.size(); ++glyph) {
		const auto lead = static_cast<unsigned char>(iLine[byte]);
		if (lead == '\t' && iTabSize > 0)
			column = (column / iTabSize + 1) * iTabSize;
		else
			++column;
		byte += utf8Length(lead);
	}
	return column;
}

auto findAssetGuidAt(const std::string_view iLine, const size_t iGlyphIndex) -> std::optional<data::assets::AssetGuid> {
	const size_t byte = byteOfGlyph(iLine, iGlyphIndex);
	constexpr auto prefix = data::assets::g_GuidReferencePrefix;
	constexpr size_t digits = 16;
	for (size_t start = iLine.find(prefix); start != std::string_view::npos; start = iLine.find(prefix, start + 1)) {
		size_t end = start + prefix.size();
		while (end < iLine.size() && !isReferenceEnd(iLine[end])) ++end;
		if (byte >= start && byte <= end)
			return data::assets::parseGuid(iLine.substr(start + prefix.size(), digits));
	}
	size_t first = byte;
	while (first > 0 && isHexDigit(iLine[first - 1])) --first;
	size_t last = byte;
	while (last < iLine.size() && isHexDigit(iLine[last])) ++last;
	const auto isWord = [&iLine](const size_t iAt) -> bool {
		return iAt < iLine.size() && (std::isalnum(static_cast<unsigned char>(iLine[iAt])) != 0 || iLine[iAt] == '_');
	};
	if (last - first != digits || (first > 0 && isWord(first - 1)) || isWord(last))
		return std::nullopt;
	return data::assets::parseGuid(iLine.substr(first, digits));
}

}// namespace owl::nest::codeEditor
