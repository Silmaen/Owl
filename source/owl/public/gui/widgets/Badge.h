/**
 * @file Badge.h
 * @author Silmaen
 * @date 11/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"

#include <cstdint>

namespace owl::gui::widgets {

/**
 * @brief
 *  Meaning of a badge, which picks its colour from the theme.
 */
enum struct BadgeKind : uint8_t {
	Neutral,///< Surface colour, muted text.
	Accent,///< Theme accent.
	Success,///< Success colour.
	Warning,///< Warning colour.
	Error,///< Error colour.
	Info,///< Information colour.
};

/**
 * @brief
 *  Small rounded pill with a short text (a count, a state), on the current line.
 * @param[in] iText The text.
 * @param[in] iKind Its meaning.
 */
OWL_API void badge(const char* iText, BadgeKind iKind = BadgeKind::Neutral);

/**
 * @brief
 *  Toggleable filter chip: a rounded pill drawn in the faint accent while selected.
 * @param[in] iLabel The label, also the ImGui identifier.
 * @param[in] iSelected Whether the chip is on.
 * @return True on click.
 */
OWL_API auto chip(const char* iLabel, bool iSelected) -> bool;

}// namespace owl::gui::widgets
