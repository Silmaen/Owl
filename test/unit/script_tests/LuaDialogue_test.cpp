/**
 * @file LuaDialogue_test.cpp
 * @author Silmaen
 * @date 30/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <app/EngineContext.h>
#include <core/Core.h>
#include <core/Log.h>
#include <data/dialogue/Dialogue.h>
#include <data/localisation/Localisation.h>
#include <scene/Scene.h>
#include <script/ScriptInstance.h>

#include <cstdint>
#include <filesystem>
#include <format>
#include <string>
#include <utility>
#include <vector>

using namespace owl;
using namespace owl::script;
using data::dialogue::Dialogue;
using data::dialogue::DialogueNode;
using data::dialogue::DialogueOption;
using data::dialogue::DialogueText;
using data::dialogue::NodeKind;

namespace {

auto runScript(const shared<scene::Scene>& iScene, const std::string& iScript, const ScriptInstance& ioInstance)
		-> bool {
	const std::vector<uint8_t> data(iScript.begin(), iScript.end());
	ioInstance.setScene(iScene.get());
	if (!ioInstance.createFromBuffer(data, "dialogue_test", 1))
		return false;
	ioInstance.onCreate();
	return true;
}

auto makeNode(const uint64_t iId, const NodeKind iKind, const uint64_t iNext = 0) -> DialogueNode {
	DialogueNode node;
	node.id = iId;
	node.kind = iKind;
	node.next = iNext;
	return node;
}

auto makeText(std::string iText, std::string iKey = {}) -> DialogueText {
	return {.text = std::move(iText), .key = std::move(iKey)};
}

auto makeSpeech(const uint64_t iId, const NodeKind iKind, DialogueText iText, const uint64_t iNext = 0)
		-> DialogueNode {
	auto node = makeNode(iId, iKind, iNext);
	node.text = std::move(iText);
	return node;
}

auto makeOption(DialogueText iText, std::string iCondition, const uint64_t iNext) -> DialogueOption {
	return {.text = std::move(iText), .condition = std::move(iCondition), .next = iNext};
}

auto makeHook(const uint64_t iId, const NodeKind iKind, std::string iFunction, const uint64_t iNext,
			  const uint64_t iNextFalse = 0) -> DialogueNode {
	auto node = makeNode(iId, iKind, iNext);
	node.function = std::move(iFunction);
	node.nextFalse = iNextFalse;
	return node;
}

auto makeDialogue() -> Dialogue {
	Dialogue dialogue;
	auto greeting = makeSpeech(2, NodeKind::Line, makeText("Halt!", "line.halt"), 3);
	greeting.speaker = makeText("Guard", "npc.guard");
	greeting.sound = "sounds/halt.wav";
	auto choice = makeSpeech(3, NodeKind::Choice, makeText("Your business?"));
	choice.options = {makeOption(makeText("Trade"), "has_goods", 4), makeOption(makeText("Bribe"), "is_rich", 4),
					  makeOption(makeText("Leave"), "", 5)};
	dialogue.nodes = {makeNode(1, NodeKind::Start, 2), greeting, choice, makeHook(4, NodeKind::Event, "open_gate", 5),
					  makeNode(5, NodeKind::End)};
	return dialogue;
}

}// namespace

TEST(LuaBindings, dialogueLinesChoicesAndHooks) {
	core::Log::init(core::Log::Level::Off);
	const auto path = std::filesystem::temp_directory_path() / "lua_guard.owldlg";
	ASSERT_TRUE(makeDialogue().saveToFile(path));
	const auto scn = mkShared<scene::Scene>();
	app::EngineContext context;
	data::localisation::Localisation localisation;
	localisation.languages = {{.code = "fr", .name = "Français", .font = {}, .exported = true}};
	localisation.fallback = "fr";
	localisation.strings = {{.key = "npc.guard", .table = "dialogue", .values = {{"fr", "Garde"}}}};
	context.setLocalisation(localisation);
	context.setLanguage("fr");
	scn->setEngineContext(&context);
	const std::string script = std::format("gate = false\n"
										   "function has_goods() return true end\n"
										   "function is_rich() return false end\n"
										   "function open_gate() gate = true end\n"
										   "function on_create()\n"
										   "  local d = dialogue.start('{}')\n"
										   "  local line = dialogue.get_line(d)\n"
										   "  kind = line.kind\n"
										   "  speaker = line.speaker\n"
										   "  text = line.text\n"
										   "  sound = line.sound\n"
										   "  lineChoices = #line.choices\n"
										   "  advanced = dialogue.next(d)\n"
										   "  local choice = dialogue.get_line(d)\n"
										   "  choices = #choice.choices\n"
										   "  firstChoice = choice.choices[1]\n"
										   "  stuck = dialogue.next(d)\n"
										   "  badChoice = dialogue.choose(d, 0)\n"
										   "  running = dialogue.is_running(d)\n"
										   "  goesOn = dialogue.choose(d, 1)\n"
										   "  over = dialogue.get_line(d) == nil and not dialogue.is_running(d)\n"
										   "  local other = dialogue.start('{}')\n"
										   "  dialogue.stop(other)\n"
										   "  stopped = not dialogue.is_running(other)\n"
										   "  missing = dialogue.start('nowhere.owldlg') == nil\n"
										   "end\n",
										   path.generic_string(), path.generic_string());
	const ScriptInstance inst;
	ASSERT_TRUE(runScript(scn, script, inst));
	EXPECT_EQ(inst.getPropertyString("kind").value_or(""), "Line");
	EXPECT_EQ(inst.getPropertyString("speaker").value_or(""), "Garde");
	EXPECT_EQ(inst.getPropertyString("text").value_or(""), "Halt!");// key without translation: the text as written
	EXPECT_EQ(inst.getPropertyString("sound").value_or(""), "sounds/halt.wav");
	EXPECT_EQ(inst.getPropertyInt("lineChoices").value_or(-1), 0);
	EXPECT_TRUE(inst.getPropertyBool("advanced").value_or(false));
	EXPECT_EQ(inst.getPropertyInt("choices").value_or(0), 2);
	EXPECT_EQ(inst.getPropertyString("firstChoice").value_or(""), "Trade");
	EXPECT_TRUE(inst.getPropertyBool("stuck").value_or(false));// still on the choice, nothing done
	EXPECT_FALSE(inst.getPropertyBool("badChoice").value_or(true));
	EXPECT_TRUE(inst.getPropertyBool("running").value_or(false));
	EXPECT_FALSE(inst.getPropertyBool("goesOn").value_or(true));
	EXPECT_TRUE(inst.getPropertyBool("gate").value_or(false));
	EXPECT_TRUE(inst.getPropertyBool("over").value_or(false));
	EXPECT_TRUE(inst.getPropertyBool("stopped").value_or(false));
	EXPECT_TRUE(inst.getPropertyBool("missing").value_or(false));
	EXPECT_TRUE(context.getDialogues().getConversations().empty());
	scn->setEngineContext(nullptr);
	std::filesystem::remove(path);
	core::Log::invalidate();
}

TEST(LuaBindings, dialogueMissingFunctionFailsItsCondition) {
	core::Log::init(core::Log::Level::Off);
	auto dialogue = makeDialogue();
	const auto path = std::filesystem::temp_directory_path() / "lua_missing.owldlg";
	ASSERT_TRUE(dialogue.saveToFile(path));
	const auto scn = mkShared<scene::Scene>();
	app::EngineContext context;
	scn->setEngineContext(&context);
	const std::string script = std::format("function is_rich() error('broken') end\n"
										   "function on_create()\n"
										   "  local d = dialogue.start('{}')\n"
										   "  dialogue.next(d)\n"
										   "  local line = dialogue.get_line(d)\n"
										   "  choices = #line.choices\n"
										   "  only = line.choices[1]\n"
										   "end\n",
										   path.generic_string());
	const ScriptInstance inst;
	ASSERT_TRUE(runScript(scn, script, inst));
	EXPECT_EQ(inst.getPropertyInt("choices").value_or(0), 1);
	EXPECT_EQ(inst.getPropertyString("only").value_or(""), "Leave");
	scn->setEngineContext(nullptr);
	std::filesystem::remove(path);
	core::Log::invalidate();
}
