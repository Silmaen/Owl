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
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace owl::nest {
/**
 * @brief
 *  Several commands undone and redone as one step (a macro replay).
 * @tparam Target Concrete editor state the commands operate on.
 */
template<typename Target>
class UndoGroupCommand final : public UndoCommand<Target> {
public:
	UndoGroupCommand(const UndoGroupCommand&) = delete;

	UndoGroupCommand(UndoGroupCommand&&) = default;

	auto operator=(const UndoGroupCommand&) -> UndoGroupCommand& = delete;

	auto operator=(UndoGroupCommand&&) -> UndoGroupCommand& = default;

	/**
	 * @brief
	 *  Build the group from commands already applied, oldest first.
	 * @param[in] iCommands The commands.
	 * @param[in] iDescription Description of the group.
	 */
	UndoGroupCommand(std::vector<uniq<UndoCommand<Target>>> iCommands, std::string iDescription)
		: m_commands{std::move(iCommands)}, m_description{std::move(iDescription)} {
		if (!m_commands.empty()) {
			this->m_selectAfterUndo = m_commands.front()->m_selectAfterUndo;
			this->m_selectAfterRedo = m_commands.back()->m_selectAfterRedo;
		}
	}

	/**
	 * @brief
	 *  Destructor.
	 */
	~UndoGroupCommand() override = default;

	/**
	 * @brief
	 *  Undo every command, newest first.
	 * @param[in,out] ioTarget The target.
	 */
	void undo(Target& ioTarget) override {
		for (auto& command: m_commands | std::views::reverse) command->undo(ioTarget);
	}

	/**
	 * @brief
	 *  Redo every command, oldest first.
	 * @param[in,out] ioTarget The target.
	 */
	void redo(Target& ioTarget) override {
		for (auto& command: m_commands) command->redo(ioTarget);
	}

	/**
	 * @brief
	 *  Description of the group.
	 * @return The description.
	 */
	[[nodiscard]] auto description() const -> std::string override { return m_description; }

	/**
	 * @brief
	 *  Number of commands in the group.
	 * @return The count.
	 */
	[[nodiscard]] auto size() const -> size_t { return m_commands.size(); }

private:
	/// The commands, oldest first.
	std::vector<uniq<UndoCommand<Target>>> m_commands;
	/// Description of the group.
	std::string m_description;
};

/**
 * @brief
 *  Manages undo/redo stacks for one editor target type.
 * @tparam Target Concrete editor state the stacks operate on (scene, node canvas...).
 *
 * Owns a stack of `UndoCommand<Target>` objects. Supports command merging
 * (coalescing rapid property edits) and dirty-state tracking for save
 * indicators. Dirty tracking uses generations: every pushed command, and every
 * merge into the top command, gets a fresh generation number; the current state
 * is the generation of the top of the undo stack, and the document is dirty
 * whenever it differs from the generation recorded by `markSaved()`. The class is header-only because it is a template; every
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
		if (m_mergeEnabled && m_undoStack.size() > undoFloor()) {
			auto& top = m_undoStack.back();
			if (top.command->typeId() != 0 && top.command->typeId() == iCommand->typeId()) {
				const auto elapsed = iCommand->m_timestamp - top.command->m_timestamp;
				if (elapsed <= s_mergeTimeoutMs && top.command->mergeWith(*iCommand)) {
					top.generation = m_nextGeneration++;
					return;
				}
			}
		}
		m_redoStack.clear();
		for (auto& group: m_groups) group.redoFloor = 0;
		if (m_groups.empty() && !m_undoStack.empty() && m_undoStack.size() >= m_maxDepth) {
			m_baseGeneration = m_undoStack.front().generation;
			m_undoStack.erase(m_undoStack.begin());
		}
		m_undoStack.push_back({.command = std::move(iCommand), .generation = m_nextGeneration++});
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
		push(std::move(iCommand));
	}

	/**
	 * @brief
	 *  Undo the most recent command.
	 * @param[in,out] ioTarget The target to undo in.
	 */
	void undo(Target& ioTarget) {
		if (!canUndo())
			return;
		auto entry = std::move(m_undoStack.back());
		m_undoStack.pop_back();
		entry.command->undo(ioTarget);
		m_lastSelectionHint = entry.command->m_selectAfterUndo;
		m_redoStack.push_back(std::move(entry));
	}

	/**
	 * @brief
	 *  Redo the most recently undone command.
	 * @param[in,out] ioTarget The target to redo in.
	 */
	void redo(Target& ioTarget) {
		if (!canRedo())
			return;
		auto entry = std::move(m_redoStack.back());
		m_redoStack.pop_back();
		entry.command->redo(ioTarget);
		m_lastSelectionHint = entry.command->m_selectAfterRedo;
		m_undoStack.push_back(std::move(entry));
	}

	/**
	 * @brief
	 *  Check if undo is possible.
	 * @return True when undo is allowed.
	 */
	[[nodiscard]] auto canUndo() const -> bool { return m_undoStack.size() > undoFloor(); }

	/**
	 * @brief
	 *  Check if redo is possible.
	 * @return True when redo is allowed.
	 */
	[[nodiscard]] auto canRedo() const -> bool {
		return m_redoStack.size() > (m_groups.empty() ? 0 : m_groups.back().redoFloor);
	}

	/**
	 * @brief
	 *  Get description of the next undo action.
	 * @return The std string.
	 */
	[[nodiscard]] auto undoDescription() const -> std::string {
		if (m_undoStack.empty())
			return {};
		return m_undoStack.back().command->description();
	}

	/**
	 * @brief
	 *  Get description of the next redo action.
	 * @return The std string.
	 */
	[[nodiscard]] auto redoDescription() const -> std::string {
		if (m_redoStack.empty())
			return {};
		return m_redoStack.back().command->description();
	}

	/**
	 * @brief
	 *  Get the UUID to select after the last undo/redo (0 = no change).
	 * @return The core UUID.
	 */
	[[nodiscard]] auto lastSelectionHint() const -> core::UUID { return m_lastSelectionHint; }

	/**
	 * @brief
	 *  Clear all history; the current state becomes the saved one.
	 */
	void clear() {
		m_undoStack.clear();
		m_redoStack.clear();
		m_groups.clear();
		m_baseGeneration = m_nextGeneration++;
		m_savedGeneration = m_baseGeneration;
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
	void markSaved() { m_savedGeneration = currentGeneration(); }

	/**
	 * @brief
	 *  Mark the current state as not saved (content replaced outside the undo history, e.g. by a recovery).
	 */
	void markUnsaved() { m_savedGeneration = m_nextGeneration++; }

	/**
	 * @brief
	 *  Check if the state has changed since the last save.
	 * @return True when the object is dirty.
	 */
	[[nodiscard]] auto isDirty() const -> bool { return currentGeneration() != m_savedGeneration; }

	/**
	 * @brief
	 *  Open a group: the commands pushed until `endGroup` become one undo step.
	 *
	 * Groups nest. While a group is open, `undo` and `redo` only reach the commands of the innermost group and the
	 * history depth is not trimmed.
	 * @param[in] iDescription Description of the step.
	 */
	void beginGroup(const std::string& iDescription) {
		m_groups.push_back(
				{.description = iDescription, .undoFloor = m_undoStack.size(), .redoFloor = m_redoStack.size()});
	}

	/**
	 * @brief
	 *  Close the innermost group: its commands, when any, are replaced by one `UndoGroupCommand`.
	 */
	void endGroup() {
		if (m_groups.empty())
			return;
		auto group = std::move(m_groups.back());
		m_groups.pop_back();
		if (m_undoStack.size() <= group.undoFloor)
			return;
		std::vector<uniq<UndoCommand<Target>>> commands;
		const auto first = m_undoStack.begin() + static_cast<std::ptrdiff_t>(group.undoFloor);
		for (auto entry = first; entry != m_undoStack.end(); ++entry) commands.push_back(std::move(entry->command));
		m_undoStack.erase(first, m_undoStack.end());
		m_undoStack.push_back(
				{.command = mkUniq<UndoGroupCommand<Target>>(std::move(commands), std::move(group.description)),
				 .generation = m_nextGeneration++});
		while (m_groups.empty() && m_undoStack.size() > m_maxDepth) {
			m_baseGeneration = m_undoStack.front().generation;
			m_undoStack.erase(m_undoStack.begin());
		}
	}

	/**
	 * @brief
	 *  Close the innermost group, undoing and forgetting its commands (a replay that failed).
	 * @param[in,out] ioTarget The target the commands changed.
	 */
	void cancelGroup(Target& ioTarget) {
		if (m_groups.empty())
			return;
		const auto floor = m_groups.back().undoFloor;
		m_groups.pop_back();
		while (m_undoStack.size() > floor) {
			m_undoStack.back().command->undo(ioTarget);
			m_lastSelectionHint = m_undoStack.back().command->m_selectAfterUndo;
			m_undoStack.pop_back();
		}
	}

	/**
	 * @brief
	 *  Check whether a group is open.
	 * @return True between `beginGroup` and the matching `endGroup` or `cancelGroup`.
	 */
	[[nodiscard]] auto isGrouping() const -> bool { return !m_groups.empty(); }

private:
	/**
	 * @brief
	 *  An open group.
	 */
	struct Group {
		/// Description of the step.
		std::string description;
		/// Undo stack size when the group opened.
		size_t undoFloor = 0;
		/// Redo stack size when the group opened (0 once a push cleared the redo stack).
		size_t redoFloor = 0;
	};

	/**
	 * @brief
	 *  Undo stack size below which the innermost open group cannot undo.
	 * @return The floor, 0 without group.
	 */
	[[nodiscard]] auto undoFloor() const -> size_t { return m_groups.empty() ? 0 : m_groups.back().undoFloor; }
	/**
	 * @brief
	 *  A recorded command and the generation of the state it leads to.
	 */
	struct Entry {
		/// The command.
		uniq<UndoCommand<Target>> command;
		/// Generation of the state reached once the command is applied.
		uint64_t generation = 0;
	};

	/**
	 * @brief
	 *  Generation of the current state.
	 * @return The generation of the top undo entry, or the base generation when the stack is empty.
	 */
	[[nodiscard]] auto currentGeneration() const -> uint64_t {
		return m_undoStack.empty() ? m_baseGeneration : m_undoStack.back().generation;
	}

	/// Undo stack (most recent at back).
	std::vector<Entry> m_undoStack;
	/// Redo stack (most recent at back).
	std::vector<Entry> m_redoStack;
	/// Maximum undo history depth.
	size_t m_maxDepth = 100;
	/// Whether merge coalescing is enabled.
	bool m_mergeEnabled = true;
	/// Merge timeout in milliseconds.
	static constexpr auto s_mergeTimeoutMs = std::chrono::milliseconds{1000};
	/// Next generation number to hand out.
	uint64_t m_nextGeneration = 1;
	/// Generation of the state below the oldest undo entry.
	uint64_t m_baseGeneration = 0;
	/// Generation of the state recorded by the last save.
	uint64_t m_savedGeneration = 0;
	/// Entity UUID to select after last undo/redo operation.
	core::UUID m_lastSelectionHint{0};
	/// Open groups, innermost last.
	std::vector<Group> m_groups;
};

/**
 * @brief
 *  Convenience alias for scene-level undo management (default editor case).
 */
using SceneUndoManager = UndoManager<scene::Scene>;

}// namespace owl::nest
