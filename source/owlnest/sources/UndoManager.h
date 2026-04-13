/**
 * @file UndoManager.h
 * @author Silmaen
 * @date 13/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "UndoCommand.h"

namespace owl::nest {

/**
 * @brief Manages undo/redo stacks for the editor.
 *
 * Owns a stack of UndoCommand objects. Supports command merging (coalescing
 * rapid property edits) and dirty-state tracking for save indicators.
 */
class UndoManager final {
public:
	UndoManager(const UndoManager&) = delete;
	UndoManager(UndoManager&&) = delete;
	auto operator=(const UndoManager&) -> UndoManager& = delete;
	auto operator=(UndoManager&&) -> UndoManager& = delete;
	UndoManager() = default;
	~UndoManager() = default;

	/**
	 * @brief Push a new command onto the undo stack (command already applied).
	 *
	 * Clears the redo stack. Attempts to merge with the top command if
	 * merging is enabled and types match within the merge timeout.
	 * @param[in] iCommand The command to push.
	 */
	void push(uniq<UndoCommand> iCommand);

	/**
	 * @brief Execute a command (calls redo()) then push it onto the undo stack.
	 * @param[in] iCommand The command to execute and push.
	 * @param[in,out] ioScene The scene to apply the command to.
	 */
	void execute(uniq<UndoCommand> iCommand, scene::Scene& ioScene);

	/**
	 * @brief Undo the most recent command.
	 * @param[in,out] ioScene The scene to undo in.
	 */
	void undo(scene::Scene& ioScene);

	/**
	 * @brief Redo the most recently undone command.
	 * @param[in,out] ioScene The scene to redo in.
	 */
	void redo(scene::Scene& ioScene);

	/// Check if undo is possible.
	[[nodiscard]] auto canUndo() const -> bool;
	/// Check if redo is possible.
	[[nodiscard]] auto canRedo() const -> bool;

	/// Get description of the next undo action.
	[[nodiscard]] auto undoDescription() const -> std::string;
	/// Get description of the next redo action.
	[[nodiscard]] auto redoDescription() const -> std::string;

	/// Get the UUID to select after the last undo/redo (0 = no change).
	[[nodiscard]] auto lastSelectionHint() const -> core::UUID { return m_lastSelectionHint; }

	/// Clear all history.
	void clear();

	/// Set the maximum undo history depth.
	void setMaxDepth(const size_t iMaxDepth) { m_maxDepth = iMaxDepth; }

	/// Enable/disable merge coalescing.
	void setMergeEnabled(const bool iEnabled) { m_mergeEnabled = iEnabled; }

	/// Mark the current state as saved.
	void markSaved();
	/// Check if the state has changed since the last save.
	[[nodiscard]] auto isDirty() const -> bool;

private:
	/// Undo stack (most recent at back).
	std::vector<uniq<UndoCommand>> m_undoStack;
	/// Redo stack (most recent at back).
	std::vector<uniq<UndoCommand>> m_redoStack;
	/// Maximum undo history depth.
	size_t m_maxDepth = 100;
	/// Whether merge coalescing is enabled.
	bool m_mergeEnabled = true;
	/// Merge timeout in milliseconds.
	static constexpr auto MergeTimeoutMs = std::chrono::milliseconds{1000};
	/// Undo stack size at last save (for dirty tracking).
	int64_t m_savedIndex = 0;
	/// Entity UUID to select after last undo/redo operation.
	core::UUID m_lastSelectionHint{0};
};

}// namespace owl::nest
