/**
 * @file DataTableDocument.cpp
 * @author Silmaen
 * @date 30/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "DataTableDocument.h"

#include "AssetKind.h"
#include "EditorLayer.h"

#include <app/Application.h>
#include <app/EngineContext.h>
#include <gui/IconFont.h>
#include <gui/Theme.h>
#include <gui/UiLayer.h>
#include <gui/utils.h>
#include <gui/widgets/AssetField.h>
#include <gui/widgets/Combo.h>
#include <gui/widgets/EmptyState.h>
#include <gui/widgets/IconButton.h>
#include <gui/widgets/SearchField.h>
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
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace owl::nest {

namespace {

using data::table::CellValue;
using data::table::Column;
using data::table::ColumnType;
using data::table::DataTable;
using data::table::Row;

class ModifyDataTableCommand final : public UndoCommand<DataTable> {
public:
	ModifyDataTableCommand(DataTable iBefore, DataTable iAfter, std::string iDescription, const bool iMerge)
		: m_before{std::move(iBefore)}, m_after{std::move(iAfter)}, m_description{std::move(iDescription)},
		  m_merge{iMerge} {}

	void undo(DataTable& ioTarget) override { ioTarget = m_before; }

	void redo(DataTable& ioTarget) override { ioTarget = m_after; }

	[[nodiscard]] auto description() const -> std::string override { return m_description; }

	[[nodiscard]] auto typeId() const -> size_t override { return m_merge ? 1 : 0; }

	[[nodiscard]] auto mergeWith(const UndoCommand& iOther) -> bool override {
		const auto& other = static_cast<const ModifyDataTableCommand&>(iOther);
		if (other.m_description != m_description)
			return false;
		m_after = other.m_after;
		return true;
	}

private:
	DataTable m_before;
	DataTable m_after;
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

auto matches(const Row& iRow, const std::string_view iSearch) -> bool {
	if (iSearch.empty() || containsText(iRow.key, iSearch))
		return true;
	return std::ranges::any_of(iRow.cells, [&](const CellValue& iCell) -> bool {
		return containsText(data::table::formatValue(iCell), iSearch);
	});
}

auto joinChoices(const std::vector<std::string>& iChoices) -> std::string {
	std::string text;
	for (const auto& choice: iChoices) text += text.empty() ? choice : "," + choice;
	return text;
}

auto typeGlyph(const ColumnType iType) -> const char* {
	switch (iType) {
		case ColumnType::Integer:
			return gui::icon::hash;
		case ColumnType::Float:
			return gui::icon::sigma;
		case ColumnType::Bool:
			return gui::icon::toggleLeft;
		case ColumnType::Text:
			return gui::icon::type;
		case ColumnType::Enum:
			return gui::icon::list;
		case ColumnType::Asset:
			return gui::icon::file;
		case ColumnType::Reference:
			return gui::icon::link;
	}
	return gui::icon::type;
}

}// namespace

DataTableDocument::DataTableDocument() { m_undo.markSaved(); }

DataTableDocument::~DataTableDocument() = default;

auto DataTableDocument::title() const -> std::string {
	return m_path.empty() ? std::string{"Data Table"} : m_path.stem().string();
}

void DataTableDocument::onAttach(EditorLayer* iEditor) { mp_editorLayer = iEditor; }

void DataTableDocument::onDetach() { mp_editorLayer = nullptr; }

void DataTableDocument::onUpdate([[maybe_unused]] const core::Timestep& iTimeStep) { commitPreview(); }

void DataTableDocument::onEvent([[maybe_unused]] event::Event& ioEvent) {}

auto DataTableDocument::loadFromFile(const std::filesystem::path& iPath) -> bool {
	auto loaded = DataTable::loadFile(iPath);
	if (!loaded) {
		OWL_CORE_WARN("DataTableDocument: {} is not a data table.", iPath.string())
		return false;
	}
	m_table = std::move(*loaded);
	m_path = iPath;
	m_undo.clear();
	m_undo.markSaved();
	m_selection.clear();
	return true;
}

auto DataTableDocument::save() -> bool {
	if (m_path.empty())
		return false;
	return saveAs(m_path);
}

auto DataTableDocument::saveAs(const std::filesystem::path& iPath) -> bool {
	commitPreview(true);
	if (!m_table.saveToFile(iPath)) {
		OWL_CORE_ERROR("DataTableDocument: Failed to write '{}'.", iPath.string())
		return false;
	}
	m_path = iPath;
	m_undo.markSaved();
	// The scripts and the other tables read the saved file again.
	if (app::Application::instanced())
		app::Application::get().getEngineContext().getDataTables().clear();
	return true;
}

auto DataTableDocument::recoverySnapshot() const -> std::optional<std::string> { return m_table.serializeToString(); }

auto DataTableDocument::restoreRecoverySnapshot(const std::string& iSnapshot) -> bool {
	auto restored = DataTable::deserializeFromString(iSnapshot);
	if (!restored) {
		OWL_WARN("Recovery: Cannot restore the autosave of the data table.")
		return false;
	}
	m_table = std::move(*restored);
	m_undo.clear();
	m_undo.markUnsaved();
	setSelection(m_selection);
	return true;
}

void DataTableDocument::performUndo() {
	cancelPreview();
	if (!m_undo.canUndo())
		return;
	m_undo.undo(m_table);
	setSelection(m_selection);
}

void DataTableDocument::performRedo() {
	cancelPreview();
	if (!m_undo.canRedo())
		return;
	m_undo.redo(m_table);
	setSelection(m_selection);
}

auto DataTableDocument::commit(DataTable iNext, const std::string& iDescription, const bool iMerge) -> bool {
	if (iNext == m_table)
		return false;
	auto before = std::move(m_table);
	m_table = iNext;
	m_undo.push(mkUniq<ModifyDataTableCommand>(std::move(before), std::move(iNext), iDescription, iMerge));
	setSelection(m_selection);
	return true;
}

auto DataTableDocument::addRow(const std::string& iKey, const size_t iIndex) -> bool {
	auto next = m_table;
	const auto key = next.getUniqueKey(iKey);
	const auto at = std::min(iIndex, next.rows.size());
	if (!next.addRow(key, at))
		return false;
	if (!commit(std::move(next), "Add row", false))
		return false;
	setSelection({at});
	return true;
}

auto DataTableDocument::removeRows(const std::span<const size_t> iIndices) -> bool {
	const auto indices = sortedUnique(iIndices, m_table.rows.size());
	if (indices.empty())
		return false;
	auto next = m_table;
	for (const auto index: indices | std::views::reverse)
		next.rows.erase(next.rows.begin() + static_cast<std::ptrdiff_t>(index));
	if (!commit(std::move(next), indices.size() > 1 ? "Remove rows" : "Remove row", false))
		return false;
	m_selection.clear();
	return true;
}

auto DataTableDocument::duplicateRows(const std::span<const size_t> iIndices) -> bool {
	const auto indices = sortedUnique(iIndices, m_table.rows.size());
	if (indices.empty())
		return false;
	auto next = m_table;
	const auto insertAt = indices.back() + 1;
	std::vector<Row> copies;
	for (const auto index: indices) {
		auto copy = m_table.rows[index];
		for (uint32_t counter = 2;; ++counter) {
			copy.key = std::format("{}_{}", m_table.rows[index].key, counter);
			if (!next.findRow(copy.key) &&
				std::ranges::none_of(copies, [&](const Row& iOther) -> bool { return iOther.key == copy.key; }))
				break;
		}
		copies.push_back(std::move(copy));
	}
	next.rows.insert(next.rows.begin() + static_cast<std::ptrdiff_t>(insertAt), copies.begin(), copies.end());
	if (!commit(std::move(next), "Duplicate rows", false))
		return false;
	std::vector<size_t> selection(copies.size());
	for (size_t i = 0; i < copies.size(); ++i) selection[i] = insertAt + i;
	setSelection(std::move(selection));
	return true;
}

auto DataTableDocument::moveRows(const std::span<const size_t> iIndices, const bool iDown) -> bool {
	const auto indices = sortedUnique(iIndices, m_table.rows.size());
	if (indices.empty() || (iDown && indices.back() + 1 >= m_table.rows.size()) || (!iDown && indices.front() == 0))
		return false;
	auto next = m_table;
	std::vector<size_t> selection;
	if (iDown) {
		for (const auto index: indices | std::views::reverse) {
			std::swap(next.rows[index], next.rows[index + 1]);
			selection.push_back(index + 1);
		}
	} else {
		for (const auto index: indices) {
			std::swap(next.rows[index], next.rows[index - 1]);
			selection.push_back(index - 1);
		}
	}
	if (!commit(std::move(next), "Move rows", false))
		return false;
	setSelection(std::move(selection));
	return true;
}

auto DataTableDocument::setRowKey(const size_t iIndex, const std::string& iKey) -> bool {
	if (iIndex < m_table.rows.size() && m_table.rows[iIndex].key == iKey)
		return false;
	auto next = m_table;
	if (!next.setRowKey(iIndex, iKey))
		return false;
	return commit(std::move(next), "Rename row", false);
}

auto DataTableDocument::setCell(const size_t iRow, const size_t iColumn, const CellValue& iValue, const bool iMerge)
		-> bool {
	auto next = m_table;
	if (!next.setCell(iRow, iColumn, iValue))
		return false;
	return commit(std::move(next), "Edit cell", iMerge);
}

auto DataTableDocument::addColumn(const Column& iColumn, const size_t iIndex) -> bool {
	auto next = m_table;
	if (!next.addColumn(iColumn, iIndex))
		return false;
	return commit(std::move(next), "Add column", false);
}

auto DataTableDocument::removeColumn(const size_t iIndex) -> bool {
	auto next = m_table;
	if (!next.removeColumn(iIndex))
		return false;
	return commit(std::move(next), "Remove column", false);
}

auto DataTableDocument::moveColumn(const size_t iFrom, const size_t iTo) -> bool {
	auto next = m_table;
	if (!next.moveColumn(iFrom, iTo))
		return false;
	return commit(std::move(next), "Move column", false);
}

auto DataTableDocument::setColumn(const size_t iIndex, const Column& iColumn) -> bool {
	auto next = m_table;
	if (!next.setColumn(iIndex, iColumn))
		return false;
	return commit(std::move(next), "Edit column", false);
}

auto DataTableDocument::getReferenceKeys(const Column& iColumn) const -> std::vector<std::string> {
	std::vector<std::string> keys;
	const DataTable* source = &m_table;
	shared<const DataTable> other;
	std::optional<DataTable> loaded;
	if (!iColumn.table.empty()) {
		if (app::Application::instanced()) {
			other = app::Application::get().getEngineContext().getDataTables().get(iColumn.table);
			source = other.get();
		} else {
			loaded = DataTable::load(iColumn.table);
			source = loaded ? &*loaded : nullptr;
		}
	}
	if (source == nullptr)
		return keys;
	keys.reserve(source->rows.size());
	for (const auto& row: source->rows) keys.push_back(row.key);
	return keys;
}

void DataTableDocument::setSelection(std::vector<size_t> iIndices) {
	m_selection = sortedUnique(iIndices, m_table.rows.size());
}

auto DataTableDocument::runCommand(const std::string_view iName, commands::CommandArgs iArgs) -> bool {
	commitPreview(true);
	return sendCommand(iName, std::move(iArgs));
}

auto DataTableDocument::sendCommand(const std::string_view iName, commands::CommandArgs iArgs) const -> bool {
	if (mp_editorLayer == nullptr) {
		OWL_WARN("DataTableDocument: {} ignored, no editor to run it.", iName)
		return false;
	}
	iArgs.set("path", m_path.generic_string());
	return mp_editorLayer->execute(iName, iArgs, commands::CommandSource::Gesture).has_value();
}

auto DataTableDocument::runColumnCommand(const Column& iBefore, const Column& iAfter) -> bool {
	return runCommand("table.set_column", {{"column", iBefore.name},
										   {"name", iAfter.name},
										   {"type", std::string{data::table::getColumnTypeName(iAfter.type)}},
										   {"choices", joinChoices(iAfter.choices)},
										   {"table", iAfter.table}});
}

auto DataTableDocument::getKeys(const std::span<const size_t> iIndices) const -> std::string {
	std::string keys;
	for (const auto index: iIndices) {
		if (index >= m_table.rows.size())
			continue;
		if (!keys.empty())
			keys += ',';
		keys += m_table.rows[index].key;
	}
	return keys;
}

void DataTableDocument::preview(DataTable iNext) {
	if (!m_previewBefore.has_value())
		m_previewBefore = m_table;
	m_table = std::move(iNext);
}

void DataTableDocument::cancelPreview() {
	if (!m_previewBefore.has_value())
		return;
	m_table = std::move(*m_previewBefore);
	m_previewBefore.reset();
}

void DataTableDocument::commitPreview(const bool iForce) {
	if (!m_previewBefore.has_value() || (!iForce && ImGui::IsAnyItemActive()))
		return;
	const auto edited = std::exchange(m_table, std::move(*m_previewBefore));
	m_previewBefore.reset();
	if (edited.columns.size() != m_table.columns.size())
		return;
	for (size_t row = 0; row < std::min(edited.rows.size(), m_table.rows.size()); ++row) {
		for (size_t column = 0; column < m_table.columns.size(); ++column) {
			const bool same = data::table::isSameValue(edited.rows[row].cells[column], m_table.rows[row].cells[column]);
			if (!same)
				std::ignore = sendCommand("table.set_cell",
										  {{"key", m_table.rows[row].key},
										   {"column", m_table.columns[column].name},
										   {"value", data::table::formatValue(edited.rows[row].cells[column])}});
		}
	}
}

auto DataTableDocument::getProjectTables() const -> std::vector<std::string> {
	if (mp_editorLayer == nullptr || !mp_editorLayer->getProject().isLoaded())
		return {};
	return listAssets(mp_editorLayer->getProject().projectDirectory, DataTable::fileExtension());
}

void DataTableDocument::onImGuiRender() {
	const auto winTitle = documentWindowTitle(DocumentType::DataTable, title(), static_cast<uint64_t>(id()));
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
		renderToolbar();
		renderGrid();
	}
	ImGui::End();
}

void DataTableDocument::renderToolbar() {
	if (gui::widgets::iconButton("##addRow", {.glyph = gui::icon::plus,
											  .tooltip = "Add a row",
											  .description = "A row of default cells after the selection."}))
		std::ignore = runCommand("table.add_row",
								 m_selection.empty()
										 ? commands::CommandArgs{{"key", std::string{"row"}}}
										 : commands::CommandArgs{{"key", std::string{"row"}},
																 {"after", m_table.rows[m_selection.back()].key}});
	ImGui::SameLine();
	const bool none = m_selection.empty();
	if (gui::widgets::iconButton("##copyRows", {.glyph = gui::icon::copy,
												.tooltip = "Duplicate",
												.shortcut = "Ctrl+D",
												.description = "Copy the selected rows under new keys.",
												.disabled = none}))
		std::ignore = runCommand("table.duplicate_rows", {{"keys", getKeys(m_selection)}});
	ImGui::SameLine();
	if (gui::widgets::iconButton("##upRows", {.glyph = gui::icon::arrowUp,
											  .tooltip = "Move up",
											  .shortcut = "Alt+Up",
											  .description = "Move the selected rows one place up.",
											  .disabled = none}))
		std::ignore = runCommand("table.move_rows", {{"keys", getKeys(m_selection)}, {"down", false}});
	ImGui::SameLine();
	if (gui::widgets::iconButton("##downRows", {.glyph = gui::icon::arrowDown,
												.tooltip = "Move down",
												.shortcut = "Alt+Down",
												.description = "Move the selected rows one place down.",
												.disabled = none}))
		std::ignore = runCommand("table.move_rows", {{"keys", getKeys(m_selection)}, {"down", true}});
	ImGui::SameLine();
	if (gui::widgets::iconButton("##removeRows", {.glyph = gui::icon::trash2,
												  .tooltip = "Remove",
												  .shortcut = "Delete",
												  .description = "Remove the selected rows.",
												  .disabled = none}))
		std::ignore = runCommand("table.remove_rows", {{"keys", getKeys(m_selection)}});
	ImGui::SameLine();
	ImGui::TextDisabled("|");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(140.f);
	const bool entered = ImGui::InputTextWithHint("##newColumn", "new column", &m_newColumnName,
												  ImGuiInputTextFlags_EnterReturnsTrue);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(110.f);
	if (gui::widgets::beginCombo("##newColumnType",
								 std::string{data::table::getColumnTypeName(m_newColumnType)}.c_str())) {
		for (const auto type: data::table::getColumnTypes())
			if (ImGui::Selectable(std::format("{} {}", typeGlyph(type), data::table::getColumnTypeName(type)).c_str(),
								  type == m_newColumnType))
				m_newColumnType = type;
		ImGui::EndCombo();
	}
	gui::widgets::itemTooltip("Type of the cells of the new column.");
	ImGui::SameLine();
	if ((gui::widgets::iconButton("##addColumn", {.glyph = gui::icon::columns3,
												  .tooltip = "Add a column",
												  .description = "Type its name first; `key` names the rows.",
												  .disabled = m_newColumnName.empty()}) ||
		 entered) &&
		runCommand("table.add_column", {{"name", m_table.getUniqueColumnName(m_newColumnName)},
										{"type", std::string{data::table::getColumnTypeName(m_newColumnType)}},
										{"choices", std::string{"a,b"}}}))
		m_newColumnName.clear();
	ImGui::SameLine();
	ImGui::SetNextItemWidth(200.f);
	std::ignore = gui::widgets::searchField("##search", m_search, "Search keys and cells");
	ImGui::SameLine();
	ImGui::TextDisabled("%zu rows, %zu columns", m_table.rows.size(), m_table.columns.size());
}

void DataTableDocument::renderColumnHeader(const size_t iIndex) {
	const auto column = m_table.columns[iIndex];
	ImGui::PushID(static_cast<int>(iIndex));
	auto edited = column;
	std::optional<Column> changed;
	ImGui::TextUnformatted(typeGlyph(column.type));
	ImGui::SameLine();
	ImGui::SetNextItemWidth(-ImGui::GetFrameHeight() * 3.3f);
	if (ImGui::InputText("##name", &edited.name, ImGuiInputTextFlags_EnterReturnsTrue))
		changed = edited;
	gui::widgets::itemTooltip("Name of the column, read by `datatable.get`; Enter renames it.");
	ImGui::SameLine();
	if (gui::widgets::iconButton("##left",
								 {.glyph = gui::icon::chevronLeft, .tooltip = "Move left", .disabled = iIndex == 0}))
		std::ignore = runCommand("table.move_column", {{"column", column.name}, {"right", false}});
	ImGui::SameLine();
	if (gui::widgets::iconButton("##right", {.glyph = gui::icon::chevronRight,
											 .tooltip = "Move right",
											 .disabled = iIndex + 1 >= m_table.columns.size()}))
		std::ignore = runCommand("table.move_column", {{"column", column.name}, {"right", true}});
	ImGui::SameLine();
	bool removed = false;
	if (gui::widgets::iconButton("##removeColumn", {.glyph = gui::icon::x,
													.tooltip = "Remove the column",
													.description = "Its cells go with it; undo brings them back."}))
		removed = true;
	ImGui::SetNextItemWidth(-1.f);
	if (gui::widgets::beginCombo("##type", std::string{data::table::getColumnTypeName(column.type)}.c_str())) {
		for (const auto type: data::table::getColumnTypes())
			if (ImGui::Selectable(std::format("{} {}", typeGlyph(type), data::table::getColumnTypeName(type)).c_str(),
								  type == column.type)) {
				edited.type = type;
				changed = edited;
			}
		ImGui::EndCombo();
	}
	gui::widgets::itemTooltip("Type of the cells: changing it converts them (a value that does not fit becomes the "
							  "default).");
	if (column.type == ColumnType::Enum) {
		auto choices = joinChoices(column.choices);
		ImGui::SetNextItemWidth(-1.f);
		if (ImGui::InputTextWithHint("##choices", "choices: a,b,c", &choices, ImGuiInputTextFlags_EnterReturnsTrue)) {
			edited.choices.clear();
			for (const auto part: choices | std::views::split(','))
				if (const std::string item{part.begin(), part.end()}; !item.empty())
					edited.choices.push_back(item);
			changed = edited;
		}
		gui::widgets::itemTooltip("Values a cell may take, separated by commas; Enter applies them.");
	}
	if (column.type == ColumnType::Reference) {
		ImGui::SetNextItemWidth(-1.f);
		if (gui::widgets::beginCombo("##table", column.table.empty() ? "(this table)" : column.table.c_str())) {
			if (ImGui::Selectable("(this table)", column.table.empty())) {
				edited.table.clear();
				changed = edited;
			}
			for (const auto& table: getProjectTables())
				if (ImGui::Selectable(table.c_str(), table == column.table)) {
					edited.table = table;
					changed = edited;
				}
			ImGui::EndCombo();
		}
		if (std::filesystem::path dropped; gui::widgets::assetDropTarget(gui::widgets::AssetKind::DataTable, dropped)) {
			edited.table = dropped.generic_string();
			changed = edited;
		}
		gui::widgets::itemTooltip("Table whose rows the cells name; drop a `.owltable` here.");
	}
	ImGui::PopID();
	if (removed)
		std::ignore = runCommand("table.remove_column", {{"column", column.name}});
	else if (changed.has_value())
		std::ignore = runColumnCommand(m_table.columns[iIndex], *changed);
}

void DataTableDocument::renderCell(const size_t iRow, const size_t iColumn) {
	const auto column = m_table.columns[iColumn];
	const auto cell = m_table.rows[iRow].cells[iColumn];
	const auto key = m_table.rows[iRow].key;
	const auto previewCell = [&](const CellValue& iValue) -> void {
		auto next = m_table;
		std::ignore = next.setCell(iRow, iColumn, iValue);
		preview(std::move(next));
	};
	const auto setNow = [&](const CellValue& iValue) -> void {
		std::ignore = runCommand("table.set_cell",
								 {{"key", key}, {"column", column.name}, {"value", data::table::formatValue(iValue)}});
	};
	ImGui::SetNextItemWidth(-1.f);
	switch (column.type) {
		case ColumnType::Integer:
			{
				auto value = std::get<int64_t>(cell);
				if (ImGui::DragScalar("##cell", ImGuiDataType_S64, &value, 0.2f))
					previewCell(value);
				break;
			}
		case ColumnType::Float:
			{
				auto value = std::get<double>(cell);
				if (ImGui::DragScalar("##cell", ImGuiDataType_Double, &value, 0.01f, nullptr, nullptr, "%.3f"))
					previewCell(value);
				break;
			}
		case ColumnType::Bool:
			{
				auto value = std::get<bool>(cell);
				if (gui::widgets::toggle("##cell", &value))
					setNow(value);
				break;
			}
		case ColumnType::Text:
			{
				auto value = std::get<std::string>(cell);
				if (ImGui::InputText("##cell", &value))
					previewCell(value);
				break;
			}
		case ColumnType::Enum:
			{
				const auto& value = std::get<std::string>(cell);
				if (gui::widgets::beginCombo("##cell", value.c_str())) {
					for (const auto& choice: column.choices)
						if (ImGui::Selectable(choice.c_str(), choice == value))
							setNow(choice);
					ImGui::EndCombo();
				}
				break;
			}
		case ColumnType::Asset:
			{
				auto value = std::get<std::string>(cell);
				if (ImGui::InputTextWithHint("##cell", "drop an asset", &value))
					previewCell(value);
				if (std::filesystem::path dropped; gui::widgets::assetDropTarget(gui::widgets::AssetKind::Any, dropped))
					setNow(dropped.generic_string());
				break;
			}
		case ColumnType::Reference:
			{
				const auto& value = std::get<std::string>(cell);
				const auto keys = getReferenceKeys(column);
				if (!value.empty() && !std::ranges::contains(keys, value))
					ImGui::TableSetBgColor(
							ImGuiTableBgTarget_CellBg,
							ImGui::GetColorU32(gui::vec(gui::Theme::withAlpha(gui::UiLayer::getTheme().error, 0.25f))));
				if (gui::widgets::beginCombo("##cell", value.empty() ? "(none)" : value.c_str())) {
					if (ImGui::Selectable("(none)", value.empty()))
						setNow(std::string{});
					for (const auto& target: keys)
						if (ImGui::Selectable(target.c_str(), target == value))
							setNow(target);
					ImGui::EndCombo();
				}
				gui::widgets::itemTooltip("Row of the referenced table; red when its key is unknown there.");
				break;
			}
	}
}

void DataTableDocument::clickRow(const size_t iIndex) {
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

void DataTableDocument::renderGrid() {
	if (m_table.columns.empty() && m_table.rows.empty()) {
		std::ignore = gui::widgets::emptyState(gui::icon::rows3, "Empty table",
											   "Add a column (type its name above), then rows.");
		return;
	}
	const auto columns = static_cast<int>(m_table.columns.size()) + 1;
	if (!ImGui::BeginTable("##grid", columns,
						   ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
								   ImGuiTableFlags_ScrollX | ImGuiTableFlags_Resizable))
		return;
	ImGui::TableSetupScrollFreeze(1, 1);
	ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed, 160.f);
	for (const auto& column: m_table.columns)
		ImGui::TableSetupColumn(column.name.c_str(), ImGuiTableColumnFlags_WidthFixed, 190.f);
	ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
	ImGui::TableNextColumn();
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted("key");
	gui::widgets::itemTooltip("Key of each row, read by `datatable.get` and named by the references.");
	for (size_t column = 0; column < m_table.columns.size(); ++column) {
		ImGui::TableNextColumn();
		renderColumnHeader(column);
		if (column >= m_table.columns.size())
			break;
	}
	std::optional<std::pair<size_t, std::string>> renamed;
	for (size_t row = 0; row < m_table.rows.size(); ++row) {
		if (!matches(m_table.rows[row], m_search))
			continue;
		ImGui::PushID(static_cast<int>(row));
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		const bool selected = std::ranges::contains(m_selection, row);
		if (ImGui::Selectable("##row", selected, ImGuiSelectableFlags_AllowOverlap, {12.f, 0.f}))
			clickRow(row);
		ImGui::SameLine();
		auto key = m_table.rows[row].key;
		ImGui::SetNextItemWidth(-1.f);
		if (ImGui::InputText("##key", &key, ImGuiInputTextFlags_EnterReturnsTrue))
			renamed = {row, key};
		if (ImGui::IsItemActivated() && !selected)
			setSelection({row});
		for (size_t column = 0; column < m_table.columns.size() && row < m_table.rows.size(); ++column) {
			ImGui::TableNextColumn();
			ImGui::PushID(static_cast<int>(column));
			renderCell(row, column);
			ImGui::PopID();
		}
		ImGui::PopID();
	}
	ImGui::EndTable();
	if (renamed.has_value() && renamed->first < m_table.rows.size())
		std::ignore = runCommand("table.rename_row",
								 {{"key", m_table.rows[renamed->first].key}, {"new_key", renamed->second}});
	const auto& io = ImGui::GetIO();
	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !io.WantTextInput && !m_selection.empty()) {
		if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
			std::ignore = runCommand("table.remove_rows", {{"keys", getKeys(m_selection)}});
		else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false))
			std::ignore = runCommand("table.duplicate_rows", {{"keys", getKeys(m_selection)}});
		else if (io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_UpArrow, false))
			std::ignore = runCommand("table.move_rows", {{"keys", getKeys(m_selection)}, {"down", false}});
		else if (io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_DownArrow, false))
			std::ignore = runCommand("table.move_rows", {{"keys", getKeys(m_selection)}, {"down", true}});
	}
	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !io.WantTextInput && io.KeyCtrl &&
		ImGui::IsKeyPressed(ImGuiKey_A, false)) {
		std::vector<size_t> all;
		for (size_t i = 0; i < m_table.rows.size(); ++i)
			if (matches(m_table.rows[i], m_search))
				all.push_back(i);
		setSelection(std::move(all));
	}
}

}// namespace owl::nest
