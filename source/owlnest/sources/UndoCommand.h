/**
 * @file UndoCommand.h
 * @author Silmaen
 * @date 13/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <owl.h>

namespace owl::nest {

/**
 * @brief Abstract base class for undoable editor commands.
 */
class UndoCommand {
public:
	UndoCommand(const UndoCommand&) = delete;
	UndoCommand(UndoCommand&&) = default;
	auto operator=(const UndoCommand&) -> UndoCommand& = delete;
	auto operator=(UndoCommand&&) -> UndoCommand& = default;
	UndoCommand() = default;
	virtual ~UndoCommand();

	/// Execute the undo action, restoring previous state.
	virtual void undo(scene::Scene& ioScene) = 0;
	/// Execute the redo action, reapplying the change.
	virtual void redo(scene::Scene& ioScene) = 0;
	/// Human-readable description for menu/tooltip display.
	[[nodiscard]] virtual auto description() const -> std::string = 0;

	/**
	 * @brief Try to merge with a subsequent command of the same type.
	 * @param[in] iOther The newer command to absorb.
	 * @return True if the merge succeeded (the other command is then discarded).
	 */
	[[nodiscard]] virtual auto mergeWith([[maybe_unused]] const UndoCommand& iOther) -> bool { return false; }

	/// Unique command type ID for merge checking (0 = no merging).
	[[nodiscard]] virtual auto typeId() const -> size_t { return 0; }

	/// Timestamp of command creation (for merge timeout).
	std::chrono::steady_clock::time_point m_timestamp = std::chrono::steady_clock::now();

	/// UUID of the entity to select after undo (0 = don't change selection).
	core::UUID m_selectAfterUndo{0};
	/// UUID of the entity to select after redo (0 = don't change selection).
	core::UUID m_selectAfterRedo{0};
};

}// namespace owl::nest
