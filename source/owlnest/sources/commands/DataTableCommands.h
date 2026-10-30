/**
 * @file DataTableCommands.h
 * @author Silmaen
 * @date 30/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <data/table/DataTable.h>

#include <cstddef>
#include <span>
#include <string>

namespace owl::nest::commands {

class CommandRegistry;

/**
 * @brief
 *  A data table open in an editor tab: the `table.*` commands edit it through its own undo history.
 */
class DataTableEditor {
public:
	DataTableEditor() = default;

	DataTableEditor(const DataTableEditor&) = default;

	DataTableEditor(DataTableEditor&&) = default;

	auto operator=(const DataTableEditor&) -> DataTableEditor& = default;

	auto operator=(DataTableEditor&&) -> DataTableEditor& = default;

	/**
	 * @brief
	 *  Destructor.
	 */
	virtual ~DataTableEditor();

	/**
	 * @brief
	 *  The table as it is edited.
	 * @return The table.
	 */
	[[nodiscard]] virtual auto table() const -> const data::table::DataTable& = 0;

	/**
	 * @brief
	 *  Insert a row of default cells.
	 * @param[in] iKey Its key; made unique when taken.
	 * @param[in] iIndex Where it goes (past the end: appended).
	 * @return True when the row was added.
	 */
	virtual auto addRow(const std::string& iKey, size_t iIndex) -> bool = 0;

	/**
	 * @brief
	 *  Remove rows.
	 * @param[in] iIndices Indices of the rows.
	 * @return True when one was removed.
	 */
	virtual auto removeRows(std::span<const size_t> iIndices) -> bool = 0;

	/**
	 * @brief
	 *  Copy rows after the last of them, under unique keys.
	 * @param[in] iIndices Indices of the rows.
	 * @return True when one was copied.
	 */
	virtual auto duplicateRows(std::span<const size_t> iIndices) -> bool = 0;

	/**
	 * @brief
	 *  Move rows one place.
	 * @param[in] iIndices Indices of the rows.
	 * @param[in] iDown Whether they move down.
	 * @return True when they moved.
	 */
	virtual auto moveRows(std::span<const size_t> iIndices, bool iDown) -> bool = 0;

	/**
	 * @brief
	 *  Rename a row.
	 * @param[in] iIndex Index of the row.
	 * @param[in] iKey The new key, not empty and not taken.
	 * @return True when it changed.
	 */
	virtual auto setRowKey(size_t iIndex, const std::string& iKey) -> bool = 0;

	/**
	 * @brief
	 *  Set a cell, converted to the type of its column.
	 * @param[in] iRow Index of the row.
	 * @param[in] iColumn Index of the column.
	 * @param[in] iValue The value.
	 * @param[in] iMerge Whether the edit merges with the previous one.
	 * @return True when it changed.
	 */
	virtual auto setCell(size_t iRow, size_t iColumn, const data::table::CellValue& iValue, bool iMerge = false)
			-> bool = 0;

	/**
	 * @brief
	 *  Insert a column of default cells.
	 * @param[in] iColumn The column; its name must be free.
	 * @param[in] iIndex Where it goes (past the end: appended).
	 * @return True when it was added.
	 */
	virtual auto addColumn(const data::table::Column& iColumn, size_t iIndex) -> bool = 0;

	/**
	 * @brief
	 *  Remove a column and its cells.
	 * @param[in] iIndex Index of the column.
	 * @return True when it was removed.
	 */
	virtual auto removeColumn(size_t iIndex) -> bool = 0;

	/**
	 * @brief
	 *  Move a column and its cells.
	 * @param[in] iFrom Index of the column.
	 * @param[in] iTo Its new index.
	 * @return True when it moved.
	 */
	virtual auto moveColumn(size_t iFrom, size_t iTo) -> bool = 0;

	/**
	 * @brief
	 *  Change a column (name, type, choices, target table), converting its cells.
	 * @param[in] iIndex Index of the column.
	 * @param[in] iColumn The new definition.
	 * @return True when it changed.
	 */
	virtual auto setColumn(size_t iIndex, const data::table::Column& iColumn) -> bool = 0;
};

/**
 * @brief
 *  Register the `table.*` commands: create a data table and edit the rows, the columns and the cells of an open one.
 * @param[in,out] ioRegistry The registry.
 */
void addDataTableCommands(CommandRegistry& ioRegistry);

}// namespace owl::nest::commands
