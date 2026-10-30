/**
 * @file DialogueDocument_test.cpp
 * @author Silmaen
 * @date 30/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "document/DialogueDocument.h"

#include <core/Log.h>
#include <data/dialogue/Dialogue.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

using namespace owl;
using namespace owl::nest;
using data::dialogue::Dialogue;
using data::dialogue::DialogueNode;
using data::dialogue::NodeKind;

namespace {

auto makeNode(const uint64_t iId, const NodeKind iKind, const uint64_t iNext = 0) -> DialogueNode {
	DialogueNode node;
	node.id = iId;
	node.kind = iKind;
	node.next = iNext;
	return node;
}

// Start → line → branch(has_key ? choice : end); the choice offers "open" (event → end) and a silent answer.
auto makeDialogue() -> Dialogue {
	auto line = makeNode(2, NodeKind::Line, 3);
	line.speaker.text = "Guard";
	line.text.text = "Halt! Who goes there, at this late hour?";
	auto branch = makeNode(3, NodeKind::Branch, 4);
	branch.function = "has_key";
	branch.nextFalse = 6;
	auto choice = makeNode(4, NodeKind::Choice);
	choice.options = {{.text = {.text = "Open", .key = {}}, .condition = "", .next = 5},
					  {.text = {.text = {}, .key = {}}, .condition = "is_rich", .next = 6}};
	auto event = makeNode(5, NodeKind::Event, 6);
	event.function = "open_gate";
	Dialogue dialogue;
	dialogue.nodes = {makeNode(1, NodeKind::Start, 2), line, branch, choice, event, makeNode(6, NodeKind::End)};
	return dialogue;
}

class DialogueDocumentTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_root = std::filesystem::temp_directory_path() / "owl_dialogue_document_test";
		std::filesystem::remove_all(m_root);
		std::filesystem::create_directories(m_root);
		m_path = m_root / "guard.owldlg";
		ASSERT_TRUE(makeDialogue().saveToFile(m_path));
		m_document.onAttach(nullptr);
		ASSERT_TRUE(m_document.loadFromFile(m_path));
	}

	void TearDown() override {
		m_document.onDetach();
		std::filesystem::remove_all(m_root);
		core::Log::invalidate();
	}

	std::filesystem::path m_root;
	std::filesystem::path m_path;
	DialogueDocument m_document;
};

}// namespace

TEST(DialogueDocument, TitlesAndLabelsNameWhatANodeDoes) {
	const auto dialogue = makeDialogue();
	EXPECT_NE(getDialogueNodeTitle(*dialogue.findNode(1)).find("Start"), std::string::npos);
	const auto line = getDialogueNodeTitle(*dialogue.findNode(2));
	EXPECT_NE(line.find("Guard: Halt!"), std::string::npos);
	EXPECT_NE(line.find("…"), std::string::npos);// shortened
	EXPECT_NE(getDialogueNodeTitle(*dialogue.findNode(3)).find("has_key?"), std::string::npos);
	EXPECT_NE(getDialogueNodeTitle(*dialogue.findNode(5)).find("open_gate()"), std::string::npos);
	EXPECT_NE(getDialogueNodeTitle(makeNode(9, NodeKind::Line)).find("Line"), std::string::npos);
	EXPECT_NE(getDialogueNodeTitle(makeNode(9, NodeKind::Event)).find("Event"), std::string::npos);
	EXPECT_EQ(getDialogueOutputLabel(*dialogue.findNode(3), 0), "True");
	EXPECT_EQ(getDialogueOutputLabel(*dialogue.findNode(3), 1), "False");
	EXPECT_EQ(getDialogueOutputLabel(*dialogue.findNode(4), 0), "Open");
	EXPECT_EQ(getDialogueOutputLabel(*dialogue.findNode(4), 1), "Answer 2");
	EXPECT_EQ(getDialogueOutputLabel(*dialogue.findNode(2), 0), "Next");
	for (const auto kind: data::dialogue::getNodeKinds()) EXPECT_NE(getDialogueNodeGlyph(kind), nullptr);
}

TEST_F(DialogueDocumentTest, EditsAreUndoneRedoneAndSaved) {
	EXPECT_EQ(m_document.type(), DocumentType::Dialogue);
	EXPECT_EQ(m_document.title(), "guard");
	EXPECT_FALSE(m_document.isDirty());
	auto next = m_document.dialogue();
	next.findNode(2)->text.text = "Stop!";
	EXPECT_FALSE(m_document.apply(m_document.dialogue(), "Nothing", false));
	ASSERT_TRUE(m_document.apply(next, "Edit node", true));
	next.findNode(2)->text.text = "Stop there!";
	ASSERT_TRUE(m_document.apply(next, "Edit node", true));// merges with the typing before
	EXPECT_TRUE(m_document.isDirty());
	ASSERT_TRUE(m_document.canUndo());
	m_document.performUndo();
	EXPECT_EQ(m_document.dialogue().findNode(2)->text.text, makeDialogue().findNode(2)->text.text);
	EXPECT_FALSE(m_document.canUndo());
	m_document.performRedo();
	EXPECT_EQ(m_document.dialogue().findNode(2)->text.text, "Stop there!");
	ASSERT_TRUE(m_document.save());
	EXPECT_FALSE(m_document.isDirty());
	const auto read = Dialogue::loadFile(m_path);
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(*read, m_document.dialogue());
	EXPECT_FALSE(m_document.loadFromFile(m_root / "missing.owldlg"));
}

TEST_F(DialogueDocumentTest, RecoveryRestoresTheDialogue) {
	auto next = m_document.dialogue();
	next.nodes.pop_back();
	ASSERT_TRUE(m_document.apply(next, "Remove node", false));
	const auto snapshot = m_document.recoverySnapshot();
	ASSERT_TRUE(snapshot.has_value());
	ASSERT_TRUE(m_document.loadFromFile(m_path));
	EXPECT_EQ(m_document.dialogue().nodes.size(), 6u);
	ASSERT_TRUE(m_document.restoreRecoverySnapshot(*snapshot));
	EXPECT_EQ(m_document.dialogue().nodes.size(), 5u);
	EXPECT_TRUE(m_document.isDirty());
	EXPECT_FALSE(m_document.restoreRecoverySnapshot("not: [a dialogue"));
}

TEST_F(DialogueDocumentTest, PreviewPlaysWithTheConditionsTicked) {
	EXPECT_FALSE(m_document.getPreview().has_value());
	m_document.setPreviewShown(true);
	ASSERT_TRUE(m_document.getPreview().has_value());
	ASSERT_NE(m_document.getPreview()->getCurrent(), nullptr);
	EXPECT_EQ(m_document.getPreview()->getCurrent()->id, 2);
	EXPECT_EQ(m_document.getSelectedNode(), core::UUID{2});// the graph follows the preview
	m_document.advancePreview();
	EXPECT_EQ(m_document.getPreview()->getCurrent(), nullptr);// has_key is not ticked: the branch ends
	m_document.setPreviewCondition("has_key", true);
	m_document.restartPreview();
	m_document.advancePreview();
	ASSERT_NE(m_document.getPreview()->getCurrent(), nullptr);
	EXPECT_EQ(m_document.getPreview()->getCurrent()->id, 4);
	EXPECT_EQ(m_document.getPreview()->getOfferedOptions().size(), 1u);// is_rich is not ticked
	m_document.choosePreview(0);
	EXPECT_EQ(m_document.getPreviewActions(), std::vector<std::string>{"open_gate"});
	EXPECT_EQ(m_document.getPreview()->getCurrent(), nullptr);
	auto next = m_document.dialogue();
	next.findNode(2)->text.text = "Stop!";
	ASSERT_TRUE(m_document.apply(next, "Edit node", false));
	EXPECT_TRUE(m_document.getPreviewActions().empty());// an edit plays the preview again
	EXPECT_EQ(m_document.getPreview()->getCurrent()->text.text, "Stop!");
	m_document.setPreviewShown(false);
	EXPECT_FALSE(m_document.getPreview().has_value());
}

TEST_F(DialogueDocumentTest, SelectionKeepsOnlyExistingNodes) {
	m_document.selectNodes({core::UUID{4}, core::UUID{5}});
	EXPECT_EQ(m_document.getSelectedNode(), core::UUID{4});
	auto next = m_document.dialogue();
	std::erase_if(next.nodes, [](const DialogueNode& iNode) -> bool { return iNode.id == 4; });
	ASSERT_TRUE(m_document.apply(next, "Remove node", false));
	m_document.selectNodes({});
	EXPECT_FALSE(m_document.getSelectedNode().has_value());
	EXPECT_FALSE(m_document.addNode(NodeKind::Line, {0.f, 0.f}).has_value());// no editor to run the command
}
