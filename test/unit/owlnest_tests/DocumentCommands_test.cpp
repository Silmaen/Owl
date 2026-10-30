/**
 * @file DocumentCommands_test.cpp
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "commands/DocumentCommands.h"
#include "commands/CommandRegistry.h"
#include "document/DataTableDocument.h"
#include "document/LocalisationDocument.h"
#include "document/SoundDocument.h"
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
using namespace owl::nest::commands;

namespace {

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wweak-vtables")
// The two documents of the test, found by their path.
class TestHost final : public DocumentHost {
public:
	auto openSoundCue(const std::filesystem::path& iPath) -> SoundCueEditor* override {
		return iPath == sound.filePath() ? &sound : nullptr;
	}

	auto openLocalisation(const std::filesystem::path& iPath) -> LocalisationEditor* override {
		return iPath == localisation.filePath() ? &localisation : nullptr;
	}

	auto openDataTable(const std::filesystem::path& iPath) -> DataTableEditor* override {
		return iPath == table.filePath() ? &table : nullptr;
	}

	SoundDocument sound;
	LocalisationDocument localisation;
	DataTableDocument table;
};

class DocumentCommandsTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_root = std::filesystem::temp_directory_path() / "owl_document_commands_test";
		std::filesystem::remove_all(m_root);
		std::filesystem::create_directories(m_root);
		m_cue = m_root / "steps.owlsound";
		ASSERT_TRUE(sound::SoundCue{}.saveToFile(m_cue));
		ASSERT_TRUE(m_host.sound.loadFromFile(m_cue));
		m_locale = m_root / "localisation.yml";
		m_host.localisation.startEmpty(m_locale);
		m_table = m_root / "goods.owltable";
		data::table::DataTable table;
		ASSERT_TRUE(table.addColumn({.name = "name", .type = data::table::ColumnType::Text, .choices = {}, .table = {}},
									0));
		ASSERT_TRUE(table.addRow("row", 0));
		ASSERT_TRUE(table.saveToFile(m_table));
		ASSERT_TRUE(m_host.table.loadFromFile(m_table));
	}

	void TearDown() override {
		m_host.sound.onDetach();
		m_host.localisation.onDetach();
		m_host.table.onDetach();
		std::filesystem::remove_all(m_root);
		core::Log::invalidate();
	}

	auto run(const std::string& iName, CommandArgs iArgs, const std::filesystem::path& iPath)
			-> expected<CommandResult, CommandFailure> {
		iArgs.set("path", iPath.generic_string());
		CommandContext context;
		context.documents = &m_host;
		return m_registry.execute(iName, iArgs, context);
	}

	std::filesystem::path m_root;
	std::filesystem::path m_cue;
	std::filesystem::path m_locale;
	std::filesystem::path m_table;
	TestHost m_host;
	CommandRegistry m_registry;
};
OWL_DIAG_POP

}// namespace

TEST(DocumentCommands, IndicesAreReadAndWrittenAsCommaLists) {
	EXPECT_EQ(parseIndices(" 0, 2,3 "), (std::vector<size_t>{0, 2, 3}));
	EXPECT_EQ(parseIndices(""), std::vector<size_t>{});
	EXPECT_FALSE(parseIndices("1,b").has_value());
	const std::vector<size_t> indices{4, 1};
	EXPECT_EQ(formatIndices(indices), "4,1");
}

TEST_F(DocumentCommandsTest, SoundCommandsEditTheOpenCueThroughItsHistory) {
	ASSERT_TRUE(run("sound.add_take", {{"file", "a.wav"}}, m_cue));
	ASSERT_TRUE(run("sound.add_take", {{"file", "b.ogg"}}, m_cue));
	EXPECT_FALSE(run("sound.add_take", {{"file", "readme.txt"}}, m_cue));
	ASSERT_TRUE(run("sound.set", {{"volume", 0.5}, {"loop", true}}, m_cue));
	ASSERT_TRUE(run("sound.set_take", {{"take", int64_t{1}}, {"weight", 3.0}}, m_cue));
	ASSERT_TRUE(run("sound.move_takes", {{"takes", "1"}}, m_cue));
	const auto& cue = m_host.sound.cue();
	ASSERT_EQ(cue.variants.size(), 2u);
	EXPECT_EQ(cue.variants[0].file, "b.ogg");
	EXPECT_FLOAT_EQ(cue.variants[0].weight, 3.f);
	EXPECT_FLOAT_EQ(cue.volume, 0.5f);
	EXPECT_TRUE(cue.loop);
	ASSERT_TRUE(run("sound.duplicate_takes", {{"takes", "0,1"}}, m_cue));
	EXPECT_EQ(m_host.sound.cue().variants.size(), 4u);
	EXPECT_FALSE(run("sound.remove_takes", {{"takes", "9"}}, m_cue));
	ASSERT_TRUE(run("sound.remove_takes", {{"takes", "2,3"}}, m_cue));
	EXPECT_EQ(m_host.sound.cue().variants.size(), 2u);
	m_host.sound.performUndo();
	EXPECT_EQ(m_host.sound.cue().variants.size(), 4u);
}

TEST_F(DocumentCommandsTest, SoundCommandsNeedAnOpenCue) {
	EXPECT_FALSE(run("sound.set", {{"volume", 0.5}}, m_root / "other.owlsound"));
	EXPECT_FALSE(run("sound.set", {{"volume", 0.5}}, m_locale));
	const CommandArgs args{{"path", m_cue.generic_string()}, {"volume", 0.5}};
	EXPECT_FALSE(m_registry.execute("sound.set", args, CommandContext{}));
}

TEST_F(DocumentCommandsTest, LocaleCommandsEditTheOpenLocalisation) {
	ASSERT_TRUE(run("locale.add_language", {{"code", "fr"}, {"name", "Français"}}, m_locale));
	EXPECT_FALSE(run("locale.add_language", {{"code", "fr"}}, m_locale));
	ASSERT_TRUE(run("locale.set_fallback", {{"code", "fr"}}, m_locale));
	ASSERT_TRUE(run("locale.set_language", {{"code", "fr"}, {"exported", false}}, m_locale));
	ASSERT_TRUE(run("locale.add_string", {{"key", "menu.play"}, {"table", "menu"}}, m_locale));
	ASSERT_TRUE(run("locale.set_value", {{"key", "menu.play"}, {"language", "fr"}, {"value", "Jouer"}}, m_locale));
	EXPECT_FALSE(run("locale.set_value", {{"key", "menu.play"}, {"language", "de"}, {"value", "x"}}, m_locale));
	ASSERT_TRUE(run("locale.rename_key", {{"key", "menu.play"}, {"new_key", "menu.start"}}, m_locale));
	ASSERT_TRUE(run("locale.duplicate_strings", {{"keys", "menu.start"}}, m_locale));
	ASSERT_TRUE(run("locale.set_table", {{"keys", "menu.start"}, {"table", "title"}}, m_locale));
	const auto& localisation = m_host.localisation.localisation();
	EXPECT_EQ(localisation.fallback, "fr");
	ASSERT_NE(localisation.findLanguage("fr"), nullptr);
	EXPECT_FALSE(localisation.findLanguage("fr")->exported);
	ASSERT_EQ(localisation.strings.size(), 2u);
	EXPECT_EQ(localisation.translate("menu.start", "fr"), "Jouer");
	EXPECT_EQ(localisation.findString("menu.start")->table, "title");
	EXPECT_FALSE(run("locale.remove_strings", {{"keys", "missing"}}, m_locale));
	ASSERT_TRUE(run("locale.remove_strings", {{"keys", "menu.start"}}, m_locale));
	EXPECT_EQ(m_host.localisation.localisation().strings.size(), 1u);
	ASSERT_TRUE(run("locale.remove_language", {{"code", "fr"}}, m_locale));
	EXPECT_EQ(m_host.localisation.localisation().findLanguage("fr"), nullptr);
}

TEST_F(DocumentCommandsTest, TableCommandsEditTheOpenDataTable) {
	ASSERT_TRUE(run("table.add_column", {{"name", "price"}, {"type", "Float"}}, m_table));
	ASSERT_TRUE(run("table.add_column", {{"name", "kind"}, {"type", "Enum"}, {"choices", "food, metal"}}, m_table));
	ASSERT_TRUE(run("table.add_column", {{"name", "stock"}, {"type", "Integer"}, {"after", "name"}}, m_table));
	EXPECT_FALSE(run("table.add_column", {{"name", "key"}, {"type", "Text"}}, m_table));
	EXPECT_FALSE(run("table.add_column", {{"name", "x"}, {"type", "Text"}, {"after", "missing"}}, m_table));
	const auto& table = m_host.table.table();
	ASSERT_EQ(table.columns.size(), 4u);
	EXPECT_EQ(table.columns[1].name, "stock");
	EXPECT_EQ(table.columns[3].choices.size(), 2u);
	ASSERT_TRUE(run("table.add_row", {{"key", "wheat"}}, m_table));
	ASSERT_TRUE(run("table.add_row", {{"key", "wheat"}, {"after", "row"}}, m_table));
	EXPECT_EQ(table.rows[1].key, "wheat_2");
	ASSERT_TRUE(run("table.set_cell", {{"key", "wheat"}, {"column", "price"}, {"value", "2.5"}}, m_table));
	EXPECT_FALSE(run("table.set_cell", {{"key", "wheat"}, {"column", "price"}, {"value", "cheap"}}, m_table));
	EXPECT_FALSE(run("table.set_cell", {{"key", "rye"}, {"column", "price"}, {"value", "1"}}, m_table));
	EXPECT_FALSE(run("table.set_cell", {{"key", "wheat"}, {"column", "weight"}, {"value", "1"}}, m_table));
	ASSERT_TRUE(run("table.set_cell", {{"key", "wheat"}, {"column", "kind"}, {"value", "metal"}}, m_table));
	EXPECT_DOUBLE_EQ(std::get<double>(*table.getCell("wheat", "price")), 2.5);
	ASSERT_TRUE(run("table.rename_row", {{"key", "wheat"}, {"new_key", "barley"}}, m_table));
	EXPECT_FALSE(run("table.rename_row", {{"key", "barley"}, {"new_key", "row"}}, m_table));
	ASSERT_TRUE(run("table.move_rows", {{"keys", "barley"}, {"down", false}}, m_table));
	EXPECT_EQ(table.rows[1].key, "barley");
	EXPECT_FALSE(run("table.move_rows", {{"keys", "row"}, {"down", false}}, m_table));
	ASSERT_TRUE(run("table.duplicate_rows", {{"keys", "barley, row"}}, m_table));
	EXPECT_EQ(table.rows.size(), 5u);
	ASSERT_TRUE(run("table.set_column", {{"column", "price"}, {"name", "cost"}, {"type", "Integer"}}, m_table));
	EXPECT_EQ(std::get<int64_t>(*table.getCell("barley", "cost")), 3);
	EXPECT_FALSE(run("table.set_column", {{"column", "cost"}}, m_table));
	EXPECT_FALSE(run("table.set_column", {{"column", "cost"}, {"name", "stock"}}, m_table));
	ASSERT_TRUE(run("table.move_column", {{"column", "cost"}, {"right", true}}, m_table));
	EXPECT_EQ(table.columns[3].name, "cost");
	EXPECT_FALSE(run("table.move_column", {{"column", "cost"}, {"right", true}}, m_table));
	EXPECT_FALSE(run("table.move_column", {{"column", "name"}, {"right", false}}, m_table));
	ASSERT_TRUE(run("table.remove_column", {{"column", "kind"}}, m_table));
	EXPECT_FALSE(run("table.remove_column", {{"column", "kind"}}, m_table));
	ASSERT_TRUE(run("table.remove_rows", {{"keys", "barley_2,row_2"}}, m_table));
	EXPECT_FALSE(run("table.remove_rows", {{"keys", "gold"}}, m_table));
	EXPECT_FALSE(run("table.remove_rows", {{"keys", ""}}, m_table));
	EXPECT_EQ(m_host.table.table().rows.size(), 3u);
	m_host.table.performUndo();
	EXPECT_EQ(m_host.table.table().rows.size(), 5u);
}

TEST_F(DocumentCommandsTest, TableCreateWritesANewTable) {
	const auto path = m_root / "new.owltable";
	const CommandContext context;
	ASSERT_TRUE(m_registry.execute("table.create", {{"path", path.generic_string()}}, context));
	const auto created = data::table::DataTable::loadFile(path);
	ASSERT_TRUE(created.has_value());
	EXPECT_EQ(created->columns.size(), 1u);
	EXPECT_EQ(created->rows.size(), 1u);
	EXPECT_FALSE(m_registry.execute("table.create", {{"path", path.generic_string()}}, context));
	EXPECT_FALSE(m_registry.execute("table.create", {{"path", (m_root / "x.yml").generic_string()}}, context));
	EXPECT_FALSE(run("table.add_row", {{"key", "a"}}, path));
	EXPECT_FALSE(run("table.add_row", {{"key", "a"}}, m_locale));
}
