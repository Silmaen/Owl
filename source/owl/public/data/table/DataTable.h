/**
 * @file DataTable.h
 * @author Silmaen
 * @date 30/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/FormatVersion.h"
#include "core/Macros.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace owl::data::table {

/**
 * @brief
 *  Type of the cells of a column.
 */
enum struct ColumnType : uint8_t {
	Integer,///< A whole number.
	Float,///< A real number.
	Bool,///< True or false.
	Text,///< A string.
	Enum,///< One of the column's choices.
	Asset,///< The path of an asset (a texture, a sound, a prefab...), stored as a renamable reference.
	Reference,///< The key of a row of another table (or of the same one).
};

/**
 * @brief
 *  Value of a cell: an integer (Integer), a number (Float), a flag (Bool) or a text (Text, Enum, Asset, Reference).
 */
using CellValue = std::variant<int64_t, double, bool, std::string>;

/**
 * @brief
 *  A column of a data table: its name, the type of its cells and what that type needs.
 */
struct OWL_API Column {
	/// Name the scripts read the cells by, unique in the table (`key` is reserved for the row keys).
	std::string name;
	/// Type of the cells.
	ColumnType type{ColumnType::Text};
	/// Allowed values of an Enum column.
	std::vector<std::string> choices;
	/// Asset path of the table a Reference column points into (empty: the table itself).
	std::string table;

	/**
	 * @brief
	 *  Compare two columns field by field.
	 * @param[in] iOther The other column.
	 * @return True when every field is equal.
	 */
	auto operator==(const Column& iOther) const -> bool = default;
};

/**
 * @brief
 *  A record of a data table: its key and one cell per column.
 */
struct OWL_API Row {
	/// Key the scripts and the references name the row by, unique in the table.
	std::string key;
	/// One value per column, in the order of the columns.
	std::vector<CellValue> cells;

	OWL_DIAG_PUSH
	OWL_DIAG_DISABLE_CLANG("-Wfloat-equal")
	/**
	 * @brief
	 *  Compare two rows field by field.
	 * @param[in] iOther The other row.
	 * @return True when every field is equal.
	 */
	auto operator==(const Row& iOther) const -> bool = default;
	OWL_DIAG_POP
};

/**
 * @brief
 *  A `.owltable` asset: typed records a game reads by key (the items of a shop, the goods of a market, the waves of a
 *  level), edited in a grid in Owl Nest and read from Lua (`datatable.*`).
 *
 * Every row holds one cell per column, of the column's type: the edits keep it so (a changed type converts the cells,
 * a value that does not fit becomes the default of the type).
 */
struct OWL_API DataTable {
	/// Columns, in the order of the grid.
	std::vector<Column> columns;
	/// Rows, in the order of the grid.
	std::vector<Row> rows;

	/**
	 * @brief
	 *  Compare two tables.
	 * @param[in] iOther The other table.
	 * @return True when they hold the same columns and rows.
	 */
	auto operator==(const DataTable& iOther) const -> bool = default;

	/**
	 * @brief
	 *  Extension of the table files.
	 * @return `.owltable`.
	 */
	static auto fileExtension() noexcept -> const char* { return ".owltable"; }

	/**
	 * @brief
	 *  Check whether a file is a data table by its extension.
	 * @param[in] iPath The file.
	 * @return True for a `.owltable`.
	 */
	[[nodiscard]] static auto isTable(const std::filesystem::path& iPath) -> bool;

	/**
	 * @brief
	 *  Version descriptor of the file format.
	 * @return The format.
	 */
	[[nodiscard]] static auto format() -> const core::DocumentFormat&;

	/**
	 * @brief
	 *  Write the table as YAML.
	 * @return The document.
	 */
	[[nodiscard]] auto serializeToString() const -> std::string;

	/**
	 * @brief
	 *  Read a table from YAML.
	 * @param[in] iYaml The document.
	 * @return The table, or nothing when the text is not a data table.
	 */
	[[nodiscard]] static auto deserializeFromString(std::string_view iYaml) -> std::optional<DataTable>;

	/**
	 * @brief
	 *  Write the table to a file (atomically).
	 * @param[in] iPath The file.
	 * @return True when written.
	 */
	[[nodiscard]] auto saveToFile(const std::filesystem::path& iPath) const -> bool;

	/**
	 * @brief
	 *  Read a table file from the disk.
	 * @param[in] iPath The file.
	 * @return The table, or nothing when it cannot be read.
	 */
	[[nodiscard]] static auto loadFile(const std::filesystem::path& iPath) -> std::optional<DataTable>;

	/**
	 * @brief
	 *  Read a table asset from the open pack, then the asset folders.
	 * @param[in] iAsset Asset path or `gid:` reference of the table.
	 * @return The table, or nothing when it cannot be read.
	 */
	[[nodiscard]] static auto load(const std::filesystem::path& iAsset) -> std::optional<DataTable>;

	/**
	 * @brief
	 *  Find a column by name.
	 * @param[in] iName The column name.
	 * @return Its index, or nothing.
	 */
	[[nodiscard]] auto findColumn(std::string_view iName) const -> std::optional<size_t>;

	/**
	 * @brief
	 *  Find a row by key.
	 * @param[in] iKey The row key.
	 * @return Its index, or nothing.
	 */
	[[nodiscard]] auto findRow(std::string_view iKey) const -> std::optional<size_t>;

	/**
	 * @brief
	 *  A cell found by row key and column name.
	 * @param[in] iKey The row key.
	 * @param[in] iColumn The column name.
	 * @return The cell, or null when the row or the column does not exist.
	 */
	[[nodiscard]] auto getCell(std::string_view iKey, std::string_view iColumn) const -> const CellValue*;

	/**
	 * @brief
	 *  A free row key: the base, or the base and a number.
	 * @param[in] iBase The wished key (`row` when empty).
	 * @return A key no row has.
	 */
	[[nodiscard]] auto getUniqueKey(std::string_view iBase) const -> std::string;

	/**
	 * @brief
	 *  A free column name: the base, or the base and a number.
	 * @param[in] iBase The wished name (`column` when empty).
	 * @return A name no column has, never `key`.
	 */
	[[nodiscard]] auto getUniqueColumnName(std::string_view iBase) const -> std::string;

	/**
	 * @brief
	 *  Append or insert a row of default cells.
	 * @param[in] iKey Its key, not empty and not taken.
	 * @param[in] iIndex Where it goes (past the end: appended).
	 * @return False when the key is empty or taken.
	 */
	auto addRow(const std::string& iKey, size_t iIndex) -> bool;

	/**
	 * @brief
	 *  Rename a row.
	 * @param[in] iIndex Index of the row.
	 * @param[in] iKey The new key, not empty and not taken.
	 * @return False when the row does not exist or the key cannot be used.
	 */
	auto setRowKey(size_t iIndex, const std::string& iKey) -> bool;

	/**
	 * @brief
	 *  Add a column of default cells.
	 * @param[in] iColumn The column; its name must be free and not `key`.
	 * @param[in] iIndex Where it goes (past the end: appended).
	 * @return False when the name cannot be used.
	 */
	auto addColumn(Column iColumn, size_t iIndex) -> bool;

	/**
	 * @brief
	 *  Remove a column and its cells.
	 * @param[in] iIndex Index of the column.
	 * @return False when the column does not exist.
	 */
	auto removeColumn(size_t iIndex) -> bool;

	/**
	 * @brief
	 *  Move a column and its cells.
	 * @param[in] iFrom Index of the column.
	 * @param[in] iTo Its new index.
	 * @return False when an index is out of the table.
	 */
	auto moveColumn(size_t iFrom, size_t iTo) -> bool;

	/**
	 * @brief
	 *  Change a column (name, type, choices, target table), converting its cells to the new type.
	 * @param[in] iIndex Index of the column.
	 * @param[in] iColumn The new definition; its name must be free (or its own) and not `key`.
	 * @return False when the column does not exist or the name cannot be used.
	 */
	auto setColumn(size_t iIndex, Column iColumn) -> bool;

	/**
	 * @brief
	 *  Set a cell, converted to the type of its column.
	 * @param[in] iRow Index of the row.
	 * @param[in] iColumn Index of the column.
	 * @param[in] iValue The value.
	 * @return False when the cell does not exist.
	 */
	auto setCell(size_t iRow, size_t iColumn, const CellValue& iValue) -> bool;

	/**
	 * @brief
	 *  Give every row one cell per column, of the column's type.
	 */
	void normalise();
};

/**
 * @brief
 *  Compare two cell values exactly (same type, same value).
 * @param[in] iFirst A value.
 * @param[in] iSecond Another value.
 * @return True when they are equal.
 */
[[nodiscard]] OWL_API auto isSameValue(const CellValue& iFirst, const CellValue& iSecond) -> bool;

/**
 * @brief
 *  Name of a column type, as the files write it.
 * @param[in] iType The type.
 * @return Its name (`Integer`, `Float`...).
 */
[[nodiscard]] OWL_API auto getColumnTypeName(ColumnType iType) -> std::string_view;

/**
 * @brief
 *  Read a column type from its name.
 * @param[in] iName The name.
 * @return The type, or nothing for an unknown name.
 */
[[nodiscard]] OWL_API auto parseColumnType(std::string_view iName) -> std::optional<ColumnType>;

/**
 * @brief
 *  Every column type, in order.
 * @return The types.
 */
[[nodiscard]] OWL_API auto getColumnTypes() -> std::span<const ColumnType>;

/**
 * @brief
 *  Default value of a cell of a column: 0, 0.0, false, an empty text, or the first choice of an Enum.
 * @param[in] iColumn The column.
 * @return The value.
 */
[[nodiscard]] OWL_API auto getDefaultValue(const Column& iColumn) -> CellValue;

/**
 * @brief
 *  Convert a value to the type of a column: numbers and texts are parsed, an Enum keeps a known choice only.
 * @param[in] iValue The value.
 * @param[in] iColumn The column.
 * @return The converted value (the default of the column when the value does not fit).
 */
[[nodiscard]] OWL_API auto convertValue(const CellValue& iValue, const Column& iColumn) -> CellValue;

/**
 * @brief
 *  Write a value as text (`true` / `false` for a flag).
 * @param[in] iValue The value.
 * @return The text.
 */
[[nodiscard]] OWL_API auto formatValue(const CellValue& iValue) -> std::string;

/**
 * @brief
 *  Read a value of a column from text.
 * @param[in] iText The text.
 * @param[in] iColumn The column.
 * @return The value, or nothing when the text does not fit the type (a number, a flag, a choice).
 */
[[nodiscard]] OWL_API auto parseValue(std::string_view iText, const Column& iColumn) -> std::optional<CellValue>;

}// namespace owl::data::table
