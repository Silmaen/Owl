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
#include <gui/widgets/PropertyGrid.h>
#include <gui/widgets/SearchField.h>
#include <gui/widgets/SectionHeader.h>
#include <gui/widgets/Tooltip.h>
#include <gui/widgets/TreeRow.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
OWL_DIAG_POP

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
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

void LocalisationDocument::onUpdate([[maybe_unused]] const core::Timestep& iTimeStep) { commitPreview(); }

void LocalisationDocument::onEvent([[maybe_unused]] event::Event& ioEvent) {}

auto LocalisationDocument::loadFromFile(const std::filesystem::path& iPath) -> bool {
	auto loaded = Localisation::loadFile(iPath);
	if (!loaded) {
		OWL_CORE_WARN("LocalisationDocument: '{}' is not a localisation.", iPath.string())
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
	cancelPreview();
	if (!m_undo.canUndo())
		return;
	m_undo.undo(m_localisation);
	setSelection(m_selection);
}

void LocalisationDocument::performRedo() {
	cancelPreview();
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

auto LocalisationDocument::runCommand(const std::string_view iName, commands::CommandArgs iArgs) -> bool {
	commitPreview(true);
	return sendCommand(iName, std::move(iArgs));
}

auto LocalisationDocument::sendCommand(const std::string_view iName, commands::CommandArgs iArgs) const -> bool {
	if (mp_editorLayer == nullptr) {
		OWL_WARN("LocalisationDocument: '{}' ignored, no editor to run it.", iName)
		return false;
	}
	iArgs.set("path", m_path.generic_string());
	return mp_editorLayer->execute(iName, iArgs, commands::CommandSource::Gesture).has_value();
}

auto LocalisationDocument::runLanguageCommand(const Language& iLanguage) -> bool {
	commitPreview(true);
	return sendLanguageCommand(iLanguage);
}

auto LocalisationDocument::sendLanguageCommand(const Language& iLanguage) const -> bool {
	return sendCommand("locale.set_language", {{"code", iLanguage.code},
											   {"name", iLanguage.name},
											   {"font", iLanguage.font},
											   {"exported", iLanguage.exported}});
}

auto LocalisationDocument::getKeys(const std::span<const size_t> iIndices) const -> std::string {
	std::string keys;
	for (const auto index: iIndices) {
		if (index >= m_localisation.strings.size())
			continue;
		if (!keys.empty())
			keys += ',';
		keys += m_localisation.strings[index].key;
	}
	return keys;
}

void LocalisationDocument::preview(Localisation iNext) {
	if (!m_previewBefore.has_value())
		m_previewBefore = m_localisation;
	m_localisation = std::move(iNext);
}

void LocalisationDocument::cancelPreview() {
	if (!m_previewBefore.has_value())
		return;
	m_localisation = std::move(*m_previewBefore);
	m_previewBefore.reset();
}

void LocalisationDocument::commitPreview(const bool iForce) {
	if (!m_previewBefore.has_value() || (!iForce && ImGui::IsAnyItemActive()))
		return;
	const auto edited = std::exchange(m_localisation, std::move(*m_previewBefore));
	m_previewBefore.reset();
	for (const auto& language: edited.languages) {
		if (const auto* before = m_localisation.findLanguage(language.code); before != nullptr && *before != language)
			std::ignore = sendLanguageCommand(language);
	}
	for (size_t i = 0; i < std::min(edited.strings.size(), m_localisation.strings.size()); ++i) {
		for (const auto& language: edited.languages) {
			const auto& code = language.code;
			const auto after = edited.strings[i].values.find(code);
			const auto before = m_localisation.strings[i].values.find(code);
			const std::string value = after != edited.strings[i].values.end() ? after->second : std::string{};
			const std::string old = before != m_localisation.strings[i].values.end() ? before->second : std::string{};
			if (value != old)
				std::ignore = sendCommand("locale.set_value",
										  {{"key", edited.strings[i].key}, {"language", code}, {"value", value}});
		}
	}
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
		renderStringToolbar();
		renderStrings();
	}
	ImGui::End();
}

auto LocalisationDocument::isLanguageShown(const std::string_view iCode) const -> bool {
	return !std::ranges::contains(m_hiddenLanguages, iCode);
}

void LocalisationDocument::setLanguageShown(const std::string_view iCode, const bool iShown) {
	std::erase(m_hiddenLanguages, iCode);
	if (!iShown)
		m_hiddenLanguages.emplace_back(iCode);
}

void LocalisationDocument::showOnlyLanguage(const std::string_view iCode) {
	m_hiddenLanguages.clear();
	for (const auto& language: m_localisation.languages)
		if (language.code != iCode)
			m_hiddenLanguages.push_back(language.code);
}

auto LocalisationDocument::isStringShown(const size_t iIndex) const -> bool {
	if (iIndex >= m_localisation.strings.size())
		return false;
	const auto& entry = m_localisation.strings[iIndex];
	if ((m_tableFilter.has_value() && entry.table != *m_tableFilter) || !matches(entry, m_search))
		return false;
	if (!m_missingOnly)
		return true;
	return std::ranges::any_of(m_localisation.languages, [&](const Language& iLanguage) -> bool {
		if (!isLanguageShown(iLanguage.code))
			return false;
		const auto found = entry.values.find(iLanguage.code);
		return found == entry.values.end() || found->second.empty();
	});
}

auto LocalisationDocument::getTableStrings(const std::string_view iTable) const -> std::vector<size_t> {
	std::vector<size_t> indices;
	for (size_t i = 0; i < m_localisation.strings.size(); ++i)
		if (m_localisation.strings[i].table == iTable)
			indices.push_back(i);
	return indices;
}

void LocalisationDocument::renderHierarchyPanel() {
	if (m_localisation.languages.empty())
		std::ignore = gui::widgets::emptyState(gui::icon::globe, "No language", "Type a language code below.");
	for (const auto& language: m_localisation.languages) renderLanguageRow(language);
	ImGui::Spacing();
	ImGui::SetNextItemWidth(std::max(
			ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() - ImGui::GetStyle().ItemSpacing.x, 40.f));
	const bool entered = ImGui::InputTextWithHint("##newLanguage", "new language code (de, ja...)", &m_newLanguageCode,
												  ImGuiInputTextFlags_EnterReturnsTrue);
	ImGui::SameLine();
	if ((gui::widgets::iconButton("##addLanguage", {.glyph = gui::icon::plus,
													.tooltip = "Add a language",
													.description = "Type its code first (en, fr, ja...).",
													.disabled = m_newLanguageCode.empty()}) ||
		 entered) &&
		runCommand("locale.add_language", {{"code", m_newLanguageCode}})) {
		m_selectedLanguage = m_newLanguageCode;
		m_newLanguageCode.clear();
	}
	if (const auto hidden = m_hiddenLanguages.size(); hidden > 0) {
		ImGui::TextDisabled("%zu hidden in the table", hidden);
		ImGui::SameLine();
		if (ImGui::SmallButton("Show all"))
			showAllLanguages();
	}
}

void LocalisationDocument::renderLanguageRow(const Language& iLanguage) {
	ImGui::PushID(iLanguage.code.c_str());
	const auto& theme = gui::UiLayer::getTheme();
	const bool shown = isLanguageShown(iLanguage.code);
	const bool fallback = m_localisation.fallback == iLanguage.code;
	const auto missing = m_localisation.getMissing(iLanguage.code).size();
	const auto label = std::format("{} ({})", iLanguage.name.empty() ? iLanguage.code : iLanguage.name, iLanguage.code);
	std::string trailing;
	if (fallback)
		trailing = "fallback";
	else if (missing > 0)
		trailing = std::format("{} missing", missing);
	const auto row = gui::widgets::treeRow(
			"##language", {.label = label.c_str(),
						   .glyph = gui::icon::globe,
						   .glyphColor = iLanguage.exported ? math::vec4{0.f, 0.f, 0.f, 0.f} : theme.textDisabled,
						   .trailing = trailing.empty() ? nullptr : trailing.c_str(),
						   .labelColor = shown ? math::vec4{0.f, 0.f, 0.f, 0.f} : theme.textDisabled,
						   .selected = m_selectedLanguage == iLanguage.code});
	if (row.clicked)
		m_selectedLanguage = iLanguage.code;
	if (ImGui::BeginPopupContextItem("##languageMenu")) {
		if (ImGui::MenuItem("Show only this language"))
			showOnlyLanguage(iLanguage.code);
		if (ImGui::MenuItem("Show every language"))
			showAllLanguages();
		ImGui::Separator();
		if (ImGui::MenuItem("Use as fallback", nullptr, fallback, !fallback))
			std::ignore = runCommand("locale.set_fallback", {{"code", iLanguage.code}});
		if (ImGui::MenuItem("Remove the language"))
			std::ignore = runCommand("locale.remove_language", {{"code", iLanguage.code}});
		ImGui::EndPopup();
	}
	const ImVec2 cursor = ImGui::GetCursorScreenPos();
	const float side = row.max.y() - row.min.y();
	const float x = row.max.x() - side - 4.f;
	ImGui::SetCursorScreenPos({x, row.min.y()});
	if (ImGui::InvisibleButton("##shown", {side, side}))
		setLanguageShown(iLanguage.code, !shown);
	const char* glyph = shown ? gui::icon::eye : gui::icon::eyeOff;
	const auto glyphSize = ImGui::CalcTextSize(glyph);
	const auto color = ImGui::IsItemHovered() ? theme.text : (shown ? theme.textMuted : theme.textDisabled);
	ImGui::GetWindowDrawList()->AddText(
			{std::floor(x + (side - glyphSize.x) * 0.5f), std::floor(row.min.y() + (side - glyphSize.y) * 0.5f)},
			ImGui::GetColorU32(gui::vec(color)), glyph);
	gui::widgets::itemTooltip(shown ? "Hide its column in the string table" : "Show its column in the string table");
	ImGui::SetCursorScreenPos(cursor);
	ImGui::PopID();
}

void LocalisationDocument::renderPropertiesPanel() {
	if (gui::widgets::sectionHeader("Language", gui::icon::globe, false).open) {
		renderLanguageProperties();
		gui::widgets::endSection();
	}
	if (gui::widgets::sectionHeader("Tables", gui::icon::rows3, false).open) {
		renderTables();
		gui::widgets::endSection();
	}
}

void LocalisationDocument::renderLanguageProperties() {
	const auto found = std::ranges::find(m_localisation.languages, m_selectedLanguage, &Language::code);
	if (found == m_localisation.languages.end()) {
		ImGui::TextDisabled("Select a language in the Languages panel.");
		return;
	}
	const Language& language = *found;
	if (!gui::widgets::beginProperties("##language"))
		return;
	gui::widgets::propertyLabel("Code", "Code of the language, used by the scripts and the save files.");
	ImGui::TextUnformatted(language.code.c_str());
	std::ignore = gui::widgets::propertyReset(false);
	auto next = language;
	gui::widgets::propertyLabel("Name", "Name shown to the player in the language menu.");
	if (ImGui::InputText("##name", &next.name)) {
		auto edited = m_localisation;
		*std::ranges::find(edited.languages, next.code, &Language::code) = next;
		preview(std::move(edited));
	}
	std::ignore = gui::widgets::propertyReset(false);
	gui::widgets::propertyLabel("Font", "Font of the localised HUD texts in this language, for its glyphs.");
	const auto fonts = data::fonts::FontLibrary::getFoundFontNames();
	if (gui::widgets::beginCombo("##font", language.font.empty() ? "(each text's font)" : language.font.c_str())) {
		if (ImGui::Selectable("(each text's font)", language.font.empty())) {
			next.font.clear();
			std::ignore = runLanguageCommand(next);
		}
		for (const auto& font: fonts)
			if (ImGui::Selectable(font.c_str(), font == language.font)) {
				next.font = font;
				std::ignore = runLanguageCommand(next);
			}
		ImGui::EndCombo();
	}
	if (gui::widgets::propertyReset(!language.font.empty())) {
		next.font.clear();
		std::ignore = runLanguageCommand(next);
	}
	if (gui::widgets::propertyBool("Export", next.exported, true, "Pack this language with the game."))
		std::ignore = runLanguageCommand(next);
	gui::widgets::propertyLabel("Fallback", "A text missing in a language shows its value in this one.");
	if (const bool fallback = m_localisation.fallback == language.code;
		gui::widgets::chip(fallback ? "Fallback" : "Use as fallback", fallback) && !fallback)
		std::ignore = runCommand("locale.set_fallback", {{"code", language.code}});
	std::ignore = gui::widgets::propertyReset(false);
	gui::widgets::propertyLabel("Missing", "Strings without a text in this language.");
	ImGui::Text("%zu", m_localisation.getMissing(language.code).size());
	std::ignore = gui::widgets::propertyReset(false);
	gui::widgets::endProperties();
	if (gui::widgets::button("##removeLanguage", {.glyph = gui::icon::trash2,
												  .label = "Remove the language",
												  .tooltip = "Remove the language",
												  .description = "Its texts go with it; undo brings them back."}))
		std::ignore = runCommand("locale.remove_language", {{"code", language.code}});
}

void LocalisationDocument::renderTables() {
	const auto tables = m_localisation.getTables();
	const auto tableLabel = [](const std::string& iTable) -> std::string {
		return iTable.empty() ? std::string{"(no table)"} : iTable;
	};
	if (gui::widgets::treeRow("##allTables", {.label = "All tables",
											  .glyph = gui::icon::layers,
											  .trailing = std::format("{}", m_localisation.strings.size()).c_str(),
											  .selected = !m_tableFilter.has_value()})
				.clicked)
		m_tableFilter.reset();
	for (const auto& table: tables) {
		ImGui::PushID(table.c_str());
		const auto label = tableLabel(table);
		const auto count = std::format("{}", getTableStrings(table).size());
		if (gui::widgets::treeRow("##table", {.label = label.c_str(),
											  .glyph = gui::icon::rows3,
											  .trailing = count.c_str(),
											  .selected = m_tableFilter == table})
					.clicked) {
			m_tableFilter = table;
			m_tableName = table;
		}
		ImGui::PopID();
	}
	ImGui::Spacing();
	if (!gui::widgets::beginProperties("##tables"))
		return;
	const bool tableShown = m_tableFilter.has_value() && !m_tableFilter->empty();
	gui::widgets::propertyLabel(tableShown ? "Rename" : "New Table",
								tableShown ? "Type a new name and press Enter: every string of the table moves to it."
										   : "Type a name and press Enter: a first string is added in the new table.");
	if (ImGui::InputTextWithHint("##tableName", "table name", &m_tableName, ImGuiInputTextFlags_EnterReturnsTrue) &&
		!m_tableName.empty()) {
		if (tableShown) {
			if (m_tableName != *m_tableFilter &&
				runCommand("locale.set_table",
						   {{"keys", getKeys(getTableStrings(*m_tableFilter))}, {"table", m_tableName}}))
				m_tableFilter = m_tableName;
		} else if (runCommand("locale.add_string",
							  {{"key", getUniqueKey(m_tableName + ".text")}, {"table", m_tableName}})) {
			m_tableFilter = m_tableName;
		}
	}
	std::ignore = gui::widgets::propertyReset(false);
	gui::widgets::propertyLabel("Move Selection", "Type a table and press Enter to move the selected strings into it.");
	ImGui::BeginDisabled(m_selection.empty());
	if (ImGui::InputTextWithHint("##moveTable", std::format("{} selected", m_selection.size()).c_str(), &m_moveTable,
								 ImGuiInputTextFlags_EnterReturnsTrue))
		std::ignore = runCommand("locale.set_table", {{"keys", getKeys(m_selection)}, {"table", m_moveTable}});
	ImGui::EndDisabled();
	std::ignore = gui::widgets::propertyReset(false);
	gui::widgets::endProperties();
	if (tableShown) {
		const auto strings = getTableStrings(*m_tableFilter);
		const auto label = std::format("Remove the table ({} strings)", strings.size());
		if (gui::widgets::button("##removeTable", {.glyph = gui::icon::trash2,
												   .label = label.c_str(),
												   .tooltip = "Remove the table",
												   .description = "Its strings go with it; undo brings them back."}) &&
			runCommand("locale.remove_strings", {{"keys", getKeys(strings)}}))
			m_tableFilter.reset();
	}
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
	if (gui::widgets::iconButton(
				"##addString",
				{.glyph = gui::icon::plus, .tooltip = "Add a string", .description = "A new key in the table shown."}))
		std::ignore = runCommand(
				"locale.add_string",
				{{"key", m_tableFilter.has_value() && !m_tableFilter->empty() ? *m_tableFilter + ".text" : "text"},
				 {"table", m_tableFilter.value_or(std::string{})}});
	ImGui::SameLine();
	const bool none = m_selection.empty();
	if (gui::widgets::iconButton("##copyStrings", {.glyph = gui::icon::copy,
												   .tooltip = "Duplicate",
												   .shortcut = "Ctrl+D",
												   .description = "Copy the selected strings under new keys.",
												   .disabled = none}))
		std::ignore = runCommand("locale.duplicate_strings", {{"keys", getKeys(m_selection)}});
	ImGui::SameLine();
	if (gui::widgets::iconButton("##removeStrings", {.glyph = gui::icon::trash2,
													 .tooltip = "Remove",
													 .shortcut = "Delete",
													 .description = "Remove the selected strings.",
													 .disabled = none}))
		std::ignore = runCommand("locale.remove_strings", {{"keys", getKeys(m_selection)}});
	ImGui::SameLine();
	if (gui::widgets::chip("Missing only", m_missingOnly))
		m_missingOnly = !m_missingOnly;
	gui::widgets::itemTooltip("List only the strings missing a text in a shown language.");
	ImGui::SameLine();
	std::ignore = gui::widgets::searchField("##search", m_search, "Search keys and texts");
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
	std::vector<const Language*> languages;
	for (const auto& language: m_localisation.languages)
		if (isLanguageShown(language.code))
			languages.push_back(&language);
	// Past a few languages the columns keep a readable width and the table scrolls sideways.
	const bool wide = languages.size() > 3;
	const bool tableColumn = !m_tableFilter.has_value();
	const auto columns = static_cast<int>(languages.size()) + (tableColumn ? 2 : 1);
	ImGuiTableFlags tableFlags =
			ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable;
	if (wide)
		tableFlags |= ImGuiTableFlags_ScrollX;
	if (!ImGui::BeginTable("##strings", columns, tableFlags))
		return;
	ImGui::TableSetupScrollFreeze(1, 1);
	ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 13.f);
	if (tableColumn)
		ImGui::TableSetupColumn("Table", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 7.f);
	for (const auto* language: languages)
		ImGui::TableSetupColumn(std::format("{} ({})", language->name, language->code).c_str(),
								wide ? ImGuiTableColumnFlags_WidthFixed : ImGuiTableColumnFlags_WidthStretch,
								wide ? ImGui::GetFontSize() * 15.f : 0.f);
	ImGui::TableHeadersRow();
	const auto warning = ImGui::GetColorU32(gui::vec(gui::Theme::withAlpha(gui::UiLayer::getTheme().warning, 0.18f)));
	std::optional<std::pair<size_t, std::string>> renamed;
	std::optional<std::pair<size_t, std::string>> moved;
	std::optional<std::tuple<size_t, std::string, std::string>> edited;
	for (size_t i = 0; i < m_localisation.strings.size(); ++i) {
		const auto& entry = m_localisation.strings[i];
		if (!isStringShown(i))
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
		if (tableColumn) {
			ImGui::TableNextColumn();
			auto table = entry.table;
			ImGui::SetNextItemWidth(-1.f);
			if (ImGui::InputText("##table", &table, ImGuiInputTextFlags_EnterReturnsTrue))
				moved = {i, table};
		}
		for (const auto* languagePtr: languages) {
			const auto& language = *languagePtr;
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
		std::ignore = runCommand("locale.rename_key",
								 {{"key", m_localisation.strings[renamed->first].key}, {"new_key", renamed->second}});
	if (moved.has_value())
		std::ignore = runCommand("locale.set_table",
								 {{"keys", m_localisation.strings[moved->first].key}, {"table", moved->second}});
	if (edited.has_value()) {
		const auto& [index, code, value] = *edited;
		auto next = m_localisation;
		if (value.empty())
			next.strings[index].values.erase(code);
		else
			next.strings[index].values[code] = value;
		preview(std::move(next));
	}
	const auto& io = ImGui::GetIO();
	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !io.WantTextInput) {
		if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
			std::ignore = runCommand("locale.remove_strings", {{"keys", getKeys(m_selection)}});
		if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false))
			std::ignore = runCommand("locale.duplicate_strings", {{"keys", getKeys(m_selection)}});
		if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A, false)) {
			std::vector<size_t> all;
			for (size_t i = 0; i < m_localisation.strings.size(); ++i)
				if (isStringShown(i))
					all.push_back(i);
			setSelection(std::move(all));
		}
	}
}

}// namespace owl::nest
