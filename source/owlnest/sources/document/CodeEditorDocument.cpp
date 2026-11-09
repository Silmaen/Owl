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
#include "codeEditor/TextLayout.h"
#include "external/imgui_text_edit.h"

#include <app/Application.h>
#include <data/assets/AssetRegistry.h>
#include <gui/IconFont.h>
#include <gui/UiLayer.h>
#include <script/ScriptDebugger.h>
#include <script/ScriptTypeChecker.h>

#include <algorithm>
#include <cstdint>
#include <format>
#include <fstream>
#include <sstream>
#include <tuple>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
#include <imgui.h>
#include <imgui_internal.h>
OWL_DIAG_POP

namespace owl::nest {

namespace {
// One checker for every script document: building it loads the declarations of the engine once.
auto typeChecker() -> script::ScriptTypeChecker& {
	static script::ScriptTypeChecker s_checker;
	return s_checker;
}

constexpr ImU32 g_errorColor = IM_COL32(229, 72, 77, 255);
constexpr ImU32 g_warningColor = IM_COL32(230, 170, 50, 255);
constexpr int g_checkDelayMs = 400;

auto severityColor(const script::ScriptDiagnostic& iDiagnostic) -> ImU32 {
	return iDiagnostic.severity == script::ScriptDiagnostic::Severity::Error ? g_errorColor : g_warningColor;
}
}// namespace

CodeEditorDocument::CodeEditorDocument() = default;

CodeEditorDocument::~CodeEditorDocument() = default;

auto CodeEditorDocument::title() const -> std::string {
	if (!m_path.empty())
		return m_path.stem().string();
	return "Untitled";
}

auto CodeEditorDocument::isDirty() const -> bool {
	if (!mp_editor)
		return false;
	return mp_editor->GetText() != m_savedText;
}

auto CodeEditorDocument::recoverySnapshot() const -> std::optional<std::string> {
	if (!mp_editor)
		return std::nullopt;
	return mp_editor->GetText();
}

auto CodeEditorDocument::restoreRecoverySnapshot(const std::string& iSnapshot) -> bool {
	if (!mp_editor)
		return false;
	mp_editor->SetText(iSnapshot);
	return true;
}

void CodeEditorDocument::onAttach(EditorLayer* iEditor) {
	mp_editorLayer = iEditor;
	mp_editor = mkUniq<TextEditor>();
	mp_editor->SetShowWhitespacesEnabled(false);
	mp_editor->SetShowLineNumbersEnabled(true);
	codeEditor::applyLanguage(*mp_editor, m_language);
	updateBreakpointGutter();
	updateTypeChecking();
}

void CodeEditorDocument::onDetach() {
	mp_editor.reset();
	mp_editorLayer = nullptr;
}

void CodeEditorDocument::onUpdate(const core::Timestep& iTimeStep) {
	if (mp_editor && m_checkPending)
		checkTypes();
	if (!mp_editor || !m_previewVisible)
		return;
	if (m_language == codeEditor::Language::Markdown) {
		m_mdPreview.update(iTimeStep, mp_editor->GetText());
	} else if (m_language == codeEditor::Language::Xml) {
		m_svgPreview.update(iTimeStep, mp_editor->GetText(), math::vec2ui{512, 512});
	}
}

void CodeEditorDocument::onEvent([[maybe_unused]] event::Event& ioEvent) {
	// The editor consumes its own key events via ImGui.
}

void CodeEditorDocument::onImGuiRender() {
	if (!mp_editor)
		return;

	const auto winTitle = documentWindowTitle(DocumentType::Code, title(), static_cast<uint64_t>(id()));

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;
	if (isDirty())
		flags |= ImGuiWindowFlags_UnsavedDocument;

	if (const auto dockspaceId = ImGui::GetID("OwlDockSpace");
		const auto* centralNode = ImGui::DockBuilderGetCentralNode(dockspaceId))
		ImGui::SetNextWindowDockID(centralNode->ID, ImGuiCond_FirstUseEver);

	ImFont* codeFont = nullptr;
	if (const auto ui = app::Application::get().getImGuiLayer(); ui != nullptr)
		codeFont = ui->getCodeFont();
	if (codeFont != nullptr)
		ImGui::PushFont(codeFont);

	const bool wantFocus = consumeFocusRequest();
	if (wantFocus)
		ImGui::SetNextWindowFocus();
	const bool open = ImGui::Begin(winTitle.c_str(), &m_pOpen, flags);
	if (mp_editorLayer != nullptr)
		mp_editorLayer->renderDocumentTabMenu(*this);
	if (wantFocus)
		ImGui::SetWindowFocus();
	if (open) {
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

		if (m_language == codeEditor::Language::Lua) {
			m_breakpoints = script::ScriptDebugger::getBreakpoints(m_path.string());
			if (windowFocused && ImGui::IsKeyPressed(ImGuiKey_F9, false))
				toggleBreakpoint(static_cast<int>(mp_editor->GetMainCursorPosition().line) + 1);
		}
		const ImVec2 editorMin = ImGui::GetCursorScreenPos();
		mp_editor->Render("##text_editor", ImVec2{editorW, bodyH});
		if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) &&
			ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) &&
			ImGui::IsMouseHoveringRect(editorMin, ImVec2{editorMin.x + editorW, editorMin.y + bodyH}))
			openReferenceUnderCursor();
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
		renderStatusLine();
	}
	ImGui::End();
	if (codeFont != nullptr)
		ImGui::PopFont();
}

void CodeEditorDocument::renderStatusLine() {
	const auto [line, column] = cursorLocation();
	ImGui::TextDisabled("%s | Line %zu, Col %zu | %s", codeEditor::languageName(m_language), line + 1, column + 1,
						mp_editor->IsOverwriteEnabled() ? "OVR" : "INS");
	if (m_language != codeEditor::Language::Lua)
		return;
	const auto errors = std::ranges::count(m_diagnostics, script::ScriptDiagnostic::Severity::Error,
										   &script::ScriptDiagnostic::severity);
	const auto warnings = static_cast<std::ptrdiff_t>(m_diagnostics.size()) - errors;
	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_Text, errors > 0 ? g_errorColor : ImGui::GetColorU32(ImGuiCol_TextDisabled));
	const auto label =
			std::format("{} {} error(s), {} warning(s)##problems", gui::icon::triangleAlert, errors, warnings);
	if (ImGui::SmallButton(label.c_str()))
		m_problemsOpen = !m_problemsOpen;
	ImGui::PopStyleColor();
	if (!m_problemsOpen || m_diagnostics.empty())
		return;
	if (!ImGui::BeginChild("##problems", ImVec2{0.f, ImGui::GetTextLineHeightWithSpacing() * 6.f},
						   ImGuiChildFlags_Borders)) {
		ImGui::EndChild();
		return;
	}
	for (size_t index = 0; index < m_diagnostics.size(); ++index) {
		const auto& diagnostic = m_diagnostics[index];
		ImGui::PushID(static_cast<int>(index));
		ImGui::PushStyleColor(ImGuiCol_Text, severityColor(diagnostic));
		const auto entry = std::format("{}:{}  {}", diagnostic.line, diagnostic.column, diagnostic.message);
		if (ImGui::Selectable(entry.c_str()))
			revealLine(diagnostic.line);
		ImGui::PopStyleColor();
		ImGui::PopID();
	}
	ImGui::EndChild();
}

void CodeEditorDocument::updateTypeChecking() {
	if (m_language != codeEditor::Language::Lua) {
		mp_editor->SetChangeCallback(nullptr);
		mp_editor->ClearMarkers();
		m_diagnostics.clear();
		return;
	}
	mp_editor->SetChangeCallback([this]() -> void { m_checkPending = true; }, g_checkDelayMs);
	m_checkPending = true;
}

void CodeEditorDocument::checkTypes() {
	m_checkPending = false;
	if (!mp_editor)
		return;
	mp_editor->ClearMarkers();
	m_diagnostics.clear();
	// Editor scripts run in the console against the `editor` module, not in a game: the game declarations do not apply.
	if (m_language != codeEditor::Language::Lua || m_path.parent_path().filename() == "editor")
		return;
	const auto name = m_path.empty() ? std::string{"script.lua"} : m_path.filename().string();
	m_diagnostics = typeChecker().check(mp_editor->GetText(), name);
	for (const auto& diagnostic: m_diagnostics) {
		const auto line = static_cast<size_t>(diagnostic.line > 0 ? diagnostic.line - 1 : 0);
		mp_editor->AddMarker(line, severityColor(diagnostic), 0, diagnostic.message, diagnostic.message);
	}
}

void CodeEditorDocument::openReferenceUnderCursor() const {
	if (!mp_editor || mp_editorLayer == nullptr)
		return;
	const auto pos = mp_editor->GetMainCursorPosition();
	const auto guid = codeEditor::findAssetGuidAt(mp_editor->GetLineText(pos.line), pos.index);
	if (!guid)
		return;
	const auto entry = app::Application::get().getAssetRegistry().find(*guid);
	if (!entry || entry->root.empty()) {
		OWL_CORE_WARN("CodeEditor: No asset of GUID {} in the asset directories.", data::assets::formatGuid(*guid))
		return;
	}
	mp_editorLayer->requestDeferredOpen(entry->root / entry->path);
}

void CodeEditorDocument::toggleBreakpoint(const int iLine) const {
	const auto path = m_path.string();
	const auto lines = script::ScriptDebugger::getBreakpoints(path);
	script::ScriptDebugger::setBreakpoint(path, iLine, !std::ranges::contains(lines, iLine));
}

void CodeEditorDocument::updateBreakpointGutter() {
	if (m_language != codeEditor::Language::Lua) {
		mp_editor->ClearLineDecorator();
		return;
	}
	mp_editor->SetLineDecorator(2, [this](const TextEditor::Decorator& iDecorator) -> void {
		drawBreakpoint(static_cast<int>(iDecorator.line) + 1, iDecorator.width, iDecorator.height);
	});
}

void CodeEditorDocument::drawBreakpoint(const int iLine, const float iWidth, const float iHeight) const {
	const auto origin = ImGui::GetCursorScreenPos();
	ImGui::InvisibleButton("##breakpoint", ImVec2{iWidth, iHeight});
	const bool hovered = ImGui::IsItemHovered();
	if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
		toggleBreakpoint(iLine);
	const bool active = std::ranges::contains(m_breakpoints, iLine);
	if (!active && !hovered)
		return;
	const float radius = std::min(iWidth, iHeight) * 0.32f;
	const ImVec2 center{origin.x + iWidth * 0.5f, origin.y + iHeight * 0.5f};
	const ImU32 color = active ? IM_COL32(229, 72, 77, 255) : IM_COL32(229, 72, 77, 90);
	ImGui::GetWindowDrawList()->AddCircleFilled(center, radius, color);
	if (hovered)
		ImGui::SetTooltip(active ? "Remove the breakpoint (F9)"
								 : "Set a breakpoint (F9): the game pauses at this line");
}

void CodeEditorDocument::revealLine(const size_t iLine) const {
	if (mp_editor == nullptr || iLine == 0)
		return;
	mp_editor->SelectLine(iLine - 1);
	mp_editor->ScrollToLine(iLine - 1, TextEditor::Scroll::alignMiddle);
}

auto CodeEditorDocument::cursorLocation() const -> CursorLocation {
	if (!mp_editor)
		return {};
	const auto pos = mp_editor->GetMainCursorPosition();
	return {.line = pos.line,
			.column = codeEditor::visualColumn(mp_editor->GetLineText(pos.line), pos.index, mp_editor->GetTabSize())};
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
		OWL_CORE_ERROR("CodeEditor: Failed to write '{}'.", iPath.string())
		return false;
	}
	const auto txt = mp_editor->GetText();
	fileOut << txt;
	fileOut.close();
	m_path = iPath;
	m_savedText = txt;
	m_language = codeEditor::detectLanguage(m_path);
	codeEditor::applyLanguage(*mp_editor, m_language);
	updateBreakpointGutter();
	updateTypeChecking();
	if (canShowPreview())
		m_previewVisible = true;
	return true;
}

auto CodeEditorDocument::loadFromFile(const std::filesystem::path& iPath) -> bool {
	if (!mp_editor)
		return false;
	std::ifstream fileIn(iPath, std::ios::binary);
	if (!fileIn.is_open()) {
		OWL_CORE_ERROR("CodeEditor: Failed to open '{}'.", iPath.string())
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
	updateBreakpointGutter();
	updateTypeChecking();
	if (canShowPreview())
		m_previewVisible = true;
	return true;
}

}// namespace owl::nest
