/**
 * @file FontDocument.cpp
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "FontDocument.h"

#include "EditorLayer.h"

#include <app/Application.h>
#include <data/fonts/FontLibrary.h>
#include <gui/FontPreviewCache.h>
#include <gui/IconFont.h>
#include <gui/utils.h>
#include <gui/widgets/EmptyState.h>
#include <gui/widgets/IconButton.h>
#include <gui/widgets/PropertyGrid.h>
#include <gui/widgets/SectionHeader.h>
#include <gui/widgets/Tooltip.h>
#include <renderer/gpu/Texture.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
#include <imgui.h>
#include <imgui_internal.h>
OWL_DIAG_POP

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <ranges>
#include <set>
#include <string>
#include <system_error>
#include <tuple>
#include <utility>

namespace owl::nest {

namespace {
using data::fonts::CodePointRange;
using data::fonts::FontSettings;

constexpr std::array<FontRangePreset, 10> g_presets{{
		{.name = "Latin Extended-B", .range = {.first = 0x0180, .last = 0x024F}},
		{.name = "Latin Extended Additional", .range = {.first = 0x1E00, .last = 0x1EFF}},
		{.name = "IPA Extensions", .range = {.first = 0x0250, .last = 0x02AF}},
		{.name = "Greek and Coptic", .range = {.first = 0x0370, .last = 0x03FF}},
		{.name = "Cyrillic", .range = {.first = 0x0400, .last = 0x04FF}},
		{.name = "Currency Symbols", .range = {.first = 0x20A0, .last = 0x20CF}},
		{.name = "Arrows", .range = {.first = 0x2190, .last = 0x21FF}},
		{.name = "Mathematical Operators", .range = {.first = 0x2200, .last = 0x22FF}},
		{.name = "Box Drawing", .range = {.first = 0x2500, .last = 0x257F}},
		{.name = "Geometric Shapes", .range = {.first = 0x25A0, .last = 0x25FF}},
}};

constexpr size_t g_editRangeTypeId = 0x466f6e7452616e67;// "FontRang"

// Settings before and after one edit.
class ModifyFontSettingsCommand final : public UndoCommand<FontSettings> {
public:
	ModifyFontSettingsCommand(FontSettings iBefore, FontSettings iAfter, std::string iDescription,
							  const bool iMergeable)
		: m_before{std::move(iBefore)}, m_after{std::move(iAfter)}, m_description{std::move(iDescription)},
		  m_mergeable{iMergeable} {}

	void undo(FontSettings& ioTarget) override { ioTarget = m_before; }

	void redo(FontSettings& ioTarget) override { ioTarget = m_after; }

	[[nodiscard]] auto description() const -> std::string override { return m_description; }

	[[nodiscard]] auto typeId() const -> size_t override { return m_mergeable ? g_editRangeTypeId : 0; }

	[[nodiscard]] auto mergeWith(const UndoCommand& iOther) -> bool override {
		const auto& other = static_cast<const ModifyFontSettingsCommand&>(iOther);
		if (other.m_description != m_description)
			return false;
		m_after = other.m_after;
		return true;
	}

private:
	FontSettings m_before;
	FontSettings m_after;
	std::string m_description;
	bool m_mergeable;
};

auto presetName(const CodePointRange& iRange) -> std::string_view {
	const auto* const found = std::ranges::find_if(
			g_presets, [&iRange](const FontRangePreset& iPreset) -> bool { return iPreset.range == iRange; });
	return found == g_presets.end() ? std::string_view{"Custom"} : found->name;
}

auto formatRange(const CodePointRange& iRange) -> std::string {
	return std::format("U+{:04X} to U+{:04X}", static_cast<uint32_t>(iRange.first), static_cast<uint32_t>(iRange.last));
}

auto codePointInput(const char* iId, char32_t& ioValue) -> bool {
	auto value = static_cast<uint32_t>(ioValue);
	constexpr uint32_t step = 1;
	ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6.f);
	if (!ImGui::InputScalar(iId, ImGuiDataType_U32, &value, &step, nullptr, "%04X",
							ImGuiInputTextFlags_CharsHexadecimal))
		return false;
	ioValue = static_cast<char32_t>(std::min<uint32_t>(value, 0x10FFFF));
	return true;
}
}// namespace

FontDocument::FontDocument() = default;

FontDocument::~FontDocument() = default;

auto FontDocument::rangePresets() -> std::span<const FontRangePreset> { return g_presets; }

auto FontDocument::title() const -> std::string {
	return m_path.empty() ? std::string{"Untitled"} : m_path.stem().string();
}

void FontDocument::onAttach(EditorLayer* iEditor) { mp_editorLayer = iEditor; }

void FontDocument::onDetach() { mp_editorLayer = nullptr; }

void FontDocument::onUpdate([[maybe_unused]] const core::Timestep& iTimeStep) {}

void FontDocument::onEvent([[maybe_unused]] event::Event& ioEvent) {}

auto FontDocument::loadFromFile(const std::filesystem::path& iPath) -> bool {
	auto fontPath = iPath;
	if (iPath.extension() == FontSettings::fileExtension()) {
		fontPath.clear();
		for (const auto* ext: {".ttf", ".otf"}) {
			if (auto candidate = std::filesystem::path{iPath}.replace_extension(ext); exists(candidate)) {
				fontPath = candidate;
				break;
			}
		}
	}
	if (fontPath.empty() || !exists(fontPath)) {
		OWL_WARN("FontDocument: No font file for '{}'.", iPath.string())
		return false;
	}
	m_path = fontPath;
	m_settings = FontSettings::loadForFont(fontPath);
	m_savedSettings = m_settings;
	m_undoManager.clear();
	m_selection.clear();
	return true;
}

auto FontDocument::save() -> bool {
	if (m_path.empty())
		return false;
	return saveAs(m_path);
}

auto FontDocument::saveAs(const std::filesystem::path& iPath) -> bool {
	const auto sidecar = FontSettings::sidecarPath(iPath);
	if (m_settings.extraRanges.empty()) {
		std::error_code error;
		std::filesystem::remove(sidecar, error);
		if (error) {
			OWL_WARN("FontDocument: Cannot remove '{}': {}.", sidecar.string(), error.message())
			return false;
		}
	} else if (!m_settings.saveToFile(sidecar)) {
		OWL_WARN("FontDocument: Cannot write '{}'.", sidecar.string())
		return false;
	}
	m_path = iPath;
	m_savedSettings = m_settings;
	if (app::Application::instanced()) {
		auto& library = app::Application::get().getFontLibrary();
		if (const auto names = library.getLoadedFontNames();
			std::ranges::find(names, m_path.stem().string()) != names.end()) {
			std::ignore = library.reloadFont(m_path.stem().string(), m_settings);
			gui::FontPreviewCache::get().clear();
		}
	}
	return true;
}

auto FontDocument::recoverySnapshot() const -> std::optional<std::string> { return m_settings.serializeToString(); }

auto FontDocument::restoreRecoverySnapshot(const std::string& iSnapshot) -> bool {
	if (!m_settings.deserializeFromString(iSnapshot)) {
		OWL_WARN("Recovery: Cannot restore the autosave of font '{}'.", title())
		return false;
	}
	return true;
}

void FontDocument::performUndo() {
	if (m_undoManager.canUndo())
		m_undoManager.undo(m_settings);
	m_selection.clear();
}

void FontDocument::performRedo() {
	if (m_undoManager.canRedo())
		m_undoManager.redo(m_settings);
	m_selection.clear();
}

void FontDocument::applyEdit(const std::string& iDescription, const FontSettings& iSettings, const bool iMergeable) {
	if (iSettings == m_settings)
		return;
	m_undoManager.push(mkUniq<ModifyFontSettingsCommand>(m_settings, iSettings, iDescription, iMergeable));
	m_settings = iSettings;
}

void FontDocument::addRange(const CodePointRange& iRange) {
	auto next = m_settings;
	next.extraRanges.push_back(
			{.first = std::min(iRange.first, iRange.last), .last = std::max(iRange.first, iRange.last)});
	applyEdit(std::format("Add range {}", formatRange(next.extraRanges.back())), next, false);
}

void FontDocument::setRange(const size_t iIndex, const CodePointRange& iRange) {
	if (iIndex >= m_settings.extraRanges.size())
		return;
	auto next = m_settings;
	next.extraRanges[iIndex] = {.first = std::min(iRange.first, iRange.last),
								.last = std::max(iRange.first, iRange.last)};
	applyEdit(std::format("Edit range {}", iIndex + 1), next, true);
}

void FontDocument::removeRanges(const std::set<size_t>& iIndices) {
	auto next = m_settings;
	for (const size_t index: iIndices | std::views::reverse)
		if (index < next.extraRanges.size())
			next.extraRanges.erase(next.extraRanges.begin() + static_cast<std::ptrdiff_t>(index));
	const auto removed = m_settings.extraRanges.size() - next.extraRanges.size();
	applyEdit(removed == 1 ? std::string{"Remove range"} : std::format("Remove {} ranges", removed), next, false);
	m_selection.clear();
}

auto FontDocument::loadedFont() const -> shared<data::fonts::Font> {
	if (m_path.empty() || !app::Application::instanced())
		return nullptr;
	auto& library = app::Application::get().getFontLibrary();
	const auto names = library.getLoadedFontNames();
	if (std::ranges::find(names, m_path.stem().string()) == names.end())
		return nullptr;
	return library.getFont(m_path.stem().string());
}

void FontDocument::onImGuiRender() {
	const auto display =
			(mp_editorLayer != nullptr) ? mp_editorLayer->getDocumentManager().displayTitleFor(this) : title();
	const auto winTitle = documentWindowTitle(DocumentType::Font, display, static_cast<uint64_t>(id()));
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
		const auto font = loadedFont();
		const auto atlas = font != nullptr ? gui::imTexture(font->getAtlasTexture()) : std::nullopt;
		if (!atlas.has_value()) {
			std::ignore = gui::widgets::emptyState(gui::icon::type, "Font not loaded",
												   "Use the font in a Text component to load its atlas here.");
		} else {
			if (isDirty())
				ImGui::TextDisabled("Unsaved charset: save (Ctrl+S) to rebuild the atlas.");
			const auto size = font->getAtlasTexture()->getSize();
			const auto avail = ImGui::GetContentRegionAvail();
			const float scale = std::min(avail.x / static_cast<float>(std::max(size.x(), 1u)),
										 avail.y / static_cast<float>(std::max(size.y(), 1u)));
			ImGui::Image(atlas.value(), {static_cast<float>(size.x()) * scale, static_cast<float>(size.y()) * scale},
						 {0, 1}, {1, 0});
		}
	}
	ImGui::End();
}

void FontDocument::renderHierarchyPanel() {
	if (!gui::widgets::sectionHeader("Font", gui::icon::type, false).open)
		return;
	if (gui::widgets::beginProperties("##fontInfo")) {
		gui::widgets::propertyLabel("File");
		ImGui::TextUnformatted(m_path.filename().string().c_str());
		std::ignore = gui::widgets::propertyReset(false);
		const auto font = loadedFont();
		gui::widgets::propertyLabel("Glyphs", "Glyphs of the charset found in the font file.");
		if (font != nullptr)
			ImGui::Text("%zu", font->getGlyphCount());
		else
			ImGui::TextDisabled("not loaded");
		std::ignore = gui::widgets::propertyReset(false);
		if (font != nullptr && font->getAtlasTexture()) {
			const auto size = font->getAtlasTexture()->getSize();
			gui::widgets::propertyLabel("Atlas", "Size of the MSDF atlas; it is cached on disk.");
			ImGui::Text("%u x %u%s", size.x(), size.y(), font->isAtlasFromCache() ? " (cached)" : "");
			std::ignore = gui::widgets::propertyReset(false);
		}
		gui::widgets::endProperties();
	}
	gui::widgets::endSection();
}

void FontDocument::renderPropertiesPanel() {
	if (gui::widgets::sectionHeader("Default Charset", gui::icon::lock, false).open) {
		ImGui::TextDisabled("Every font loads these ranges.");
		for (const auto& range: FontSettings::defaultRanges()) ImGui::BulletText("%s", formatRange(range).c_str());
		gui::widgets::endSection();
	}
	if (!gui::widgets::sectionHeader("Extra Ranges", gui::icon::list, false).open)
		return;
	if (gui::widgets::iconButton("##addRange", {.glyph = gui::icon::plus,
												.tooltip = "Add a range",
												.description = "Add a Unicode block or a custom range."}))
		ImGui::OpenPopup("##rangePresets");
	if (ImGui::BeginPopup("##rangePresets")) {
		for (const auto& [name, range]: g_presets) {
			const std::string label{name};
			if (ImGui::MenuItem(label.c_str(), formatRange(range).c_str()))
				addRange(range);
		}
		ImGui::Separator();
		if (ImGui::MenuItem("Custom"))
			addRange({.first = 0x0180, .last = 0x0180});
		ImGui::EndPopup();
	}
	ImGui::SameLine();
	if (gui::widgets::iconButton("##removeRanges", {.glyph = gui::icon::trash2,
													.tooltip = "Remove the selected ranges",
													.shortcut = "Delete",
													.disabled = m_selection.empty()}))
		removeRanges(m_selection);
	ImGui::SameLine();
	if (gui::widgets::iconButton("##clearRanges", {.glyph = gui::icon::eraser,
												   .tooltip = "Remove every extra range",
												   .disabled = m_settings.extraRanges.empty()})) {
		std::set<size_t> all;
		for (size_t index = 0; index < m_settings.extraRanges.size(); ++index) all.insert(index);
		removeRanges(all);
	}
	if (!m_selection.empty() && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
		!ImGui::IsAnyItemActive() && ImGui::IsKeyPressed(ImGuiKey_Delete, false))
		removeRanges(m_selection);
	if (m_settings.extraRanges.empty()) {
		ImGui::TextDisabled("Latin only: add a range for other scripts.");
		gui::widgets::endSection();
		return;
	}
	std::optional<std::pair<size_t, CodePointRange>> edited;
	for (size_t index = 0; index < m_settings.extraRanges.size(); ++index) {
		auto range = m_settings.extraRanges[index];
		ImGui::PushID(static_cast<int>(index));
		const bool selected = m_selection.contains(index);
		const std::string name{presetName(range)};
		if (ImGui::Selectable(name.c_str(), selected, ImGuiSelectableFlags_AllowOverlap,
							  {ImGui::GetFontSize() * 9.f, ImGui::GetFrameHeight()})) {
			if (!ImGui::GetIO().KeyCtrl)
				m_selection.clear();
			if (selected && ImGui::GetIO().KeyCtrl)
				m_selection.erase(index);
			else
				m_selection.insert(index);
		}
		ImGui::SameLine();
		bool changed = codePointInput("##first", range.first);
		gui::widgets::itemTooltip("First code point", nullptr, "Hexadecimal, included.");
		ImGui::SameLine();
		changed = codePointInput("##last", range.last) || changed;
		gui::widgets::itemTooltip("Last code point", nullptr, "Hexadecimal, included.");
		if (changed)
			edited = {index, range};
		ImGui::PopID();
	}
	if (edited)
		setRange(edited->first, edited->second);
	gui::widgets::endSection();
}

}// namespace owl::nest
