/**
 * @file DataTable.cpp
 * @author Silmaen
 * @date 30/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "data/table/DataTable.h"

#include "core/FormatVersionYaml.h"
#include "core/external/yaml.h"
#include "data/assets/AssetReference.h"
#include "data/assets/AssetSearchPaths.h"
#include "platform/AtomicFile.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <format>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <utility>

namespace owl::data::table {

namespace {

constexpr std::array<core::MigrationStep, 0> g_dataTableMigrations{};
constexpr core::DocumentFormat g_dataTableFormat{.name = "DataTable", .migrations = g_dataTableMigrations};

constexpr std::string_view g_keyField{"key"};

auto isUsableName(const std::string_view iName) -> bool { return !iName.empty() && iName != g_keyField; }

auto parseInteger(std::string_view iText) -> std::optional<int64_t> {
	while (!iText.empty() && iText.front() == ' ') iText.remove_prefix(1);
	while (!iText.empty() && iText.back() == ' ') iText.remove_suffix(1);
	int64_t value = 0;
	if (const auto [end, error] = std::from_chars(iText.data(), iText.data() + iText.size(), value);
		error == std::errc{} && end == iText.data() + iText.size())
		return value;
	return std::nullopt;
}

auto parseDouble(std::string_view iText) -> std::optional<double> {
	while (!iText.empty() && iText.front() == ' ') iText.remove_prefix(1);
	while (!iText.empty() && iText.back() == ' ') iText.remove_suffix(1);
	double value = 0.;
	if (const auto [end, error] = std::from_chars(iText.data(), iText.data() + iText.size(), value);
		error == std::errc{} && end == iText.data() + iText.size() && std::isfinite(value))
		return value;
	return std::nullopt;
}

auto parseBool(const std::string_view iText) -> std::optional<bool> {
	if (iText == "true" || iText == "1" || iText == "yes")
		return true;
	if (iText == "false" || iText == "0" || iText == "no")
		return false;
	return std::nullopt;
}

auto emitCell(YAML::Emitter& ioOut, const CellValue& iValue, const Column& iColumn) -> void {
	if (const auto* integer = std::get_if<int64_t>(&iValue); integer != nullptr)
		ioOut << *integer;
	else if (const auto* number = std::get_if<double>(&iValue); number != nullptr)
		ioOut << *number;
	else if (const auto* flag = std::get_if<bool>(&iValue); flag != nullptr)
		ioOut << *flag;
	else if (iColumn.type == ColumnType::Asset)
		ioOut << data::assets::toAssetReference(std::get<std::string>(iValue));
	else
		ioOut << std::get<std::string>(iValue);
}

auto readCell(const YAML::Node& iNode, const Column& iColumn) -> CellValue {
	if (!iNode || !iNode.IsScalar())
		return getDefaultValue(iColumn);
	const auto text = iNode.as<std::string>();
	if (iColumn.type == ColumnType::Asset)
		return data::assets::toAssetPath(text);
	return parseValue(text, iColumn).value_or(getDefaultValue(iColumn));
}

auto readColumn(const YAML::Node& iNode) -> Column {
	Column column{.name = iNode["name"].as<std::string>(""),
				  .type = ColumnType::Text,
				  .choices = {},
				  .table = data::assets::toAssetPath(iNode["table"].as<std::string>(""))};
	if (const auto parsed = magic_enum::enum_cast<ColumnType>(iNode["type"].as<std::string>("Text"));
		parsed.has_value())
		column.type = *parsed;
	for (const auto& choice: iNode["choices"]) column.choices.push_back(choice.as<std::string>());
	return column;
}

}// namespace

auto getColumnTypeName(const ColumnType iType) -> std::string_view { return magic_enum::enum_name(iType); }

auto parseColumnType(const std::string_view iName) -> std::optional<ColumnType> {
	return magic_enum::enum_cast<ColumnType>(iName);
}

auto getColumnTypes() -> std::span<const ColumnType> {
	static constexpr auto s_types = magic_enum::enum_values<ColumnType>();
	return s_types;
}

auto getDefaultValue(const Column& iColumn) -> CellValue {
	switch (iColumn.type) {
		case ColumnType::Integer:
			return int64_t{0};
		case ColumnType::Float:
			return 0.;
		case ColumnType::Bool:
			return false;
		case ColumnType::Enum:
			return iColumn.choices.empty() ? std::string{} : iColumn.choices.front();
		case ColumnType::Text:
		case ColumnType::Asset:
		case ColumnType::Reference:
			return std::string{};
	}
	return std::string{};
}

auto formatValue(const CellValue& iValue) -> std::string {
	if (const auto* integer = std::get_if<int64_t>(&iValue); integer != nullptr)
		return std::format("{}", *integer);
	if (const auto* number = std::get_if<double>(&iValue); number != nullptr)
		return std::format("{}", *number);
	if (const auto* flag = std::get_if<bool>(&iValue); flag != nullptr)
		return *flag ? "true" : "false";
	return std::get<std::string>(iValue);
}

auto parseValue(const std::string_view iText, const Column& iColumn) -> std::optional<CellValue> {
	switch (iColumn.type) {
		case ColumnType::Integer:
			if (const auto value = parseInteger(iText); value)
				return *value;
			if (const auto value = parseDouble(iText); value)
				return static_cast<int64_t>(std::llround(*value));
			return std::nullopt;
		case ColumnType::Float:
			if (const auto value = parseDouble(iText); value)
				return *value;
			return std::nullopt;
		case ColumnType::Bool:
			if (const auto value = parseBool(iText); value)
				return *value;
			return std::nullopt;
		case ColumnType::Enum:
			if (std::ranges::contains(iColumn.choices, iText))
				return std::string{iText};
			return std::nullopt;
		case ColumnType::Text:
		case ColumnType::Asset:
		case ColumnType::Reference:
			return std::string{iText};
	}
	return std::nullopt;
}

auto convertValue(const CellValue& iValue, const Column& iColumn) -> CellValue {
	switch (iColumn.type) {
		case ColumnType::Integer:
			if (const auto* number = std::get_if<double>(&iValue); number != nullptr)
				return static_cast<int64_t>(std::llround(*number));
			if (const auto* flag = std::get_if<bool>(&iValue); flag != nullptr)
				return int64_t{*flag ? 1 : 0};
			break;
		case ColumnType::Float:
			if (const auto* integer = std::get_if<int64_t>(&iValue); integer != nullptr)
				return static_cast<double>(*integer);
			if (const auto* flag = std::get_if<bool>(&iValue); flag != nullptr)
				return *flag ? 1. : 0.;
			break;
		case ColumnType::Bool:
			if (const auto* integer = std::get_if<int64_t>(&iValue); integer != nullptr)
				return *integer != 0;
			if (const auto* number = std::get_if<double>(&iValue); number != nullptr)
				return std::abs(*number) > 0.;
			break;
		case ColumnType::Text:
		case ColumnType::Enum:
		case ColumnType::Asset:
		case ColumnType::Reference:
			break;
	}
	return parseValue(formatValue(iValue), iColumn).value_or(getDefaultValue(iColumn));
}

auto DataTable::isTable(const std::filesystem::path& iPath) -> bool { return iPath.extension() == fileExtension(); }

auto DataTable::format() -> const core::DocumentFormat& { return g_dataTableFormat; }

auto DataTable::serializeToString() const -> std::string {
	YAML::Emitter out;
	out << YAML::BeginMap;
	out << YAML::Key << "DataTable" << YAML::Value << "";
	core::emitFormatVersion(out, g_dataTableFormat);
	out << YAML::Key << "columns" << YAML::Value << YAML::BeginSeq;
	for (const auto& column: columns) {
		out << YAML::Flow << YAML::BeginMap;
		out << YAML::Key << "name" << YAML::Value << column.name;
		out << YAML::Key << "type" << YAML::Value << std::string{magic_enum::enum_name(column.type)};
		if (column.type == ColumnType::Enum) {
			out << YAML::Key << "choices" << YAML::Value << YAML::Flow << YAML::BeginSeq;
			for (const auto& choice: column.choices) out << choice;
			out << YAML::EndSeq;
		}
		if (column.type == ColumnType::Reference && !column.table.empty())
			out << YAML::Key << "table" << YAML::Value << data::assets::toAssetReference(column.table);
		out << YAML::EndMap;
	}
	out << YAML::EndSeq;
	out << YAML::Key << "rows" << YAML::Value << YAML::BeginSeq;
	for (const auto& row: rows) {
		out << YAML::BeginMap;
		out << YAML::Key << std::string{g_keyField} << YAML::Value << row.key;
		for (size_t index = 0; index < columns.size() && index < row.cells.size(); ++index) {
			out << YAML::Key << columns[index].name << YAML::Value;
			emitCell(out, row.cells[index], columns[index]);
		}
		out << YAML::EndMap;
	}
	out << YAML::EndSeq;
	out << YAML::EndMap;
	return std::string{out.c_str()};
}

auto DataTable::deserializeFromString(const std::string_view iYaml) -> std::optional<DataTable> {
	YAML::Node root;
	try {
		root = YAML::Load(std::string{iYaml});
	} catch (const YAML::Exception& e) {
		OWL_CORE_ERROR("DataTable: Failed to parse YAML: {}.", e.what())
		return std::nullopt;
	}
	if (!root || !root.IsMap() || !root["DataTable"]) {
		OWL_CORE_WARN("DataTable: Document is not a data table.")
		return std::nullopt;
	}
	if (!core::upgradeYamlDocument(g_dataTableFormat, root, "<buffer>")) {
		OWL_CORE_ERROR("DataTable: Document cannot be read.")
		return std::nullopt;
	}
	try {
		DataTable table;
		for (const auto& node: root["columns"]) {
			auto column = readColumn(node);
			if (!isUsableName(column.name) || table.findColumn(column.name)) {
				OWL_CORE_WARN("DataTable: Column '{}' skipped (empty, reserved or twice).", column.name)
				continue;
			}
			table.columns.push_back(std::move(column));
		}
		for (const auto& node: root["rows"]) {
			Row row{.key = node[std::string{g_keyField}].as<std::string>(""), .cells = {}};
			if (row.key.empty() || table.findRow(row.key)) {
				OWL_CORE_WARN("DataTable: Row '{}' skipped (no key or twice).", row.key)
				continue;
			}
			for (const auto& column: table.columns) row.cells.push_back(readCell(node[column.name], column));
			table.rows.push_back(std::move(row));
		}
		return table;
	} catch (const YAML::Exception& e) {
		OWL_CORE_ERROR("DataTable: Invalid value: {}.", e.what())
		return std::nullopt;
	}
}

auto DataTable::saveToFile(const std::filesystem::path& iPath) const -> bool {
	if (const auto written = platform::writeFileAtomic(iPath, serializeToString()); !written) {
		OWL_CORE_ERROR("DataTable: Failed to write '{}': {}.", iPath.string(), describe(written.error()))
		return false;
	}
	return true;
}

auto DataTable::loadFile(const std::filesystem::path& iPath) -> std::optional<DataTable> {
	const std::ifstream file(iPath);
	if (!file) {
		OWL_CORE_WARN("DataTable: Cannot open '{}'.", iPath.string())
		return std::nullopt;
	}
	std::stringstream text;
	text << file.rdbuf();
	return deserializeFromString(text.str());
}

auto DataTable::load(const std::filesystem::path& iAsset) -> std::optional<DataTable> {
	const auto bytes = data::assets::readAssetBytes(iAsset);
	if (!bytes) {
		OWL_CORE_WARN("DataTable: '{}' not found in the pack nor the asset folders.", iAsset.string())
		return std::nullopt;
	}
	return deserializeFromString({reinterpret_cast<const char*>(bytes->data()), bytes->size()});
}

auto DataTable::findColumn(const std::string_view iName) const -> std::optional<size_t> {
	const auto found = std::ranges::find(columns, iName, &Column::name);
	if (found == columns.end())
		return std::nullopt;
	return static_cast<size_t>(std::distance(columns.begin(), found));
}

auto DataTable::findRow(const std::string_view iKey) const -> std::optional<size_t> {
	const auto found = std::ranges::find(rows, iKey, &Row::key);
	if (found == rows.end())
		return std::nullopt;
	return static_cast<size_t>(std::distance(rows.begin(), found));
}

auto DataTable::getCell(const std::string_view iKey, const std::string_view iColumn) const -> const CellValue* {
	const auto row = findRow(iKey);
	const auto column = findColumn(iColumn);
	if (!row || !column || *column >= rows[*row].cells.size())
		return nullptr;
	return &rows[*row].cells[*column];
}

auto DataTable::getUniqueKey(const std::string_view iBase) const -> std::string {
	const std::string base = iBase.empty() ? std::string{"row"} : std::string{iBase};
	if (!findRow(base))
		return base;
	for (uint32_t counter = 2;; ++counter)
		if (auto key = std::format("{}_{}", base, counter); !findRow(key))
			return key;
}

auto DataTable::getUniqueColumnName(const std::string_view iBase) const -> std::string {
	const std::string base = isUsableName(iBase) ? std::string{iBase} : std::string{"column"};
	if (!findColumn(base))
		return base;
	for (uint32_t counter = 2;; ++counter)
		if (auto name = std::format("{}_{}", base, counter); !findColumn(name))
			return name;
}

auto DataTable::addRow(const std::string& iKey, const size_t iIndex) -> bool {
	if (iKey.empty() || findRow(iKey)) {
		OWL_CORE_WARN("DataTable: Row key '{}' is empty or taken.", iKey)
		return false;
	}
	Row row{.key = iKey, .cells = {}};
	for (const auto& column: columns) row.cells.push_back(getDefaultValue(column));
	rows.insert(rows.begin() + static_cast<std::ptrdiff_t>(std::min(iIndex, rows.size())), std::move(row));
	return true;
}

auto DataTable::setRowKey(const size_t iIndex, const std::string& iKey) -> bool {
	if (iIndex >= rows.size() || iKey.empty()) {
		OWL_CORE_WARN("DataTable: Cannot rename row {} to '{}'.", iIndex, iKey)
		return false;
	}
	if (const auto other = findRow(iKey); other && *other != iIndex) {
		OWL_CORE_WARN("DataTable: Row key '{}' is taken.", iKey)
		return false;
	}
	rows[iIndex].key = iKey;
	return true;
}

auto DataTable::addColumn(Column iColumn, const size_t iIndex) -> bool {
	if (!isUsableName(iColumn.name) || findColumn(iColumn.name)) {
		OWL_CORE_WARN("DataTable: Column name '{}' is empty, reserved or taken.", iColumn.name)
		return false;
	}
	const auto at = std::min(iIndex, columns.size());
	const auto value = getDefaultValue(iColumn);
	columns.insert(columns.begin() + static_cast<std::ptrdiff_t>(at), std::move(iColumn));
	for (auto& row: rows) row.cells.insert(row.cells.begin() + static_cast<std::ptrdiff_t>(at), value);
	return true;
}

auto DataTable::removeColumn(const size_t iIndex) -> bool {
	if (iIndex >= columns.size()) {
		OWL_CORE_WARN("DataTable: No column {}.", iIndex)
		return false;
	}
	columns.erase(columns.begin() + static_cast<std::ptrdiff_t>(iIndex));
	for (auto& row: rows)
		if (iIndex < row.cells.size())
			row.cells.erase(row.cells.begin() + static_cast<std::ptrdiff_t>(iIndex));
	return true;
}

auto DataTable::moveColumn(const size_t iFrom, const size_t iTo) -> bool {
	if (iFrom >= columns.size() || iTo >= columns.size()) {
		OWL_CORE_WARN("DataTable: Cannot move column {} to {}.", iFrom, iTo)
		return false;
	}
	const auto move = [iFrom, iTo](auto& ioItems) -> void {
		auto item = std::move(ioItems[iFrom]);
		ioItems.erase(ioItems.begin() + static_cast<std::ptrdiff_t>(iFrom));
		ioItems.insert(ioItems.begin() + static_cast<std::ptrdiff_t>(iTo), std::move(item));
	};
	move(columns);
	for (auto& row: rows) move(row.cells);
	return true;
}

auto DataTable::setColumn(const size_t iIndex, Column iColumn) -> bool {
	if (iIndex >= columns.size()) {
		OWL_CORE_WARN("DataTable: No column {}.", iIndex)
		return false;
	}
	if (const auto other = findColumn(iColumn.name); !isUsableName(iColumn.name) || (other && *other != iIndex)) {
		OWL_CORE_WARN("DataTable: Column name '{}' is empty, reserved or taken.", iColumn.name)
		return false;
	}
	columns[iIndex] = std::move(iColumn);
	for (auto& row: rows)
		if (iIndex < row.cells.size())
			row.cells[iIndex] = convertValue(row.cells[iIndex], columns[iIndex]);
	return true;
}

auto DataTable::setCell(const size_t iRow, const size_t iColumn, const CellValue& iValue) -> bool {
	if (iRow >= rows.size() || iColumn >= columns.size() || iColumn >= rows[iRow].cells.size()) {
		OWL_CORE_WARN("DataTable: No cell ({}, {}).", iRow, iColumn)
		return false;
	}
	rows[iRow].cells[iColumn] = convertValue(iValue, columns[iColumn]);
	return true;
}

void DataTable::normalise() {
	for (auto& row: rows) {
		row.cells.resize(columns.size(), std::string{});
		for (size_t index = 0; index < columns.size(); ++index)
			row.cells[index] = convertValue(row.cells[index], columns[index]);
	}
}

}// namespace owl::data::table
