/**
 * @file NodeGraphCommands.h
 * @author Silmaen
 * @date 24/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "../UndoCommand.h"

#include <gui/widgets/NodeCanvas.h>

namespace owl::nest::commands {

using NodeCanvas = gui::widgets::NodeCanvas;
using NodeGraphUndoCommand = UndoCommand<NodeCanvas>;

/**
 * @brief
 *  Add a node to the canvas. Stored snapshot holds the node data for redo.
 */
class AddNodeCommand final : public NodeGraphUndoCommand {
public:
	/**
	 * @brief
	 * 	Constructor.
	 * @param[in] iNode Node to add — copied fully (its UUID is reused on every redo).
	 */
	explicit AddNodeCommand(gui::widgets::Node iNode);

	/**
	 * @brief
	 *  Undo.
	 * @param[in,out] ioTarget The node canvas the action is applied to.
	 */
	void undo(NodeCanvas& ioTarget) override;

	/**
	 * @brief
	 *  Redo.
	 * @param[in,out] ioTarget The node canvas the action is applied to.
	 */
	void redo(NodeCanvas& ioTarget) override;

	/**
	 * @brief
	 *  Description.
	 * @return Human-readable description for menus and tooltips.
	 */
	[[nodiscard]] auto description() const -> std::string override { return "Add Node"; }

private:
	gui::widgets::Node m_node;///< Full node snapshot; its UUID is reused on every redo.
};
/**
 * @brief
 *  Remove a node (and its attached links). Snapshot kept for undo.
 */
class RemoveNodeCommand final : public NodeGraphUndoCommand {
public:
	/**
	 * @param[in] iNode Node to remove — copied fully.
	 * @param[in] iAttachedLinks Links currently touching the node's pins (captured before removal).
	 */
	RemoveNodeCommand(gui::widgets::Node iNode, std::vector<gui::widgets::Link> iAttachedLinks);

	/**
	 * @brief
	 *  Undo.
	 * @param[in,out] ioTarget The node canvas the action is applied to.
	 */
	void undo(NodeCanvas& ioTarget) override;

	/**
	 * @brief
	 *  Redo.
	 * @param[in,out] ioTarget The node canvas the action is applied to.
	 */
	void redo(NodeCanvas& ioTarget) override;

	/**
	 * @brief
	 *  Description.
	 * @return Human-readable description for menus and tooltips.
	 */
	[[nodiscard]] auto description() const -> std::string override { return "Remove Node"; }

private:
	/// Snapshot of the node being added/removed.
	gui::widgets::Node m_node;
	/// Links attached to the node when it was removed (restored on undo).
	std::vector<gui::widgets::Link> m_attachedLinks;
};
/**
 * @brief
 *  Move a node. Mergeable with rapid successive moves to coalesce a drag into one command.
 */
class MoveNodeCommand final : public NodeGraphUndoCommand {
public:
	/**
	 * @param[in] iNodeId Node being moved.
	 * @param[in] iBeforePosition Position before the drag.
	 * @param[in] iAfterPosition Position after the drag.
	 */
	MoveNodeCommand(core::UUID iNodeId, math::vec2f iBeforePosition, math::vec2f iAfterPosition);

	/**
	 * @brief
	 *  Undo.
	 * @param[in,out] ioTarget The node canvas the action is applied to.
	 */
	void undo(NodeCanvas& ioTarget) override;

	/**
	 * @brief
	 *  Redo.
	 * @param[in,out] ioTarget The node canvas the action is applied to.
	 */
	void redo(NodeCanvas& ioTarget) override;

	/**
	 * @brief
	 *  Description.
	 * @return Human-readable description for menus and tooltips.
	 */
	[[nodiscard]] auto description() const -> std::string override { return "Move Node"; }

	/**
	 * @brief
	 * 	Coalesce two consecutive same-target moves into one undo step.
	 * @param[in] iOther The previously pushed command, candidate for merging.
	 * @return True when the merge succeeded; false otherwise.
	 */
	[[nodiscard]] auto mergeWith(const NodeGraphUndoCommand& iOther) -> bool override;

	/**
	 * @brief
	 * 	Stable identifier of the command type for merge compatibility checks.
	 * @return The command type id.
	 */
	[[nodiscard]] auto typeId() const -> size_t override;

private:
	/// UUID of the moved node.
	core::UUID m_nodeId;
	/// Position before the move (used by undo).
	math::vec2f m_before;
	/// Position after the move (used by redo).
	math::vec2f m_after;
};
/**
 * @brief
 *  Add a link between two pins.
 */
class AddLinkCommand final : public NodeGraphUndoCommand {
public:
	/**
	 * @brief
	 *  Constructor.
	 * @param[in] iFromPin Output pin (source of the link).
	 * @param[in] iToPin Input pin (destination of the link).
	 */
	AddLinkCommand(core::UUID iFromPin, core::UUID iToPin);

	/**
	 * @brief
	 *  Undo.
	 * @param[in,out] ioTarget The node canvas the action is applied to.
	 */
	void undo(NodeCanvas& ioTarget) override;

	/**
	 * @brief
	 *  Redo.
	 * @param[in,out] ioTarget The node canvas the action is applied to.
	 */
	void redo(NodeCanvas& ioTarget) override;

	/**
	 * @brief
	 *  Description.
	 * @return Human-readable description for menus and tooltips.
	 */
	[[nodiscard]] auto description() const -> std::string override { return "Add Link"; }

private:
	/// Source pin of the link.
	core::UUID m_fromPin;
	/// Destination pin of the link.
	core::UUID m_toPin;
	/// Link UUID populated on first redo so undo can find the exact link back.
	core::UUID m_linkId{0};
};
/**
 * @brief
 *  Remove a link between two pins.
 */
class RemoveLinkCommand final : public NodeGraphUndoCommand {
public:
	/** @param[in] iLink Snapshot of the link at the time of removal. */
	explicit RemoveLinkCommand(gui::widgets::Link iLink);

	/**
	 * @brief
	 *  Undo.
	 * @param[in,out] ioTarget The node canvas the action is applied to.
	 */
	void undo(NodeCanvas& ioTarget) override;

	/**
	 * @brief
	 *  Redo.
	 * @param[in,out] ioTarget The node canvas the action is applied to.
	 */
	void redo(NodeCanvas& ioTarget) override;

	/**
	 * @brief
	 *  Description.
	 * @return Human-readable description for menus and tooltips.
	 */
	[[nodiscard]] auto description() const -> std::string override { return "Remove Link"; }

private:
	/// Snapshot of the link at the time of removal (restored on undo).
	gui::widgets::Link m_link;
};
/**
 * @brief
 *  Append an output pin to an existing node. Symmetric `RemoveOutputPinCommand` undoes it.
 */
class AddOutputPinCommand final : public NodeGraphUndoCommand {
public:
	/**
	 * @param[in] iNodeId Node that owns the pin.
	 * @param[in] iPin Pin snapshot to add (its UUID is reused on every redo).
	 */
	AddOutputPinCommand(core::UUID iNodeId, gui::widgets::NodePin iPin);

	/**
	 * @brief
	 *  Undo.
	 * @param[in,out] ioTarget The node canvas the action is applied to.
	 */
	void undo(NodeCanvas& ioTarget) override;

	/**
	 * @brief
	 *  Redo.
	 * @param[in,out] ioTarget The node canvas the action is applied to.
	 */
	void redo(NodeCanvas& ioTarget) override;

	/**
	 * @brief
	 *  Description.
	 * @return Human-readable description for menus and tooltips.
	 */
	[[nodiscard]] auto description() const -> std::string override { return "Add Pin"; }

private:
	/// Owner node UUID.
	core::UUID m_nodeId;
	/// Snapshot of the pin being added (its UUID is reused on every redo).
	gui::widgets::NodePin m_pin;
};
/**
 * @brief
 *  Remove an output pin (and any links still attached to it). Undo restores the pin only —
 *        attached links must be tracked by the caller if it cares about restoring them.
 */
class RemoveOutputPinCommand final : public NodeGraphUndoCommand {
public:
	/**
	 * @param[in] iNodeId Node that owns the pin.
	 * @param[in] iPin Pin snapshot captured before removal (used to restore on undo).
	 */
	RemoveOutputPinCommand(core::UUID iNodeId, gui::widgets::NodePin iPin);

	/**
	 * @brief
	 *  Undo.
	 * @param[in,out] ioTarget The node canvas the action is applied to.
	 */
	void undo(NodeCanvas& ioTarget) override;

	/**
	 * @brief
	 *  Redo.
	 * @param[in,out] ioTarget The node canvas the action is applied to.
	 */
	void redo(NodeCanvas& ioTarget) override;

	/**
	 * @brief
	 *  Description.
	 * @return Human-readable description for menus and tooltips.
	 */
	[[nodiscard]] auto description() const -> std::string override { return "Remove Pin"; }

private:
	/// Owner node UUID.
	core::UUID m_nodeId;
	/// Pin snapshot captured before removal (restored on undo).
	gui::widgets::NodePin m_pin;
};

}// namespace owl::nest::commands
