/**
 * @file NodeCanvasSerializer_test.cpp
 * @author Silmaen
 * @date 24/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "testHelper.h"

#include "gui/widgets/NodeCanvas.h"
#include "gui/widgets/NodeCanvasSerializer.h"

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
		pin.typeTag = "test";
		pin.kind = PinKind::Input;
		node.inputs.push_back(std::move(pin));
	}
	for (auto& label: iOutputLabels) {
		NodePin pin;
		pin.label = std::move(label);
		pin.typeTag = "test";
		pin.kind = PinKind::Output;
		node.outputs.push_back(std::move(pin));
	}
	return node;
}

}// namespace

TEST(NodeCanvasSerializer, EmptyCanvasRoundTrip) {
	NodeCanvas src;
	const auto yaml = NodeCanvasSerializer::serializeToString(src, "empty");

	NodeCanvas dst;
	ASSERT_TRUE(NodeCanvasSerializer::deserializeFromString(dst, yaml));
	EXPECT_TRUE(dst.nodes().empty());
	EXPECT_TRUE(dst.links().empty());
}

TEST(NodeCanvasSerializer, FullCanvasRoundTripPreservesTopology) {
	NodeCanvas src;
	auto a = makeNode("A", {10.0f, 20.0f}, {}, {"out_a"});
	auto b = makeNode("B", {100.0f, 50.0f}, {"in_b"}, {"out_b"});
	auto c = makeNode("C", {200.0f, 80.0f}, {"in_c"}, {});
	const auto pinAOut = a.outputs[0].id;
	const auto pinBIn = b.inputs[0].id;
	const auto pinBOut = b.outputs[0].id;
	const auto pinCIn = c.inputs[0].id;
	const auto idA = src.addNode(std::move(a));
	const auto idB = src.addNode(std::move(b));
	const auto idC = src.addNode(std::move(c));
	src.addLink(pinAOut, pinBIn);
	src.addLink(pinBOut, pinCIn);

	const auto yaml = NodeCanvasSerializer::serializeToString(src, "test_flow");

	NodeCanvas dst;
	ASSERT_TRUE(NodeCanvasSerializer::deserializeFromString(dst, yaml));
	EXPECT_EQ(dst.nodes().size(), 3u);
	EXPECT_EQ(dst.links().size(), 2u);

	// UUIDs must be preserved — round-trip is stable (paste is a separate operation).
	ASSERT_NE(dst.findNode(idA), nullptr);
	ASSERT_NE(dst.findNode(idB), nullptr);
	ASSERT_NE(dst.findNode(idC), nullptr);
	EXPECT_EQ(dst.findNode(idA)->title, "A");
	EXPECT_EQ(dst.findNode(idB)->position.x(), 100.0f);
}

TEST(NodeCanvasSerializer, CustomDataRoundTrips) {
	NodeCanvas src;
	auto a = makeNode("Scene1", {0.0f, 0.0f});
	a.customData = "scenePath: scenes/level1.owl\ntriggerCount: 3\n";
	const auto id = src.addNode(std::move(a));

	const auto yaml = NodeCanvasSerializer::serializeToString(src);
	NodeCanvas dst;
	ASSERT_TRUE(NodeCanvasSerializer::deserializeFromString(dst, yaml));
	const auto* node = dst.findNode(id);
	ASSERT_NE(node, nullptr);
	EXPECT_NE(node->customData.find("scenePath: scenes/level1.owl"), std::string::npos);
	EXPECT_NE(node->customData.find("triggerCount: 3"), std::string::npos);
}

TEST(NodeCanvasSerializer, MalformedYamlReturnsFalse) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	NodeCanvas dst;
	// Unclosed flow sequence — yaml-cpp raises on this.
	EXPECT_FALSE(NodeCanvasSerializer::deserializeFromString(dst, "Nodes: [ {id: 1, title: broken\n"));
	owl::core::Log::invalidate();
}

TEST(NodeCanvasSerializer, DeserializeClearsPriorContents) {
	NodeCanvas dst;
	dst.addNode(makeNode("Old", {0.0f, 0.0f}));

	NodeCanvas src;
	src.addNode(makeNode("New", {50.0f, 0.0f}));
	const auto yaml = NodeCanvasSerializer::serializeToString(src);

	ASSERT_TRUE(NodeCanvasSerializer::deserializeFromString(dst, yaml));
	EXPECT_EQ(dst.nodes().size(), 1u);
	EXPECT_EQ(dst.nodes()[0].title, "New");
}

TEST(NodeCanvasSerializer, SubsetIncludesOnlyRequestedNodes) {
	NodeCanvas src;
	const auto idA = src.addNode(makeNode("A", {0.0f, 0.0f}, {}, {"o"}));
	const auto idB = src.addNode(makeNode("B", {100.0f, 0.0f}, {"i"}, {}));
	src.addNode(makeNode("C", {200.0f, 0.0f}));// not in subset
	src.addLink(src.nodes()[0].outputs[0].id, src.nodes()[1].inputs[0].id);

	const std::vector<core::UUID> subsetIds{idA, idB};
	const auto yaml = NodeCanvasSerializer::serializeSubset(src, subsetIds);
	EXPECT_FALSE(yaml.empty());

	NodeCanvas dst;
	ASSERT_TRUE(NodeCanvasSerializer::deserializeFromString(dst, yaml));
	EXPECT_EQ(dst.nodes().size(), 2u);
	EXPECT_EQ(dst.links().size(), 1u);
}

TEST(NodeCanvasSerializer, PasteGeneratesFreshUuids) {
	NodeCanvas src;
	const auto idA = src.addNode(makeNode("A", {0.0f, 0.0f}, {}, {"o"}));
	const auto idB = src.addNode(makeNode("B", {100.0f, 0.0f}, {"i"}, {}));
	src.addLink(src.nodes()[0].outputs[0].id, src.nodes()[1].inputs[0].id);

	const std::vector<core::UUID> subsetIds{idA, idB};
	const auto yaml = NodeCanvasSerializer::serializeSubset(src, subsetIds);

	// Paste into the same canvas — new copies must get fresh UUIDs so we end up with 4 nodes.
	const auto pasted = NodeCanvasSerializer::pasteSubset(src, yaml);
	EXPECT_EQ(pasted.size(), 2u);
	EXPECT_EQ(src.nodes().size(), 4u);
	EXPECT_EQ(src.links().size(), 2u);// original + pasted link

	// None of the freshly pasted UUIDs collide with the originals.
	for (const auto newId: pasted) {
		EXPECT_NE(newId, idA);
		EXPECT_NE(newId, idB);
	}
}

TEST(NodeCanvasSerializer, EmptySubsetReturnsEmptyString) {
	NodeCanvas src;
	src.addNode(makeNode("A", {0.0f, 0.0f}));
	EXPECT_TRUE(NodeCanvasSerializer::serializeSubset(src, {}).empty());
}
