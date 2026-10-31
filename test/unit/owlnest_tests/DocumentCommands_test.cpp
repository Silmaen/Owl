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
#include "document/DialogueDocument.h"
#include "document/LocalisationDocument.h"
#include "document/SequenceDocument.h"
#include "document/SoundDocument.h"
#include "testHelper.h"

#include <core/Log.h>
#include <core/Timestep.h>
#include <data/dialogue/Dialogue.h>
#include <data/sequence/Sequence.h>

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
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

	SoundDocument sound;
	LocalisationDocument localisation;
	DataTableDocument table;
	DialogueDocument dialogue;
	SequenceDocument sequence;
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
	}

	void TearDown() override {
		m_host.sound.onDetach();
		m_host.localisation.onDetach();
		m_host.table.onDetach();
		m_host.dialogue.onDetach();
		m_host.sequence.onDetach();
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
