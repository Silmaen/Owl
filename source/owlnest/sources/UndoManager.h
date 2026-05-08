/**
 * @file UndoManager.h
 * @author Silmaen
 * @date 13/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "UndoCommand.h"

#include <chrono>
#include <string>
#include <utility>
#include <vector>

namespace owl::nest {
/**
 * @brief
 *  Manages undo/redo stacks for one editor target type.
 * @tparam Target Concrete editor state the stacks operate on (scene, node canvas...).
 *
 * Owns a stack of `UndoCommand<Target>` objects. Supports command merging
 * (coalescing rapid property edits) and dirty-state tracking for save
 * indicators. The class is header-only because it is a template; every
 * concrete instantiation (`SceneUndoManager`, `NodeGraphUndoManager`, ...) is
 * produced where it is used.
 */
template<typename Target>
class UndoManager final {
public:
	UndoManager(const UndoManager&) = delete;

	UndoManager(UndoManager&&) = delete;

	auto operator=(const UndoManager&) -> UndoManager& = delete;

	auto operator=(UndoManager&&) -> UndoManager& = delete;

	UndoManager() = default;

	~UndoManager() = default;

	/**
	 * @brief
	 *  Push a new command onto the undo stack (command already applied).
	 *
	 * Clears the redo stack. Attempts to merge with the top command if
	 * merging is enabled and types match within the merge timeout.
	 * @param[in] iCommand The command to push.
	 */
	void push(uniq<UndoCommand<Target>> iCommand) {
		if (m_mergeEnabled && !m_undoStack.empty()) {
			auto& top = m_undoStack.back();
			if (top->typeId() != 0 && top->typeId() == iCommand->typeId()) {
				const auto elapsed = iCommand->m_timestamp - top->m_timestamp;
				if (elapsed <= s_mergeTimeoutMs && top->mergeWith(*iCommand))
					return;// Merged: discard the new command
			}
		}
		m_redoStack.clear();
		if (m_undoStack.size() >= m_maxDepth) {
			m_undoStack.erase(m_undoStack.begin());
			--m_savedIndex;
		}
		m_undoStack.push_back(std::move(iCommand));
	}

	/**
	 * @brief
	 *  Execute a command (calls redo()) then push it onto the undo stack.
	 * @param[in] iCommand The command to execute and push.
	 * @param[in,out] ioTarget The target to apply the command to.
	 */
	void execute(uniq<UndoCommand<Target>> iCommand, Target& ioTarget) {
		iCommand->redo(ioTarget);
		m_lastSelectionHint = iCommand->m_selectAfterRedo;
		/**
		 * @brief
		 *  Push.
		 */
		push(std::move(iCommand));
	}

	/**
	 * @brief
	 *  Undo the most recent command.
	 * @param[in,out] ioTarget The target to undo in.
	 */
	void undo(Target& ioTarget) {
		if (m_undoStack.empty())
			return;
		auto command = std::move(m_undoStack.back());
		m_undoStack.pop_back();
		command->undo(ioTarget);
		m_lastSelectionHint = command->m_selectAfterUndo;
		m_redoStack.push_back(std::move(command));
	}

	/**
	 * @brief
	 *  Redo the most recently undone command.
	 * @param[in,out] ioTarget The target to redo in.
	 */
	void redo(Target& ioTarget) {
		if (m_redoStack.empty())
			return;
		auto command = std::move(m_redoStack.back());
		m_redoStack.pop_back();
		command->redo(ioTarget);
		m_lastSelectionHint = command->m_selectAfterRedo;
		m_undoStack.push_back(std::move(command));
	}

	/**
	 * @brief
	 *  Check if undo is possible.
	 * @return True when undo is allowed.
	 */
	[[nodiscard]] auto canUndo() const -> bool { return !m_undoStack.empty(); }

	/**
	 * @brief
	 *  Check if redo is possible.
	 * @return True when redo is allowed.
	 */
	[[nodiscard]] auto canRedo() const -> bool { return !m_redoStack.empty(); }

	/**
	 * @brief
	 *  Get description of the next undo action.
	 * @return The std string.
	 */
	[[nodiscard]] auto undoDescription() const -> std::string {
		if (m_undoStack.empty())
			return {};
		return m_undoStack.back()->description();
	}

	/**
	 * @brief
	 *  Get description of the next redo action.
	 * @return The std string.
	 */
	[[nodiscard]] auto redoDescription() const -> std::string {
		if (m_redoStack.empty())
			return {};
		return m_redoStack.back()->description();
	}

	/**
	 * @brief
	 *  Get the UUID to select after the last undo/redo (0 = no change).
	 * @return The core UUID.
	 */
	[[nodiscard]] auto lastSelectionHint() const -> core::UUID { return m_lastSelectionHint; }

	/**
	 * @brief
	 *  Clear all history.
	 */
	void clear() {
		m_undoStack.clear();
		m_redoStack.clear();
		m_savedIndex = 0;
		m_lastSelectionHint = core::UUID{0};
	}

	/**
	 * @brief
	 *  Set the maximum undo history depth.
	 */
	void setMaxDepth(const size_t iMaxDepth) { m_maxDepth = iMaxDepth; }

	/**
	 * @brief
	 *  Enable/disable merge coalescing.
	 */
	void setMergeEnabled(const bool iEnabled) { m_mergeEnabled = iEnabled; }

	/**
	 * @brief
	 *  Mark the current state as saved.
	 */
	void markSaved() { m_savedIndex = static_cast<int64_t>(m_undoStack.size()); }

	/**
	 * @brief
	 *  Check if the state has changed since the last save.
	 * @return True when the object is dirty.
	 */
	[[nodiscard]] auto isDirty() const -> bool { return static_cast<int64_t>(m_undoStack.size()) != m_savedIndex; }

private:
	/// Undo stack (most recent at back).
	std::vector<uniq<UndoCommand<Target>>> m_undoStack;
	/// Redo stack (most recent at back).
	std::vector<uniq<UndoCommand<Target>>> m_redoStack;
	/// Maximum undo history depth.
	size_t m_maxDepth = 100;
	/// Whether merge coalescing is enabled.
	bool m_mergeEnabled = true;
	/// Merge timeout in milliseconds.
	static constexpr auto s_mergeTimeoutMs = std::chrono::milliseconds{1000};
	/// Undo stack size at last save (for dirty tracking).
	int64_t m_savedIndex = 0;
	/// Entity UUID to select after last undo/redo operation.
	core::UUID m_lastSelectionHint{0};
};

/**
 * @brief
 *  Convenience alias for scene-level undo management (default editor case).
 */
using SceneUndoManager = UndoManager<scene::Scene>;

}// namespace owl::nest
