/**
 * @file DocumentCommands_test.cpp
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "commands/DocumentCommands.h"
#include "commands/CommandRegistry.h"
#include "commands/MaterialCommands.h"
#include "document/BehaviourTreeDocument.h"
#include "document/DataTableDocument.h"
#include "document/DialogueDocument.h"
#include "document/HudThemeDocument.h"
#include "document/LocalisationDocument.h"
#include "document/SequenceDocument.h"
#include "document/SoundDocument.h"
#include "testHelper.h"

#include <core/Log.h>
#include <core/Timestep.h>
#include <data/dialogue/Dialogue.h>
#include <data/sequence/Sequence.h>
#include <gui/IconFont.h>
#include <scene/HudTheme.h>
#include <scene/ai/BehaviourTree.h>

#include <gtest/gtest.h>

#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <string>
#include <tuple>
#include <variant>
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

	auto openDialogue(const std::filesystem::path& iPath) -> DialogueEditor* override {
		return iPath == dialogue.filePath() ? &dialogue : nullptr;
	}

	auto openSequence(const std::filesystem::path& iPath) -> SequenceEditor* override {
		return iPath == sequence.filePath() ? &sequence : nullptr;
	}

	auto openBehaviourTree(const std::filesystem::path& iPath) -> BehaviourTreeEditor* override {
		return iPath == tree.filePath() ? &tree : nullptr;
	}

	auto openHudTheme(const std::filesystem::path& iPath) -> HudThemeEditor* override {
		return iPath == theme.filePath() ? &theme : nullptr;
	}

	auto openMaterial(const std::filesystem::path& /*iPath*/) -> MaterialEditor* override { return nullptr; }

	SoundDocument sound;
	LocalisationDocument localisation;
	DataTableDocument table;
	DialogueDocument dialogue;
	SequenceDocument sequence;
	HudThemeDocument theme;
	BehaviourTreeDocument tree;
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
		m_dialogue = m_root / "guard.owldlg";
		ASSERT_TRUE(data::dialogue::Dialogue::makeDefault().saveToFile(m_dialogue));
		ASSERT_TRUE(m_host.dialogue.loadFromFile(m_dialogue));
		m_sequence = m_root / "intro.owlseq";
		ASSERT_TRUE(data::sequence::Sequence::makeDefault().saveToFile(m_sequence));
		ASSERT_TRUE(m_host.sequence.loadFromFile(m_sequence));
		m_theme = m_root / "demo.owlhudtheme";
		ASSERT_TRUE(scene::HudTheme::makeDefault().saveToFile(m_theme));
		ASSERT_TRUE(m_host.theme.loadFromFile(m_theme));
		m_tree = m_root / "guard.owlbt";
		const CommandContext context;
		ASSERT_TRUE(m_registry.execute("bt.create", {{"path", m_tree.generic_string()}}, context));
		ASSERT_TRUE(m_host.tree.loadFromFile(m_tree));
	}

	void TearDown() override {
		m_host.sound.onDetach();
		m_host.localisation.onDetach();
		m_host.table.onDetach();
		m_host.dialogue.onDetach();
		m_host.sequence.onDetach();
		m_host.theme.onDetach();
		m_host.tree.onDetach();
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
	std::filesystem::path m_dialogue;
	std::filesystem::path m_sequence;
	std::filesystem::path m_theme;
	std::filesystem::path m_tree;
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

TEST_F(DocumentCommandsTest, ThemeCommandsEditTheOpenThemeThroughItsHistory) {
	const auto count = m_host.theme.theme().styles.size();
	ASSERT_TRUE(run("theme.add_style", {{"name", "card"}}, m_theme));
	ASSERT_TRUE(run("theme.add_style", {{"name", "button"}, {"from", "button"}}, m_theme));
	EXPECT_NE(m_host.theme.theme().findStyle("button 2"), nullptr);
	EXPECT_FALSE(run("theme.add_style", {{"from", "missing"}}, m_theme));
	ASSERT_TRUE(run("theme.rename_style", {{"name", "card"}, {"new_name", "frame"}}, m_theme));
	EXPECT_FALSE(run("theme.rename_style", {{"name", "frame"}, {"new_name", "panel"}}, m_theme));
	ASSERT_TRUE(run("theme.set_style", {{"name", "frame"}, {"style", "{cornerRadius: 12, background: [1, 0, 0, 1]}"}},
					m_theme));
	const auto* frame = m_host.theme.theme().findStyle("frame");
	ASSERT_NE(frame, nullptr);
	EXPECT_FLOAT_EQ(frame->box.cornerRadius, 12.f);
	EXPECT_TRUE(frame->defines(scene::component::UiStyleProperty::Background));
	EXPECT_FALSE(frame->defines(scene::component::UiStyleProperty::Hover));
	EXPECT_FALSE(run("theme.set_style", {{"name", "frame"}, {"style", "[1, 2]"}}, m_theme));
	ASSERT_TRUE(run("theme.move_style", {{"name", "frame"}}, m_theme));
	EXPECT_EQ(m_host.theme.theme().findStyleIndex("frame"), count - 1);
	EXPECT_FALSE(run("theme.move_style", {{"name", "panel"}}, m_theme));
	ASSERT_TRUE(run("theme.set_name", {{"theme_name", "Warm"}}, m_theme));
	EXPECT_EQ(m_host.theme.theme().name, "Warm");
	ASSERT_TRUE(run("theme.remove_style", {{"name", "frame"}}, m_theme));
	EXPECT_EQ(m_host.theme.theme().styles.size(), count + 1);
	EXPECT_TRUE(m_host.theme.isDirty());
	m_host.theme.performUndo();
	EXPECT_NE(m_host.theme.theme().findStyle("frame"), nullptr);
	m_host.theme.performRedo();
	EXPECT_EQ(m_host.theme.theme().findStyle("frame"), nullptr);
	ASSERT_TRUE(m_host.theme.save());
	const auto saved = scene::HudTheme::load(m_theme);
	ASSERT_TRUE(saved.has_value());
	EXPECT_EQ(saved->name, "Warm");
	EXPECT_FALSE(m_host.theme.isDirty());
	EXPECT_FALSE(run("theme.add_style", {{"name", "x"}}, m_root / "other.owlhudtheme"));
	EXPECT_FALSE(run("theme.add_style", {{"name", "x"}}, m_cue));
}

TEST_F(DocumentCommandsTest, ThemeCreateWritesTheStartingStyles) {
	const auto path = m_root / "new.owlhudtheme";
	ASSERT_TRUE(run("theme.create", {}, path));
	const auto created = scene::HudTheme::load(path);
	ASSERT_TRUE(created.has_value());
	EXPECT_EQ(created->name, "new");
	EXPECT_NE(created->findStyle("button"), nullptr);
	EXPECT_FALSE(run("theme.create", {}, path));
	EXPECT_FALSE(run("theme.create", {}, m_root / "new.txt"));
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

namespace {

auto resultText(const expected<CommandResult, CommandFailure>& iResult, const std::string& iField) -> std::string {
	if (!iResult)
		return {};
	const auto found = iResult->data.find(iField);
	return found == iResult->data.end() ? std::string{} : std::get<std::string>(found->second);
}

}// namespace

TEST_F(DocumentCommandsTest, DialogueCommandsBuildAConversation) {
	using data::dialogue::Dialogue;
	using data::dialogue::NodeKind;
	const auto& dialogue = m_host.dialogue.dialogue();
	const auto start = std::to_string(static_cast<uint64_t>(dialogue.findStart()->id));
	const auto end = std::to_string(static_cast<uint64_t>(dialogue.findStart()->next));
	EXPECT_FALSE(run("dialogue.add_node", {{"kind", "Loop"}}, m_dialogue));
	EXPECT_FALSE(run("dialogue.add_node", {{"kind", "Line"}, {"from", "42"}}, m_dialogue));
	const auto line =
			resultText(run("dialogue.add_node",
						   {{"kind", "Line"}, {"from", start}, {"position", math::vec2{200.f, 0.f}}}, m_dialogue),
					   "node");
	ASSERT_FALSE(line.empty());
	EXPECT_EQ(std::to_string(static_cast<uint64_t>(dialogue.findStart()->next)), line);
	ASSERT_TRUE(run("dialogue.set_node",
					{{"node", line}, {"speaker", "Guard"}, {"text", "Halt!"}, {"text_key", "line.halt"}}, m_dialogue));
	const auto* lineNode = dialogue.findNode(std::stoull(line));
	ASSERT_NE(lineNode, nullptr);
	EXPECT_EQ(lineNode->speaker.text, "Guard");
	EXPECT_EQ(lineNode->text.key, "line.halt");
	const auto choice = resultText(run("dialogue.add_node", {{"kind", "Choice"}, {"from", line}}, m_dialogue), "node");
	ASSERT_FALSE(choice.empty());
	ASSERT_TRUE(run("dialogue.add_option", {{"node", choice}, {"text", "Leave"}}, m_dialogue));
	EXPECT_FALSE(run("dialogue.add_option", {{"node", line}}, m_dialogue));
	ASSERT_TRUE(run("dialogue.set_option",
					{{"node", choice}, {"option", int64_t{0}}, {"text", "Trade"}, {"condition", "has_goods"}},
					m_dialogue));
	EXPECT_FALSE(run("dialogue.set_option", {{"node", choice}, {"option", int64_t{5}}, {"text", "x"}}, m_dialogue));
	ASSERT_TRUE(run("dialogue.link", {{"from", choice}, {"output", int64_t{0}}, {"to", end}}, m_dialogue));
	ASSERT_TRUE(run("dialogue.set_route", {{"node", choice}, {"output", int64_t{0}}, {"points", "10,20; 30,40"}},
					m_dialogue));
	EXPECT_FALSE(run("dialogue.set_route", {{"node", choice}, {"output", int64_t{0}}, {"points", "oops"}}, m_dialogue));
	ASSERT_NE(dialogue.findRoute(std::stoull(choice), 0), nullptr);
	ASSERT_TRUE(
			run("dialogue.set_link_color", {{"node", choice}, {"output", int64_t{0}}, {"color", "Red"}}, m_dialogue));
	EXPECT_EQ(dialogue.findRoute(std::stoull(choice), 0)->color, "Red");
	EXPECT_EQ(dialogue.findRoute(std::stoull(choice), 0)->points.size(), 2u);// the colour keeps the bends
	EXPECT_FALSE(
			run("dialogue.set_link_color", {{"node", choice}, {"output", int64_t{0}}, {"color", "Mauve"}}, m_dialogue));
	EXPECT_FALSE(run("dialogue.set_link_color", {{"node", choice}, {"output", int64_t{1}}, {"color", "Red"}},
					 m_dialogue));// no link there
	ASSERT_TRUE(run("dialogue.move_option", {{"node", choice}, {"option", int64_t{0}}, {"down", true}}, m_dialogue));
	const auto* choiceNode = dialogue.findNode(std::stoull(choice));
	ASSERT_EQ(choiceNode->options.size(), 2u);
	EXPECT_EQ(choiceNode->options[1].text.text, "Trade");
	EXPECT_EQ(choiceNode->options[1].next, std::stoull(end));
	ASSERT_NE(dialogue.findRoute(choiceNode->id, 1), nullptr);// the bends follow the moved answer
	EXPECT_FALSE(run("dialogue.move_option", {{"node", choice}, {"option", int64_t{1}}, {"down", true}}, m_dialogue));
	EXPECT_FALSE(run("dialogue.link", {{"from", choice}, {"output", int64_t{0}}, {"to", start}}, m_dialogue));
	ASSERT_TRUE(run("dialogue.move_nodes", {{"nodes", line + "," + choice}, {"offset", math::vec2{10.f, 5.f}}},
					m_dialogue));
	EXPECT_FLOAT_EQ(dialogue.findNode(std::stoull(line))->position.x(), 210.f);
	const auto copies =
			resultText(run("dialogue.duplicate_nodes", {{"nodes", line + "," + choice}}, m_dialogue), "nodes");
	ASSERT_EQ(dialogue.nodes.size(), 6u);
	const auto copyLine = copies.substr(0, copies.find(','));
	const auto copyChoice = copies.substr(copies.find(',') + 1);
	EXPECT_EQ(dialogue.findNode(std::stoull(copyLine))->next, std::stoull(copyChoice));// inner link follows the copies
	EXPECT_EQ(dialogue.findNode(std::stoull(copyChoice))->options[1].next, std::stoull(end));// outer link kept
	ASSERT_TRUE(run("dialogue.remove_option", {{"node", choice}, {"option", int64_t{0}}}, m_dialogue));
	ASSERT_NE(dialogue.findRoute(choiceNode->id, 0), nullptr);
	ASSERT_TRUE(run("dialogue.remove_nodes", {{"nodes", copies + "," + end}}, m_dialogue));
	EXPECT_EQ(dialogue.nodes.size(), 3u);
	EXPECT_EQ(dialogue.findNode(std::stoull(choice))->options[0].next, 0);
	EXPECT_TRUE(dialogue.routes.empty());
	EXPECT_FALSE(run("dialogue.remove_nodes", {{"nodes", "1"}}, m_dialogue));
	ASSERT_TRUE(run("dialogue.link", {{"from", line}, {"to", ""}}, m_dialogue));
	EXPECT_EQ(dialogue.findNode(std::stoull(line))->next, 0);
	EXPECT_FALSE(run("dialogue.link", {{"from", line}, {"to", ""}}, m_dialogue));// nothing changes
	EXPECT_TRUE(m_host.dialogue.isDirty());
	m_host.dialogue.performUndo();
	EXPECT_EQ(dialogue.findNode(std::stoull(line))->next, std::stoull(choice));
	EXPECT_FALSE(run("dialogue.link", {{"from", line}, {"to", ""}}, m_root / "goods.owltable"));
	EXPECT_FALSE(run("dialogue.link", {{"from", line}, {"to", ""}}, m_root / "other.owldlg"));
}

TEST_F(DocumentCommandsTest, DialogueCreateWritesANewDialogue) {
	const auto path = m_root / "new.owldlg";
	ASSERT_TRUE(run("dialogue.create", {}, path));
	const auto read = data::dialogue::Dialogue::loadFile(path);
	ASSERT_TRUE(read.has_value());
	EXPECT_TRUE(read->getProblems().empty());
	EXPECT_FALSE(run("dialogue.create", {}, path));
	EXPECT_FALSE(run("dialogue.create", {}, m_root / "new.txt"));
}

TEST_F(DocumentCommandsTest, SequenceCommandsEditTracksAndKeys) {
	const auto& sequence = m_host.sequence.sequence();
	ASSERT_EQ(sequence.cameraTracks.size(), 1u);
	ASSERT_TRUE(run("sequence.set_duration", {{"duration", 8.0}}, m_sequence));
	EXPECT_FLOAT_EQ(sequence.duration, 8.f);
	const auto key =
			run("sequence.add_key",
				{{"track", int64_t{0}}, {"time", 2.5}, {"rotation", math::vec3{0.f, 90.f, 0.f}}, {"fov", 30.0}},
				m_sequence);
	ASSERT_TRUE(key);
	EXPECT_EQ(std::get<int64_t>(key->data.at("key")), 1);
	const auto& added = sequence.cameraTracks[0].keys[1];
	EXPECT_FLOAT_EQ(added.time, 2.5f);
	EXPECT_NEAR(added.rotation.y(), 1.5708f, 1e-4f);
	EXPECT_NEAR(added.fov, 0.5236f, 1e-4f);
	EXPECT_NEAR(added.position.z(), 7.5f, 1e-3f);// the pose of the track at 2.5 s, between z 10 and 5
	const auto moved = run("sequence.set_key",
						   {{"track", int64_t{0}}, {"key", int64_t{1}}, {"time", 6.0}, {"easing", "Step"}}, m_sequence);
	ASSERT_TRUE(moved);
	EXPECT_EQ(std::get<int64_t>(moved->data.at("key")), 2);
	EXPECT_EQ(sequence.cameraTracks[0].keys[2].easing, data::sequence::Easing::Step);
	ASSERT_TRUE(run("sequence.move_keys", {{"track", int64_t{0}}, {"keys", "0"}, {"offset", 1.0}}, m_sequence));
	EXPECT_FLOAT_EQ(sequence.cameraTracks[0].keys[0].time, 1.f);
	ASSERT_TRUE(run("sequence.move_keys", {{"track", int64_t{0}}, {"keys", "0"}, {"offset", -3.0}}, m_sequence));
	EXPECT_FLOAT_EQ(sequence.cameraTracks[0].keys[0].time, 0.f);// never before 0
	EXPECT_FALSE(run("sequence.move_keys", {{"track", int64_t{0}}, {"keys", "0"}, {"offset", -1.0}}, m_sequence));
	EXPECT_FALSE(run("sequence.set_key", {{"track", int64_t{0}}, {"key", int64_t{9}}}, m_sequence));
	EXPECT_FALSE(run("sequence.add_key", {{"track", int64_t{4}}}, m_sequence));

	const auto events = run("sequence.add_track", {{"kind", "Event"}, {"name", "Cues"}}, m_sequence);
	ASSERT_TRUE(events);
	const auto eventTrack = std::get<int64_t>(events->data.at("track"));
	ASSERT_TRUE(run("sequence.add_key", {{"kind", "Event"}, {"track", eventTrack}, {"time", 1.0}, {"function", "boom"}},
					m_sequence));
	ASSERT_TRUE(run("sequence.set_key",
					{{"kind", "Event"}, {"track", eventTrack}, {"key", int64_t{0}}, {"function", "bang"}}, m_sequence));
	EXPECT_EQ(sequence.eventTracks[static_cast<size_t>(eventTrack)].keys[0].function, "bang");
	ASSERT_TRUE(run("sequence.set_track", {{"kind", "Event"}, {"track", eventTrack}, {"muted", true}}, m_sequence));
	EXPECT_TRUE(sequence.eventTracks[static_cast<size_t>(eventTrack)].muted);
	ASSERT_TRUE(run("sequence.remove_keys", {{"kind", "Event"}, {"track", eventTrack}, {"keys", "0"}}, m_sequence));
	EXPECT_TRUE(sequence.eventTracks[static_cast<size_t>(eventTrack)].keys.empty());

	ASSERT_TRUE(run("sequence.add_track", {}, m_sequence));
	ASSERT_EQ(sequence.cameraTracks.size(), 2u);
	EXPECT_EQ(sequence.cameraTracks[1].keys.size(), 2u);// starts where the previous shot ends
	ASSERT_TRUE(run("sequence.move_track", {{"track", int64_t{1}}}, m_sequence));
	EXPECT_EQ(sequence.cameraTracks[0].name, "Shot 2");
	EXPECT_FALSE(run("sequence.move_track", {{"track", int64_t{0}}}, m_sequence));
	ASSERT_TRUE(run("sequence.remove_track", {{"track", int64_t{0}}}, m_sequence));
	EXPECT_EQ(sequence.cameraTracks.size(), 1u);
	EXPECT_TRUE(m_host.sequence.isDirty());
	m_host.sequence.performUndo();
	EXPECT_EQ(sequence.cameraTracks.size(), 2u);
	m_host.sequence.performRedo();
	EXPECT_EQ(sequence.cameraTracks.size(), 1u);
	ASSERT_TRUE(m_host.sequence.save());
	EXPECT_EQ(*data::sequence::Sequence::loadFile(m_sequence), sequence);
}

TEST_F(DocumentCommandsTest, SequenceCreateWritesANewSequence) {
	const auto path = m_root / "new.owlseq";
	ASSERT_TRUE(run("sequence.create", {}, path));
	EXPECT_TRUE(data::sequence::Sequence::loadFile(path).has_value());
	EXPECT_FALSE(run("sequence.create", {}, path));
	EXPECT_FALSE(run("sequence.create", {}, m_root / "new.txt"));
	EXPECT_FALSE(run("sequence.set_duration", {{"duration", 1.0}}, m_root / "other.owlseq"));
}

TEST_F(DocumentCommandsTest, SequenceDocumentSelectionAndRecovery) {
	auto& document = m_host.sequence;
	document.setSelection({.camera = true, .track = 0, .keys = {1, 1, 7, 0}});
	EXPECT_EQ(document.getSelection().keys, (std::vector<size_t>{0, 1}));
	document.setSelection({.camera = false, .track = 3, .keys = {0}});
	EXPECT_TRUE(document.getSelection().keys.empty());
	document.setTime(99.f);
	EXPECT_FLOAT_EQ(document.getTime(), document.sequence().duration);
	document.setPlaying(true);
	EXPECT_FLOAT_EQ(document.getTime(), 0.f);
	core::Timestep step;
	step.forceUpdate(std::chrono::milliseconds(500));
	document.onUpdate(step);
	EXPECT_NEAR(document.getTime(), 0.5f, 1e-4f);
	EXPECT_FALSE(document.addKeyAtPlayhead());// no editor to run the command
	const auto snapshot = document.recoverySnapshot();
	ASSERT_TRUE(snapshot.has_value());
	EXPECT_TRUE(document.restoreRecoverySnapshot(*snapshot));
	EXPECT_TRUE(document.isDirty());
	EXPECT_FALSE(document.restoreRecoverySnapshot("not: a sequence"));
	EXPECT_EQ(document.title(), "intro");
	EXPECT_EQ(document.type(), DocumentType::Sequence);
}

namespace {

auto textOf(const expected<CommandResult, CommandFailure>& iResult, const std::string& iField) -> std::string {
	if (!iResult)
		return {};
	const auto found = iResult->data.find(iField);
	if (found == iResult->data.end())
		return {};
	const auto* text = std::get_if<std::string>(&found->second);
	return text == nullptr ? std::string{} : *text;
}

auto idOf(const std::string& iText) -> core::UUID {
	uint64_t value = 0;
	std::ignore = std::from_chars(iText.data(), iText.data() + iText.size(), value);
	return core::UUID{value};
}

}// namespace

TEST_F(DocumentCommandsTest, TreeCreateWritesARunnableTree) {
	const auto& tree = m_host.tree.tree();
	ASSERT_NE(tree.getRoot(), nullptr);
	EXPECT_EQ(tree.nodes.size(), 3u);
	EXPECT_TRUE(tree.validate().empty());
	EXPECT_FALSE(run("bt.create", {}, m_tree));
	EXPECT_FALSE(run("bt.create", {}, m_root / "guard.txt"));
}

TEST_F(DocumentCommandsTest, TreeCommandsBuildAndEditATree) {
	const auto rootId = m_host.tree.tree().getRoot()->id;
	const auto selectorId = m_host.tree.tree().getChildren(rootId).front();
	const auto added = run("bt.add_node",
						   {{"type", std::string{"composite.sequence"}},
							{"position", math::vec2{200.f, 300.f}},
							{"parent", std::format("{}", static_cast<uint64_t>(selectorId))}},
						   m_tree);
	ASSERT_TRUE(added);
	const auto sequence = idOf(textOf(added, "node"));
	EXPECT_EQ(m_host.tree.tree().getParent(sequence), selectorId);
	const auto wait = run("bt.add_node", {{"type", std::string{"action.wait"}}}, m_tree);
	const auto waitId = idOf(textOf(wait, "node"));
	ASSERT_TRUE(run("bt.link", {{"parent", textOf(added, "node")}, {"child", textOf(wait, "node")}}, m_tree));
	EXPECT_EQ(m_host.tree.tree().getParent(waitId), sequence);
	// A node is never its own ancestor; a leaf has no child; a second root is refused.
	EXPECT_FALSE(run("bt.link", {{"parent", textOf(wait, "node")}, {"child", textOf(added, "node")}}, m_tree));
	EXPECT_FALSE(
			run("bt.link",
				{{"parent", textOf(added, "node")}, {"child", std::format("{}", static_cast<uint64_t>(selectorId))}},
				m_tree));
	EXPECT_FALSE(run("bt.add_node", {{"type", std::string{"root"}}}, m_tree));
	EXPECT_FALSE(run("bt.add_node", {{"type", std::string{"action.dance"}}}, m_tree));
	ASSERT_TRUE(run("bt.set_value",
					{{"node", textOf(wait, "node")}, {"key", std::string{"seconds"}}, {"value", std::string{"3"}}},
					m_tree));
	EXPECT_EQ(scene::ai::BehaviourTree::getValue(*m_host.tree.tree().findNode(waitId), "seconds"), "3");
	EXPECT_FALSE(run("bt.set_value",
					 {{"node", textOf(wait, "node")}, {"key", std::string{"speed"}}, {"value", std::string{"3"}}},
					 m_tree));
	ASSERT_TRUE(run("bt.set_comment", {{"node", textOf(wait, "node")}, {"comment", std::string{"Catch a breath"}}},
					m_tree));
	EXPECT_EQ(m_host.tree.tree().findNode(waitId)->comment, "Catch a breath");
	ASSERT_TRUE(run("bt.move_nodes", {{"nodes", textOf(wait, "node")}, {"offset", math::vec2{10.f, 5.f}}}, m_tree));
	EXPECT_FLOAT_EQ(m_host.tree.tree().findNode(waitId)->position[1], 5.f);
	const auto copies = run("bt.duplicate_nodes",
							{{"nodes", std::format("{},{}", textOf(added, "node"), textOf(wait, "node"))}}, m_tree);
	ASSERT_TRUE(copies);
	EXPECT_EQ(m_host.tree.tree().nodes.size(), 7u);
	ASSERT_TRUE(run("bt.unlink", {{"child", textOf(wait, "node")}}, m_tree));
	EXPECT_FALSE(m_host.tree.tree().getParent(waitId).has_value());
	ASSERT_TRUE(run("bt.remove_nodes", {{"nodes", textOf(copies, "nodes")}}, m_tree));
	EXPECT_EQ(m_host.tree.tree().nodes.size(), 5u);
	m_host.tree.performUndo();
	EXPECT_EQ(m_host.tree.tree().nodes.size(), 7u);
	m_host.tree.performRedo();
	EXPECT_EQ(m_host.tree.tree().nodes.size(), 5u);
	EXPECT_TRUE(m_host.tree.isDirty());
	EXPECT_TRUE(m_host.tree.save());
	EXPECT_FALSE(m_host.tree.isDirty());
}

TEST_F(DocumentCommandsTest, TreeKeyCommandsEditTheBlackboard) {
	ASSERT_TRUE(run("bt.add_key",
					{{"name", std::string{"health"}}, {"type", std::string{"Number"}}, {"value", std::string{"10"}}},
					m_tree));
	EXPECT_FALSE(run("bt.add_key", {{"name", std::string{"health"}}}, m_tree));
	EXPECT_FALSE(run("bt.add_key",
					 {{"name", std::string{"alive"}}, {"type", std::string{"Bool"}}, {"value", std::string{"maybe"}}},
					 m_tree));
	const auto guard = run("bt.add_node", {{"type", std::string{"condition.compare"}}}, m_tree);
	ASSERT_TRUE(run("bt.set_value",
					{{"node", textOf(guard, "node")}, {"key", std::string{"key"}}, {"value", std::string{"health"}}},
					m_tree));
	ASSERT_TRUE(run("bt.set_key",
					{{"name", std::string{"health"}},
					 {"rename", std::string{"life"}},
					 {"value", std::string{"7"}},
					 {"description", std::string{"Hit points"}}},
					m_tree));
	const auto* key = m_host.tree.tree().findKey("life");
	ASSERT_NE(key, nullptr);
	EXPECT_EQ(scene::ai::toText(key->value), "7");
	EXPECT_EQ(key->description, "Hit points");
	EXPECT_EQ(scene::ai::BehaviourTree::getValue(*m_host.tree.tree().findNode(idOf(textOf(guard, "node"))), "key"),
			  "life");
	ASSERT_TRUE(run("bt.set_key", {{"name", std::string{"life"}}, {"type", std::string{"Vector"}}}, m_tree));
	EXPECT_EQ(scene::ai::getBlackboardType(m_host.tree.tree().findKey("life")->value),
			  scene::ai::BlackboardType::Vector);
	ASSERT_TRUE(run("bt.remove_key", {{"name", std::string{"life"}}}, m_tree));
	EXPECT_EQ(m_host.tree.tree().findKey("life"), nullptr);
	EXPECT_FALSE(run("bt.remove_key", {{"name", std::string{"life"}}}, m_tree));
}

TEST_F(DocumentCommandsTest, TreeNodesHaveAGlyphAndASummary) {
	const scene::ai::TreeNode guard{.id = core::UUID{1},
									.type = "decorator.guard",
									.position = {0.f, 0.f},
									.values = {{"key", "health"}, {"op", ">"}, {"value", "3"}},
									.comment = {}};
	EXPECT_EQ(getBehaviourNodeSummary(guard), "health > 3");
	const scene::ai::TreeNode lua{.id = core::UUID{2},
								  .type = "action.lua",
								  .position = {0.f, 0.f},
								  .values = {{"function", "eat"}},
								  .comment = {}};
	EXPECT_EQ(getBehaviourNodeSummary(lua), "eat()");
	for (const auto& type: scene::ai::getNodeTypes())
		EXPECT_STRNE(getBehaviourNodeGlyph(type.id), gui::icon::circleHelp) << type.id;
}
