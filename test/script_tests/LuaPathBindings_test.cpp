/**
 * @file LuaPathBindings_test.cpp
 * @author Silmaen
 * @date 16/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/TilemapAsset.h>
#include <scene/component/components.h>
#include <scene/nav/NavWorld.h>
#include <script/ScriptInstance.h>

#include <cstdint>
#include <format>
#include <string>
#include <tuple>
#include <vector>

using namespace owl;
using namespace owl::script;

namespace {

/// A 100x20 map with a road along the row 5 (x 0 to 90), a second one along the row 9 joined at both ends, and the
/// navigation rules of a road (overlay 1 of the layer 0).
class LuaPath : public testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_scene = mkShared<scene::Scene>();
		m_map = m_scene->createEntity("Map");
		auto asset = mkShared<scene::TilemapAsset>();
		asset->resize(100, 20);
		auto& layer = asset->addLayer("roads");
		const auto road = [&layer](const uint32_t iX, const uint32_t iY) -> void {
			std::ignore = layer.setCell(iX, iY, {.terrain = -1, .overlay = 1, .flags = 0});
		};
		for (uint32_t x = 0; x <= 90; ++x) {
			road(x, 5);
			road(x, 9);
		}
		for (uint32_t y = 5; y <= 9; ++y) {
			road(0, y);
			road(90, y);
		}
		m_asset = asset;
		auto& tilemap = m_map.addComponent<scene::component::Tilemap>();
		tilemap.asset = asset;
		tilemap.navigation.rules.push_back({.tile = 1});
		m_bare = m_scene->createEntity("Bare");
		m_bare.addComponent<scene::component::Tilemap>().asset = mkShared<scene::TilemapAsset>();
	}

	void TearDown() override { core::Log::invalidate(); }

	/// Run a script on creation and return the instance holding its globals.
	auto run(const std::string& iBody, const std::string& iUpdate = "") -> shared<ScriptInstance> {
		const std::string text =
				std::format("map = {}\nbare = {}\nfunction on_create()\n{}\nend\n"
							"function on_update(dt)\n{}\nend\n",
							static_cast<int64_t>(static_cast<uint64_t>(m_map.getUUID())),
							static_cast<int64_t>(static_cast<uint64_t>(m_bare.getUUID())), iBody, iUpdate);
		const std::vector<uint8_t> data(text.begin(), text.end());
		auto instance = mkShared<ScriptInstance>();
		instance->setScene(m_scene.get());
		EXPECT_TRUE(instance->createFromBuffer(data, "path_test", static_cast<uint64_t>(m_map.getUUID())));
		instance->onCreate();
		return instance;
	}

	shared<scene::Scene> m_scene;
	scene::Entity m_map;
	scene::Entity m_bare;
	shared<scene::TilemapAsset> m_asset;
};

}// namespace

TEST_F(LuaPath, findNowReturnsTheCellsAndTheCost) {
	const auto instance =
			run("cells, cost = path.find_now(map, 0, 5, 4, 5)\n"
				"count = #cells\nfirst_x, first_y = cells[1], cells[2]\nlast_x, last_y = cells[9], cells[10]\n"
				"nothing = path.find_now(map, 0, 5, 4, 7)\nnothing_is_nil = nothing == nil");
	EXPECT_TRUE(instance->getPropertyBool("nothing_is_nil").value_or(false));
	EXPECT_EQ(instance->getPropertyInt("count").value_or(0), 10);
	EXPECT_EQ(instance->getPropertyInt("first_x").value_or(-1), 0);
	EXPECT_EQ(instance->getPropertyInt("first_y").value_or(-1), 5);
	EXPECT_EQ(instance->getPropertyInt("last_x").value_or(-1), 4);
	EXPECT_EQ(instance->getPropertyInt("last_y").value_or(-1), 5);
	EXPECT_NEAR(instance->getPropertyFloat("cost").value_or(-1.f), 4.f, 1e-4f);
}

TEST_F(LuaPath, noReverseKeepsTheHeadingAndGoesRoundTheLoop) {
	// From (45, 5) heading East, the goal (40, 5) lies behind: the route goes round the loop of the two rows (45 + 4 +
	// 90 + 4 + 40 moves) where the plain one steps back 5 cells.
	const auto instance =
			run("plain = path.find_now(map, 45, 5, 40, 5)\nplain_count = #plain\n"
				"round = path.find_now(map, 45, 5, 40, 5, {no_reverse = true, heading_x = 1, heading_y = 0})\n"
				"round_count = #round\nsecond_x = round[3]\n"
				"with_mode = path.find_now(map, 45, 5, 40, 5, 'flat', {no_reverse = true, heading_x = -1})\n"
				"west_count = #with_mode\n"
				"request = path.find(map, 45, 5, 40, 5, {no_reverse = true, heading_x = 1})",
				"status = path.poll(map, request)\nif status == 'found' then cells, cost = path.get(map, request)\n"
				"request_count = #cells\nend");
	m_scene->getNavigation().update(*m_scene);
	instance->onUpdate(0.016f);
	EXPECT_EQ(instance->getPropertyInt("plain_count").value_or(0), 2 * 6);
	EXPECT_EQ(instance->getPropertyInt("round_count").value_or(0), 2 * (45 + 4 + 90 + 4 + 40 + 1));
	EXPECT_EQ(instance->getPropertyInt("second_x").value_or(0), 46) << "the route goes on East first";
	EXPECT_EQ(instance->getPropertyInt("west_count").value_or(0), 2 * 6) << "heading West the plain way is allowed";
	EXPECT_EQ(instance->getPropertyString("status").value_or(""), "found");
	EXPECT_EQ(instance->getPropertyInt("request_count").value_or(0), 2 * (45 + 4 + 90 + 4 + 40 + 1));
}

TEST_F(LuaPath, aRequestIsServedOverTheNextFrames) {
	m_scene->getNavigation().setBudget(40);
	const auto instance =
			run("request = path.find(map, 0, 5, 90, 9, 'flat')\nfirst = path.poll(map, request)",
				"status = path.poll(map, request)\nif status == 'found' then cells, cost = path.get(map, request)\n"
				"count = #cells\nvalid = path.is_valid(map, request)\nend");
	EXPECT_GT(instance->getPropertyInt("request").value_or(0), 0);
	EXPECT_EQ(instance->getPropertyString("first").value_or(""), "pending");
	int frames = 0;
	while (instance->getPropertyString("status").value_or("pending") == "pending" && frames < 1000) {
		m_scene->getNavigation().update(*m_scene);
		instance->onUpdate(0.016f);
		++frames;
	}
	EXPECT_GT(frames, 2) << "a budget of 40 nodes does not cover 95 cells";
	EXPECT_EQ(instance->getPropertyString("status").value_or(""), "found");
	EXPECT_EQ(instance->getPropertyInt("count").value_or(0), 2 * 95);
	EXPECT_TRUE(instance->getPropertyBool("valid").value_or(false));
	EXPECT_NEAR(instance->getPropertyFloat("cost").value_or(-1.f), 94.f, 1e-3f);
}

TEST_F(LuaPath, aBulldozedRoadInvalidatesTheFoundPathAndChangesTheVersion) {
	const auto instance = run("version = path.get_version(map)\nrequest = path.find(map, 0, 5, 90, 5)",
							  "status = path.poll(map, request)\nvalid = path.is_valid(map, request)\n"
							  "valid_after = path.is_valid(map, request, 60)\n"
							  "valid_before = path.is_valid(map, request, 40)\n"
							  "version_now = path.get_version(map)");
	m_scene->getNavigation().update(*m_scene);
	instance->onUpdate(0.016f);
	EXPECT_EQ(instance->getPropertyString("status").value_or(""), "found");
	EXPECT_TRUE(instance->getPropertyBool("valid").value_or(false));
	const auto version = instance->getPropertyInt("version").value_or(0);
	EXPECT_EQ(instance->getPropertyInt("version_now").value_or(-1), version);
	std::ignore = m_asset->layers[0].setCell(45, 5, {.terrain = -1, .overlay = -1, .flags = 0});
	instance->onUpdate(0.016f);
	EXPECT_FALSE(instance->getPropertyBool("valid").value_or(true));
	EXPECT_TRUE(instance->getPropertyBool("valid_after").value_or(false)) << "the bulldozed cell is the 46th";
	EXPECT_FALSE(instance->getPropertyBool("valid_before").value_or(true));
	EXPECT_NE(instance->getPropertyInt("version_now").value_or(version), version);
	const auto rerouted = run("cells, cost = path.find_now(map, 0, 5, 90, 5)\ncount = #cells");
	EXPECT_EQ(rerouted->getPropertyInt("count").value_or(0), 2 * (91 + 8));
}

TEST_F(LuaPath, aRoadAddedLaterShortensTheNextSearchInEveryMode) {
	for (const char* mode: {"auto", "flat", "hierarchical"}) {
		for (uint32_t y = 6; y <= 8; ++y)
			std::ignore = m_asset->layers[0].setCell(40, y, {.terrain = -1, .overlay = -1, .flags = 0});
		const std::string search = std::format("cells, cost = path.find_now(map, 40, 5, 40, 9, '{}')\n"
											   "count = #cells\nversion = path.get_version(map)",
											   mode);
		const auto before = run(search);
		EXPECT_EQ(before->getPropertyInt("count").value_or(0), 2 * 85) << mode << " goes round by the ends";
		const auto version = before->getPropertyInt("version").value_or(0);
		for (uint32_t y = 6; y <= 8; ++y)
			std::ignore = m_asset->layers[0].setCell(40, y, {.terrain = -1, .overlay = 1, .flags = 0});
		const auto after = run(search);
		EXPECT_NE(after->getPropertyInt("version").value_or(version), version) << mode;
		EXPECT_EQ(after->getPropertyInt("count").value_or(0), 2 * 5) << mode << " takes the new road";
	}
}

TEST_F(LuaPath, aRoadAddedInARunningCopyOfTheSceneIsUsed) {
	// Owl Nest Play: the running scene is a copy sharing the tilemap asset of the edit scene.
	for (uint32_t y = 6; y <= 8; ++y)
		std::ignore = m_asset->layers[0].setCell(40, y, {.terrain = -1, .overlay = -1, .flags = 0});
	const auto running = scene::Scene::copy(m_scene);
	running->onStartRuntime();
	const std::string text =
			std::format("map = {}\nfunction on_create()\ncells = path.find_now(map, 40, 5, 40, 9)\nbefore = #cells\n"
						"for y = 6, 8 do isometric.set_cell(map, 0, 40, y, -1, 1) end\n"
						"cells = path.find_now(map, 40, 5, 40, 9)\nafter = #cells\nend\n",
						static_cast<int64_t>(static_cast<uint64_t>(m_map.getUUID())));
	const std::vector<uint8_t> data(text.begin(), text.end());
	ScriptInstance instance;
	instance.setScene(running.get());
	ASSERT_TRUE(instance.createFromBuffer(data, "path_play", static_cast<uint64_t>(m_map.getUUID())));
	instance.onCreate();
	EXPECT_EQ(instance.getPropertyInt("before").value_or(0), 2 * 85);
	EXPECT_EQ(instance.getPropertyInt("after").value_or(0), 2 * 5);
	running->onEndRuntime();
}

TEST_F(LuaPath, theDebugOverlayIsSwitchedAndFedByScripts) {
	const auto instance = run("before = path.get_debug()\n"
							  "path.set_debug(true)\nduring = path.get_debug()\n"
							  "path.show_route(map, 1, {0, 5, 1, 5, 2, 5}, 1, 0.5, 0)\n"
							  "path.show_route(map, 2, {0, 9, 1, 9}, 0, 1, 0)\n"
							  "path.show_route(map, 2, nil)\n"
							  "path.show_route(bare, 1, {0, 0, 1, 0})\n"
							  "path.set_debug(false)\nafter = path.get_debug()");
	EXPECT_FALSE(instance->getPropertyBool("before").value_or(true));
	EXPECT_TRUE(instance->getPropertyBool("during").value_or(false));
	EXPECT_FALSE(instance->getPropertyBool("after").value_or(true));
	const auto* entry = m_scene->getNavigation().find(static_cast<uint64_t>(m_map.getUUID()));
	ASSERT_NE(entry, nullptr);
	ASSERT_EQ(entry->debugRoutes.size(), 1u) << "route 2 was removed, the map without rules keeps none";
	const auto& route = entry->debugRoutes.at(1);
	ASSERT_EQ(route.cells.size(), 3u);
	EXPECT_EQ(route.cells[2], (math::vec2ui{2, 5}));
	EXPECT_FLOAT_EQ(route.color.y(), 0.5f);
	std::ignore = run("path.clear_routes(map)");
	EXPECT_TRUE(m_scene->getNavigation().find(static_cast<uint64_t>(m_map.getUUID()))->debugRoutes.empty());
}

TEST_F(LuaPath, edgesAndNodesAreQueryable) {
	const auto instance = run("cost = path.get_edge_cost(map, 3, 5, 4, 5)\n"
							  "gap = path.get_edge_cost(map, 3, 5, 3, 6)\nfar = path.get_edge_cost(map, 3, 5, 9, 5)\n"
							  "on_road = path.is_walkable(map, 3, 5)\noff_road = path.is_walkable(map, 3, 6)\n"
							  "gap_is_nil = gap == nil\nfar_is_nil = far == nil");
	EXPECT_NEAR(instance->getPropertyFloat("cost").value_or(-1.f), 1.f, 1e-4f);
	EXPECT_TRUE(instance->getPropertyBool("gap_is_nil").value_or(false));
	EXPECT_TRUE(instance->getPropertyBool("far_is_nil").value_or(false));
	EXPECT_TRUE(instance->getPropertyBool("on_road").value_or(false));
	EXPECT_FALSE(instance->getPropertyBool("off_road").value_or(true));
}

TEST_F(LuaPath, anEntityWithoutRulesHasNoGraph) {
	const auto instance = run("request = path.find(bare, 0, 0, 1, 1)\nnothing = path.find_now(bare, 0, 0, 1, 1)\n"
							  "status = path.poll(bare, 7)\nwalk = path.is_walkable(bare, 0, 0)\n"
							  "version = path.get_version(bare)\nnone_is_nil = nothing == nil\n"
							  "negative = path.find(map, -1, 0, 1, 1)\n"
							  "unknown = path.poll(map, 99)\npath.release(map, 99)\npath.set_budget(100000)");
	EXPECT_EQ(instance->getPropertyInt("request").value_or(-1), 0);
	EXPECT_TRUE(instance->getPropertyBool("none_is_nil").value_or(false));
	EXPECT_EQ(instance->getPropertyString("status").value_or(""), "unknown");
	EXPECT_FALSE(instance->getPropertyBool("walk").value_or(true));
	EXPECT_EQ(instance->getPropertyInt("version").value_or(-1), 0);
	EXPECT_EQ(instance->getPropertyInt("negative").value_or(-1), 0);
	EXPECT_EQ(instance->getPropertyString("unknown").value_or(""), "unknown");
	EXPECT_EQ(m_scene->getNavigation().getBudget(), 100000u);
}

TEST_F(LuaPath, aReleasedRequestIsForgotten) {
	const auto instance = run("request = path.find(map, 0, 5, 3, 5)");
	m_scene->getNavigation().update(*m_scene);
	const auto after =
			run("request = " + std::to_string(instance->getPropertyInt("request").value_or(0)) +
				"\nbefore = path.poll(map, request)\npath.release(map, request)\nafter = path.poll(map, request)");
	EXPECT_EQ(after->getPropertyString("before").value_or(""), "found");
	EXPECT_EQ(after->getPropertyString("after").value_or(""), "unknown");
}
