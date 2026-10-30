/**
 * @file Dialogue_test.cpp
 * @author Silmaen
 * @date 30/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <data/dialogue/Conversation.h>
#include <data/dialogue/Dialogue.h>
#include <data/dialogue/DialogueLibrary.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

using namespace owl;
using namespace owl::data::dialogue;

namespace {

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

auto makeSpeech(const uint64_t iId, const NodeKind iKind, DialogueText iText, const uint64_t iNext = 0) -> DialogueNode {
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

// Start → greeting → (has_key ? choice : refusal); the choice offers "open" (needs can_open) and "leave".
auto makeDialogue() -> Dialogue {
	Dialogue dialogue;
	auto greeting = makeSpeech(2, NodeKind::Line, makeText("Halt!"), 3);
	greeting.speaker = makeText("Guard", "npc.guard");
	greeting.sound = "sounds/halt.wav";
	auto choice = makeSpeech(4, NodeKind::Choice, makeText("What now?"));
	choice.options = {makeOption(makeText("Open", "choice.open"), "can_open", 5), makeOption(makeText("Leave"), "", 7)};
	dialogue.nodes = {makeNode(1, NodeKind::Start, 2),
					  greeting,
					  makeHook(3, NodeKind::Branch, "has_key", 4, 6),
					  choice,
					  makeHook(5, NodeKind::Event, "open_gate", 7),
					  makeSpeech(6, NodeKind::Line, makeText("No key, no entry."), 7),
					  makeNode(7, NodeKind::End)};
	return dialogue;
}

struct Recorder {
	bool hasKey = true;
	bool canOpen = true;
	std::vector<std::string> actions;

	auto hooks() -> ConversationHooks {
		return {.condition = [this](const std::string_view iName) -> bool {
					return iName == "has_key" ? hasKey : canOpen;
				},
				.action = [this](const std::string_view iName) -> void { actions.emplace_back(iName); }};
	}
};

}// namespace

TEST(Dialogue, RoundTripsThroughYamlAndFiles) {
	core::Log::init(core::Log::Level::Off);
	const auto dialogue = makeDialogue();
	const auto text = dialogue.serializeToString();
	const auto read = Dialogue::deserializeFromString(text);
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(*read, dialogue);
	const auto path = std::filesystem::temp_directory_path() / "guard.owldlg";
	ASSERT_TRUE(dialogue.saveToFile(path));
	const auto loaded = Dialogue::loadFile(path);
	ASSERT_TRUE(loaded.has_value());
	EXPECT_EQ(*loaded, dialogue);
	EXPECT_TRUE(Dialogue::isDialogue(path));
	EXPECT_FALSE(Dialogue::isDialogue("goods.owltable"));
	std::filesystem::remove(path);
	EXPECT_FALSE(Dialogue::loadFile(path).has_value());
	EXPECT_FALSE(Dialogue::deserializeFromString("DataTable: ''").has_value());
	EXPECT_FALSE(Dialogue::deserializeFromString(": [").has_value());
	core::Log::invalidate();
}

TEST(Dialogue, SkipsNodesWithoutOrWithTwiceTheSameId) {
	core::Log::init(core::Log::Level::Off);
	const auto read = Dialogue::deserializeFromString("Dialogue: ''\nnodes:\n"
													  "  - {id: 4, kind: Line}\n"
													  "  - {kind: End}\n"
													  "  - {id: 4, kind: End}\n"
													  "  - {id: 5, kind: Unknown}\n");
	ASSERT_TRUE(read.has_value());
	ASSERT_EQ(read->nodes.size(), 2);
	EXPECT_EQ(read->nodes[0].kind, NodeKind::Line);
	EXPECT_EQ(read->nodes[1].kind, NodeKind::Line);
	core::Log::invalidate();
}

TEST(Dialogue, OutputsAndRoutesFollowTheKind) {
	core::Log::init(core::Log::Level::Off);
	auto dialogue = makeDialogue();
	EXPECT_EQ(Dialogue::getOutputCount(*dialogue.findNode(1)), 1);
	EXPECT_EQ(Dialogue::getOutputCount(*dialogue.findNode(3)), 2);
	EXPECT_EQ(Dialogue::getOutputCount(*dialogue.findNode(4)), 2);
	EXPECT_EQ(Dialogue::getOutputCount(*dialogue.findNode(7)), 0);
	EXPECT_EQ(Dialogue::getOutput(*dialogue.findNode(3), 1), 6);
	EXPECT_EQ(Dialogue::getOutput(*dialogue.findNode(4), 1), 7);
	EXPECT_EQ(Dialogue::getOutput(*dialogue.findNode(4), 2), 0);
	EXPECT_TRUE(Dialogue::setOutput(*dialogue.findNode(4), 0, 6));
	EXPECT_EQ(dialogue.findNode(4)->options[0].next, 6);
	EXPECT_TRUE(Dialogue::setOutput(*dialogue.findNode(3), 1, 0));
	EXPECT_EQ(dialogue.findNode(3)->nextFalse, 0);
	EXPECT_FALSE(Dialogue::setOutput(*dialogue.findNode(7), 0, 1));
	dialogue.routes = {{.node = 3, .output = 0, .points = {{10.f, 20.f}, {30.f, 40.f}}}};
	const auto read = Dialogue::deserializeFromString(dialogue.serializeToString() +
													  "\nextra: 0\n");// unknown keys are ignored
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(read->routes, dialogue.routes);
	ASSERT_NE(read->findRoute(3, 0), nullptr);
	EXPECT_EQ(read->findRoute(3, 1), nullptr);
	const auto dropped = Dialogue::deserializeFromString("Dialogue: ''\nnodes:\n  - {id: 1, kind: End}\n"
														 "routes:\n  - {node: 1, output: 0, points: [[1, 2]]}\n"
														 "  - {node: 9, output: 0, points: []}\n");
	ASSERT_TRUE(dropped.has_value());
	EXPECT_TRUE(dropped->routes.empty());
	core::Log::invalidate();
}

TEST(Dialogue, DefaultHasAStartLinkedToAnEnd) {
	const auto dialogue = Dialogue::makeDefault();
	ASSERT_EQ(dialogue.nodes.size(), 2);
	const auto* start = dialogue.findStart();
	ASSERT_NE(start, nullptr);
	const auto* end = dialogue.findNode(start->next);
	ASSERT_NE(end, nullptr);
	EXPECT_EQ(end->kind, NodeKind::End);
	EXPECT_TRUE(dialogue.getProblems().empty());
}

TEST(Dialogue, ProblemsNameTheFaultyNodes) {
	EXPECT_TRUE(makeDialogue().getProblems().empty());
	Dialogue dialogue = makeDialogue();
	dialogue.findNode(3)->function.clear();
	dialogue.findNode(4)->options.clear();
	dialogue.findNode(6)->next = 99;
	dialogue.nodes.push_back(makeNode(8, NodeKind::Line));
	const auto problems = dialogue.getProblems();
	const auto concerns = [&problems](const uint64_t iNode) {
		return std::ranges::any_of(problems, [iNode](const auto& iProblem) { return iProblem.node == iNode; });
	};
	EXPECT_TRUE(concerns(3));
	EXPECT_TRUE(concerns(4));
	EXPECT_TRUE(concerns(6));
	EXPECT_TRUE(concerns(5));// no longer reached once the choice lost its options
	EXPECT_TRUE(concerns(8));
	Dialogue empty;
	ASSERT_EQ(empty.getProblems().size(), 1);
	EXPECT_EQ(empty.getProblems().front().node, 0);
	empty.nodes = {makeNode(1, NodeKind::Start), makeNode(2, NodeKind::Start)};
	EXPECT_EQ(empty.getProblems().size(), 1);
}

TEST(Dialogue, ListsItsKeysAndFunctions) {
	const auto dialogue = makeDialogue();
	EXPECT_EQ(dialogue.getTextKeys(), (std::vector<std::string>{"npc.guard", "choice.open"}));
	EXPECT_EQ(dialogue.getFunctions(), (std::vector<std::string>{"has_key", "can_open", "open_gate"}));
	EXPECT_EQ(getNodeKindName(NodeKind::Choice), "Choice");
	EXPECT_EQ(getNodeKinds().size(), 6);
	EXPECT_EQ(parseNodeKind("Branch"), NodeKind::Branch);
	EXPECT_FALSE(parseNodeKind("Loop").has_value());
}

TEST(Conversation, FollowsBranchesChoicesAndEvents) {
	const auto dialogue = mkShared<const Dialogue>(makeDialogue());
	Recorder recorder;
	Conversation conversation{dialogue};
	ASSERT_TRUE(conversation.start(recorder.hooks()));
	EXPECT_EQ(conversation.getCurrent()->id, 2);
	EXPECT_FALSE(conversation.choose(0, recorder.hooks()));// not on a choice
	ASSERT_TRUE(conversation.advance(recorder.hooks()));
	EXPECT_EQ(conversation.getCurrent()->id, 4);
	ASSERT_EQ(conversation.getOfferedOptions().size(), 2);
	EXPECT_FALSE(conversation.advance(recorder.hooks()));// a choice waits for an answer
	EXPECT_TRUE(conversation.isRunning());
	EXPECT_FALSE(conversation.choose(2, recorder.hooks()));
	EXPECT_FALSE(conversation.choose(0, recorder.hooks()));// open → event → end
	EXPECT_FALSE(conversation.isRunning());
	EXPECT_EQ(recorder.actions, (std::vector<std::string>{"open_gate"}));
}

TEST(Conversation, ConditionsHideOptionsAndPickBranches) {
	const auto dialogue = mkShared<const Dialogue>(makeDialogue());
	Recorder recorder;
	recorder.canOpen = false;
	Conversation conversation{dialogue};
	ASSERT_TRUE(conversation.start(recorder.hooks()));
	ASSERT_TRUE(conversation.advance(recorder.hooks()));
	ASSERT_EQ(conversation.getOfferedOptions().size(), 1);
	EXPECT_EQ(conversation.getOfferedOptions().front(), 1);
	recorder.hasKey = false;
	ASSERT_TRUE(conversation.start(recorder.hooks()));
	ASSERT_TRUE(conversation.advance(recorder.hooks()));
	EXPECT_EQ(conversation.getCurrent()->id, 6);
	EXPECT_FALSE(conversation.advance(recorder.hooks()));
	EXPECT_EQ(conversation.getCurrent(), nullptr);
}

TEST(Conversation, EndsOnLoopsMissingNodesAndEmptyChoices) {
	core::Log::init(core::Log::Level::Off);
	Dialogue loop;
	loop.nodes = {makeNode(1, NodeKind::Start, 2), makeNode(2, NodeKind::Event, 2)};
	Conversation looping{mkShared<const Dialogue>(loop)};
	EXPECT_FALSE(looping.start({}));
	Dialogue dangling;
	dangling.nodes = {makeNode(1, NodeKind::Start, 42)};
	EXPECT_FALSE(Conversation{mkShared<const Dialogue>(dangling)}.start({}));
	EXPECT_FALSE(Conversation{mkShared<const Dialogue>(Dialogue{})}.start({}));
	Recorder recorder;
	recorder.canOpen = false;
	Dialogue closed = makeDialogue();
	closed.findNode(4)->options[1].condition = "can_open";
	Conversation conversation{mkShared<const Dialogue>(closed)};
	ASSERT_TRUE(conversation.start(recorder.hooks()));
	EXPECT_FALSE(conversation.advance(recorder.hooks()));
	core::Log::invalidate();
}

TEST(Conversation, AnActionMayMoveTheConversation) {
	Dialogue dialogue;
	dialogue.nodes = {makeNode(1, NodeKind::Start, 2), makeHook(2, NodeKind::Event, "jump", 3),
					  makeNode(3, NodeKind::Line)};
	Conversation conversation{mkShared<const Dialogue>(dialogue)};
	const ConversationHooks hooks{.condition = {}, .action = [&conversation](std::string_view) { conversation.stop(); }};
	EXPECT_FALSE(conversation.start(hooks));
	EXPECT_TRUE(conversation.start({}));// no action hook: the event is passed through
	EXPECT_EQ(conversation.getCurrent()->id, 3);
}

TEST(DialogueLibrary, PlaysConversationsByNumber) {
	core::Log::init(core::Log::Level::Off);
	const auto path = std::filesystem::temp_directory_path() / "library.owldlg";
	ASSERT_TRUE(makeDialogue().saveToFile(path));
	DialogueLibrary library;
	Recorder recorder;
	const auto first = library.start(path.generic_string(), recorder.hooks());
	ASSERT_TRUE(first.has_value());
	const auto second = library.start(path.generic_string(), recorder.hooks());
	ASSERT_TRUE(second.has_value());
	EXPECT_NE(*first, *second);
	EXPECT_EQ(library.getLoadCount(), 1);
	EXPECT_EQ(library.getConversations(), (std::vector<uint64_t>{*first, *second}));
	EXPECT_TRUE(library.advance(*first, recorder.hooks()));
	EXPECT_FALSE(library.choose(*first, 1, recorder.hooks()));// leave → end
	EXPECT_EQ(library.find(*first), nullptr);
	library.stop(*second);
	EXPECT_TRUE(library.getConversations().empty());
	EXPECT_FALSE(library.advance(*second, recorder.hooks()));
	EXPECT_FALSE(library.start("missing.owldlg", recorder.hooks()).has_value());
	library.invalidate(path.generic_string());
	std::ignore = library.get(path.generic_string());
	EXPECT_EQ(library.getLoadCount(), 3);
	const auto third = library.start(path.generic_string(), recorder.hooks());
	ASSERT_TRUE(third.has_value());
	library.invalidateAll();
	EXPECT_NE(library.find(*third), nullptr);// a playing conversation keeps its copy
	std::ignore = library.get(path.generic_string());
	EXPECT_EQ(library.getLoadCount(), 4);
	library.clear();
	EXPECT_EQ(library.find(*third), nullptr);
	std::filesystem::remove(path);
	core::Log::invalidate();
}

TEST(DialogueLibrary, AHookMayStopItsOwnConversation) {
	core::Log::init(core::Log::Level::Off);
	const auto path = std::filesystem::temp_directory_path() / "selfstop.owldlg";
	ASSERT_TRUE(makeDialogue().saveToFile(path));
	DialogueLibrary library;
	Recorder recorder;
	const auto id = library.start(path.generic_string(), recorder.hooks());
	ASSERT_TRUE(id.has_value());
	ASSERT_TRUE(library.advance(*id, recorder.hooks()));
	const ConversationHooks stopping{.condition = {},
									 .action = [&library, &id](std::string_view) { library.stop(*id); }};
	EXPECT_FALSE(library.choose(*id, 0, stopping));
	EXPECT_EQ(library.find(*id), nullptr);
	std::filesystem::remove(path);
	core::Log::invalidate();
}
