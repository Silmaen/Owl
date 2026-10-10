/**
 * @file TextLayout.cpp
 * @author Silmaen
 * @date 10/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "TextLayout.h"

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

}// namespace owl::nest::codeEditor
