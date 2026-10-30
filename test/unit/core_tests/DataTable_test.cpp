/**
 * @file DataTable_test.cpp
 * @author Silmaen
 * @date 30/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <app/EngineContext.h>
#include <core/Log.h>
#include <data/table/DataTable.h>
#include <data/table/DataTableLibrary.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <string>

using namespace owl;
using namespace owl::data::table;

namespace {

auto makeTable() -> DataTable {
	DataTable table;
	EXPECT_TRUE(table.addColumn({.name = "price", .type = ColumnType::Float, .choices = {}, .table = {}}, 0));
	EXPECT_TRUE(table.addColumn({.name = "stock", .type = ColumnType::Integer, .choices = {}, .table = {}}, 9));
	EXPECT_TRUE(table.addColumn({.name = "legal", .type = ColumnType::Bool, .choices = {}, .table = {}}, 9));
	EXPECT_TRUE(
			table.addColumn({.name = "kind", .type = ColumnType::Enum, .choices = {"food", "ore"}, .table = {}}, 9));
	EXPECT_TRUE(table.addColumn({.name = "label", .type = ColumnType::Text, .choices = {}, .table = {}}, 9));
	EXPECT_TRUE(table.addColumn({.name = "icon", .type = ColumnType::Asset, .choices = {}, .table = {}}, 9));
	EXPECT_TRUE(table.addColumn(
			{.name = "market", .type = ColumnType::Reference, .choices = {}, .table = "tables/markets.owltable"}, 9));
	EXPECT_TRUE(table.addRow("wheat", 0));
	EXPECT_TRUE(table.addRow("iron", 9));
	EXPECT_TRUE(table.setCell(0, 0, 2.5));
	EXPECT_TRUE(table.setCell(0, 1, int64_t{40}));
	EXPECT_TRUE(table.setCell(0, 2, true));
	EXPECT_TRUE(table.setCell(1, 3, std::string{"ore"}));
	EXPECT_TRUE(table.setCell(0, 4, std::string{"Golden wheat"}));
	EXPECT_TRUE(table.setCell(0, 5, std::string{"textures/wheat.png"}));
	EXPECT_TRUE(table.setCell(1, 6, std::string{"harbour"}));
	return table;
}

}// namespace

TEST(DataTable, RoundTripsThroughYamlAndFiles) {
	core::Log::init(core::Log::Level::Off);
	const auto table = makeTable();
	const auto text = table.serializeToString();
	EXPECT_NE(text.find("FormatVersion: 1"), std::string::npos);
	const auto read = DataTable::deserializeFromString(text);
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(*read, table);
	EXPECT_FALSE(DataTable::deserializeFromString("[unclosed").has_value());
	EXPECT_FALSE(DataTable::deserializeFromString("Other: {}").has_value());
	EXPECT_FALSE(DataTable::deserializeFromString("DataTable: ''\nFormatVersion: 99").has_value());

	const auto tolerant = DataTable::deserializeFromString(
			"DataTable: ''\ncolumns: [{name: a, type: Integer}, {name: a}, {name: key}, {name: b, type: Strange}]\n"
			"rows: [{key: one, a: 3, b: x}, {key: one, a: 4}, {a: 5}, {key: two, a: nope}]");
	ASSERT_TRUE(tolerant.has_value());
	ASSERT_EQ(tolerant->columns.size(), 2u);
	EXPECT_EQ(tolerant->columns[1].type, ColumnType::Text);
	ASSERT_EQ(tolerant->rows.size(), 2u);
	EXPECT_EQ(std::get<int64_t>(tolerant->rows[0].cells[0]), 3);
	EXPECT_EQ(std::get<int64_t>(tolerant->rows[1].cells[0]), 0);
	EXPECT_EQ(std::get<std::string>(tolerant->rows[1].cells[1]), "");

	const auto path = std::filesystem::temp_directory_path() / "goods.owltable";
	ASSERT_TRUE(table.saveToFile(path));
	const auto loaded = DataTable::loadFile(path);
	ASSERT_TRUE(loaded.has_value());
	EXPECT_EQ(*loaded, table);
	const auto asset = DataTable::load(path);
	ASSERT_TRUE(asset.has_value());
	EXPECT_EQ(*asset, table);
	EXPECT_TRUE(DataTable::isTable(path));
	EXPECT_FALSE(DataTable::isTable("goods.yml"));
	EXPECT_FALSE(DataTable::loadFile(path.parent_path() / "missing.owltable").has_value());
	EXPECT_FALSE(DataTable::load("missing/nowhere.owltable").has_value());
	std::filesystem::remove(path);
	core::Log::invalidate();
}

TEST(DataTable, CellsKeepTheTypeOfTheirColumn) {
	core::Log::init(core::Log::Level::Off);
	auto table = makeTable();
	ASSERT_NE(table.getCell("wheat", "price"), nullptr);
	EXPECT_DOUBLE_EQ(std::get<double>(*table.getCell("wheat", "price")), 2.5);
	EXPECT_EQ(std::get<std::string>(*table.getCell("wheat", "kind")), "food");
	EXPECT_EQ(table.getCell("rye", "price"), nullptr);
	EXPECT_EQ(table.getCell("wheat", "weight"), nullptr);
	// Values are converted to the column type.
	EXPECT_TRUE(table.setCell(1, 0, std::string{"7.25"}));
	EXPECT_DOUBLE_EQ(std::get<double>(table.rows[1].cells[0]), 7.25);
	EXPECT_TRUE(table.setCell(1, 1, 3.6));
	EXPECT_EQ(std::get<int64_t>(table.rows[1].cells[1]), 4);
	EXPECT_TRUE(table.setCell(1, 2, int64_t{1}));
	EXPECT_TRUE(std::get<bool>(table.rows[1].cells[2]));
	EXPECT_TRUE(table.setCell(1, 3, std::string{"gems"}));
	EXPECT_EQ(std::get<std::string>(table.rows[1].cells[3]), "food");
	EXPECT_FALSE(table.setCell(5, 0, 1.));
	// A type change converts the cells.
	auto stock = table.columns[1];
	stock.type = ColumnType::Text;
	stock.name = "count";
	EXPECT_TRUE(table.setColumn(1, stock));
	EXPECT_EQ(std::get<std::string>(table.rows[0].cells[1]), "40");
	stock.type = ColumnType::Float;
	EXPECT_TRUE(table.setColumn(1, stock));
	EXPECT_DOUBLE_EQ(std::get<double>(table.rows[0].cells[1]), 40.);
	stock.name = "price";
	EXPECT_FALSE(table.setColumn(1, stock));
	stock.name = "key";
	EXPECT_FALSE(table.setColumn(1, stock));
	EXPECT_FALSE(table.setColumn(42, stock));
	auto legal = table.columns[2];
	legal.type = ColumnType::Integer;
	EXPECT_TRUE(table.setColumn(2, legal));
	EXPECT_EQ(std::get<int64_t>(table.rows[0].cells[2]), 1);
	legal.type = ColumnType::Bool;
	EXPECT_TRUE(table.setColumn(2, legal));
	EXPECT_TRUE(std::get<bool>(table.rows[0].cells[2]));
	table.rows[0].cells.pop_back();
	table.normalise();
	EXPECT_EQ(table.rows[0].cells.size(), table.columns.size());
	core::Log::invalidate();
}

TEST(DataTable, RowsAndColumnsAreAddedMovedAndRemoved) {
	core::Log::init(core::Log::Level::Off);
	auto table = makeTable();
	EXPECT_FALSE(table.addRow("wheat", 0));
	EXPECT_FALSE(table.addRow("", 0));
	EXPECT_EQ(table.getUniqueKey("wheat"), "wheat_2");
	EXPECT_EQ(table.getUniqueKey(""), "row");
	EXPECT_EQ(table.getUniqueColumnName("price"), "price_2");
	EXPECT_EQ(table.getUniqueColumnName("key"), "column");
	EXPECT_TRUE(table.addRow("rye", 1));
	EXPECT_EQ(table.rows[1].key, "rye");
	EXPECT_EQ(table.rows[1].cells.size(), table.columns.size());
	EXPECT_TRUE(table.setRowKey(1, "barley"));
	EXPECT_FALSE(table.setRowKey(1, "iron"));
	EXPECT_FALSE(table.setRowKey(1, ""));
	EXPECT_FALSE(table.setRowKey(10, "x"));
	EXPECT_TRUE(table.setRowKey(1, "barley"));
	EXPECT_FALSE(table.addColumn({.name = "price", .type = ColumnType::Float, .choices = {}, .table = {}}, 0));
	EXPECT_FALSE(table.addColumn({.name = "key", .type = ColumnType::Float, .choices = {}, .table = {}}, 0));
	EXPECT_TRUE(table.moveColumn(0, 2));
	EXPECT_EQ(table.columns[2].name, "price");
	EXPECT_DOUBLE_EQ(std::get<double>(table.rows[0].cells[2]), 2.5);
	EXPECT_FALSE(table.moveColumn(0, 99));
	EXPECT_TRUE(table.removeColumn(2));
	EXPECT_FALSE(table.findColumn("price").has_value());
	EXPECT_EQ(table.rows[0].cells.size(), table.columns.size());
	EXPECT_FALSE(table.removeColumn(99));
	EXPECT_EQ(table.findRow("iron"), std::optional<size_t>{2});
	core::Log::invalidate();
}

TEST(DataTable, ValuesAreFormattedAndParsed) {
	const Column integer{.name = "n", .type = ColumnType::Integer, .choices = {}, .table = {}};
	const Column flag{.name = "f", .type = ColumnType::Bool, .choices = {}, .table = {}};
	const Column choice{.name = "c", .type = ColumnType::Enum, .choices = {"a", "b"}, .table = {}};
	const Column number{.name = "x", .type = ColumnType::Float, .choices = {}, .table = {}};
	EXPECT_EQ(formatValue(int64_t{-3}), "-3");
	EXPECT_EQ(formatValue(true), "true");
	EXPECT_EQ(formatValue(1.5), "1.5");
	EXPECT_EQ(formatValue(std::string{"txt"}), "txt");
	EXPECT_EQ(std::get<int64_t>(*parseValue(" 12 ", integer)), 12);
	EXPECT_EQ(std::get<int64_t>(*parseValue("2.6", integer)), 3);
	EXPECT_FALSE(parseValue("twelve", integer).has_value());
	EXPECT_TRUE(std::get<bool>(*parseValue("yes", flag)));
	EXPECT_FALSE(std::get<bool>(*parseValue("0", flag)));
	EXPECT_FALSE(parseValue("maybe", flag).has_value());
	EXPECT_EQ(std::get<std::string>(*parseValue("b", choice)), "b");
	EXPECT_FALSE(parseValue("z", choice).has_value());
	EXPECT_FALSE(parseValue("nan", number).has_value());
	EXPECT_EQ(std::get<std::string>(getDefaultValue(choice)), "a");
	EXPECT_DOUBLE_EQ(std::get<double>(convertValue(false, number)), 0.);
	EXPECT_EQ(std::get<int64_t>(convertValue(true, integer)), 1);
	EXPECT_FALSE(std::get<bool>(convertValue(0., flag)));
}

TEST(DataTableLibrary, ReadsEachTableOnceUntilForgotten) {
	core::Log::init(core::Log::Level::Off);
	const auto path = std::filesystem::temp_directory_path() / "library.owltable";
	ASSERT_TRUE(makeTable().saveToFile(path));
	app::EngineContext context;
	auto& library = context.getDataTables();
	const auto first = library.get(path.string());
	ASSERT_NE(first, nullptr);
	EXPECT_EQ(first->rows.size(), 2u);
	EXPECT_EQ(library.get(path.string()), first);
	EXPECT_EQ(library.getLoadCount(), 1u);
	EXPECT_EQ(library.get("missing.owltable"), nullptr);
	EXPECT_EQ(library.size(), 2u);
	library.invalidate(path.string());
	EXPECT_NE(library.get(path.string()), first);
	EXPECT_EQ(library.getLoadCount(), 3u);
	library.clear();
	EXPECT_EQ(library.size(), 0u);
	std::filesystem::remove(path);
	core::Log::invalidate();
}
