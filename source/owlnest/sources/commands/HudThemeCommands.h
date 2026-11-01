/**
 * @file HudThemeCommands.h
 * @author Silmaen
 * @date 01/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <scene/HudTheme.h>

#include <string>

namespace owl::nest::commands {

class CommandRegistry;

/**
 * @brief
 *  A HUD theme open in an editor tab, edited by the `theme.*` commands through its undo history.
 */
class HudThemeEditor {
public:
	HudThemeEditor() = default;

	HudThemeEditor(const HudThemeEditor&) = default;

	HudThemeEditor(HudThemeEditor&&) = default;

	auto operator=(const HudThemeEditor&) -> HudThemeEditor& = default;

	auto operator=(HudThemeEditor&&) -> HudThemeEditor& = default;

	/**
	 * @brief
	 *  Destructor.
	 */
	virtual ~HudThemeEditor();

	/**
	 * @brief
	 *  The theme as it is.
	 * @return The theme.
	 */
	[[nodiscard]] virtual auto theme() const -> const scene::HudTheme& = 0;

	/**
	 * @brief
	 *  Replace the theme as one undoable step.
	 * @param[in] iTheme The theme after the edit.
	 * @param[in] iDescription Name of the step in the history.
	 * @return False when nothing changes.
	 */
	virtual auto setTheme(const scene::HudTheme& iTheme, const std::string& iDescription) -> bool = 0;
};

/**
 * @brief
 *  Register `theme.create`, `theme.add_style`, `theme.remove_style`, `theme.rename_style`, `theme.move_style`,
 *  `theme.set_style` and `theme.set_name`.
 * @param[in,out] ioRegistry The registry.
 */
void addHudThemeCommands(CommandRegistry& ioRegistry);

}// namespace owl::nest::commands
