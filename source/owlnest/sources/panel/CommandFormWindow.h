/**
 * @file CommandFormWindow.h
 * @author Silmaen
 * @date 13/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "commands/CommandForm.h"

#include <core/UUID.h>

#include <optional>
#include <string>
#include <vector>

namespace owl::nest::panel {

/**
 * @brief
 *  An entity the entity fields of the form can pick.
 */
struct EntityChoice {
	/// Entity UUID.
	core::UUID uuid{0};
	/// Entity tag, shown in the list.
	std::string tag;
};

/**
 * @brief
 *  A command the user asked to run from the form.
 */
struct CommandSubmission {
	/// Command name.
	std::string command;
	/// Checked arguments.
	commands::CommandArgs args;
};

/**
 * @brief
 *  Argument form of a registry command, opened from the command palette: one widget per argument, prefilled with
 *  the declared defaults, checked before the command runs.
 */
class CommandFormWindow final {
public:
	/**
	 * @brief
	 *  Show the form of a command.
	 * @param[in] iSpec The command declaration.
	 * @param[in] iSelection The selected entity, given to the required entity fields (0 for none).
	 */
	void open(const commands::CommandSpec& iSpec, core::UUID iSelection);

	/**
	 * @brief
	 *  Hide the form.
	 */
	void close() { m_open = false; }

	/**
	 * @brief
	 *  Whether the form is shown.
	 * @return True when open.
	 */
	[[nodiscard]] auto isOpen() const -> bool { return m_open; }

	/**
	 * @brief
	 *  Draw the form when open.
	 * @param[in] iEntities The entities an entity field can pick.
	 * @return The command to run when the user confirmed a valid form (the form then closes).
	 */
	auto onImGuiRender(const std::vector<EntityChoice>& iEntities) -> std::optional<CommandSubmission>;

private:
	/// The form is shown.
	bool m_open = false;
	/// The window takes the focus on the next frame.
	bool m_focus = false;
	/// Description of the command.
	std::string m_description;
	/// Values entered.
	commands::CommandForm m_form;
};

}// namespace owl::nest::panel
