/**
 * @file SearchField.h
 * @author Silmaen
 * @date 11/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"

#include <string>
#include <string_view>

namespace owl::gui::widgets {

/**
 * @brief
 *  Whether a text matches a filter: every word of the filter appears in the text, case-insensitively.
 * @param[in] iText The text.
 * @param[in] iFilter The filter; an empty or blank filter matches everything.
 * @return True on a match.
 */
OWL_API auto matchesFilter(std::string_view iText, std::string_view iFilter) -> bool;

/**
 * @brief
 *  Full-width search field with a magnifier glyph, a placeholder and a clear button while it holds text.
 *  `Escape` clears it while it has the keyboard focus.
 * @param[in] iId Unique ImGui identifier.
 * @param[in,out] ioText The search text.
 * @param[in] iHint Placeholder shown while empty.
 * @return True if the text changed.
 */
OWL_API auto searchField(const char* iId, std::string& ioText, const char* iHint = "Search") -> bool;

}// namespace owl::gui::widgets
