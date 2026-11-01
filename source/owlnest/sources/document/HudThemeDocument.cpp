/**
 * @file HudThemeDocument.cpp
 * @author Silmaen
 * @date 01/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "HudThemeDocument.h"

#include "EditorLayer.h"

#include <app/Application.h>
#include <app/EngineContext.h>
#include <gui/IconFont.h>
#include <gui/component/render.h>
#include <gui/widgets/EmptyState.h>
#include <gui/widgets/IconButton.h>
#include <gui/widgets/SectionHeader.h>
#include <scene/HudThemeLibrary.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
OWL_DIAG_POP

#include <algorithm>
#include <cfloat>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <utility>

namespace owl::nest {

namespace {

using scene::HudStyle;
using scene::HudTheme;

class ModifyHudThemeCommand final : public UndoCommand<HudTheme> {
public:
	ModifyHudThemeCommand(HudTheme iBefore, HudTheme iAfter, std::string iDescription)
		: m_before{std::move(iBefore)}, m_after{std::move(iAfter)}, m_description{std::move(iDescription)} {}

	void undo(HudTheme& ioTarget) override { ioTarget = m_before; }

	void redo(HudTheme& ioTarget) override { ioTarget = m_after; }

	[[nodiscard]] auto description() const -> std::string override { return m_description; }

	[[nodiscard]] auto typeId() const -> size_t override { return 0; }

private:
	HudTheme m_before;
	HudTheme m_after;
	std::string m_description;
};

}// namespace

HudThemeDocument::HudThemeDocument() { m_undo.markSaved(); }

HudThemeDocument::~HudThemeDocument() = default;

auto HudThemeDocument::title() const -> std::string {
	return m_path.empty() ? std::string{"HUD Theme"} : m_path.stem().string();
}

void HudThemeDocument::onAttach(EditorLayer* iEditor) { mp_editorLayer = iEditor; }

void HudThemeDocument::onDetach() {
	// The engine reads the saved file again: the unsaved edits shown live leave with the tab.
	if (app::Application::instanced() && !m_path.empty())
		app::Application::get().getEngineContext().getHudThemeLibrary().invalidate(getAssetPath());
	mp_editorLayer = nullptr;
}

void HudThemeDocument::onUpdate([[maybe_unused]] const core::Timestep& iTimeStep) { commitPreview(); }

void HudThemeDocument::onEvent([[maybe_unused]] event::Event& ioEvent) {}

auto HudThemeDocument::loadFromFile(const std::filesystem::path& iPath) -> bool {
	auto loaded = HudTheme::load(iPath);
	if (!loaded) {
		OWL_CORE_WARN("HudThemeDocument: {} is not a HUD theme.", iPath.string())
		return false;
	}
	m_theme = std::move(*loaded);
	m_path = iPath;
	m_undo.clear();
	m_undo.markSaved();
	m_preview.reset();
	setSelection(m_theme.styles.empty() ? std::nullopt : std::optional<size_t>{0});
	return true;
}

auto HudThemeDocument::save() -> bool {
	if (m_path.empty())
		return false;
	return saveAs(m_path);
}

auto HudThemeDocument::saveAs(const std::filesystem::path& iPath) -> bool {
	commitPreview(true);
	if (!m_theme.saveToFile(iPath)) {
		OWL_CORE_ERROR("HudThemeDocument: Failed to write '{}'.", iPath.string())
		return false;
	}
	m_path = iPath;
	m_undo.markSaved();
	publish();
	return true;
}

auto HudThemeDocument::recoverySnapshot() const -> std::optional<std::string> { return m_theme.serializeToString(); }

auto HudThemeDocument::restoreRecoverySnapshot(const std::string& iSnapshot) -> bool {
	auto restored = HudTheme::deserializeFromString(iSnapshot, "<recovery>");
	if (!restored) {
		OWL_WARN("Recovery: Cannot restore the autosave of the HUD theme.")
		return false;
	}
	m_theme = std::move(*restored);
	m_undo.clear();
	m_undo.markUnsaved();
	setSelection(m_selection);
	publish();
	return true;
}

void HudThemeDocument::performUndo() {
	m_preview.reset();
	if (!m_undo.canUndo())
		return;
	m_undo.undo(m_theme);
	setSelection(m_selection);
	publish();
}

void HudThemeDocument::performRedo() {
	m_preview.reset();
	if (!m_undo.canRedo())
		return;
	m_undo.redo(m_theme);
	setSelection(m_selection);
	publish();
}

auto HudThemeDocument::setTheme(const HudTheme& iTheme, const std::string& iDescription) -> bool {
	if (iTheme == m_theme)
		return false;
	auto before = std::exchange(m_theme, iTheme);
	m_undo.push(mkUniq<ModifyHudThemeCommand>(std::move(before), iTheme, iDescription));
	setSelection(m_selection);
	publish();
	return true;
}

void HudThemeDocument::setSelection(const std::optional<size_t> iIndex) {
	m_preview.reset();
	if (m_theme.styles.empty()) {
		m_selection.reset();
		return;
	}
	m_selection = iIndex ? std::optional{std::min(*iIndex, m_theme.styles.size() - 1)} : std::nullopt;
}

auto HudThemeDocument::getAssetPath() const -> std::string {
	if (mp_editorLayer != nullptr && mp_editorLayer->getProject().isLoaded()) {
		std::error_code error;
		const auto relative = std::filesystem::relative(m_path, mp_editorLayer->getProject().projectDirectory, error);
		if (!error && !relative.empty() && !relative.native().starts_with(".."))
			return relative.generic_string();
	}
	return m_path.generic_string();
}

void HudThemeDocument::publish() const {
	if (!app::Application::instanced() || m_path.empty())
		return;
	HudTheme shown = m_theme;
	if (m_preview && m_selection && *m_selection < shown.styles.size())
		shown.styles[*m_selection] = *m_preview;
	app::Application::get().getEngineContext().getHudThemeLibrary().set(getAssetPath(), shown);
}

auto HudThemeDocument::runCommand(const std::string_view iName, commands::CommandArgs iArgs) -> bool {
	if (mp_editorLayer == nullptr) {
		OWL_WARN("HudThemeDocument: {} ignored, no editor to run it.", iName)
		return false;
	}
	iArgs.set("path", m_path.generic_string());
	return mp_editorLayer->execute(iName, iArgs, commands::CommandSource::Gesture).has_value();
}

void HudThemeDocument::commitPreview(const bool iForce) {
	if (!m_preview || !m_selection || *m_selection >= m_theme.styles.size() || (!iForce && ImGui::IsAnyItemActive()))
		return;
	HudStyle edited = std::move(*m_preview);
	m_preview.reset();
	const std::string current = m_theme.styles[*m_selection].name;
	std::string name = current;
	if (edited.name != current && !edited.name.empty() && m_theme.findStyle(edited.name) == nullptr &&
		runCommand("theme.rename_style", {{"name", current}, {"new_name", edited.name}}))
		name = edited.name;
	edited.name = name;
	if (!(edited == m_theme.styles[*m_selection]))
		std::ignore = runCommand("theme.set_style", {{"name", name}, {"style", HudTheme::serializeStyle(edited)}});
	publish();
}

void HudThemeDocument::onImGuiRender() {
	const auto winTitle = documentWindowTitle(DocumentType::HudTheme, title(), static_cast<uint64_t>(id()));
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;
	if (isDirty())
		flags |= ImGuiWindowFlags_UnsavedDocument;
	if (const auto dockspaceId = ImGui::GetID("OwlDockSpace");
		const auto* centralNode = ImGui::DockBuilderGetCentralNode(dockspaceId))
		ImGui::SetNextWindowDockID(centralNode->ID, ImGuiCond_FirstUseEver);
	const bool wantFocus = consumeFocusRequest();
	if (wantFocus)
		ImGui::SetNextWindowFocus();
	const bool open = ImGui::Begin(winTitle.c_str(), &m_pOpen, flags);
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
		if (ImGui::BeginTable("##themeLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
			ImGui::TableSetupColumn("Styles", ImGuiTableColumnFlags_WidthStretch, 0.3f);
			ImGui::TableSetupColumn("Properties", ImGuiTableColumnFlags_WidthStretch, 0.7f);
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			renderStyleList();
			ImGui::TableNextColumn();
			renderStyleProperties();
			ImGui::EndTable();
		}
	}
	ImGui::End();
}

void HudThemeDocument::renderStyleList() {
	if (gui::widgets::sectionHeader("Theme", gui::icon::palette, false).open) {
		std::string name = m_theme.name;
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (ImGui::InputText("##themeName", &name, ImGuiInputTextFlags_EnterReturnsTrue) && name != m_theme.name)
			std::ignore = runCommand("theme.set_name", {{"theme_name", name}});
		gui::widgets::endSection();
	}
	if (!gui::widgets::sectionHeader("Styles", gui::icon::list, false).open)
		return;
	const auto selectedName = m_selection ? m_theme.styles[*m_selection].name : std::string{};
	if (gui::widgets::iconButton("##addStyle", {.glyph = gui::icon::plus, .tooltip = "Add an empty style"}))
		std::ignore = runCommand("theme.add_style", {{"name", std::string{"style"}}});
	ImGui::SameLine();
	ImGui::BeginDisabled(!m_selection);
	if (gui::widgets::iconButton("##duplicateStyle", {.glyph = gui::icon::copy, .tooltip = "Duplicate the style"}))
		std::ignore = runCommand("theme.add_style", {{"name", selectedName}, {"from", selectedName}});
	ImGui::SameLine();
	if (gui::widgets::iconButton("##upStyle", {.glyph = gui::icon::arrowUp, .tooltip = "Move the style up"}))
		std::ignore = runCommand("theme.move_style", {{"name", selectedName}, {"down", false}});
	ImGui::SameLine();
	if (gui::widgets::iconButton("##downStyle", {.glyph = gui::icon::arrowDown, .tooltip = "Move the style down"}))
		std::ignore = runCommand("theme.move_style", {{"name", selectedName}, {"down", true}});
	ImGui::SameLine();
	if (gui::widgets::iconButton("##removeStyle", {.glyph = gui::icon::trash2, .tooltip = "Remove the style"}))
		std::ignore = runCommand("theme.remove_style", {{"name", selectedName}});
	ImGui::EndDisabled();
	if (m_theme.styles.empty())
		std::ignore = gui::widgets::emptyState(gui::icon::palette, "No style",
											   "Add a style, then name it in the Style field of the widgets.");
	const auto count = m_theme.styles.size();
	for (size_t index = 0; index < count; ++index) {
		ImGui::PushID(static_cast<int>(index));
		if (ImGui::Selectable(m_theme.styles[index].name.c_str(), m_selection == index)) {
			commitPreview(true);
			setSelection(index);
		}
		ImGui::PopID();
	}
	gui::widgets::endSection();
	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::IsAnyItemActive() && m_selection &&
		ImGui::IsKeyPressed(ImGuiKey_Delete, false))
		std::ignore = runCommand("theme.remove_style", {{"name", m_theme.styles[*m_selection].name}});
}

void HudThemeDocument::renderStyleProperties() {
	if (!m_selection || *m_selection >= m_theme.styles.size()) {
		std::ignore = gui::widgets::emptyState(gui::icon::palette, "No style selected",
											   "Select a style in the list to edit it.");
		return;
	}
	if (!gui::widgets::sectionHeader("Style", gui::icon::paintbrush, false).open)
		return;
	HudStyle edited = m_preview ? *m_preview : m_theme.styles[*m_selection];
	if (gui::component::renderProps(edited)) {
		m_preview = std::move(edited);
		publish();
	}
	ImGui::Spacing();
	ImGui::TextDisabled("Open a HUD or a menu following this theme in another tab: it shows the edits live.");
	gui::widgets::endSection();
}

}// namespace owl::nest
