/**
 * @file LuaTilemapBindings_test.cpp
 * @author Silmaen
 * @date 24/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <app/Application.h>
#include <core/Log.h>
#include <core/Timestep.h>
#include <physics/PhysicCommand.h>
#include <renderer/gpu/RenderAPI.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneBody.h>
#include <scene/TilemapAsset.h>
#include <scene/Tileset.h>
#include <scene/component/components.h>
#include <script/EntityHandle.h>
#include <script/ScriptInstance.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <format>
#include <string>
#include <vector>

using namespace owl;
using namespace owl::script;

namespace {

/// A scene whose `Map` entity shows `first.owltilemap` (4x3), beside `second.owltilemap` (6x5, one tile at (2, 1)) in
/// a temporary folder, and a script running `iBody` on creation.
class LuaTilemap : public testing::Test {
protected:
	static void SetUpTestSuite() {
		core::Log::init(core::Log::Level::Off);
		s_app = mkShared<app::Application>(app::AppParams{.args = nullptr,
														  .frameLogFrequency = 0,
														  .name = "luaTilemap",
														  .assetsPattern = "",
														  .icon = "",
														  .width = 0,
														  .height = 0,
														  .argCount = 0,
														  .renderer = renderer::gpu::RenderAPI::Type::Null,
														  .hasGui = false,
														  .useDebugging = false,
														  .isDummy = true});
	}

	static void TearDownTestSuite() {
		app::Application::invalidate();
		s_app.reset();
		core::Log::invalidate();
	}

	void SetUp() override {
		m_dir = std::filesystem::temp_directory_path() / "owl_lua_tilemap";
		std::filesystem::remove_all(m_dir);
		std::filesystem::create_directories(m_dir);
		scene::TilemapAsset first;
		first.resize(4, 3);
		first.addLayer("ground");
		ASSERT_TRUE(first.saveToFile(m_dir / "first.owltilemap", "first"));
		scene::TilemapAsset second;
		second.resize(6, 5);
		second.addLayer("ground");
		ASSERT_TRUE(second.layers[0].setCell(2, 1, {.terrain = 7, .overlay = -1, .flags = 0}));
		ASSERT_TRUE(second.saveToFile(m_dir / "second.owltilemap", "second"));
		m_scene = mkShared<scene::Scene>();
		m_map = m_scene->createEntity("Map");
		m_map.addComponent<scene::component::Tilemap>().tilemapPath = m_dir / "first.owltilemap";
		m_scene->onStartRuntime();
	}

	void TearDown() override {
		m_scene->onEndRuntime();
		m_scene.reset();
		std::filesystem::remove_all(m_dir);
	}

	/// Run a script on creation and return the instance holding its globals.
	auto run(const std::string& iBody) -> shared<ScriptInstance> {
		const std::string text =
				std::format("map = {}\ndir = '{}'\nfunction on_create()\n{}\nend\n",
							static_cast<int64_t>(getEntityHandle(*m_scene, m_map)), m_dir.generic_string(), iBody);
		const std::vector<uint8_t> data(text.begin(), text.end());
		auto instance = mkShared<ScriptInstance>();
		instance->setScene(m_scene.get());
		EXPECT_TRUE(instance->createFromBuffer(data, "tilemap_test", static_cast<uint64_t>(m_map.getUUID())));
		instance->onCreate();
		return instance;
	}

	auto tilemap() -> scene::component::Tilemap& { return m_map.getComponent<scene::component::Tilemap>(); }

	inline static shared<app::Application> s_app;
	std::filesystem::path m_dir;
	shared<scene::Scene> m_scene;
	scene::Entity m_map;
};

}// namespace

TEST_F(LuaTilemap, theSceneLoadsTheFirstMap) {
	ASSERT_NE(tilemap().asset, nullptr);
	EXPECT_EQ(tilemap().asset->width, 4u);
}

TEST_F(LuaTilemap, setAssetShowsAnotherTilemap) {
	const auto instance = run("before = tilemap.get_asset(map)\n"
							  "done = tilemap.set_asset(map, dir .. '/second.owltilemap')\n"
							  "after = tilemap.get_asset(map)\n"
							  "w, h = tilemap.get_size(map)\n"
							  "tile = tilemap.get_cell(map, 0, 2, 1)");
	EXPECT_EQ(instance->getPropertyString("before").value_or(""), (m_dir / "first.owltilemap").generic_string());
	EXPECT_TRUE(instance->getPropertyBool("done").value_or(false));
	EXPECT_EQ(instance->getPropertyString("after").value_or(""), (m_dir / "second.owltilemap").generic_string());
	EXPECT_EQ(instance->getPropertyInt("w").value_or(0), 6);
	EXPECT_EQ(instance->getPropertyInt("h").value_or(0), 5);
	EXPECT_EQ(instance->getPropertyInt("tile").value_or(0), 7);
	EXPECT_EQ(tilemap().tilemapPath, m_dir / "second.owltilemap");
}

TEST_F(LuaTilemap, aMissingFileKeepsTheTilemap) {
	const auto* shown = tilemap().asset.get();
	const auto instance = run("done = tilemap.set_asset(map, dir .. '/nowhere.owltilemap')\n"
							  "w = tilemap.get_size(map)");
	EXPECT_FALSE(instance->getPropertyBool("done").value_or(true));
	EXPECT_EQ(instance->getPropertyInt("w").value_or(0), 4);
	EXPECT_EQ(tilemap().asset.get(), shown);
	EXPECT_EQ(tilemap().tilemapPath, m_dir / "first.owltilemap");
}

TEST_F(LuaTilemap, anEntityWithoutTilemapIsRefused) {
	const auto other = m_scene->createEntity("Other");
	const auto instance = run(std::format("done = tilemap.set_asset({}, dir .. '/second.owltilemap')\n"
										  "none = tilemap.get_asset({}) == nil",
										  static_cast<int64_t>(getEntityHandle(*m_scene, other)),
										  static_cast<int64_t>(getEntityHandle(*m_scene, other))));
	EXPECT_FALSE(instance->getPropertyBool("done").value_or(true));
	EXPECT_TRUE(instance->getPropertyBool("none").value_or(false));
}

TEST_F(LuaTilemap, generateFillsTheTilemapFromAGraphAndASeed) {
	const auto graph = (test::getRootPath() / "engine_demo" / "pcg" / "caves.owlpcg").generic_string();
	const auto instance = run(std::format(
			"graph = '{}'\n"
			"function cells()\n"
			"  local text = ''\n"
			"  for y = 0, 2 do for x = 0, 3 do text = text .. tilemap.get_cell(map, 0, x, y) .. ',' end end\n"
			"  return text\n"
			"end\n"
			"empty = cells()\n"
			"changed = tilemap.generate(map, graph, 7)\n"
			"first = cells()\n"
			"tilemap.generate(map, graph, 8)\n"
			"tilemap.generate(map, graph, 7)\n"
			"again = cells()",
			graph));
	EXPECT_EQ(instance->getPropertyInt("changed").value_or(0), 12);
	EXPECT_EQ(instance->getPropertyString("empty").value_or(""), "-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,");
	EXPECT_NE(instance->getPropertyString("first").value_or(""), instance->getPropertyString("empty").value_or(""));
	EXPECT_EQ(instance->getPropertyString("again").value_or("x"), instance->getPropertyString("first").value_or("y"));
}

TEST_F(LuaTilemap, generateKeepsTheCellsOutsideItsRegion) {
	const auto graph = (test::getRootPath() / "engine_demo" / "pcg" / "caves.owlpcg").generic_string();
	const auto instance = run(std::format("changed = tilemap.generate(map, '{}', 3, 0, 1, 1, 2, 1)\n"
										  "inside = tilemap.get_cell(map, 0, 1, 1)\n"
										  "outside = tilemap.get_cell(map, 0, 0, 1)\n"
										  "below = tilemap.get_cell(map, 0, 1, 2)",
										  graph));
	EXPECT_EQ(instance->getPropertyInt("changed").value_or(0), 2);
	EXPECT_GE(instance->getPropertyInt("inside").value_or(-1), 0);
	EXPECT_EQ(instance->getPropertyInt("outside").value_or(0), -1);
	EXPECT_EQ(instance->getPropertyInt("below").value_or(0), -1);
}

TEST_F(LuaTilemap, generateRefusesAMissingGraphAndAnEntityWithoutTilemap) {
	const auto other = m_scene->createEntity("Other");
	const auto instance = run(std::format("missing, why = tilemap.generate(map, dir .. '/nowhere.owlpcg', 1)\n"
										  "bare = tilemap.generate({}, dir .. '/nowhere.owlpcg', 1)\n"
										  "reason = why",
										  static_cast<int64_t>(getEntityHandle(*m_scene, other))));
	EXPECT_FALSE(instance->getPropertyInt("missing").has_value());
	EXPECT_FALSE(instance->getPropertyInt("bare").has_value());
	EXPECT_NE(instance->getPropertyString("reason").value_or("").find("nowhere.owlpcg"), std::string::npos);
}

TEST_F(LuaTilemap, theCollidersFollowTheNewCells) {
	OWL_REQUIRE_MODULE(PHYSICS);
	auto tileset = mkShared<scene::Tileset>();
	tileset->columns = 4;
	tileset->rows = 4;
	tileset->tileWidth = 16;
	tileset->tileHeight = 16;
	tileset->tiles.resize(tileset->tileCount());
	tileset->tiles[7].collidable = true;
	tilemap().asset->tileset = tileset;
	// A dynamic box above the cell (2, 1) of the second map, which the first map leaves empty.
	auto box = m_scene->createEntity("Box");
	box.getComponent<scene::component::Transform>().transform.translation() = {-0.5f, 2.f, 0.f};
	box.getComponent<scene::component::Transform>().transform.scale() = {0.5f, 0.5f, 1.f};
	box.addComponent<scene::component::PhysicBody>().body.type = scene::SceneBody::BodyType::Dynamic;
	ASSERT_TRUE(m_scene->setTilemapAsset(m_map, m_dir / "second.owltilemap"));
	tilemap().asset->tileset = tileset;
	physics::PhysicCommand::rebuildTilemapBody(m_map);
	core::Timestep step;
	step.forceUpdate(std::chrono::milliseconds(16));
	for (int frame = 0; frame < 240; ++frame) m_scene->onUpdateRuntime(step, false);
	// The cell (2, 1) of a 6x5 map of unit cells is centred on (-0.5, 1): the box rests on it instead of falling.
	EXPECT_GT(box.getComponent<scene::component::Transform>().transform.translation().y(), 1.f);
}
