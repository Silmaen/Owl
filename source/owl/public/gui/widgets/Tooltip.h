/**
 * @file Tooltip.h
 * @author Silmaen
 * @date 11/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"

namespace owl::gui::widgets {

/**
 * @brief
 *  Draw a tooltip with a bold title, an optional shortcut aligned right and an optional description line.
 * @param[in] iTitle The title.
 * @param[in] iShortcut Shortcut text ("Ctrl+S"), or null.
 * @param[in] iDescription One sentence about the action, or null.
 */
OWL_API void richTooltip(const char* iTitle, const char* iShortcut = nullptr, const char* iDescription = nullptr);

/**
 * @brief
 *  Show `richTooltip` when the last item is hovered, after the normal hover delay.
 * @param[in] iTitle The title.
 * @param[in] iShortcut Shortcut text, or null.
 * @param[in] iDescription One sentence about the action, or null.
 */
OWL_API void itemTooltip(const char* iTitle, const char* iShortcut = nullptr, const char* iDescription = nullptr);

}// namespace owl::gui::widgets
