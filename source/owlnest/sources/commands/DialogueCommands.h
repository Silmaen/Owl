/**
 * @file DialogueCommands.h
 * @author Silmaen
 * @date 30/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <data/dialogue/Dialogue.h>

#include <string>

namespace owl::nest::commands {

class CommandRegistry;

/**
 * @brief
 *  What the `dialogue.*` commands edit: an open `.owldlg`, changed as a whole so each command is one undo entry.
 */
class DialogueEditor {
public:
	/**
	 * @brief
	 *  Default constructor.
	 */
	DialogueEditor() = default;

	/**
	 * @brief
	 *  Copy constructor.
	 */
	DialogueEditor(const DialogueEditor&) = default;

	/**
	 * @brief
	 *  Move constructor.
	 */
	DialogueEditor(DialogueEditor&&) = default;

	/**
	 * @brief
	 *  Copy assignment.
	 * @return This editor.
	 */
	auto operator=(const DialogueEditor&) -> DialogueEditor& = default;

	/**
	 * @brief
	 *  Move assignment.
	 * @return This editor.
	 */
	auto operator=(DialogueEditor&&) -> DialogueEditor& = default;

	/**
	 * @brief
	 *  Destructor.
	 */
	virtual ~DialogueEditor();

	/**
	 * @brief
	 *  The dialogue edited.
	 * @return The dialogue.
	 */
	[[nodiscard]] virtual auto dialogue() const -> const data::dialogue::Dialogue& = 0;

	/**
	 * @brief
	 *  Replace the dialogue, as one undo entry.
	 * @param[in] iNext The dialogue after the edit.
	 * @param[in] iDescription Description of the undo entry.
	 * @param[in] iMerge Whether the entry merges with the previous one of the same description (typing).
	 * @return False when nothing changes.
	 */
	virtual auto apply(data::dialogue::Dialogue iNext, const std::string& iDescription, bool iMerge) -> bool = 0;
};

/**
 * @brief
 *  Register the `dialogue.*` commands: create a dialogue, add, remove, duplicate, move and link nodes, edit their
 *  texts and functions, the options of a choice and the bends of a link.
 * @param[in,out] ioRegistry The registry.
 */
void addDialogueCommands(CommandRegistry& ioRegistry);

}// namespace owl::nest::commands
