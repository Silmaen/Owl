/**
 * @file Tilemap_test.cpp
 * @author Silmaen
 * @date 02/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Application.h>
#include <core/Log.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/component/components.h>

using namespace owl;
using namespace owl::scene;

TEST(TilemapComponent, keyAndName) {
	EXPECT_STREQ(component::Tilemap::key(), "Tilemap");
	EXPECT_STREQ(component::Tilemap::name(), "Tilemap");
}

TEST(TilemapComponent, addLayerInitialisesGrid) {
	component::Tilemap map;
	map.width = 4;
	map.height = 3;
	auto& layer = map.addLayer("ground");
	EXPECT_EQ(layer.name, "ground");
	EXPECT_TRUE(layer.visible);
	EXPECT_EQ(layer.tiles.size(), 12u);
	for (const auto v: layer.tiles)
		EXPECT_EQ(v, component::g_EmptyTileIndex);
}

TEST(TilemapComponent, setAndGetTile) {
	component::Tilemap map;
	map.width = 3;
	map.height = 3;
	map.addLayer("a");
	map.setTile(0, 1, 2, 7);
	EXPECT_EQ(map.getTile(0, 1, 2), 7);
	EXPECT_EQ(map.getTile(0, 0, 0), component::g_EmptyTileIndex);
	EXPECT_EQ(map.getTile(0, 99, 99), component::g_EmptyTileIndex);
	EXPECT_EQ(map.getTile(99, 0, 0), component::g_EmptyTileIndex);
}

TEST(TilemapComponent, resizePreservesOverlap) {
	component::Tilemap map;
	map.width = 3;
	map.height = 3;
	map.addLayer("a");
	map.setTile(0, 0, 0, 1);
	map.setTile(0, 2, 2, 9);
	map.resize(4, 2);
	EXPECT_EQ(map.width, 4u);
	EXPECT_EQ(map.height, 2u);
	EXPECT_EQ(map.layers[0].tiles.size(), 8u);
	EXPECT_EQ(map.getTile(0, 0, 0), 1);
	// (2, 2) is now out of bounds → empty
	EXPECT_EQ(map.getTile(0, 2, 2), component::g_EmptyTileIndex);
}

TEST(TilemapComponent, resizeClampsToOne) {
	component::Tilemap map;
	map.addLayer("a");
	map.resize(0, 0);
	EXPECT_EQ(map.width, 1u);
	EXPECT_EQ(map.height, 1u);
}

TEST(TilemapComponent, serializeRoundTrip) {
	core::Log::init(core::Log::Level::Off);
	auto app = mkShared<core::Application>(core::AppParams{.args = nullptr,
														   .frameLogFrequency = 0,
														   .name = "tilemapRoundTrip",
														   .assetsPattern = "",
														   .icon = "",
														   .width = 0,
														   .height = 0,
														   .argCount = 0,
														   .renderer = renderer::gpu::RenderAPI::Type::Null,
														   .hasGui = false,
														   .useDebugging = false,
														   .isDummy = true});

	const auto sc = mkShared<Scene>();
	auto entity = sc->createEntityWithUUID(0xBEEF, "TilemapEntity");
	auto& map = entity.addComponent<component::Tilemap>();
	map.tilesetPath = "assets/tilesets/dungeon.owltileset";
	map.width = 4;
	map.height = 3;
	map.cellSize = 0.5f;
	auto& ground = map.addLayer("ground");
	map.setTile(0, 0, 0, 5);
	map.setTile(0, 3, 2, 11);
	auto& props = map.addLayer("props");
	props.visible = false;
	props.parallax = math::vec2{0.5f, 0.5f};
	map.setTile(1, 1, 1, 2);
	(void) ground;// silence unused-warning if compilers complain

	const SceneSerializer saver(sc);
	const auto fs = std::filesystem::temp_directory_path() / "tilemapRoundTrip.yml";
	saver.serialize(fs);

	const auto sc2 = mkShared<Scene>();
	const SceneSerializer loader(sc2);
	ASSERT_TRUE(loader.deserialize(fs));

	const auto entities = sc2->getAllEntities();
	ASSERT_EQ(entities.size(), 1u);
	const auto& restored = entities[0].getComponent<component::Tilemap>();
	EXPECT_EQ(restored.tilesetPath.generic_string(), "assets/tilesets/dungeon.owltileset");
	EXPECT_EQ(restored.width, 4u);
	EXPECT_EQ(restored.height, 3u);
	EXPECT_FLOAT_EQ(restored.cellSize, 0.5f);
	ASSERT_EQ(restored.layers.size(), 2u);
	EXPECT_EQ(restored.layers[0].name, "ground");
	EXPECT_TRUE(restored.layers[0].visible);
	EXPECT_FLOAT_EQ(restored.layers[0].parallax.x(), 1.f);
	EXPECT_EQ(restored.getTile(0, 0, 0), 5);
	EXPECT_EQ(restored.getTile(0, 3, 2), 11);
	EXPECT_EQ(restored.getTile(0, 1, 1), component::g_EmptyTileIndex);
	EXPECT_EQ(restored.layers[1].name, "props");
	EXPECT_FALSE(restored.layers[1].visible);
	EXPECT_FLOAT_EQ(restored.layers[1].parallax.x(), 0.5f);
	EXPECT_EQ(restored.getTile(1, 1, 1), 2);

	std::filesystem::remove(fs);
	core::Application::invalidate();
	app.reset();
	core::Log::invalidate();
}
