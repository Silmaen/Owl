/**
 * @file PluginDocument.cpp
 * @author Silmaen
 * @date 24/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "PluginDocument.h"

#include "../EditorLayer.h"

#include <gui/IconFont.h>
#include <gui/widgets/EmptyState.h>
#include <imgui_internal.h>
#include <platform/AtomicFile.h>
#include <platform/FileUtils.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <system_error>
#include <tuple>
#include <utility>

namespace owl::nest {

namespace {

// One text edit: the whole text before and after.
class SetTextCommand final : public UndoCommand<std::string> {
public:
	SetTextCommand(std::string iBefore, std::string iAfter, std::string iDescription)
		: m_before{std::move(iBefore)}, m_after{std::move(iAfter)}, m_description{std::move(iDescription)} {}

	void undo(std::string& ioText) override { ioText = m_before; }

	void redo(std::string& ioText) override { ioText = m_after; }

	[[nodiscard]] auto description() const -> std::string override { return m_description; }

private:
	std::string m_before;
	std::string m_after;
	std::string m_description;
};

}// namespace

PluginDocument::PluginDocument(std::string iType) : m_type{std::move(iType)} { m_undo.markSaved(); }

PluginDocument::~PluginDocument() = default;

auto PluginDocument::title() const -> std::string {
	return m_path.empty() ? std::string{"Untitled"} : m_path.filename().string();
}

void PluginDocument::onAttach(EditorLayer* iEditor) { mp_editorLayer = iEditor; }

void PluginDocument::onDetach() { mp_editorLayer = nullptr; }

void PluginDocument::onUpdate([[maybe_unused]] const core::Timestep& iTimeStep) {}

void PluginDocument::onEvent([[maybe_unused]] event::Event& ioEvent) {}

auto PluginDocument::loadFromFile(const std::filesystem::path& iPath) -> bool {
	std::error_code error;
	if (!std::filesystem::is_regular_file(iPath, error)) {
		OWL_CORE_WARN("PluginDocument: '{}' is no file.", iPath.string())
		return false;
	}
	m_path = iPath;
	m_text = platform::fileToString(iPath);
	m_undo.clear();
	m_undo.markSaved();
	return true;
}

auto PluginDocument::save() -> bool {
	if (m_path.empty())
		return false;
	return saveAs(m_path);
}

auto PluginDocument::saveAs(const std::filesystem::path& iPath) -> bool {
	if (const auto written = platform::writeFileAtomic(iPath, m_text); !written) {
		OWL_CORE_ERROR("PluginDocument: Failed to write '{}': {}.", iPath.string(), platform::describe(written.error()))
		return false;
	}
	m_path = iPath;
	m_undo.markSaved();
	return true;
}

auto PluginDocument::restoreRecoverySnapshot(const std::string& iSnapshot) -> bool {
	m_text = iSnapshot;
	m_undo.clear();
	m_undo.markUnsaved();
	return true;
}

void PluginDocument::performUndo() {
	if (m_undo.canUndo())
		m_undo.undo(m_text);
}

void PluginDocument::performRedo() {
	if (m_undo.canRedo())
		m_undo.redo(m_text);
}

auto PluginDocument::setText(const std::string& iText, const std::string& iDescription) -> bool {
	if (iText == m_text)
		return false;
	auto before = std::exchange(m_text, iText);
	m_undo.push(mkUniq<SetTextCommand>(std::move(before), iText, iDescription));
	return true;
}

void PluginDocument::onImGuiRender() {
	const auto winTitle = documentWindowTitle(DocumentType::Plugin, title(), static_cast<uint64_t>(id()));
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;
	if (isDirty())
		flags |= ImGuiWindowFlags_UnsavedDocument;
	if (const auto dockspaceId = ImGui::GetID("OwlDockSpace");
		const auto* centralNode = ImGui::DockBuilderGetCentralNode(dockspaceId))
		ImGui::SetNextWindowDockID(centralNode->ID, ImGuiCond_FirstUseEver);
	const bool wantFocus = consumeFocusRequest();
	if (wantFocus)
		ImGui::SetNextWindowFocus();
	const bool open = ImGui::Begin(winTitle.c_str(), &m_open, flags);
	if (mp_editorLayer != nullptr)
		mp_editorLayer->renderDocumentTabMenu(*this);
	if (wantFocus) {
		ImGui::SetWindowFocus();
		if (mp_editorLayer != nullptr)
			mp_editorLayer->getDocumentManager().setActive(this);
	}
	if (open) {
		const bool windowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
		if (windowFocused && !m_wasFocused && mp_editorLayer != nullptr)
			mp_editorLayer->getDocumentManager().setActive(this);
		m_wasFocused = windowFocused;
		if (windowFocused && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false))
			std::ignore = save();
		// The handler receives a copy: an edit replaces the text while it is still being drawn.
		const std::string text = m_text;
		m_ui.begin("document", false);
		const bool drawn = mp_editorLayer != nullptr &&
						   mp_editorLayer->getPlugins().findContribution(session::PluginContributionKind::DocumentType,
																		 m_type) != nullptr;
		if (drawn)
			std::ignore = mp_editorLayer->getPlugins().drawDocument(m_type, m_path, text, m_ui);
		m_ui.end();
		if (!drawn)
			gui::widgets::emptyState(gui::icon::puzzle, "Plugin not loaded",
									 "The plugin of this document type is disabled or failed to load: enable it in "
									 "the Extensions panel.");
	}
	ImGui::End();
}

}// namespace owl::nest
