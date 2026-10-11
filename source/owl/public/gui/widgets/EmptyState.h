/**
 * @file EmptyState.h
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
 *  Centre, in the remaining space of the window, what an empty panel will show and how to start: a large glyph,
 *  a title, a description and an optional primary button.
 * @param[in] iGlyph Lucide glyph (`gui::icon::*`), or null.
 * @param[in] iTitle Short title ("No entity selected").
 * @param[in] iDescription One or two sentences, or null.
 * @param[in] iAction Label of the primary button, or null for none.
 * @return True when the button was clicked.
 */
OWL_API auto emptyState(const char* iGlyph, const char* iTitle, const char* iDescription = nullptr,
						const char* iAction = nullptr) -> bool;

}// namespace owl::gui::widgets
