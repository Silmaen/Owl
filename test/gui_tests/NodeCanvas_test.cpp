/**
 * @file NodeCanvas_test.cpp
 * @author Silmaen
 * @date 24/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "testHelper.h"

#include "gui/widgets/NodeCanvas.h"

using namespace owl;
using namespace owl::gui::widgets;

namespace {
auto makeNode(const std::string& iTitle, math::vec2f iPos,
			  std::vector<std::string> iInputLabels = {},
			  std::vector<std::string> iOutputLabels = {}) -> Node {
	Node node;
	node.title = iTitle;
	node.position = iPos;
	for (auto& label: iInputLabels) {
		NodePin pin;
		pin.label = std::move(label);
		pin.typeTag = "generic";
		pin.kind = PinKind::Input;
		node.inputs.push_back(std::move(pin));
	}
	for (auto& label: iOutputLabels) {
		NodePin pin;
		pin.label = std::move(label);
		pin.typeTag = "generic";
		pin.kind = PinKind::Output;
		node.outputs.push_back(std::move(pin));
	}
	return node;
}

}// namespace

TEST(NodeCanvas, AddNodeReturnsStableUuid) {
	NodeCanvas canvas;
	const auto id1 = canvas.addNode(makeNode("A", {0.0f, 0.0f}));
	const auto id2 = canvas.addNode(makeNode("B", {100.0f, 0.0f}));
	EXPECT_NE(static_cast<uint64_t>(id1), 0u);
	EXPECT_NE(static_cast<uint64_t>(id2), 0u);
	EXPECT_NE(static_cast<uint64_t>(id1), static_cast<uint64_t>(id2));
	EXPECT_EQ(canvas.nodes().size(), 2u);
}

TEST(NodeCanvas, FindNodeByUuid) {
	NodeCanvas canvas;
	const auto idA = canvas.addNode(makeNode("A", {0.0f, 0.0f}));
	canvas.addNode(makeNode("B", {100.0f, 0.0f}));

	const auto* node = canvas.findNode(idA);
	ASSERT_NE(node, nullptr);
	EXPECT_EQ(node->title, "A");
	EXPECT_EQ(canvas.findNode(core::UUID{12345}), nullptr);
}

TEST(NodeCanvas, RemoveNodeDropsAttachedLinks) {
	NodeCanvas canvas;
	auto nodeA = makeNode("A", {0.0f, 0.0f}, {}, {"out"});
	auto nodeB = makeNode("B", {100.0f, 0.0f}, {"in"}, {});
	const auto pinOut = nodeA.outputs[0].id;
	const auto pinIn = nodeB.inputs[0].id;
	const auto idA = canvas.addNode(std::move(nodeA));
	canvas.addNode(std::move(nodeB));

	canvas.addLink(pinOut, pinIn);
	EXPECT_EQ(canvas.links().size(), 1u);

	canvas.removeNode(idA);
	EXPECT_EQ(canvas.nodes().size(), 1u);
	EXPECT_EQ(canvas.links().size(), 0u)
			<< "Links touching a removed node's pins must be dropped along with the node.";
}

TEST(NodeCanvas, ProgrammaticAddLinkBypassesValidator) {
	// Validator gates **user drags** (`Impl::AddLink` from GraphEditor) only — programmatic adds
	// must always succeed so refresh paths and undo/redo of removed links work regardless of the
	// validator's domain rules.
	NodeCanvas canvas;
	auto nodeA = makeNode("A", {0.0f, 0.0f}, {}, {"out"});
	auto nodeB = makeNode("B", {100.0f, 0.0f}, {"in"}, {});
	const auto pinOut = nodeA.outputs[0].id;
	const auto pinIn = nodeB.inputs[0].id;
	canvas.addNode(std::move(nodeA));
	canvas.addNode(std::move(nodeB));

	canvas.setLinkValidator([](core::UUID, core::UUID) -> bool { return false; });
	const auto id = canvas.addLink(pinOut, pinIn);
	EXPECT_NE(static_cast<uint64_t>(id), 0u);
	EXPECT_EQ(canvas.links().size(), 1u);
}

TEST(NodeCanvas, FindNodeByPinWalksBothSides) {
	NodeCanvas canvas;
	auto nodeA = makeNode("A", {0.0f, 0.0f}, {"in_a"}, {"out_a"});
	const auto pinIn = nodeA.inputs[0].id;
	const auto pinOut = nodeA.outputs[0].id;
	const auto idA = canvas.addNode(std::move(nodeA));

	const auto* byIn = canvas.findNodeByPin(pinIn);
	const auto* byOut = canvas.findNodeByPin(pinOut);
	ASSERT_NE(byIn, nullptr);
	ASSERT_NE(byOut, nullptr);
	EXPECT_EQ(byIn->id, idA);
	EXPECT_EQ(byOut->id, idA);
	EXPECT_EQ(canvas.findNodeByPin(core::UUID{99999}), nullptr);
}

TEST(NodeCanvas, SelectionRoundTripsThroughIds) {
	NodeCanvas canvas;
	const auto idA = canvas.addNode(makeNode("A", {0.0f, 0.0f}));
	const auto idB = canvas.addNode(makeNode("B", {100.0f, 0.0f}));
	canvas.addNode(makeNode("C", {200.0f, 0.0f}));

	const std::vector<core::UUID> toSelect{idA, idB};
	canvas.setSelection(toSelect);

	const auto selection = canvas.selection();
	EXPECT_EQ(selection.size(), 2u);
	EXPECT_NE(std::ranges::find(selection, idA), selection.end());
	EXPECT_NE(std::ranges::find(selection, idB), selection.end());
}

TEST(NodeCanvas, ClearResetsAll) {
	NodeCanvas canvas;
	auto nodeA = makeNode("A", {0.0f, 0.0f}, {}, {"out"});
	auto nodeB = makeNode("B", {100.0f, 0.0f}, {"in"}, {});
	const auto pinOut = nodeA.outputs[0].id;
	const auto pinIn = nodeB.inputs[0].id;
	canvas.addNode(std::move(nodeA));
	canvas.addNode(std::move(nodeB));
	canvas.addLink(pinOut, pinIn);

	canvas.clear();
	EXPECT_TRUE(canvas.nodes().empty());
	EXPECT_TRUE(canvas.links().empty());
}

TEST(NodeCanvas, RemoveLinkByIdWorks) {
	NodeCanvas canvas;
	auto nodeA = makeNode("A", {0.0f, 0.0f}, {}, {"out"});
	auto nodeB = makeNode("B", {100.0f, 0.0f}, {"in"}, {});
	const auto pinOut = nodeA.outputs[0].id;
	const auto pinIn = nodeB.inputs[0].id;
	canvas.addNode(std::move(nodeA));
	canvas.addNode(std::move(nodeB));

	const auto linkId = canvas.addLink(pinOut, pinIn);
	ASSERT_NE(static_cast<uint64_t>(linkId), 0u);
	ASSERT_NE(canvas.findLink(linkId), nullptr);

	canvas.removeLink(linkId);
	EXPECT_EQ(canvas.findLink(linkId), nullptr);
	EXPECT_EQ(canvas.links().size(), 0u);
}

TEST(NodeCanvas, AddOutputPinAppendsAndKeepsId) {
	NodeCanvas canvas;
	const auto nodeId = canvas.addNode(makeNode("A", {0.0f, 0.0f}));
	NodePin pin;
	pin.label = "out";
	pin.typeTag = "generic";
	const auto pinId = canvas.addOutputPin(nodeId, pin);
	EXPECT_NE(static_cast<uint64_t>(pinId), 0u);
	const auto* node = canvas.findNode(nodeId);
	ASSERT_NE(node, nullptr);
	ASSERT_EQ(node->outputs.size(), 1u);
	EXPECT_EQ(node->outputs[0].id, pinId);
	EXPECT_EQ(node->outputs[0].kind, PinKind::Output);
}

TEST(NodeCanvas, AddOutputPinFailsForUnknownNode) {
	NodeCanvas canvas;
	NodePin pin;
	pin.label = "out";
	const auto pinId = canvas.addOutputPin(core::UUID{99999}, pin);
	EXPECT_EQ(static_cast<uint64_t>(pinId), 0u);
}

TEST(NodeCanvas, RemoveOutputPinDropsDanglingLinks) {
	NodeCanvas canvas;
	auto nodeA = makeNode("A", {0.0f, 0.0f}, {}, {"out"});
	auto nodeB = makeNode("B", {100.0f, 0.0f}, {"in"}, {});
	const auto pinOut = nodeA.outputs[0].id;
	const auto pinIn = nodeB.inputs[0].id;
	const auto idA = canvas.addNode(std::move(nodeA));
	canvas.addNode(std::move(nodeB));
	canvas.addLink(pinOut, pinIn);
	ASSERT_EQ(canvas.links().size(), 1u);

	canvas.removeOutputPin(idA, pinOut);
	const auto* node = canvas.findNode(idA);
	ASSERT_NE(node, nullptr);
	EXPECT_TRUE(node->outputs.empty());
	EXPECT_TRUE(canvas.links().empty()) << "Removing a pin must strip every link still attached to it.";
}

TEST(NodeCanvas, AddInputPinAppendsAndKeepsId) {
	NodeCanvas canvas;
	const auto nodeId = canvas.addNode(makeNode("A", {0.0f, 0.0f}));
	NodePin pin;
	pin.label = "in";
	pin.typeTag = "generic";
	const auto pinId = canvas.addInputPin(nodeId, pin);
	EXPECT_NE(static_cast<uint64_t>(pinId), 0u);
	const auto* node = canvas.findNode(nodeId);
	ASSERT_NE(node, nullptr);
	ASSERT_EQ(node->inputs.size(), 1u);
	EXPECT_EQ(node->inputs[0].kind, PinKind::Input);
}

TEST(NodeCanvas, RemoveInputPinDropsDanglingLinks) {
	NodeCanvas canvas;
	auto nodeA = makeNode("A", {0.0f, 0.0f}, {}, {"out"});
	auto nodeB = makeNode("B", {100.0f, 0.0f}, {"in"}, {});
	const auto pinOut = nodeA.outputs[0].id;
	const auto pinIn = nodeB.inputs[0].id;
	canvas.addNode(std::move(nodeA));
	const auto idB = canvas.addNode(std::move(nodeB));
	canvas.addLink(pinOut, pinIn);
	ASSERT_EQ(canvas.links().size(), 1u);

	canvas.removeInputPin(idB, pinIn);
	EXPECT_TRUE(canvas.links().empty());
}

TEST(NodeCanvas, ZoomLodThresholds) {
	// Pin labels appear above 0.6, disappear at or below.
	EXPECT_TRUE(shouldDrawPinLabels(1.0f));
	EXPECT_TRUE(shouldDrawPinLabels(0.61f));
	EXPECT_FALSE(shouldDrawPinLabels(0.6f));
	EXPECT_FALSE(shouldDrawPinLabels(0.3f));

	// Node titles survive longer — only fade out at the lowest zoom band.
	EXPECT_TRUE(shouldDrawNodeTitles(1.0f));
	EXPECT_TRUE(shouldDrawNodeTitles(0.5f));
	EXPECT_TRUE(shouldDrawNodeTitles(0.31f));
	EXPECT_FALSE(shouldDrawNodeTitles(0.3f));
	EXPECT_FALSE(shouldDrawNodeTitles(0.05f));
}
