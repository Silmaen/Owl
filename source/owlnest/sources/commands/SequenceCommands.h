/**
 * @file SequenceCommands.h
 * @author Silmaen
 * @date 31/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <data/sequence/Sequence.h>

#include <string>

namespace owl::nest::commands {

class CommandRegistry;

/**
 * @brief
 *  What the `sequence.*` commands edit: the sequence of an open document, changed as a whole.
 */
class SequenceEditor {
public:
	/**
	 * @brief
	 *  Default constructor.
	 */
	SequenceEditor() = default;

	/**
	 * @brief
	 *  Copy constructor.
	 */
	SequenceEditor(const SequenceEditor&) = default;

	/**
	 * @brief
	 *  Move constructor.
	 */
	SequenceEditor(SequenceEditor&&) = default;

	/**
	 * @brief
	 *  Copy assignment.
	 * @return This editor.
	 */
	auto operator=(const SequenceEditor&) -> SequenceEditor& = default;

	/**
	 * @brief
	 *  Move assignment.
	 * @return This editor.
	 */
	auto operator=(SequenceEditor&&) -> SequenceEditor& = default;

	/**
	 * @brief
	 *  Destructor.
	 */
	virtual ~SequenceEditor();

	/**
	 * @brief
	 *  The sequence as it is.
	 * @return The sequence.
	 */
	[[nodiscard]] virtual auto sequence() const -> const data::sequence::Sequence& = 0;

	/**
	 * @brief
	 *  Replace the sequence, as one undo entry.
	 * @param[in] iNext The sequence after the edit.
	 * @param[in] iDescription Description of the undo entry.
	 * @param[in] iMerge Whether it merges with the previous entry of the same description (a drag, typing).
	 * @return False when nothing changes.
	 */
	virtual auto apply(data::sequence::Sequence iNext, const std::string& iDescription, bool iMerge) -> bool = 0;
};

/**
 * @brief
 *  Register the `sequence.*` commands: create a `.owlseq`, edit its tracks and keys.
 * @param[in,out] ioRegistry The registry.
 */
void addSequenceCommands(CommandRegistry& ioRegistry);

}// namespace owl::nest::commands
