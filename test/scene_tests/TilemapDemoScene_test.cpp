/**
 * @file TilemapDemoScene_test.cpp
 * @author Silmaen
 * @date 02/05/2026
 * Copyright © 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Application.h>
#include <core/Log.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/Tileset.h>
#include <scene/component/components.h>

using namespace owl;
using namespace owl::scene;

TEST(TilemapDemoScene, loadsAndExposesExpectedShape) {
	core::Log::init(core::Log::Level::Off);

	// Fixtures live under engine_assets/ — tests must not depend on sample_project/.
	const auto root = owl::test::getRootPath();
	const auto scenePath = root / "engine_assets" / "scenes" / "tilemap_demo.owl";
	const auto tilesetPath = root / "engine_assets" / "tilesets" / "demo.owltileset";
	ASSERT_TRUE(std::filesystem::exists(scenePath)) << scenePath;
	ASSERT_TRUE(std::filesystem::exists(tilesetPath)) << tilesetPath;

	// Load the scene through the engine serializer (no rendering involved).
	const auto scene = mkShared<Scene>();
	const SceneSerializer loader(scene);
	ASSERT_TRUE(loader.deserialize(scenePath));

	// Find the tilemap entity by tag.
	Entity tilemapEntity;
	for (const auto& ent: scene->getAllEntities()) {
		if (ent.hasComponent<component::Tilemap>()) {
			tilemapEntity = ent;
			break;
		}
	}
	ASSERT_TRUE(static_cast<bool>(tilemapEntity));

	const auto& tm = tilemapEntity.getComponent<component::Tilemap>();
	EXPECT_EQ(tm.width, 16u);
	EXPECT_EQ(tm.height, 10u);
	EXPECT_FLOAT_EQ(tm.cellSize, 1.f);
	ASSERT_EQ(tm.layers.size(), 2u);

	EXPECT_EQ(tm.layers[0].name, "background");
	EXPECT_FLOAT_EQ(tm.layers[0].parallax.x(), 0.5f);
	EXPECT_FLOAT_EQ(tm.layers[0].parallax.y(), 0.5f);
	EXPECT_EQ(tm.layers[0].tiles.size(), 160u);

	EXPECT_EQ(tm.layers[1].name, "world");
	EXPECT_FLOAT_EQ(tm.layers[1].parallax.x(), 1.f);
	EXPECT_EQ(tm.layers[1].tiles.size(), 160u);

	// Spot-check a few authored cells:
	// world (5, 5) is a ladder (tile 4)
	EXPECT_EQ(tm.getTile(1, 5, 5), 4);
	// world (8, 8) is grass (tile 0)
	EXPECT_EQ(tm.getTile(1, 8, 8), 0);
	// world (9, 8) is a spike (tile 6)
	EXPECT_EQ(tm.getTile(1, 9, 8), 6);
	// world (12, 9) is water (tile 5)
	EXPECT_EQ(tm.getTile(1, 12, 9), 5);
	// world (0, 0) is empty
	EXPECT_EQ(tm.getTile(1, 0, 0), component::g_EmptyTileIndex);

	// The companion tileset asset deserialises and reports the expected collidable tiles.
	Tileset tileset;
	ASSERT_TRUE(tileset.loadFromFile(tilesetPath));
	EXPECT_EQ(tileset.columns, 4u);
	EXPECT_EQ(tileset.rows, 2u);
	EXPECT_EQ(tileset.tileWidth, 64u);
	EXPECT_FALSE(tileset.isCollidable(0));// grass
	EXPECT_FALSE(tileset.isCollidable(1));// dirt
	EXPECT_TRUE(tileset.isCollidable(2));// stone
	EXPECT_TRUE(tileset.isCollidable(3));// brick
	EXPECT_FALSE(tileset.isCollidable(4));// ladder
	EXPECT_FALSE(tileset.isCollidable(5));// water
	EXPECT_TRUE(tileset.isCollidable(6));// spike
	EXPECT_FALSE(tileset.isCollidable(7));// bush

	core::Log::invalidate();
}
