/**
 * @file NodeGraphCommands.cpp
 * @author Silmaen
 * @date 24/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "NodeGraphCommands.h"

#include <typeinfo>

namespace owl::nest::commands {
// --- AddNodeCommand --------------------------------------------------------

AddNodeCommand::AddNodeCommand(gui::widgets::Node iNode) : m_node{std::move(iNode)} {
	m_selectAfterUndo = core::UUID{0};
	m_selectAfterRedo = m_node.id;
}

void AddNodeCommand::undo(NodeCanvas& ioTarget) { ioTarget.removeNode(m_node.id); }

void AddNodeCommand::redo(NodeCanvas& ioTarget) { ioTarget.addNode(m_node); }

// --- RemoveNodeCommand -----------------------------------------------------
RemoveNodeCommand::RemoveNodeCommand(gui::widgets::Node iNode, std::vector<gui::widgets::Link> iAttachedLinks)
	: m_node{std::move(iNode)}, m_attachedLinks{std::move(iAttachedLinks)} {
	m_selectAfterUndo = m_node.id;
	m_selectAfterRedo = core::UUID{0};
}

void RemoveNodeCommand::undo(NodeCanvas& ioTarget) {
	ioTarget.addNode(m_node);
	for (const auto& link: m_attachedLinks)
		ioTarget.addLink(link.fromPin, link.toPin);
}

void RemoveNodeCommand::redo(NodeCanvas& ioTarget) { ioTarget.removeNode(m_node.id); }

// --- MoveNodeCommand -------------------------------------------------------
MoveNodeCommand::MoveNodeCommand(core::UUID iNodeId, math::vec2f iBeforePosition, math::vec2f iAfterPosition)
	: m_nodeId{iNodeId}, m_before{iBeforePosition}, m_after{iAfterPosition} {}

void MoveNodeCommand::undo(NodeCanvas& ioTarget) {
	if (auto* node = ioTarget.findNode(m_nodeId))
		node->position = m_before;
}

void MoveNodeCommand::redo(NodeCanvas& ioTarget) {
	if (auto* node = ioTarget.findNode(m_nodeId))
		node->position = m_after;
}

auto MoveNodeCommand::mergeWith(const NodeGraphUndoCommand& iOther) -> bool {
	const auto* other = dynamic_cast<const MoveNodeCommand*>(&iOther);
	if (other == nullptr || other->m_nodeId != m_nodeId)
		return false;
	// Keep our "before" (original position at drag start), take the newer "after".
	m_after = other->m_after;
	m_timestamp = other->m_timestamp;
	return true;
}

auto MoveNodeCommand::typeId() const -> size_t {
	// Non-zero constant so UndoManager merge coalescing can kick in for drag events.
	return typeid(MoveNodeCommand).hash_code();
}

// --- AddLinkCommand --------------------------------------------------------
AddLinkCommand::AddLinkCommand(core::UUID iFromPin, core::UUID iToPin) : m_fromPin{iFromPin}, m_toPin{iToPin} {}

void AddLinkCommand::undo(NodeCanvas& ioTarget) {
	if (static_cast<uint64_t>(m_linkId) != 0)
		ioTarget.removeLink(m_linkId);
}

void AddLinkCommand::redo(NodeCanvas& ioTarget) { m_linkId = ioTarget.addLink(m_fromPin, m_toPin); }

// --- RemoveLinkCommand -----------------------------------------------------
RemoveLinkCommand::RemoveLinkCommand(gui::widgets::Link iLink) : m_link{iLink} {}

void RemoveLinkCommand::undo(NodeCanvas& ioTarget) {
	// Re-create the link. `addLink` generates a fresh UUID; we update our snapshot so redo hits
	// the same link later on.
	m_link.id = ioTarget.addLink(m_link.fromPin, m_link.toPin);
}

void RemoveLinkCommand::redo(NodeCanvas& ioTarget) { ioTarget.removeLink(m_link.id); }

// --- AddOutputPinCommand ---------------------------------------------------
AddOutputPinCommand::AddOutputPinCommand(core::UUID iNodeId, gui::widgets::NodePin iPin)
	: m_nodeId{iNodeId}, m_pin{std::move(iPin)} {}

void AddOutputPinCommand::undo(NodeCanvas& ioTarget) { ioTarget.removeOutputPin(m_nodeId, m_pin.id); }

void AddOutputPinCommand::redo(NodeCanvas& ioTarget) { ioTarget.addOutputPin(m_nodeId, m_pin); }

// --- RemoveOutputPinCommand ------------------------------------------------
RemoveOutputPinCommand::RemoveOutputPinCommand(core::UUID iNodeId, gui::widgets::NodePin iPin)
	: m_nodeId{iNodeId}, m_pin{std::move(iPin)} {}

void RemoveOutputPinCommand::undo(NodeCanvas& ioTarget) { ioTarget.addOutputPin(m_nodeId, m_pin); }

void RemoveOutputPinCommand::redo(NodeCanvas& ioTarget) { ioTarget.removeOutputPin(m_nodeId, m_pin.id); }

}// namespace owl::nest::commands
