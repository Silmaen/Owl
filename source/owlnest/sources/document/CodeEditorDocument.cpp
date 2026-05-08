/**
 * @file CodeEditorDocument.cpp
 * @author Silmaen
 * @date 19/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "CodeEditorDocument.h"

#include "EditorLayer.h"
#include "EditorSettings.h"
#include "external/imgui_text_edit.h"

#include <core/Application.h>
#include <gui/UiLayer.h>

#include <fstream>
#include <sstream>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
#include <imgui.h>
#include <imgui_internal.h>
OWL_DIAG_POP

namespace owl::nest {

CodeEditorDocument::CodeEditorDocument() = default;

CodeEditorDocument::~CodeEditorDocument() = default;

auto CodeEditorDocument::title() const -> std::string {
	if (!m_path.empty())
		return m_path.filename().string();
	return "Untitled";
}

auto CodeEditorDocument::isDirty() const -> bool {
	if (!mp_editor)
		return false;
	return mp_editor->GetText() != m_savedText;
}

void CodeEditorDocument::onAttach(EditorLayer* iEditor) {
	mp_editorLayer = iEditor;
	mp_editor = mkUniq<TextEditor>();
	mp_editor->SetShowWhitespacesEnabled(false);
	mp_editor->SetShowLineNumbersEnabled(true);
	codeEditor::applyLanguage(*mp_editor, m_language);
}

void CodeEditorDocument::onDetach() {
	mp_editor.reset();
	mp_editorLayer = nullptr;
}

void CodeEditorDocument::onUpdate(const core::Timestep& iTimeStep) {
	if (!mp_editor || !m_previewVisible)
		return;
	if (m_language == codeEditor::Language::Markdown) {
		m_mdPreview.update(iTimeStep, mp_editor->GetText());
	} else if (m_language == codeEditor::Language::Xml) {
		// Use the latest known preview area; the rasterized side is bounded by the
		// shorter dimension of the panel (set during onImGuiRender via the size we
		// pass below). We feed a hint based on a typical pane size — render() reads
		// the actual ImGui content region.
		m_svgPreview.update(iTimeStep, mp_editor->GetText(), math::vec2ui{512, 512});
	}
}

void CodeEditorDocument::onEvent([[maybe_unused]] event::Event& ioEvent) {
	// The editor consumes its own key events via ImGui.
}

void CodeEditorDocument::onImGuiRender() {
	if (!mp_editor)
		return;

	const auto winTitle = std::format("{}##code_{:x}", title(), static_cast<uint64_t>(id()));

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;
	if (isDirty())
		flags |= ImGuiWindowFlags_UnsavedDocument;

	if (const auto dockspaceId = ImGui::GetID("OwlDockSpace");
		const auto* centralNode = ImGui::DockBuilderGetCentralNode(dockspaceId))
		ImGui::SetNextWindowDockID(centralNode->ID, ImGuiCond_FirstUseEver);

	// Push the dedicated code-editor font (Roboto at `UiLayer::codeFontSize()` = 13 px) so the
	// TextEditor widget gets clean glyph metrics.  Push BEFORE `Begin` so the tab title uses the
	// same font and the content-region size is computed for the correct glyph size.
	ImFont* codeFont = nullptr;
	if (const auto ui = core::Application::get().getImGuiLayer(); ui != nullptr)
		codeFont = ui->getCodeFont();
	if (codeFont != nullptr)
		ImGui::PushFont(codeFont);

	const bool open = ImGui::Begin(winTitle.c_str(), &m_pOpen, flags);
	if (open) {
		// The code font is rasterised at the user-configured size during `UiLayer::onAttach`, so
		// no per-window scaling is needed.  A size change in the settings takes effect on restart.
		// Intercept Ctrl+S as "Save" while the editor window has focus (the TextEditor consumes
		// other shortcuts internally, including Ctrl+Z/Ctrl+Y for text undo/redo).
		const bool windowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
		if (windowFocused && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
			if (ImGui::GetIO().KeyShift) {
				// Save As not implemented here — let the EditorLayer handle via action registry.
			} else {
				std::ignore = save();
			}
		}
		const auto avail = ImGui::GetContentRegionAvail();
		const float bodyH = std::max(0.f, avail.y - ImGui::GetFrameHeightWithSpacing());
		const bool showPreview = m_previewVisible && canShowPreview();
		const float editorW = showPreview ? std::max(60.f, avail.x * m_splitRatio) : avail.x;
		const float previewW = showPreview ? std::max(60.f, avail.x - editorW - 6.f) : 0.f;

		mp_editor->Render("##text_editor", ImVec2{editorW, bodyH});
		// Accept Content Browser drag-drops on the editor area (same dispatch as the Viewport).
		if (mp_editorLayer != nullptr && ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
				const auto* path = static_cast<const char*>(payload->Data);
				mp_editorLayer->handleContentBrowserDrop(std::filesystem::path{path});
			}
			ImGui::EndDragDropTarget();
		}
		if (showPreview) {
			ImGui::SameLine(0.f, 0.f);
			// Draggable splitter: a 6px-wide invisible button between editor and preview.
			ImGui::Button("##split", ImVec2{6.f, bodyH});
			if (ImGui::IsItemActive()) {
				const float dx = ImGui::GetIO().MouseDelta.x;
				if (avail.x > 0.f)
					m_splitRatio = std::clamp(m_splitRatio + dx / avail.x, 0.1f, 0.9f);
			}
			if (ImGui::IsItemHovered() || ImGui::IsItemActive())
				ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
			ImGui::SameLine(0.f, 0.f);
			if (m_language == codeEditor::Language::Markdown)
				m_mdPreview.render(ImVec2{previewW, bodyH});
			else if (m_language == codeEditor::Language::Xml)
				m_svgPreview.render(ImVec2{previewW, bodyH});
		}
		// Footer status line.
		int line = 0;
		int column = 0;
		mp_editor->GetMainCursor(line, column);
		ImGui::TextDisabled("%s | Line %d, Col %d | %s", codeEditor::languageName(m_language), line + 1,
							column + 1, mp_editor->IsOverwriteEnabled() ? "OVR" : "INS");
	}
	ImGui::End();
	if (codeFont != nullptr)
		ImGui::PopFont();
}

auto CodeEditorDocument::save() -> bool {
	if (m_path.empty() || !mp_editor)
		return false;
	return saveAs(m_path);
}

auto CodeEditorDocument::saveAs(const std::filesystem::path& iPath) -> bool {
	if (!mp_editor)
		return false;
	std::ofstream fileOut(iPath, std::ios::binary);
	if (!fileOut.is_open()) {
		OWL_CORE_ERROR("CodeEditor: failed to write '{}'", iPath.string())
		return false;
	}
	const auto txt = mp_editor->GetText();
	fileOut << txt;
	fileOut.close();
	m_path = iPath;
	m_savedText = txt;
	m_language = codeEditor::detectLanguage(m_path);
	codeEditor::applyLanguage(*mp_editor, m_language);
	if (canShowPreview())
		m_previewVisible = true;
	return true;
}

auto CodeEditorDocument::loadFromFile(const std::filesystem::path& iPath) -> bool {
	if (!mp_editor)
		return false;
	std::ifstream fileIn(iPath, std::ios::binary);
	if (!fileIn.is_open()) {
		OWL_CORE_ERROR("CodeEditor: failed to open '{}'", iPath.string())
		return false;
	}
	std::stringstream buffer;
	buffer << fileIn.rdbuf();
	const auto content = buffer.str();
	mp_editor->SetText(content);
	m_savedText = content;
	m_path = iPath;
	m_language = codeEditor::detectLanguage(m_path);
	codeEditor::applyLanguage(*mp_editor, m_language);
	if (canShowPreview())
		m_previewVisible = true;
	return true;
}

}// namespace owl::nest
