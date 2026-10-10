/**
 * @file TextLayout.h
 * @author Silmaen
 * @date 10/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <cstddef>
#include <string_view>

namespace owl::nest::codeEditor {
/**
 * @brief
 *  Visual column of a glyph in a line, tabs expanded to the next tab stop.
 *
 * The text editor locates its cursors by glyph index (one glyph per UTF-8 code point); the status line shows the
 * column the user sees.
 * @param[in] iLine The UTF-8 text of the line.
 * @param[in] iGlyphIndex Zero-based glyph index in the line (clamped to its length).
 * @param[in] iTabSize Width of a tab stop, in columns (0 counts a tab as one column).
 * @return The zero-based visual column.
 */
[[nodiscard]] auto visualColumn(std::string_view iLine, size_t iGlyphIndex, size_t iTabSize) -> size_t;

}// namespace owl::nest::codeEditor
