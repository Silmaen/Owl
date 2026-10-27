/**
 * @file VisualScriptCanvas_test.cpp
 * @author Silmaen
 * @date 27/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "document/VisualScriptCanvas.h"
#include "testHelper.h"

#include <core/Log.h>
#include <platform/FileUtils.h>
#include <script/visual/VisualScript.h>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace owl;
using namespace owl::nest;
namespace vs = owl::script::visual;

namespace {

// Wider than the editor font, so a graph that fits here fits on the canvas.
auto measure(const std::string_view iText) -> float { return 8.f * static_cast<float>(iText.size()); }

auto node(const std::string& iType, std::map<std::string, std::string> iValues = {}) -> vs::GraphNode {
	return {.id = core::UUID{}, .type = iType, .position = {0.f, 0.f}, .values = std::move(iValues)};
}

}// namespace

TEST(VisualScriptCanvas, PinsOfARowShareItsHeight) {
	vs::VisualScript script;
	script.nodes.push_back(node("flow.for"));
	const auto layout = layoutNode(script, script.nodes.front(), measure);
	ASSERT_EQ(layout.inputs.size(), 3u);
	ASSERT_EQ(layout.outputs.size(), 3u);
	for (size_t i = 0; i < 3; ++i) {
		EXPECT_FLOAT_EQ(layout.inputs[i][1], layout.outputs[i][1]);
		EXPECT_GT(layout.inputs[i][1], layout.headerHeight);
		EXPECT_LT(layout.outputs[i][1], layout.size[1]);
		EXPECT_LT(layout.inputs[i][0], layout.outputs[i][0]);
	}
	EXPECT_GT(layout.inputs[1][1], layout.inputs[0][1]);
}

TEST(VisualScriptCanvas, LinkedInputsHaveNoField) {
	vs::VisualScript script;
	script.nodes.push_back(node("math.add"));
	script.nodes.push_back(node("value.number"));
	const auto alone = layoutNode(script, script.nodes.front(), measure);
	EXPECT_GT(alone.fields[0][1], 0.f);
	script.links.push_back(
			{.fromNode = script.nodes[1].id, .fromPin = "Result", .toNode = script.nodes[0].id, .toPin = "A"});
	const auto linked = layoutNode(script, script.nodes.front(), measure);
	EXPECT_FLOAT_EQ(linked.fields[0][1], 0.f);
	EXPECT_GT(linked.fields[1][1], 0.f);
}

TEST(VisualScriptCanvas, TitlesAndStyle) {
	OWL_REQUIRE_MODULE(SCRIPT);
	const auto* call = vs::findNodeType("call.transform.get_position");
	ASSERT_NE(call, nullptr);
	EXPECT_EQ(getNodeTitle(node(call->id), *call), "Get Position");
	EXPECT_EQ(getNodeSubtitle(*call), "transform");
	const auto* set = vs::findNodeType("var.set");
	EXPECT_EQ(getNodeTitle(node(set->id, {{"variable", "speed"}}), *set), "Set speed");
	EXPECT_EQ(getPinTypeName(vs::PinType::Color), "color");
	EXPECT_NE(getPinColor(vs::PinType::Exec), getPinColor(vs::PinType::Number));
	for (const auto& type: vs::getNodeTypes()) {
		EXPECT_NE(getNodeGlyph(type), nullptr) << type.id;
		EXPECT_TRUE(std::string_view{getNodeIcon(type)}.starts_with("vs_node_")) << type.id;
	}
	EXPECT_EQ(std::string_view{getPinTypeIcon(vs::PinType::Vector3)}, "vs_type_vec3");
	EXPECT_NE(std::string_view{getPinGlyph(vs::PinType::Boolean)}, std::string_view{getPinGlyph(vs::PinType::Number)});
}

TEST(VisualScriptCanvas, EngineDemoGraphsDoNotOverlap) {
	OWL_REQUIRE_MODULE(SCRIPT);
	core::Log::init(core::Log::Level::Off);
	size_t graphs = 0;
	for (const auto& entry: std::filesystem::recursive_directory_iterator(test::getRootPath() / "engine_demo")) {
		if (!entry.is_regular_file() || entry.path().extension() != vs::g_VisualScriptExtension)
			continue;
		++graphs;
		const auto script = vs::deserialize(platform::fileToString(entry.path()), entry.path().string());
		ASSERT_TRUE(script.has_value()) << entry.path();
		std::vector<std::array<float, 4>> boxes;
		for (const auto& graphNode: script->nodes) {
			const auto layout = layoutNode(*script, graphNode, measure);
			boxes.push_back({graphNode.position[0], graphNode.position[1], graphNode.position[0] + layout.size[0],
							 graphNode.position[1] + layout.size[1]});
		}
		for (size_t i = 0; i < boxes.size(); ++i)
			for (size_t j = i + 1; j < boxes.size(); ++j) {
				const bool overlap = boxes[i][0] < boxes[j][2] && boxes[j][0] < boxes[i][2] &&
									 boxes[i][1] < boxes[j][3] && boxes[j][1] < boxes[i][3];
				EXPECT_FALSE(overlap) << entry.path() << ": nodes " << i << " and " << j;
			}
	}
	EXPECT_GT(graphs, 0u);
	core::Log::invalidate();
}
