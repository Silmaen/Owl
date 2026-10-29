/**
 * @file LocalisationDocument.cpp
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "LocalisationDocument.h"

#include "EditorLayer.h"

#include <app/Application.h>
#include <app/EngineContext.h>
#include <data/fonts/FontLibrary.h>
#include <gui/IconFont.h>
#include <gui/Theme.h>
#include <gui/UiLayer.h>
#include <gui/utils.h>
#include <gui/widgets/Badge.h>
#include <gui/widgets/Combo.h>
#include <gui/widgets/EmptyState.h>
#include <gui/widgets/IconButton.h>
#include <gui/widgets/Menu.h>
#include <gui/widgets/SearchField.h>
#include <gui/widgets/SectionHeader.h>
#include <gui/widgets/Toggle.h>
#include <gui/widgets/Tooltip.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
OWL_DIAG_POP

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <format>
#include <ranges>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace owl::nest {

namespace {

using data::localisation::Language;
using data::localisation::Localisation;
using data::localisation::LocalisedString;

class ModifyLocalisationCommand final : public UndoCommand<Localisation> {
public:
	ModifyLocalisationCommand(Localisation iBefore, Localisation iAfter, std::string iDescription, const bool iMerge)
		: m_before{std::move(iBefore)}, m_after{std::move(iAfter)}, m_description{std::move(iDescription)},
		  m_merge{iMerge} {}

	void undo(Localisation& ioTarget) override { ioTarget = m_before; }

	void redo(Localisation& ioTarget) override { ioTarget = m_after; }

	[[nodiscard]] auto description() const -> std::string override { return m_description; }

	[[nodiscard]] auto typeId() const -> size_t override { return m_merge ? 1 : 0; }

	[[nodiscard]] auto mergeWith(const UndoCommand& iOther) -> bool override {
		const auto& other = static_cast<const ModifyLocalisationCommand&>(iOther);
		if (other.m_description != m_description)
			return false;
		m_after = other.m_after;
		return true;
	}

private:
	Localisation m_before;
	Localisation m_after;
	std::string m_description;
	bool m_merge;
};

auto sortedUnique(std::span<const size_t> iIndices, const size_t iCount) -> std::vector<size_t> {
	std::vector<size_t> sorted;
	for (const auto index: iIndices)
		if (index < iCount)
			sorted.push_back(index);
	std::ranges::sort(sorted);
	const auto [first, last] = std::ranges::unique(sorted);
	sorted.erase(first, last);
	return sorted;
}

auto containsText(const std::string_view iText, const std::string_view iSearch) -> bool {
	return std::ranges::search(iText, iSearch, [](const char iA, const char iB) -> bool {
			   return std::tolower(static_cast<unsigned char>(iA)) == std::tolower(static_cast<unsigned char>(iB));
		   }).begin() != iText.end();
}

auto matches(const LocalisedString& iEntry, const std::string_view iSearch) -> bool {
	if (iSearch.empty() || containsText(iEntry.key, iSearch))
		return true;
	return std::ranges::any_of(iEntry.values,
							   [&](const auto& iValue) -> bool { return containsText(iValue.second, iSearch); });
}

}// namespace

LocalisationDocument::LocalisationDocument() { m_undo.markSaved(); }

LocalisationDocument::~LocalisationDocument() = default;

void LocalisationDocument::onAttach(EditorLayer* iEditor) { mp_editorLayer = iEditor; }

void LocalisationDocument::onDetach() { mp_editorLayer = nullptr; }

void LocalisationDocument::onUpdate([[maybe_unused]] const core::Timestep& iTimeStep) {}

void LocalisationDocument::onEvent([[maybe_unused]] event::Event& ioEvent) {}

auto LocalisationDocument::loadFromFile(const std::filesystem::path& iPath) -> bool {
	auto loaded = Localisation::loadFile(iPath);
	if (!loaded) {
		OWL_CORE_WARN("LocalisationDocument: {} is not a localisation.", iPath.string())
		return false;
	}
	m_localisation = std::move(*loaded);
	m_path = iPath;
	m_undo.clear();
	m_undo.markSaved();
	m_selection.clear();
	return true;
}

void LocalisationDocument::startEmpty(const std::filesystem::path& iPath) {
	m_localisation = {.languages = {{.code = "en", .name = "English", .font = {}, .exported = true}},
					  .fallback = "en",
					  .strings = {}};
	m_path = iPath;
	m_undo.clear();
	m_undo.markUnsaved();
	m_selection.clear();
}

auto LocalisationDocument::save() -> bool {
	if (m_path.empty())
		return false;
	return saveAs(m_path);
}

auto LocalisationDocument::saveAs(const std::filesystem::path& iPath) -> bool {
	if (!m_localisation.saveToFile(iPath)) {
		OWL_CORE_ERROR("LocalisationDocument: Failed to write '{}'.", iPath.string())
		return false;
	}
	m_path = iPath;
	m_undo.markSaved();
	if (app::Application::instanced() && iPath.filename() == Localisation::fileName)
		app::Application::get().getEngineContext().setLocalisation(m_localisation);
	return true;
}

auto LocalisationDocument::recoverySnapshot() const -> std::optional<std::string> {
	return m_localisation.serializeToString();
}

auto LocalisationDocument::restoreRecoverySnapshot(const std::string& iSnapshot) -> bool {
	auto restored = Localisation::deserializeFromString(iSnapshot);
	if (!restored) {
		OWL_WARN("Recovery: Cannot restore the autosave of the localisation.")
		return false;
	}
	m_localisation = std::move(*restored);
	m_undo.clear();
	m_undo.markUnsaved();
	setSelection(m_selection);
	return true;
}

void LocalisationDocument::performUndo() {
	if (!m_undo.canUndo())
		return;
	m_undo.undo(m_localisation);
	setSelection(m_selection);
}

void LocalisationDocument::performRedo() {
	if (!m_undo.canRedo())
		return;
	m_undo.redo(m_localisation);
	setSelection(m_selection);
}

auto LocalisationDocument::commit(Localisation iNext, const std::string& iDescription, const bool iMerge) -> bool {
	if (iNext == m_localisation)
		return false;
	auto before = std::move(m_localisation);
	m_localisation = iNext;
	m_undo.push(mkUniq<ModifyLocalisationCommand>(std::move(before), std::move(iNext), iDescription, iMerge));
	setSelection(m_selection);
	return true;
}

auto LocalisationDocument::addLanguage(const std::string& iCode, const std::string& iName) -> bool {
	if (iCode.empty() || m_localisation.findLanguage(iCode) != nullptr) {
		OWL_CORE_WARN("LocalisationDocument: Language code '{}' is empty or taken.", iCode)
		return false;
	}
	auto next = m_localisation;
	next.languages.push_back({.code = iCode, .name = iName.empty() ? iCode : iName, .font = {}, .exported = true});
	if (next.fallback.empty())
		next.fallback = iCode;
	return commit(std::move(next), "Add language", false);
}

auto LocalisationDocument::removeLanguage(const std::string_view iCode) -> bool {
	if (m_localisation.findLanguage(iCode) == nullptr) {
		OWL_CORE_WARN("LocalisationDocument: No language '{}'.", iCode)
		return false;
	}
	auto next = m_localisation;
	std::erase_if(next.languages, [&](const Language& iLanguage) -> bool { return iLanguage.code == iCode; });
	for (auto& entry: next.strings)
		if (const auto value = entry.values.find(iCode); value != entry.values.end())
			entry.values.erase(value);
	if (next.fallback == iCode)
		next.fallback = next.languages.empty() ? std::string{} : next.languages.front().code;
	return commit(std::move(next), "Remove language", false);
}

auto LocalisationDocument::setLanguage(const Language& iLanguage, const bool iMerge) -> bool {
	auto next = m_localisation;
	const auto found = std::ranges::find(next.languages, iLanguage.code, &Language::code);
	if (found == next.languages.end()) {
		OWL_CORE_WARN("LocalisationDocument: No language '{}'.", iLanguage.code)
		return false;
	}
	*found = iLanguage;
	return commit(std::move(next), "Edit language", iMerge);
}

auto LocalisationDocument::setFallback(const std::string_view iCode) -> bool {
	if (m_localisation.findLanguage(iCode) == nullptr) {
		OWL_CORE_WARN("LocalisationDocument: No language '{}'.", iCode)
		return false;
	}
	auto next = m_localisation;
	next.fallback = iCode;
	return commit(std::move(next), "Set fallback language", false);
}

auto LocalisationDocument::getUniqueKey(const std::string_view iBase) const -> std::string {
	std::string base = iBase.empty() ? std::string{"text"} : std::string{iBase};
	if (m_localisation.findString(base) == nullptr)
		return base;
	for (uint32_t counter = 2;; ++counter)
		if (auto key = std::format("{}_{}", base, counter); m_localisation.findString(key) == nullptr)
			return key;
}

auto LocalisationDocument::addString(const std::string& iKey, const std::string& iTable) -> size_t {
	auto next = m_localisation;
	next.strings.push_back({.key = getUniqueKey(iKey), .table = iTable, .values = {}});
	const auto index = next.strings.size() - 1;
	std::ignore = commit(std::move(next), "Add string", false);
	setSelection({index});
	return index;
}

auto LocalisationDocument::setKey(const size_t iIndex, const std::string& iKey) -> bool {
	if (iIndex >= m_localisation.strings.size() || iKey.empty()) {
		OWL_CORE_WARN("LocalisationDocument: Cannot rename string {} to '{}'.", iIndex, iKey)
		return false;
	}
	if (const auto* other = m_localisation.findString(iKey);
		other != nullptr && other != &m_localisation.strings[iIndex]) {
		OWL_CORE_WARN("LocalisationDocument: Key '{}' is taken.", iKey)
		return false;
	}
	auto next = m_localisation;
	next.strings[iIndex].key = iKey;
	return commit(std::move(next), "Rename key", false);
}

auto LocalisationDocument::setValue(const size_t iIndex, const std::string& iCode, const std::string& iValue,
									const bool iMerge) -> bool {
	if (iIndex >= m_localisation.strings.size() || m_localisation.findLanguage(iCode) == nullptr) {
		OWL_CORE_WARN("LocalisationDocument: No string {} in language '{}'.", iIndex, iCode)
		return false;
	}
	auto next = m_localisation;
	if (iValue.empty())
		next.strings[iIndex].values.erase(iCode);
	else
		next.strings[iIndex].values[iCode] = iValue;
	return commit(std::move(next), std::format("Edit {} text", iCode), iMerge);
}

auto LocalisationDocument::setTable(const std::span<const size_t> iIndices, const std::string& iTable) -> bool {
	const auto indices = sortedUnique(iIndices, m_localisation.strings.size());
	auto next = m_localisation;
	for (const auto index: indices) next.strings[index].table = iTable;
	return commit(std::move(next), "Move strings to a table", false);
}

auto LocalisationDocument::removeStrings(const std::span<const size_t> iIndices) -> bool {
	const auto indices = sortedUnique(iIndices, m_localisation.strings.size());
	if (indices.empty())
		return false;
	auto next = m_localisation;
	for (const auto index: indices | std::views::reverse)
		next.strings.erase(next.strings.begin() + static_cast<std::ptrdiff_t>(index));
	if (!commit(std::move(next), indices.size() > 1 ? "Remove strings" : "Remove string", false))
		return false;
	m_selection.clear();
	return true;
}

auto LocalisationDocument::duplicateStrings(const std::span<const size_t> iIndices) -> bool {
	const auto indices = sortedUnique(iIndices, m_localisation.strings.size());
	if (indices.empty())
		return false;
	auto next = m_localisation;
	const auto insertAt = indices.back() + 1;
	std::vector<LocalisedString> copies;
	for (const auto index: indices) {
		auto copy = m_localisation.strings[index];
		for (uint32_t counter = 2;; ++counter) {
			copy.key = std::format("{}_{}", m_localisation.strings[index].key, counter);
			if (next.findString(copy.key) == nullptr &&
				std::ranges::none_of(copies,
									 [&](const LocalisedString& iOther) -> bool { return iOther.key == copy.key; }))
				break;
		}
		copies.push_back(std::move(copy));
	}
	next.strings.insert(next.strings.begin() + static_cast<std::ptrdiff_t>(insertAt), copies.begin(), copies.end());
	if (!commit(std::move(next), "Duplicate strings", false))
		return false;
	std::vector<size_t> selection(copies.size());
	for (size_t i = 0; i < copies.size(); ++i) selection[i] = insertAt + i;
	setSelection(std::move(selection));
	return true;
}

void LocalisationDocument::setSelection(std::vector<size_t> iIndices) {
	m_selection = sortedUnique(iIndices, m_localisation.strings.size());
}

void LocalisationDocument::onImGuiRender() {
	const auto winTitle = documentWindowTitle(DocumentType::Localisation, title(), static_cast<uint64_t>(id()));
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
		if (gui::widgets::sectionHeader("Languages", "ribbon_book", false).open) {
			renderLanguages();
			gui::widgets::endSection();
		}
		if (gui::widgets::sectionHeader("Strings", "comp_ui_text", false).open) {
			renderStringToolbar();
			renderStrings();
			gui::widgets::endSection();
		}
	}
	ImGui::End();
}

void LocalisationDocument::renderLanguages() {
	if (!ImGui::BeginTable("##languages", 5, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp))
		return;
	ImGui::TableSetupColumn("Code", ImGuiTableColumnFlags_WidthFixed, 60.f);
	ImGui::TableSetupColumn("Name");
	ImGui::TableSetupColumn("Font");
	ImGui::TableSetupColumn("Export / fallback", ImGuiTableColumnFlags_WidthFixed, 170.f);
	ImGui::TableSetupColumn("##remove", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFrameHeight());
	ImGui::TableHeadersRow();
	const auto fonts = data::fonts::FontLibrary::getFoundFontNames();
	std::optional<std::string> removed;
	for (const auto& language: m_localisation.languages) {
		ImGui::PushID(language.code.c_str());
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(language.code.c_str());
		ImGui::TableNextColumn();
		auto next = language;
		ImGui::SetNextItemWidth(-1.f);
		if (ImGui::InputText("##name", &next.name)) {
			const int frame = ImGui::GetFrameCount();
			std::ignore = setLanguage(next, frame - m_lastEditFrame <= 1);
			m_lastEditFrame = frame;
		}
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-1.f);
		if (gui::widgets::beginCombo("##font", language.font.empty() ? "(each text's font)" : language.font.c_str())) {
			if (ImGui::Selectable("(each text's font)", language.font.empty())) {
				next.font.clear();
				std::ignore = setLanguage(next);
			}
			for (const auto& font: fonts)
				if (ImGui::Selectable(font.c_str(), font == language.font)) {
					next.font = font;
					std::ignore = setLanguage(next);
				}
			ImGui::EndCombo();
		}
		gui::widgets::itemTooltip("Font of the localised HUD texts in this language, for its glyphs.");
		ImGui::TableNextColumn();
		if (gui::widgets::checkbox("Export", &next.exported))
			std::ignore = setLanguage(next);
		gui::widgets::itemTooltip("Pack this language with the game.");
		ImGui::SameLine();
		if (const bool fallback = m_localisation.fallback == language.code;
			gui::widgets::chip("Fallback", fallback) && !fallback)
			std::ignore = setFallback(language.code);
		gui::widgets::itemTooltip("A text missing in a language shows its value in this one.");
		ImGui::TableNextColumn();
		if (gui::widgets::iconButton("##removeLanguage",
									 {.glyph = gui::icon::trash2,
									  .tooltip = "Remove the language",
									  .description = "Its texts go with it; undo brings them back."}))
			removed = language.code;
		ImGui::PopID();
	}
	ImGui::EndTable();
	if (removed.has_value())
		std::ignore = removeLanguage(*removed);
	ImGui::SetNextItemWidth(80.f);
	const bool entered =
			ImGui::InputTextWithHint("##newLanguage", "code", &m_newLanguageCode, ImGuiInputTextFlags_EnterReturnsTrue);
	ImGui::SameLine();
	if ((gui::widgets::iconButton("##addLanguage", {.glyph = gui::icon::plus,
													.tooltip = "Add a language",
													.description = "Type its code first (en, fr, ja...).",
													.disabled = m_newLanguageCode.empty()}) ||
		 entered) &&
		addLanguage(m_newLanguageCode, m_newLanguageCode))
		m_newLanguageCode.clear();
}

void LocalisationDocument::renderStringToolbar() {
	const auto tables = m_localisation.getTables();
	ImGui::SetNextItemWidth(160.f);
	std::string shown{"All tables"};
	if (m_tableFilter.has_value())
		shown = m_tableFilter->empty() ? std::string{"(no table)"} : *m_tableFilter;
	if (gui::widgets::beginCombo("##tableFilter", shown.c_str())) {
		if (ImGui::Selectable("All tables", !m_tableFilter.has_value()))
			m_tableFilter.reset();
		for (const auto& table: tables)
			if (ImGui::Selectable(table.empty() ? "(no table)" : table.c_str(), m_tableFilter == table))
				m_tableFilter = table;
		ImGui::EndCombo();
	}
	gui::widgets::itemTooltip("String table shown.");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(200.f);
	std::ignore = gui::widgets::searchField("##search", m_search, "Search keys and texts");
	ImGui::SameLine();
	if (gui::widgets::iconButton(
				"##addString",
				{.glyph = gui::icon::plus, .tooltip = "Add a string", .description = "A new key in the table shown."}))
		std::ignore =
				addString(m_tableFilter.has_value() && !m_tableFilter->empty() ? *m_tableFilter + ".text" : "text",
						  m_tableFilter.value_or(std::string{}));
	ImGui::SameLine();
	const bool none = m_selection.empty();
	if (gui::widgets::iconButton("##copyStrings", {.glyph = gui::icon::copy,
												   .tooltip = "Duplicate",
												   .shortcut = "Ctrl+D",
												   .description = "Copy the selected strings under new keys.",
												   .disabled = none}))
		std::ignore = duplicateStrings(m_selection);
	ImGui::SameLine();
	if (gui::widgets::iconButton("##removeStrings", {.glyph = gui::icon::trash2,
													 .tooltip = "Remove",
													 .shortcut = "Delete",
													 .description = "Remove the selected strings.",
													 .disabled = none}))
		std::ignore = removeStrings(m_selection);
	ImGui::SameLine();
	ImGui::BeginDisabled(none);
	ImGui::SetNextItemWidth(140.f);
	if (ImGui::InputTextWithHint("##moveTable", "move to table", &m_moveTable, ImGuiInputTextFlags_EnterReturnsTrue))
		std::ignore = setTable(m_selection, m_moveTable);
	ImGui::EndDisabled();
	gui::widgets::itemTooltip("Type a table and press Enter to move the selected strings into it.");
	for (const auto& language: m_localisation.languages)
		if (const auto count = m_localisation.getMissing(language.code).size(); count > 0) {
			ImGui::SameLine();
			gui::widgets::badge(std::format("{} {} missing", language.code, count).c_str(),
								gui::widgets::BadgeKind::Warning);
		}
}

void LocalisationDocument::clickRow(const size_t iIndex) {
	const auto& io = ImGui::GetIO();
	const bool selected = std::ranges::contains(m_selection, iIndex);
	if (io.KeyCtrl) {
		auto selection = m_selection;
		if (selected)
			std::erase(selection, iIndex);
		else
			selection.push_back(iIndex);
		setSelection(std::move(selection));
	} else if (io.KeyShift && !m_selection.empty()) {
		std::vector<size_t> range;
		for (size_t i = std::min(m_selection.front(), iIndex); i <= std::max(m_selection.back(), iIndex); ++i)
			range.push_back(i);
		setSelection(std::move(range));
	} else {
		setSelection({iIndex});
	}
}

void LocalisationDocument::renderStrings() {
	if (m_localisation.strings.empty()) {
		std::ignore = gui::widgets::emptyState(gui::icon::type, "No string",
											   "Add a string, then type its text in each language.");
		return;
	}
	const auto& languages = m_localisation.languages;
	const auto columns = static_cast<int>(languages.size()) + 2;
	if (!ImGui::BeginTable("##strings", columns,
						   ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
								   ImGuiTableFlags_Resizable))
		return;
	ImGui::TableSetupScrollFreeze(1, 1);
	ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed, 200.f);
	ImGui::TableSetupColumn("Table", ImGuiTableColumnFlags_WidthFixed, 100.f);
	for (const auto& language: languages)
		ImGui::TableSetupColumn(std::format("{} ({})", language.name, language.code).c_str());
	ImGui::TableHeadersRow();
	const auto warning = ImGui::GetColorU32(gui::vec(gui::Theme::withAlpha(gui::UiLayer::getTheme().warning, 0.18f)));
	std::optional<std::pair<size_t, std::string>> renamed;
	std::optional<std::pair<size_t, std::string>> moved;
	std::optional<std::tuple<size_t, std::string, std::string>> edited;
	for (size_t i = 0; i < m_localisation.strings.size(); ++i) {
		const auto& entry = m_localisation.strings[i];
		if ((m_tableFilter.has_value() && entry.table != *m_tableFilter) || !matches(entry, m_search))
			continue;
		ImGui::PushID(static_cast<int>(i));
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		const bool selected = std::ranges::contains(m_selection, i);
		if (ImGui::Selectable("##row", selected, ImGuiSelectableFlags_AllowOverlap, {12.f, 0.f}))
			clickRow(i);
		ImGui::SameLine();
		auto key = entry.key;
		ImGui::SetNextItemWidth(-1.f);
		if (ImGui::InputText("##key", &key, ImGuiInputTextFlags_EnterReturnsTrue))
			renamed = {i, key};
		if (ImGui::IsItemActivated() && !selected)
			setSelection({i});
		ImGui::TableNextColumn();
		auto table = entry.table;
		ImGui::SetNextItemWidth(-1.f);
		if (ImGui::InputText("##table", &table, ImGuiInputTextFlags_EnterReturnsTrue))
			moved = {i, table};
		for (const auto& language: languages) {
			ImGui::TableNextColumn();
			const auto found = entry.values.find(language.code);
			auto value = found != entry.values.end() ? found->second : std::string{};
			if (value.empty())
				ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, warning);
			ImGui::SetNextItemWidth(-1.f);
			if (ImGui::InputTextWithHint(std::format("##{}", language.code).c_str(),
										 std::string{m_localisation.translate(entry.key, language.code)}.c_str(),
										 &value))
				edited = {i, language.code, value};
		}
		ImGui::PopID();
	}
	ImGui::EndTable();
	if (renamed.has_value())
		std::ignore = setKey(renamed->first, renamed->second);
	if (moved.has_value()) {
		const std::vector indices{moved->first};
		std::ignore = setTable(indices, moved->second);
	}
	if (edited.has_value()) {
		const int frame = ImGui::GetFrameCount();
		std::ignore = setValue(std::get<0>(*edited), std::get<1>(*edited), std::get<2>(*edited),
							   frame - m_lastEditFrame <= 1);
		m_lastEditFrame = frame;
	}
	const auto& io = ImGui::GetIO();
	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !io.WantTextInput) {
		if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
			std::ignore = removeStrings(m_selection);
		if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false))
			std::ignore = duplicateStrings(m_selection);
		if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A, false)) {
			std::vector<size_t> all;
			for (size_t i = 0; i < m_localisation.strings.size(); ++i)
				if ((!m_tableFilter.has_value() || m_localisation.strings[i].table == *m_tableFilter) &&
					matches(m_localisation.strings[i], m_search))
					all.push_back(i);
			setSelection(std::move(all));
		}
	}
}

}// namespace owl::nest
