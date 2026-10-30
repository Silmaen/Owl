/**
 * @file DataTableDocument_test.cpp
 * @author Silmaen
 * @date 30/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "document/DataTableDocument.h"
#include "testHelper.h"

#include <core/Log.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <tuple>
#include <vector>

using namespace owl;
using namespace owl::nest;
using data::table::ColumnType;
using data::table::DataTable;

namespace {

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wweak-vtables")
class DataTableDocumentTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_root = std::filesystem::temp_directory_path() / "owl_data_table_document_test";
		std::filesystem::remove_all(m_root);
		std::filesystem::create_directories(m_root);
		DataTable markets;
		ASSERT_TRUE(markets.addColumn({.name = "name", .type = ColumnType::Text, .choices = {}, .table = {}}, 0));
		ASSERT_TRUE(markets.addRow("harbour", 0));
		ASSERT_TRUE(markets.addRow("mine", 1));
		m_markets = m_root / "markets.owltable";
		ASSERT_TRUE(markets.saveToFile(m_markets));
		DataTable goods;
		ASSERT_TRUE(goods.addColumn({.name = "price", .type = ColumnType::Float, .choices = {}, .table = {}}, 0));
		ASSERT_TRUE(goods.addRow("wheat", 0));
		m_file = m_root / "goods.owltable";
		ASSERT_TRUE(goods.saveToFile(m_file));
		ASSERT_TRUE(m_document.loadFromFile(m_file));
	}

	void TearDown() override {
		m_document.onDetach();
		std::filesystem::remove_all(m_root);
		core::Log::invalidate();
	}

	[[nodiscard]] auto keys() const -> std::vector<std::string> {
		std::vector<std::string> names;
		for (const auto& row: m_document.table().rows) names.push_back(row.key);
		return names;
	}

	std::filesystem::path m_root;
	std::filesystem::path m_file;
	std::filesystem::path m_markets;
	DataTableDocument m_document;
};
OWL_DIAG_POP

}// namespace

TEST_F(DataTableDocumentTest, LoadsSavesAndRestoresTheTable) {
	EXPECT_EQ(m_document.type(), DocumentType::DataTable);
	EXPECT_EQ(m_document.title(), "goods");
	EXPECT_EQ(m_document.filePath(), m_file);
	EXPECT_FALSE(m_document.isDirty());
	EXPECT_FALSE(m_document.loadFromFile(m_root / "missing.owltable"));
	ASSERT_TRUE(m_document.setCell(0, 0, 3.5));
	EXPECT_TRUE(m_document.isDirty());
	EXPECT_TRUE(m_document.canUndo());
	ASSERT_TRUE(m_document.save());
	EXPECT_FALSE(m_document.isDirty());
	const auto read = DataTable::loadFile(m_file);
	ASSERT_TRUE(read.has_value());
	EXPECT_DOUBLE_EQ(std::get<double>(read->rows[0].cells[0]), 3.5);
	const auto snapshot = m_document.recoverySnapshot();
	ASSERT_TRUE(snapshot.has_value());
	ASSERT_TRUE(m_document.setCell(0, 0, 9.0));
	ASSERT_TRUE(m_document.restoreRecoverySnapshot(*snapshot));
	EXPECT_DOUBLE_EQ(std::get<double>(m_document.table().rows[0].cells[0]), 3.5);
	EXPECT_TRUE(m_document.isDirty());
	EXPECT_FALSE(m_document.restoreRecoverySnapshot("not a table: ["));
	const auto other = m_root / "copy.owltable";
	ASSERT_TRUE(m_document.saveAs(other));
	EXPECT_EQ(m_document.filePath(), other);
	EXPECT_TRUE(exists(other));
}

TEST_F(DataTableDocumentTest, RowsAreAddedMovedDuplicatedAndRemovedAsGroups) {
	ASSERT_TRUE(m_document.addRow("wheat", 9));
	EXPECT_EQ(keys(), (std::vector<std::string>{"wheat", "wheat_2"}));
	EXPECT_EQ(m_document.getSelection(), std::vector<size_t>{1});
	ASSERT_TRUE(m_document.addRow("iron", 0));
	ASSERT_TRUE(m_document.moveRows(std::vector<size_t>{0}, true));
	EXPECT_EQ(keys(), (std::vector<std::string>{"wheat", "iron", "wheat_2"}));
	EXPECT_FALSE(m_document.moveRows(std::vector<size_t>{2}, true));
	EXPECT_FALSE(m_document.moveRows(std::vector<size_t>{0}, false));
	ASSERT_TRUE(m_document.moveRows(std::vector<size_t>{1, 2}, false));
	EXPECT_EQ(keys(), (std::vector<std::string>{"iron", "wheat_2", "wheat"}));
	ASSERT_TRUE(m_document.duplicateRows(std::vector<size_t>{0, 2}));
	EXPECT_EQ(keys().size(), 5u);
	EXPECT_EQ(m_document.getSelection(), (std::vector<size_t>{3, 4}));
	EXPECT_FALSE(m_document.duplicateRows(std::vector<size_t>{}));
	ASSERT_TRUE(m_document.setRowKey(0, "copper"));
	EXPECT_FALSE(m_document.setRowKey(0, "copper"));
	EXPECT_FALSE(m_document.setRowKey(0, "wheat"));
	ASSERT_TRUE(m_document.removeRows(std::vector<size_t>{3, 4, 3}));
	EXPECT_EQ(keys(), (std::vector<std::string>{"copper", "wheat_2", "wheat"}));
	EXPECT_TRUE(m_document.getSelection().empty());
	EXPECT_FALSE(m_document.removeRows(std::vector<size_t>{8}));
	m_document.setSelection({2, 0, 7});
	EXPECT_EQ(m_document.getSelection(), (std::vector<size_t>{0, 2}));
	m_document.performUndo();
	EXPECT_EQ(keys().size(), 5u);
	m_document.performRedo();
	EXPECT_EQ(keys().size(), 3u);
}

TEST_F(DataTableDocumentTest, ColumnsAndReferencesAreEdited) {
	const data::table::Column market{.name = "market",
									 .type = ColumnType::Reference,
									 .choices = {},
									 .table = m_markets.generic_string()};
	ASSERT_TRUE(m_document.addColumn(market, 5));
	EXPECT_FALSE(m_document.addColumn(market, 0));
	EXPECT_EQ(m_document.getReferenceKeys(m_document.table().columns[1]),
			  (std::vector<std::string>{"harbour", "mine"}));
	auto self = market;
	self.table.clear();
	EXPECT_EQ(m_document.getReferenceKeys(self), std::vector<std::string>{"wheat"});
	auto lost = market;
	lost.table = (m_root / "lost.owltable").generic_string();
	EXPECT_TRUE(m_document.getReferenceKeys(lost).empty());
	ASSERT_TRUE(m_document.setCell(0, 1, std::string{"mine"}));
	ASSERT_TRUE(m_document.moveColumn(1, 0));
	EXPECT_EQ(m_document.table().columns[0].name, "market");
	EXPECT_FALSE(m_document.moveColumn(0, 4));
	auto renamed = m_document.table().columns[1];
	renamed.name = "cost";
	renamed.type = ColumnType::Integer;
	ASSERT_TRUE(m_document.setColumn(1, renamed));
	EXPECT_FALSE(m_document.setColumn(1, m_document.table().columns[0]));
	EXPECT_EQ(m_document.table().columns[1].name, "cost");
	ASSERT_TRUE(m_document.removeColumn(0));
	EXPECT_FALSE(m_document.removeColumn(3));
	EXPECT_FALSE(m_document.setCell(3, 0, 1.0));
	EXPECT_EQ(m_document.table().columns.size(), 1u);
}
