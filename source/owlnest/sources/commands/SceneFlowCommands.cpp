/**
 * @file SceneFlowCommands.cpp
 * @author Silmaen
 * @date 27/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "SceneFlowCommands.h"

#include "../EditorLayer.h"
#include "../document/SceneDocument.h"

namespace owl::nest::commands {
// --- SceneFlowCompositeCommand ---------------------------------------------

SceneFlowCompositeCommand::SceneFlowCompositeCommand(uniq<SceneUndoCommand> iSceneCmd,
													 uniq<NodeGraphUndoCommand> iCanvasCmd,
													 std::filesystem::path iSourceScenePath, EditorLayer* iEditor,
													 std::string iDescription)
	: m_sceneCmd{std::move(iSceneCmd)}, m_canvasCmd{std::move(iCanvasCmd)},
	  m_sourceScenePath{std::move(iSourceScenePath)}, mp_editor{iEditor}, m_description{std::move(iDescription)} {}

SceneFlowCompositeCommand::~SceneFlowCompositeCommand() = default;

void SceneFlowCompositeCommand::undo(gui::widgets::NodeCanvas& ioCanvas) {
	if (m_canvasCmd)
		m_canvasCmd->undo(ioCanvas);
	if (m_sceneCmd && mp_editor != nullptr) {
		if (auto* doc = mp_editor->loadOrOpenSceneDocument(m_sourceScenePath); doc != nullptr) {
			if (const auto& scene = doc->getActiveScene())
				m_sceneCmd->undo(*scene);
		}
	}
}

void SceneFlowCompositeCommand::redo(gui::widgets::NodeCanvas& ioCanvas) {
	// Scene first — the new entity must exist before the link references its pin.
	if (m_sceneCmd && mp_editor != nullptr) {
		if (auto* doc = mp_editor->loadOrOpenSceneDocument(m_sourceScenePath); doc != nullptr) {
			if (const auto& scene = doc->getActiveScene())
				m_sceneCmd->redo(*scene);
		}
	}
	if (m_canvasCmd)
		m_canvasCmd->redo(ioCanvas);
}

// --- AddPinAndLinkCommand --------------------------------------------------
AddPinAndLinkCommand::AddPinAndLinkCommand(core::UUID iSourceNodeId, gui::widgets::NodePin iPin,
										   core::UUID iDestInputPinId)
	: m_sourceNodeId{iSourceNodeId}, m_pin{std::move(iPin)}, m_destInputPinId{iDestInputPinId} {}

void AddPinAndLinkCommand::undo(gui::widgets::NodeCanvas& ioTarget) {
	if (static_cast<uint64_t>(m_linkId) != 0)
		ioTarget.removeLink(m_linkId);
	ioTarget.removeOutputPin(m_sourceNodeId, m_pin.id);
}

void AddPinAndLinkCommand::redo(gui::widgets::NodeCanvas& ioTarget) {
	ioTarget.addOutputPin(m_sourceNodeId, m_pin);
	m_linkId = ioTarget.addLink(m_pin.id, m_destInputPinId);
}

// --- RemovePinAndLinkCommand -----------------------------------------------
RemovePinAndLinkCommand::RemovePinAndLinkCommand(core::UUID iSourceNodeId, gui::widgets::NodePin iPin,
												 gui::widgets::Link iLink)
	: m_sourceNodeId{iSourceNodeId}, m_pin{std::move(iPin)}, m_link{iLink} {}

void RemovePinAndLinkCommand::undo(gui::widgets::NodeCanvas& ioTarget) {
	ioTarget.addOutputPin(m_sourceNodeId, m_pin);
	m_link.id = ioTarget.addLink(m_link.fromPin, m_link.toPin);
}

void RemovePinAndLinkCommand::redo(gui::widgets::NodeCanvas& ioTarget) {
	if (static_cast<uint64_t>(m_link.id) != 0)
		ioTarget.removeLink(m_link.id);
	ioTarget.removeOutputPin(m_sourceNodeId, m_pin.id);
}

}// namespace owl::nest::commands
