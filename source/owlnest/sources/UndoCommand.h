/**
 * @file UndoCommand.h
 * @author Silmaen
 * @date 13/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <owl.h>

#include <chrono>
#include <string>

namespace owl::nest {
/**
 * @brief
 *  Marker base class for every target type an UndoCommand can act on.
 *
 * Concrete editor targets such as `scene::Scene` and `gui::widgets::NodeCanvas`
 * are used as the `Target` template parameter of `UndoCommand`. `IUndoTarget`
 * itself is intentionally empty — it only exists so that unrelated targets do
 * not need a common artificial base class while still giving the command
 * hierarchy a single conceptual root.
 */
struct IUndoTarget {
	IUndoTarget() = default;

	IUndoTarget(const IUndoTarget&) = default;

	IUndoTarget(IUndoTarget&&) = default;

	auto operator=(const IUndoTarget&) -> IUndoTarget& = default;

	auto operator=(IUndoTarget&&) -> IUndoTarget& = default;

	virtual ~IUndoTarget() = default;
};

/**
 * @brief
 *  Abstract base class for undoable editor commands.
 * @tparam Target Concrete editor state the command mutates (e.g. `scene::Scene`
 *         for scene edits, `gui::widgets::NodeCanvas` for node-graph edits).
 *
 * Each command captures just enough state to reverse itself. The owning
 * `UndoManager<Target>` drives the execute/undo/redo lifecycle and calls
 * `mergeWith` to coalesce rapid property edits within
 * `UndoManager::s_mergeTimeoutMs`.
 */
template<typename Target>
class UndoCommand {
public:
	UndoCommand(const UndoCommand&) = delete;

	UndoCommand(UndoCommand&&) = default;

	auto operator=(const UndoCommand&) -> UndoCommand& = delete;

	auto operator=(UndoCommand&&) -> UndoCommand& = default;

	UndoCommand() = default;

	virtual ~UndoCommand() = default;

	/**
	 * @brief
	 *  Execute the undo action, restoring previous state.
	 */
	virtual void undo(Target& ioTarget) = 0;

	/**
	 * @brief
	 *  Execute the redo action, reapplying the change.
	 */
	virtual void redo(Target& ioTarget) = 0;

	/**
	 * @brief
	 *  Human-readable description for menu/tooltip display.
	 * @return The std string = 0.
	 */
	[[nodiscard]] virtual auto description() const -> std::string = 0;

	/**
	 * @brief
	 *  Try to merge with a subsequent command of the same type.
	 * @param[in] iOther The newer command to absorb.
	 * @return True if the merge succeeded (the other command is then discarded).
	 */
	[[nodiscard]] virtual auto mergeWith([[maybe_unused]] const UndoCommand& iOther) -> bool { return false; }

	/**
	 * @brief
	 *  Unique command type ID for merge checking (0 = no merging).
	 * @return The size_t.
	 */
	[[nodiscard]] virtual auto typeId() const -> size_t { return 0; }

	/// Timestamp of command creation (for merge timeout).
	std::chrono::steady_clock::time_point m_timestamp = std::chrono::steady_clock::now();

	/// UUID of the entity to select after undo (0 = don't change selection).
	core::UUID m_selectAfterUndo{0};
	/// UUID of the entity to select after redo (0 = don't change selection).
	core::UUID m_selectAfterRedo{0};
};

/**
 * @brief
 *  Convenience alias for scene-level undo commands (the vast majority of editor actions).
 */
using SceneUndoCommand = UndoCommand<scene::Scene>;

}// namespace owl::nest
