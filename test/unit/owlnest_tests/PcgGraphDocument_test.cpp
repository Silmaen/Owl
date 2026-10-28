/**
 * @file PcgGraphDocument_test.cpp
 * @author Silmaen
 * @date 28/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 *
 * Headless tests of the procedural generation graph document and of the generator of the tilemap document: nodes,
 * links, parameters, undo, files, the preview, and the tiles written on a tilemap. Drawing needs an ImGui frame and
 * stays a manual check.
 */

#include "document/PcgGraphDocument.h"
#include "document/TilemapDocument.h"

#include <core/Log.h>
#include <scene/pcg/PcgGraph.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <set>
#include <string_view>
#include <tuple>

using namespace owl;
using namespace owl::nest;

namespace {
class PcgGraphDocumentTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_root = std::filesystem::temp_directory_path() / "owl_pcg_document_test";
		std::filesystem::remove_all(m_root);
		std::filesystem::create_directories(m_root);
		m_document.onAttach(nullptr);
	}

	void TearDown() override {
		m_document.onDetach();
		std::filesystem::remove_all(m_root);
		core::Log::invalidate();
	}

	std::filesystem::path m_root;
	PcgGraphDocument m_document;
};
}// namespace

TEST_F(PcgGraphDocumentTest, TheStarterGraphWritesTiles) {
	const auto graph = PcgGraphDocument::starterGraph();
	EXPECT_EQ(graph.nodes.size(), 3u);
	const auto result = scene::pcg::evaluate(graph, {.size = {16, 16}, .seed = 1});
	ASSERT_TRUE(result.has_value());
	EXPECT_TRUE(result->warnings.empty());
	EXPECT_TRUE(std::ranges::contains(result->tiles, 0));
}

TEST_F(PcgGraphDocumentTest, NodesLinkByTypeAndUndo) {
	const auto noise = m_document.addNode("source.noise", {0.f, 0.f});
	const auto scatter = m_document.addNode("points.scatter", {200.f, 0.f});
	const auto output = m_document.addNode("output.tiles", {400.f, 0.f});
	ASSERT_TRUE(noise && scatter && output);
	EXPECT_FALSE(m_document.addNode("source.unknown", {0.f, 0.f}).has_value());
	EXPECT_EQ(m_document.getGraph().nodes.size(), 3u);
	EXPECT_EQ(m_document.getSelectedNode(), output);
	EXPECT_TRUE(m_document.connect(*noise, "Field", *scatter, "Mask"));
	EXPECT_FALSE(m_document.connect(*scatter, "Points", *output, "Mask"));
	EXPECT_FALSE(m_document.connect(*noise, "Field", *noise, "Field"));
	EXPECT_FALSE(m_document.connect(*noise, "Missing", *output, "Mask"));
	EXPECT_TRUE(m_document.connect(*noise, "Field", *output, "Mask"));
	EXPECT_EQ(m_document.getGraph().links.size(), 2u);
	EXPECT_TRUE(m_document.isDirty());
	m_document.performUndo();
	EXPECT_EQ(m_document.getGraph().links.size(), 1u);
	m_document.performRedo();
	EXPECT_EQ(m_document.getGraph().links.size(), 2u);
	const std::array nodes{*noise};
	m_document.deleteNodes(nodes);
	EXPECT_EQ(m_document.getGraph().nodes.size(), 2u);
	EXPECT_TRUE(m_document.getGraph().links.empty());
	EXPECT_EQ(m_document.canvas().nodes().size(), 2u);
}

TEST_F(PcgGraphDocumentTest, ALinkReplacesTheOneOfItsInputAndLoopsAreRefused) {
	const auto first = m_document.addNode("source.constant", {0.f, 0.f});
	const auto second = m_document.addNode("source.gradient", {0.f, 100.f});
	const auto invert = m_document.addNode("field.invert", {200.f, 0.f});
	const auto blur = m_document.addNode("field.blur", {400.f, 0.f});
	ASSERT_TRUE(m_document.connect(*first, "Field", *invert, "Field"));
	ASSERT_TRUE(m_document.connect(*second, "Field", *invert, "Field"));
	ASSERT_EQ(m_document.getGraph().links.size(), 1u);
	EXPECT_EQ(m_document.getGraph().links.front().fromNode, *second);
	ASSERT_TRUE(m_document.connect(*invert, "Field", *blur, "Field"));
	EXPECT_FALSE(m_document.connect(*blur, "Field", *invert, "Field"));
}

TEST_F(PcgGraphDocumentTest, ParameterDragsMergeIntoOneEntry) {
	const auto noise = m_document.addNode("source.noise", {0.f, 0.f});
	ASSERT_TRUE(noise);
	m_document.setNodeValue(*noise, "scale", "10", true);
	m_document.setNodeValue(*noise, "scale", "12", true);
	EXPECT_EQ(m_document.getGraph().findNode(*noise)->values.at("scale"), "12");
	m_document.performUndo();
	EXPECT_FALSE(m_document.getGraph().findNode(*noise)->values.contains("scale"));
	m_document.selectNode(core::UUID{12345});
	EXPECT_FALSE(m_document.getSelectedNode().has_value());
	m_document.selectNode(noise);
	EXPECT_EQ(m_document.getSelectedNode(), noise);
}

TEST_F(PcgGraphDocumentTest, FilesAndRecoveryRoundTrip) {
	std::ignore = m_document.addNode("source.random", {10.f, 20.f});
	const auto file = m_root / "caves.owlpcg";
	ASSERT_TRUE(m_document.saveAs(file));
	EXPECT_FALSE(m_document.isDirty());
	PcgGraphDocument other;
	other.onAttach(nullptr);
	ASSERT_TRUE(other.loadFromFile(file));
	EXPECT_EQ(other.getGraph(), m_document.getGraph());
	EXPECT_FALSE(other.isDirty());
	const auto snapshot = m_document.recoverySnapshot();
	ASSERT_TRUE(snapshot.has_value());
	ASSERT_TRUE(other.restoreRecoverySnapshot(scene::pcg::serialize(PcgGraphDocument::starterGraph())));
	EXPECT_TRUE(other.isDirty());
	EXPECT_FALSE(other.restoreRecoverySnapshot("[broken"));
	EXPECT_FALSE(other.loadFromFile(m_root / "missing.owlpcg"));
	other.onDetach();
}

TEST_F(PcgGraphDocumentTest, ThePreviewFollowsTheGraphAndItsSettings) {
	ASSERT_TRUE(m_document.restoreRecoverySnapshot(scene::pcg::serialize(PcgGraphDocument::starterGraph())));
	m_document.setPreview({20, 10}, 4);
	const auto& preview = m_document.getPreview();
	ASSERT_TRUE(preview.has_value());
	EXPECT_EQ(preview->size, (math::vec2ui{20, 10}));
	const auto tiles = preview->tiles;
	m_document.setPreview({20, 10}, 5);
	EXPECT_NE(m_document.getPreview()->tiles, tiles);
	m_document.setPreview({0, 1000}, 5);
	EXPECT_EQ(m_document.getPreview()->size, (math::vec2ui{1, 256}));
}

TEST_F(PcgGraphDocumentTest, TheTilemapGeneratorWritesItsLayerAndUndoes) {
	const auto graphFile = m_root / "fill.owlpcg";
	scene::pcg::Graph graph;
	graph.nodes.push_back({.id = core::UUID{1}, .type = "source.constant", .position = {0.f, 0.f}, .values = {}});
	graph.nodes.push_back(
			{.id = core::UUID{2}, .type = "output.tiles", .position = {0.f, 0.f}, .values = {{"tile", "4"}}});
	graph.links.push_back({.fromNode = core::UUID{1}, .fromPin = "Field", .toNode = core::UUID{2}, .toPin = "Mask"});
	ASSERT_TRUE(scene::pcg::saveToFile(graph, graphFile));

	TilemapDocument tilemap;
	tilemap.onAttach(nullptr);
	tilemap.asset().resize(6, 4);
	tilemap.asset().addLayer("ground");
	EXPECT_FALSE(tilemap.generate());
	tilemap.setGenerator(
			scene::TilemapGenerator{.graphPath = graphFile, .seed = 3, .layer = 0, .origin = {1, 1}, .size = {2, 2}});
	ASSERT_TRUE(tilemap.asset().generator.has_value());
	EXPECT_TRUE(tilemap.generate(9));
	EXPECT_EQ(tilemap.asset().generator->seed, 9u);
	EXPECT_EQ(tilemap.asset().getTile(0, 1, 1), 4);
	EXPECT_EQ(tilemap.asset().getTile(0, 2, 2), 4);
	EXPECT_NE(tilemap.asset().getTile(0, 3, 3), 4);
	const auto& preview = tilemap.getGeneratorPreview();
	ASSERT_TRUE(preview.has_value());
	EXPECT_EQ(preview->size, (math::vec2ui{2, 2}));
	tilemap.performUndo();
	EXPECT_NE(tilemap.asset().getTile(0, 1, 1), 4);
	EXPECT_EQ(tilemap.asset().generator->seed, 3u);
	tilemap.setGenerator(std::nullopt);
	EXPECT_FALSE(tilemap.asset().generator.has_value());
	EXPECT_FALSE(tilemap.getGeneratorPreview().has_value());
	tilemap.onDetach();
}

TEST_F(PcgGraphDocumentTest, EveryNodeTypeHasItsGlyph) {
	std::set<std::string_view> glyphs;
	for (const auto& type: scene::pcg::getNodeTypes()) {
		const std::string_view glyph = getPcgNodeGlyph(type.id);
		EXPECT_NE(glyph, std::string_view{getPcgNodeGlyph("source.unknown")}) << type.id;
		glyphs.insert(glyph);
	}
	EXPECT_EQ(glyphs.size(), scene::pcg::getNodeTypes().size());
}

TEST_F(PcgGraphDocumentTest, ThePreviewImageShowsANodeOrTheTiles) {
	scene::pcg::Graph graph;
	graph.nodes.push_back({.id = core::UUID{1}, .type = "source.gradient", .position = {0.f, 0.f}, .values = {}});
	graph.nodes.push_back(
			{.id = core::UUID{2}, .type = "field.threshold", .position = {0.f, 0.f}, .values = {{"min", "0.5"}}});
	graph.nodes.push_back(
			{.id = core::UUID{3}, .type = "output.tiles", .position = {0.f, 0.f}, .values = {{"tile", "2"}}});
	graph.links.push_back({.fromNode = core::UUID{1}, .fromPin = "Field", .toNode = core::UUID{2}, .toPin = "Field"});
	graph.links.push_back({.fromNode = core::UUID{2}, .fromPin = "Mask", .toNode = core::UUID{3}, .toPin = "Mask"});
	const auto result = scene::pcg::evaluate(graph, {.size = {5, 1}, .seed = 0});
	ASSERT_TRUE(result.has_value());
	const auto field = buildPcgPreviewImage(*result, core::UUID{1});
	ASSERT_EQ(field.size(), 20u);
	EXPECT_EQ(field[0], 0);
	EXPECT_EQ(field[16], 255);
	const auto tiles = buildPcgPreviewImage(*result, std::nullopt);
	EXPECT_EQ(tiles[0], tiles[4]);
	EXPECT_NE(tiles[0], tiles[16]);
	EXPECT_EQ(tiles[16], tiles[12]);
	m_document.setViewMode(PcgViewMode::Output);
	EXPECT_EQ(m_document.getViewMode(), PcgViewMode::Output);
}
