/**
 * @file TextLayout.h
 * @author Silmaen
 * @date 10/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <data/assets/AssetReference.h>

#include <cstddef>
#include <optional>
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

/**
 * @brief
 *  GUID of the asset reference under a glyph of a line: a whole `gid:<16 hex digits>:<path hint>` reference, or
 *  16 hex digits alone (the `Guid:` of an `.owlmeta`).
 * @param[in] iLine The UTF-8 text of the line.
 * @param[in] iGlyphIndex Zero-based glyph index in the line.
 * @return The GUID, or nullopt when no reference is under the glyph.
 */
[[nodiscard]] auto findAssetGuidAt(std::string_view iLine, size_t iGlyphIndex)
		-> std::optional<data::assets::AssetGuid>;

}// namespace owl::nest::codeEditor
