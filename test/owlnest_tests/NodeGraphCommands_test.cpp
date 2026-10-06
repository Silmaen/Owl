/**
 * @file NodeGraphCommands_test.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "commands/NodeGraphCommands.h"
#include "UndoManager.h"
#include "testHelper.h"

#include <string>

using namespace owl;
using namespace owl::nest;
using namespace owl::nest::commands;
using gui::widgets::Node;
using gui::widgets::NodeCanvas;
using gui::widgets::NodePin;
using gui::widgets::PinKind;

namespace {

auto makeNode(const std::string& iTitle) -> Node {
	Node node;
	node.id = core::UUID{};
	node.title = iTitle;
	node.inputs.push_back({.id = core::UUID{}, .label = "in", .typeTag = {}, .kind = PinKind::Input, .labelColor = {}});
	node.outputs.push_back(
			{.id = core::UUID{}, .label = "out", .typeTag = {}, .kind = PinKind::Output, .labelColor = {}});
	return node;
}

class NodeGraphCommandsTest : public ::testing::Test {
protected:
	NodeCanvas m_canvas;
	UndoManager<NodeCanvas> m_undo;
};

}// namespace

TEST_F(NodeGraphCommandsTest, AddNodeUndoRedo) {
	const auto node = makeNode("A");
	m_undo.execute(mkUniq<AddNodeCommand>(node), m_canvas);
	ASSERT_EQ(m_canvas.nodes().size(), 1u);
	EXPECT_EQ(m_undo.lastSelectionHint(), node.id);

	m_undo.undo(m_canvas);
	EXPECT_TRUE(m_canvas.nodes().empty());

	m_undo.redo(m_canvas);
	ASSERT_NE(m_canvas.findNode(node.id), nullptr);
	EXPECT_EQ(m_canvas.findNode(node.id)->title, "A");
}

TEST_F(NodeGraphCommandsTest, RemoveNodeUndoRestoresLinks) {
	const auto from = makeNode("From");
	const auto to = makeNode("To");
	m_canvas.addNode(from);
	m_canvas.addNode(to);
	m_canvas.addLink(from.outputs[0].id, to.inputs[0].id);
	const auto links = m_canvas.links();

	m_undo.execute(mkUniq<RemoveNodeCommand>(to, links), m_canvas);
	EXPECT_EQ(m_canvas.nodes().size(), 1u);
	EXPECT_TRUE(m_canvas.links().empty());

	m_undo.undo(m_canvas);
	EXPECT_EQ(m_canvas.nodes().size(), 2u);
	ASSERT_EQ(m_canvas.links().size(), 1u);
	EXPECT_EQ(m_canvas.links()[0].fromPin, from.outputs[0].id);
	EXPECT_EQ(m_canvas.links()[0].toPin, to.inputs[0].id);
}

TEST_F(NodeGraphCommandsTest, MoveNodeMergesDrag) {
	const auto node = makeNode("A");
	m_canvas.addNode(node);
	m_canvas.findNode(node.id)->position = {30.f, 0.f};
	m_undo.push(mkUniq<MoveNodeCommand>(node.id, math::vec2f{0.f, 0.f}, math::vec2f{10.f, 0.f}));
	m_undo.push(mkUniq<MoveNodeCommand>(node.id, math::vec2f{10.f, 0.f}, math::vec2f{30.f, 0.f}));

	m_undo.undo(m_canvas);
	EXPECT_FLOAT_EQ(m_canvas.findNode(node.id)->position.x(), 0.f);
	EXPECT_FALSE(m_undo.canUndo());
	m_undo.redo(m_canvas);
	EXPECT_FLOAT_EQ(m_canvas.findNode(node.id)->position.x(), 30.f);
}

TEST_F(NodeGraphCommandsTest, LinkUndoRedo) {
	const auto from = makeNode("From");
	const auto to = makeNode("To");
	m_canvas.addNode(from);
	m_canvas.addNode(to);

	m_undo.execute(mkUniq<AddLinkCommand>(from.outputs[0].id, to.inputs[0].id), m_canvas);
	ASSERT_EQ(m_canvas.links().size(), 1u);
	m_undo.undo(m_canvas);
	EXPECT_TRUE(m_canvas.links().empty());
	m_undo.redo(m_canvas);
	ASSERT_EQ(m_canvas.links().size(), 1u);

	m_undo.execute(mkUniq<RemoveLinkCommand>(m_canvas.links()[0]), m_canvas);
	EXPECT_TRUE(m_canvas.links().empty());
	m_undo.undo(m_canvas);
	EXPECT_EQ(m_canvas.links().size(), 1u);
}

TEST_F(NodeGraphCommandsTest, OutputPinUndoRedo) {
	const auto node = makeNode("A");
	m_canvas.addNode(node);
	const NodePin pin{.id = core::UUID{}, .label = "extra", .typeTag = {}, .kind = PinKind::Output, .labelColor = {}};

	m_undo.execute(mkUniq<AddOutputPinCommand>(node.id, pin), m_canvas);
	EXPECT_EQ(m_canvas.findNode(node.id)->outputs.size(), 2u);
	m_undo.undo(m_canvas);
	EXPECT_EQ(m_canvas.findNode(node.id)->outputs.size(), 1u);

	m_undo.execute(mkUniq<RemoveOutputPinCommand>(node.id, node.outputs[0]), m_canvas);
	EXPECT_TRUE(m_canvas.findNode(node.id)->outputs.empty());
	m_undo.undo(m_canvas);
	EXPECT_EQ(m_canvas.findNode(node.id)->outputs.size(), 1u);
}
