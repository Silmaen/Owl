/**
 * @file DataTableDocument.h
 * @author Silmaen
 * @date 30/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Document.h"
#include "UndoManager.h"
#include "commands/CommandRegistry.h"
#include "commands/DataTableCommands.h"

#include <data/table/DataTable.h>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace owl::nest {

/// Undo stack of a data table.
using DataTableUndoManager = UndoManager<data::table::DataTable>;

/**
 * @brief
 *  A `.owltable` open in a tab: a grid of typed cells, one row per record, one column per field.
 *
 * Every gesture of the grid runs a `table.*` command (journaled, undoable); the typing of a number or a text is
 * previewed and becomes one `table.set_cell` when the field is left.
 */
class DataTableDocument final : public Document, public commands::DataTableEditor {
public:
	DataTableDocument(const DataTableDocument&) = delete;

	DataTableDocument(DataTableDocument&&) = delete;

	auto operator=(const DataTableDocument&) -> DataTableDocument& = delete;

	auto operator=(DataTableDocument&&) -> DataTableDocument& = delete;

	/**
	 * @brief
	 *  Build an empty document.
	 */
	DataTableDocument();

	/**
	 * @brief
	 *  Destructor.
	 */
	~DataTableDocument() override;

	// --- Document interface --------------------------------------------------

	/**
	 * @brief
	 *  Type of the document.
	 * @return `DocumentType::DataTable`.
	 */
	[[nodiscard]] auto type() const -> DocumentType override { return DocumentType::DataTable; }

	/**
	 * @brief
	 *  Name shown in the tab.
	 * @return The file name, without its extension.
	 */
	[[nodiscard]] auto title() const -> std::string override;

	/**
	 * @brief
	 *  File of the document.
	 * @return The `.owltable` path.
	 */
	[[nodiscard]] auto filePath() const -> std::filesystem::path override { return m_path; }

	/**
	 * @brief
	 *  Whether the table changed since it was saved.
	 * @return True when dirty.
	 */
	[[nodiscard]] auto isDirty() const -> bool override { return m_undo.isDirty(); }

	/**
	 * @brief
	 *  Attach to the editor.
	 * @param[in] iEditor The editor.
	 */
	void onAttach(EditorLayer* iEditor) override;

	/**
	 * @brief
	 *  Detach from the editor.
	 */
	void onDetach() override;

	/**
	 * @brief
	 *  Per-frame update: commit the typing once the field is left.
	 * @param[in] iTimeStep The frame time.
	 */
	void onUpdate(const core::Timestep& iTimeStep) override;

	/**
	 * @brief
	 *  Event handler (unused).
	 * @param[in,out] ioEvent The event.
	 */
	void onEvent(event::Event& ioEvent) override;

	/**
	 * @brief
	 *  Draw the tab: the toolbar and the grid.
	 */
	void onImGuiRender() override;

	/**
	 * @brief
	 *  Save the table to its file.
	 * @return True when written.
	 */
	auto save() -> bool override;

	/**
	 * @brief
	 *  Save the table to another file, which becomes its file.
	 * @param[in] iPath The file.
	 * @return True when written.
	 */
	auto saveAs(const std::filesystem::path& iPath) -> bool override;

	/**
	 * @brief
	 *  The table as text, for the autosave.
	 * @return The YAML of the table.
	 */
	[[nodiscard]] auto recoverySnapshot() const -> std::optional<std::string> override;

	/**
	 * @brief
	 *  Restore an autosave.
	 * @param[in] iSnapshot The YAML of the table.
	 * @return True when restored.
	 */
	auto restoreRecoverySnapshot(const std::string& iSnapshot) -> bool override;

	/**
	 * @brief
	 *  Placeholder scene history (the table has its own).
	 * @return An empty history.
	 */
	[[nodiscard]] auto undoManager() -> SceneUndoManager& override { return m_emptySceneUndo; }

	/**
	 * @brief
	 *  Placeholder scene history (the table has its own).
	 * @return An empty history.
	 */
	[[nodiscard]] auto undoManager() const -> const SceneUndoManager& override { return m_emptySceneUndo; }

	/**
	 * @brief
	 *  Whether an edit can be undone.
	 * @return True when the history has one.
	 */
	[[nodiscard]] auto canUndo() const -> bool override { return m_undo.canUndo(); }

	/**
	 * @brief
	 *  Whether an edit can be redone.
	 * @return True when the history has one.
	 */
	[[nodiscard]] auto canRedo() const -> bool override { return m_undo.canRedo(); }

	/**
	 * @brief
	 *  Undo the last edit.
	 */
	void performUndo() override;

	/**
	 * @brief
	 *  Redo the last edit undone.
	 */
	void performRedo() override;

	// --- Table API -----------------------------------------------------------

	/**
	 * @brief
	 *  The table as it is edited.
	 * @return The table.
	 */
	[[nodiscard]] auto table() const -> const data::table::DataTable& override { return m_table; }

	/**
	 * @brief
	 *  Read a table file.
	 * @param[in] iPath The `.owltable` file.
	 * @return False when it is not a data table.
	 */
	auto loadFromFile(const std::filesystem::path& iPath) -> bool;

	auto addRow(const std::string& iKey, size_t iIndex) -> bool override;

	auto removeRows(std::span<const size_t> iIndices) -> bool override;

	auto duplicateRows(std::span<const size_t> iIndices) -> bool override;

	auto moveRows(std::span<const size_t> iIndices, bool iDown) -> bool override;

	auto setRowKey(size_t iIndex, const std::string& iKey) -> bool override;

	auto setCell(size_t iRow, size_t iColumn, const data::table::CellValue& iValue, bool iMerge = false)
			-> bool override;

	auto addColumn(const data::table::Column& iColumn, size_t iIndex) -> bool override;

	auto removeColumn(size_t iIndex) -> bool override;

	auto moveColumn(size_t iFrom, size_t iTo) -> bool override;

	auto setColumn(size_t iIndex, const data::table::Column& iColumn) -> bool override;

	/**
	 * @brief
	 *  Keys of the rows a Reference column may point at: the rows of its table (this one when it names none).
	 * @param[in] iColumn The column.
	 * @return The keys, empty when the table cannot be read.
	 */
	[[nodiscard]] auto getReferenceKeys(const data::table::Column& iColumn) const -> std::vector<std::string>;

	// --- Selection -----------------------------------------------------------

	/**
	 * @brief
	 *  The selected rows.
	 * @return Their indices, in increasing order.
	 */
	[[nodiscard]] auto getSelection() const -> const std::vector<size_t>& { return m_selection; }

	/**
	 * @brief
	 *  Select rows.
	 * @param[in] iIndices Their indices (the unknown ones are dropped).
	 */
	void setSelection(std::vector<size_t> iIndices);

	// --- Tab -----------------------------------------------------------------

	/**
	 * @brief
	 *  Whether the tab is open.
	 * @return False once the user closed it.
	 */
	[[nodiscard]] auto isOpen() const -> bool { return m_pOpen; }

	/**
	 * @brief
	 *  Open or close the tab.
	 * @param[in] iOpen Whether it is open.
	 */
	void setOpen(const bool iOpen) { m_pOpen = iOpen; }

private:
	/**
	 * @brief
	 *  Replace the table by an edited copy, as one undo entry.
	 * @param[in] iNext The edited table.
	 * @param[in] iDescription Undo entry name.
	 * @param[in] iMerge Whether the entry merges with the previous one of the same name.
	 * @return False when nothing changed.
	 */
	auto commit(data::table::DataTable iNext, const std::string& iDescription, bool iMerge) -> bool;

	/**
	 * @brief
	 *  Commit the pending typing, then run a command on this table.
	 * @param[in] iName Command name.
	 * @param[in] iArgs Its arguments (`path` is added).
	 * @return True when it ran.
	 */
	auto runCommand(std::string_view iName, commands::CommandArgs iArgs) -> bool;

	/**
	 * @brief
	 *  Run a command on this table.
	 * @param[in] iName Command name.
	 * @param[in] iArgs Its arguments (`path` is added).
	 * @return True when it ran.
	 */
	[[nodiscard]] auto sendCommand(std::string_view iName, commands::CommandArgs iArgs) const -> bool;

	/**
	 * @brief
	 *  Run `table.set_column` for a column changed in the header.
	 * @param[in] iBefore The column as it was.
	 * @param[in] iAfter The column as edited.
	 * @return True when it ran.
	 */
	auto runColumnCommand(const data::table::Column& iBefore, const data::table::Column& iAfter) -> bool;

	/**
	 * @brief
	 *  Keys of rows, as the `keys` argument of the commands.
	 * @param[in] iIndices Indices of the rows.
	 * @return The keys separated by commas.
	 */
	[[nodiscard]] auto getKeys(std::span<const size_t> iIndices) const -> std::string;

	/**
	 * @brief
	 *  Show a typed value before it is committed.
	 * @param[in] iNext The table with the value.
	 */
	void preview(data::table::DataTable iNext);

	/**
	 * @brief
	 *  Turn the previewed values into `table.set_cell` commands, once no field is active.
	 * @param[in] iForce Commit even while a field is active.
	 */
	void commitPreview(bool iForce = false);

	/**
	 * @brief
	 *  Drop the previewed values.
	 */
	void cancelPreview();

	/**
	 * @brief
	 *  Draw the toolbar: rows, columns and search.
	 */
	void renderToolbar();

	/**
	 * @brief
	 *  Draw the header of a column: its name, type and type settings.
	 * @param[in] iIndex Index of the column.
	 */
	void renderColumnHeader(size_t iIndex);

	/**
	 * @brief
	 *  Draw the grid.
	 */
	void renderGrid();

	/**
	 * @brief
	 *  Draw the widget of a cell.
	 * @param[in] iRow Index of the row.
	 * @param[in] iColumn Index of the column.
	 */
	void renderCell(size_t iRow, size_t iColumn);

	/**
	 * @brief
	 *  Select a row by a click: Ctrl toggles, Shift extends.
	 * @param[in] iIndex Index of the row.
	 */
	void clickRow(size_t iIndex);

	/**
	 * @brief
	 *  The `.owltable` files of the project, for the table of a Reference column.
	 * @return Their asset paths.
	 */
	[[nodiscard]] auto getProjectTables() const -> std::vector<std::string>;

	/// Disk path of the `.owltable`.
	std::filesystem::path m_path;
	/// Table being edited.
	data::table::DataTable m_table;
	/// Undo stack.
	DataTableUndoManager m_undo;
	/// Editor back-pointer.
	EditorLayer* mp_editorLayer{nullptr};
	/// Selected rows, in increasing order.
	std::vector<size_t> m_selection;
	/// Text a shown key or cell contains.
	std::string m_search;
	/// Name typed for a new column.
	std::string m_newColumnName;
	/// Type of a new column.
	data::table::ColumnType m_newColumnType{data::table::ColumnType::Text};
	/// The table before the typing being previewed, while there is some.
	std::optional<data::table::DataTable> m_previewBefore;
	/// Tracks the ImGui "Open" state of the tab.
	bool m_pOpen{true};
	/// Whether the tab was focused last frame.
	bool m_wasFocused{false};
	/// Placeholder required by `Document::undoManager()`.
	SceneUndoManager m_emptySceneUndo;
};

}// namespace owl::nest
