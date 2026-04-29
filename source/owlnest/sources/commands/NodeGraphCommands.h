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

/// @brief Add a node to the canvas. Stored snapshot holds the node data for redo.
class AddNodeCommand final : public NodeGraphUndoCommand {
public:
	explicit AddNodeCommand(gui::widgets::Node iNode);

	void undo(NodeCanvas& ioTarget) override;
	void redo(NodeCanvas& ioTarget) override;
	[[nodiscard]] auto description() const -> std::string override { return "Add Node"; }

private:
	gui::widgets::Node m_node;///< Full node snapshot; its UUID is reused on every redo.
};

/// @brief Remove a node (and its attached links). Snapshot kept for undo.
class RemoveNodeCommand final : public NodeGraphUndoCommand {
public:
	/// @param[in] iNode Node to remove — copied fully.
	/// @param[in] iAttachedLinks Links currently touching the node's pins (captured before removal).
	RemoveNodeCommand(gui::widgets::Node iNode, std::vector<gui::widgets::Link> iAttachedLinks);

	void undo(NodeCanvas& ioTarget) override;
	void redo(NodeCanvas& ioTarget) override;
	[[nodiscard]] auto description() const -> std::string override { return "Remove Node"; }

private:
	gui::widgets::Node m_node;
	std::vector<gui::widgets::Link> m_attachedLinks;
};

/// @brief Move a node. Mergeable with rapid successive moves to coalesce a drag into one command.
class MoveNodeCommand final : public NodeGraphUndoCommand {
public:
	/// @param[in] iNodeId Node being moved.
	/// @param[in] iBeforePosition Position before the drag.
	/// @param[in] iAfterPosition Position after the drag.
	MoveNodeCommand(core::UUID iNodeId, math::vec2f iBeforePosition, math::vec2f iAfterPosition);

	void undo(NodeCanvas& ioTarget) override;
	void redo(NodeCanvas& ioTarget) override;
	[[nodiscard]] auto description() const -> std::string override { return "Move Node"; }
	[[nodiscard]] auto mergeWith(const NodeGraphUndoCommand& iOther) -> bool override;
	[[nodiscard]] auto typeId() const -> size_t override;

private:
	core::UUID m_nodeId;
	math::vec2f m_before;
	math::vec2f m_after;
};

/// @brief Add a link between two pins.
class AddLinkCommand final : public NodeGraphUndoCommand {
public:
	AddLinkCommand(core::UUID iFromPin, core::UUID iToPin);

	void undo(NodeCanvas& ioTarget) override;
	void redo(NodeCanvas& ioTarget) override;
	[[nodiscard]] auto description() const -> std::string override { return "Add Link"; }

private:
	core::UUID m_fromPin;
	core::UUID m_toPin;
	core::UUID m_linkId{0};///< Populated on first redo so undo can find the exact link back.
};

/// @brief Remove a link between two pins.
class RemoveLinkCommand final : public NodeGraphUndoCommand {
public:
	/// @param[in] iLink Snapshot of the link at the time of removal.
	explicit RemoveLinkCommand(gui::widgets::Link iLink);

	void undo(NodeCanvas& ioTarget) override;
	void redo(NodeCanvas& ioTarget) override;
	[[nodiscard]] auto description() const -> std::string override { return "Remove Link"; }

private:
	gui::widgets::Link m_link;
};

/// @brief Append an output pin to an existing node. Symmetric `RemoveOutputPinCommand` undoes it.
class AddOutputPinCommand final : public NodeGraphUndoCommand {
public:
	/// @param[in] iNodeId Node that owns the pin.
	/// @param[in] iPin Pin snapshot to add (its UUID is reused on every redo).
	AddOutputPinCommand(core::UUID iNodeId, gui::widgets::NodePin iPin);

	void undo(NodeCanvas& ioTarget) override;
	void redo(NodeCanvas& ioTarget) override;
	[[nodiscard]] auto description() const -> std::string override { return "Add Pin"; }

private:
	core::UUID m_nodeId;
	gui::widgets::NodePin m_pin;
};

/// @brief Remove an output pin (and any links still attached to it). Undo restores the pin only —
///        attached links must be tracked by the caller if it cares about restoring them.
class RemoveOutputPinCommand final : public NodeGraphUndoCommand {
public:
	/// @param[in] iNodeId Node that owns the pin.
	/// @param[in] iPin Pin snapshot captured before removal (used to restore on undo).
	RemoveOutputPinCommand(core::UUID iNodeId, gui::widgets::NodePin iPin);

	void undo(NodeCanvas& ioTarget) override;
	void redo(NodeCanvas& ioTarget) override;
	[[nodiscard]] auto description() const -> std::string override { return "Remove Pin"; }

private:
	core::UUID m_nodeId;
	gui::widgets::NodePin m_pin;
};

}// namespace owl::nest::commands
