/**
 * @file PcgGraph_test.cpp
 * @author Silmaen
 * @date 28/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <scene/TilemapAsset.h>
#include <scene/pcg/PcgGraph.h>
#include <scene/pcg/PcgTilemap.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <variant>

using namespace owl;
using namespace owl::scene;
using namespace owl::scene::pcg;

namespace {
class PcgGraphTest : public testing::Test {
protected:
	static void SetUpTestSuite() { core::Log::init(core::Log::Level::Off); }
};

auto addNode(Graph& ioGraph, const uint64_t iId, const std::string& iType, const float iY = 0.f,
			 std::map<std::string, std::string> iValues = {}) -> core::UUID {
	ioGraph.nodes.push_back({.id = core::UUID{iId}, .type = iType, .position = {0.f, iY}, .values = std::move(iValues)});
	return core::UUID{iId};
}

void link(Graph& ioGraph, const uint64_t iFrom, const std::string& iFromPin, const uint64_t iTo,
		  const std::string& iToPin) {
	ioGraph.links.push_back(
			{.fromNode = core::UUID{iFrom}, .fromPin = iFromPin, .toNode = core::UUID{iTo}, .toPin = iToPin});
}

auto fieldOf(const Result& iResult, const uint64_t iNode, const std::string& iPin) -> const Field& {
	return std::get<Field>(iResult.values.at(core::UUID{iNode}).at(iPin));
}

// Noise thresholded into a mask, written as tile 3.
auto makeNoiseGraph() -> Graph {
	Graph graph;
	addNode(graph, 1, "source.noise", 0.f, {{"scale", "4"}});
	addNode(graph, 2, "field.threshold", 0.f, {{"min", "0.5"}});
	addNode(graph, 3, "output.tiles", 0.f, {{"tile", "3"}});
	link(graph, 1, "Field", 2, "Field");
	link(graph, 2, "Mask", 3, "Mask");
	return graph;
}
}// namespace

TEST_F(PcgGraphTest, NodeTypesAreUniqueAndConnectByType) {
	std::set<std::string> ids;
	for (const auto& type: getNodeTypes()) {
		EXPECT_TRUE(ids.insert(type.id).second) << type.id;
		EXPECT_FALSE(type.title.empty());
		for (const auto& param: type.params)
			EXPECT_TRUE(param.type != ParamType::Choice || !param.choices.empty()) << type.id << "." << param.name;
	}
	EXPECT_NE(findNodeType("source.noise"), nullptr);
	EXPECT_EQ(findNodeType("source.unknown"), nullptr);
	EXPECT_TRUE(canConnect(PinType::Field, PinType::Field));
	EXPECT_FALSE(canConnect(PinType::Points, PinType::Field));
}

TEST_F(PcgGraphTest, SerializationRoundTrips) {
	const auto graph = makeNoiseGraph();
	const auto text = serialize(graph);
	EXPECT_NE(text.find("FormatVersion: 1"), std::string::npos);
	const auto read = deserialize(text, "test");
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(*read, graph);
	EXPECT_EQ(deserialize("[unclosed", "bad").error(), PcgError::InvalidYaml);
	EXPECT_EQ(deserialize("Other: {}", "bad").error(), PcgError::InvalidContent);
	EXPECT_EQ(deserialize("FormatVersion: 99\nPcgGraph: {}", "newer").error(), PcgError::UnsupportedVersion);
	EXPECT_EQ(deserialize("PcgGraph: {nodes: [{id: x}]}", "bad").error(), PcgError::InvalidContent);
}

TEST_F(PcgGraphTest, FilesRoundTrip) {
	const auto file = std::filesystem::temp_directory_path() / "owl_pcg_graph.owlpcg";
	ASSERT_TRUE(saveToFile(makeNoiseGraph(), file));
	const auto read = loadFromFile(file);
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(read->nodes.size(), 3u);
	std::filesystem::remove(file);
	EXPECT_FALSE(loadFromFile(file).has_value());
}

TEST_F(PcgGraphTest, RemovingANodeRemovesItsLinks) {
	auto graph = makeNoiseGraph();
	graph.removeNode(core::UUID{2});
	EXPECT_EQ(graph.nodes.size(), 2u);
	EXPECT_TRUE(graph.links.empty());
	EXPECT_EQ(graph.findNode(core::UUID{2}), nullptr);
	EXPECT_EQ(Graph::getValue(*graph.findNode(core::UUID{1}), "octaves"), "3");
	EXPECT_EQ(Graph::getValue(*graph.findNode(core::UUID{1}), "nothing"), "");
}

TEST_F(PcgGraphTest, TheSameSeedGivesTheSameTiles) {
	const auto graph = makeNoiseGraph();
	const auto first = evaluate(graph, {.size = {32, 24}, .seed = 7});
	const auto second = evaluate(graph, {.size = {32, 24}, .seed = 7});
	const auto other = evaluate(graph, {.size = {32, 24}, .seed = 8});
	ASSERT_TRUE(first && second && other);
	EXPECT_EQ(first->tiles, second->tiles);
	EXPECT_NE(first->tiles, other->tiles);
	const auto& noise = fieldOf(*first, 1, "Field");
	EXPECT_TRUE(std::ranges::all_of(noise.values, [](const float iValue) -> bool { return iValue >= 0.f && iValue <= 1.f; }));
	EXPECT_TRUE(std::ranges::all_of(first->tiles, [](const int32_t iTile) -> bool {
		return iTile == 3 || iTile == g_UntouchedTile;
	}));
	EXPECT_TRUE(std::ranges::contains(first->tiles, 3));
	EXPECT_TRUE(std::ranges::contains(first->tiles, g_UntouchedTile));
}

TEST_F(PcgGraphTest, EmptyRegionsAndCyclesAreRefused) {
	EXPECT_EQ(evaluate(makeNoiseGraph(), {.size = {0, 4}, .seed = 0}).error(), PcgError::EmptyRegion);
	Graph graph;
	addNode(graph, 1, "field.invert");
	addNode(graph, 2, "field.invert");
	link(graph, 1, "Field", 2, "Field");
	link(graph, 2, "Field", 1, "Field");
	EXPECT_EQ(evaluate(graph, {.size = {4, 4}, .seed = 0}).error(), PcgError::Cycle);
}

TEST_F(PcgGraphTest, UnlinkedInputsAndUnknownNodesWarn) {
	Graph graph;
	addNode(graph, 1, "output.tiles");
	addNode(graph, 2, "source.missing");
	const auto result = evaluate(graph, {.size = {2, 2}, .seed = 0});
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->warnings.size(), 2u);
	EXPECT_TRUE(std::ranges::all_of(result->tiles, [](const int32_t iTile) -> bool { return iTile == g_UntouchedTile; }));
}

TEST_F(PcgGraphTest, TheLowestOutputWins) {
	Graph graph;
	addNode(graph, 1, "source.constant", 0.f, {{"value", "1"}});
	addNode(graph, 2, "output.tiles", 200.f, {{"tile", "5"}});
	addNode(graph, 3, "output.tiles", 100.f, {{"tile", "4"}});
	link(graph, 1, "Field", 2, "Mask");
	link(graph, 1, "Field", 3, "Mask");
	auto result = evaluate(graph, {.size = {3, 3}, .seed = 0});
	ASSERT_TRUE(result.has_value());
	EXPECT_TRUE(std::ranges::all_of(result->tiles, [](const int32_t iTile) -> bool { return iTile == 5; }));
	addNode(graph, 4, "output.erase", 300.f);
	link(graph, 1, "Field", 4, "Mask");
	result = evaluate(graph, {.size = {3, 3}, .seed = 0});
	ASSERT_TRUE(result.has_value());
	EXPECT_TRUE(std::ranges::all_of(result->tiles, [](const int32_t iTile) -> bool { return iTile == -1; }));
}

TEST_F(PcgGraphTest, FieldOperations) {
	Graph graph;
	addNode(graph, 1, "source.gradient", 0.f, {{"direction", "horizontal"}});
	addNode(graph, 2, "source.constant", 0.f, {{"value", "0.25"}});
	addNode(graph, 3, "field.combine", 0.f, {{"operation", "add"}});
	addNode(graph, 4, "field.invert");
	addNode(graph, 5, "field.blur", 0.f, {{"radius", "1"}});
	addNode(graph, 6, "source.gradient", 0.f, {{"direction", "radial"}});
	addNode(graph, 7, "field.combine", 0.f, {{"operation", "max"}});
	link(graph, 1, "Field", 3, "A");
	link(graph, 2, "Field", 3, "B");
	link(graph, 1, "Field", 4, "Field");
	link(graph, 2, "Field", 5, "Field");
	link(graph, 1, "Field", 7, "A");
	link(graph, 2, "Field", 7, "B");
	const auto result = evaluate(graph, {.size = {5, 5}, .seed = 0});
	ASSERT_TRUE(result.has_value());
	EXPECT_FLOAT_EQ(fieldOf(*result, 1, "Field").get(4, 0), 1.f);
	EXPECT_FLOAT_EQ(fieldOf(*result, 3, "Field").get(0, 2), 0.25f);
	EXPECT_FLOAT_EQ(fieldOf(*result, 4, "Field").get(4, 3), 0.f);
	EXPECT_FLOAT_EQ(fieldOf(*result, 5, "Field").get(2, 2), 0.25f);
	EXPECT_FLOAT_EQ(fieldOf(*result, 6, "Field").get(2, 2), 0.f);
	EXPECT_FLOAT_EQ(fieldOf(*result, 7, "Field").get(0, 0), 0.25f);
	EXPECT_FLOAT_EQ(fieldOf(*result, 1, "Field").get(9, 9), 0.f);
}

TEST_F(PcgGraphTest, CellularAutomatonAndEdges) {
	Graph graph;
	addNode(graph, 1, "source.random");
	addNode(graph, 2, "field.threshold", 0.f, {{"min", "0.45"}});
	addNode(graph, 3, "field.cellular");
	addNode(graph, 4, "source.constant", 0.f, {{"value", "1"}});
	addNode(graph, 5, "field.edge", 0.f, {{"neighbours", "4"}});
	addNode(graph, 6, "field.cellular", 0.f, {{"edges", "open"}, {"iterations", "1"}});
	link(graph, 1, "Field", 2, "Field");
	link(graph, 2, "Mask", 3, "Mask");
	link(graph, 4, "Field", 5, "Mask");
	link(graph, 4, "Field", 6, "Mask");
	const auto result = evaluate(graph, {.size = {24, 16}, .seed = 3});
	ASSERT_TRUE(result.has_value());
	const auto& cave = fieldOf(*result, 3, "Mask");
	EXPECT_TRUE(std::ranges::all_of(cave.values, [](const float iValue) -> bool { return iValue == 0.f || iValue == 1.f; }));
	const auto& edge = fieldOf(*result, 5, "Mask");
	EXPECT_FLOAT_EQ(edge.get(0, 0), 1.f);
	EXPECT_FLOAT_EQ(edge.get(5, 0), 1.f);
	EXPECT_FLOAT_EQ(edge.get(5, 5), 0.f);
	const auto& open = fieldOf(*result, 6, "Mask");
	EXPECT_FLOAT_EQ(open.get(0, 0), 0.f);
	EXPECT_FLOAT_EQ(open.get(5, 5), 1.f);
}

TEST_F(PcgGraphTest, ScatterKeepsPointsApart) {
	Graph graph;
	addNode(graph, 1, "source.constant", 0.f, {{"value", "1"}});
	addNode(graph, 2, "points.scatter", 0.f, {{"density", "1"}, {"spacing", "3"}});
	addNode(graph, 3, "source.gradient");
	addNode(graph, 4, "points.filter", 0.f, {{"min", "0.5"}, {"max", "1"}});
	addNode(graph, 5, "points.to_mask", 0.f, {{"radius", "1"}});
	addNode(graph, 6, "output.points", 0.f, {{"tile", "9"}});
	link(graph, 1, "Field", 2, "Mask");
	link(graph, 2, "Points", 4, "Points");
	link(graph, 3, "Field", 4, "Field");
	link(graph, 4, "Points", 5, "Points");
	link(graph, 4, "Points", 6, "Points");
	const auto result = evaluate(graph, {.size = {20, 20}, .seed = 11});
	ASSERT_TRUE(result.has_value());
	const auto& scattered = std::get<Points>(result->values.at(core::UUID{2}).at("Points")).cells;
	ASSERT_GT(scattered.size(), 10u);
	for (size_t i = 0; i < scattered.size(); ++i) {
		for (size_t j = i + 1; j < scattered.size(); ++j) {
			const int64_t dx = static_cast<int64_t>(scattered[i].x()) - scattered[j].x();
			const int64_t dy = static_cast<int64_t>(scattered[i].y()) - scattered[j].y();
			EXPECT_GE(dx * dx + dy * dy, 9);
		}
	}
	const auto& kept = std::get<Points>(result->values.at(core::UUID{4}).at("Points")).cells;
	EXPECT_LT(kept.size(), scattered.size());
	EXPECT_TRUE(std::ranges::all_of(kept, [](const math::vec2ui& iCell) -> bool { return iCell.x() >= 10; }));
	const auto& mask = fieldOf(*result, 5, "Mask");
	EXPECT_FLOAT_EQ(mask.get(kept.front().x(), kept.front().y()), 1.f);
	EXPECT_EQ(std::ranges::count(result->tiles, 9), static_cast<std::ptrdiff_t>(kept.size()));
}

TEST_F(PcgGraphTest, GenerationWritesTheRegionOfATilemapLayer) {
	TilemapAsset asset;
	asset.resize(10, 8);
	asset.addLayer("ground");
	TilemapGenerator generator{.graphPath = "caves.owlpcg", .seed = 5, .layer = 0, .origin = {2, 3}, .size = {0, 0}};
	EXPECT_EQ(getRegion(asset, generator).size, (math::vec2ui{8, 5}));
	generator.size = {4, 20};
	EXPECT_EQ(getRegion(asset, generator).size, (math::vec2ui{4, 5}));
	Graph graph;
	addNode(graph, 1, "source.constant", 0.f, {{"value", "1"}});
	addNode(graph, 2, "output.tiles", 0.f, {{"tile", "2"}});
	link(graph, 1, "Field", 2, "Mask");
	const auto changed = generateTilemap(graph, asset, generator);
	ASSERT_TRUE(changed.has_value());
	EXPECT_EQ(*changed, 20u);
	EXPECT_EQ(asset.getTile(0, 2, 3), 2);
	EXPECT_EQ(asset.getTile(0, 5, 7), 2);
	EXPECT_NE(asset.getTile(0, 1, 3), 2);
	EXPECT_NE(asset.getTile(0, 6, 3), 2);
	EXPECT_EQ(*generateTilemap(graph, asset, generator), 0u);
	generator.layer = 4;
	EXPECT_EQ(*generateTilemap(graph, asset, generator), 0u);
	generator.origin = {20, 0};
	EXPECT_EQ(generateTilemap(graph, asset, generator).error(), PcgError::EmptyRegion);
}

TEST_F(PcgGraphTest, TilemapsKeepTheirGenerator) {
	TilemapAsset asset;
	asset.resize(4, 4);
	asset.addLayer("ground");
	asset.generator =
			TilemapGenerator{.graphPath = "pcg/caves.owlpcg", .seed = 42, .layer = 0, .origin = {1, 1}, .size = {2, 0}};
	TilemapAsset read;
	ASSERT_TRUE(read.deserializeFromString(asset.serializeToString("map")));
	ASSERT_TRUE(read.generator.has_value());
	EXPECT_EQ(*read.generator, *asset.generator);
	EXPECT_TRUE(read.hasSameContent(asset));
	read.generator->seed = 43;
	EXPECT_FALSE(read.hasSameContent(asset));
	asset.generator.reset();
	ASSERT_TRUE(read.deserializeFromString(asset.serializeToString("map")));
	EXPECT_FALSE(read.generator.has_value());
}

TEST_F(PcgGraphTest, TheEngineDemoCavesGenerate) {
	const auto demo = test::getRootPath() / "engine_demo";
	const auto graph = loadFromFile(demo / "pcg" / "caves.owlpcg");
	ASSERT_TRUE(graph.has_value());
	TilemapAsset caves;
	ASSERT_TRUE(caves.loadFromFile(demo / "tilemaps" / "pcg_caves.owltilemap"));
	ASSERT_TRUE(caves.generator.has_value());
	EXPECT_EQ(caves.generator->graphPath, std::filesystem::path{"pcg/caves.owlpcg"});
	const auto written = generateTilemap(*graph, caves, *caves.generator);
	ASSERT_TRUE(written.has_value());
	EXPECT_EQ(*written, caves.width * caves.height);
	uint32_t walls = 0;
	for (uint32_t y = 0; y < caves.height; ++y) {
		for (uint32_t x = 0; x < caves.width; ++x) walls += caves.getTile(0, x, y) == 1 ? 1u : 0u;
	}
	EXPECT_GT(walls, 0u);
	EXPECT_LT(walls, caves.width * caves.height);
}
