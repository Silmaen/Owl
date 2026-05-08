/**
 * @file NodeGraphDocument.cpp
 * @author Silmaen
 * @date 24/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "NodeGraphDocument.h"

#include "EditorLayer.h"

#include <gui/widgets/NodeCanvasSerializer.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
#include <imgui.h>
#include <imgui_internal.h>
OWL_DIAG_POP

namespace owl::nest {

NodeGraphDocument::NodeGraphDocument() = default;

NodeGraphDocument::~NodeGraphDocument() = default;

auto NodeGraphDocument::title() const -> std::string {
	if (!m_path.empty())
		return m_path.filename().string();
	return "Untitled";
}

auto NodeGraphDocument::isDirty() const -> bool {
	return gui::widgets::NodeCanvasSerializer::serializeToString(m_canvas) != m_savedSnapshot;
}

void NodeGraphDocument::onAttach(EditorLayer* iEditor) {
	mp_editorLayer = iEditor;
	onCanvasReady();
	m_savedSnapshot = gui::widgets::NodeCanvasSerializer::serializeToString(m_canvas);
}

void NodeGraphDocument::onDetach() { mp_editorLayer = nullptr; }

void NodeGraphDocument::onUpdate([[maybe_unused]] const core::Timestep& iTimeStep) {
	// No per-frame update — the canvas is a pure UI widget.
}

void NodeGraphDocument::onEvent([[maybe_unused]] event::Event& ioEvent) {
	// ImGui consumes its own events; nothing to dispatch at the document level.
}

void NodeGraphDocument::onImGuiRender() {
	const auto winTitle = std::format("{}##nodegraph_{:x}", title(), static_cast<uint64_t>(id()));

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;
	if (isDirty())
		flags |= ImGuiWindowFlags_UnsavedDocument;

	if (const auto dockspaceId = ImGui::GetID("OwlDockSpace");
		const auto* centralNode = ImGui::DockBuilderGetCentralNode(dockspaceId))
		ImGui::SetNextWindowDockID(centralNode->ID, ImGuiCond_FirstUseEver);

	const bool open = ImGui::Begin(winTitle.c_str(), &m_pOpen, flags);
	if (open) {
		const bool windowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
		if (windowFocused && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
			std::ignore = save();
		}
		m_canvas.onRender();
	}
	ImGui::End();
}

auto NodeGraphDocument::save() -> bool {
	if (m_path.empty())
		return false;
	return saveAs(m_path);
}

auto NodeGraphDocument::saveAs(const std::filesystem::path& iPath) -> bool {
	if (!gui::widgets::NodeCanvasSerializer::serializeToFile(m_canvas, iPath)) {
		OWL_CORE_ERROR("NodeGraphDocument: failed to write '{}'.", iPath.string())
		return false;
	}
	m_path = iPath;
	m_savedSnapshot = gui::widgets::NodeCanvasSerializer::serializeToString(m_canvas);
	return true;
}

auto NodeGraphDocument::loadFromFile(const std::filesystem::path& iPath) -> bool {
	if (!gui::widgets::NodeCanvasSerializer::deserializeFromFile(m_canvas, iPath)) {
		OWL_CORE_ERROR("NodeGraphDocument: failed to load '{}'.", iPath.string())
		return false;
	}
	m_path = iPath;
	m_savedSnapshot = gui::widgets::NodeCanvasSerializer::serializeToString(m_canvas);
	return true;
}

}// namespace owl::nest
