/**
 * @file Tilemap_test.cpp
 * @author Silmaen
 * @date 02/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <app/Application.h>
#include <core/Log.h>
#include <core/SerializerImpl.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/TilemapAsset.h>
#include <scene/component/components.h>

using namespace owl;
using namespace owl::scene;

namespace {

class TilemapComponentFixture : public testing::Test {
protected:
	static void SetUpTestSuite() {
		core::Log::init(core::Log::Level::Off);
		s_app = mkShared<app::Application>(app::AppParams{.args = nullptr,
														  .frameLogFrequency = 0,
														  .name = "tilemapComponent",
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

	inline static shared<app::Application> s_app;
};

}// namespace

TEST_F(TilemapComponentFixture, KeyAndName) {
	EXPECT_STREQ(component::Tilemap::key(), "Tilemap");
	EXPECT_STREQ(component::Tilemap::name(), "Tilemap");
}

TEST_F(TilemapComponentFixture, DefaultStateIsEmpty) {
	const component::Tilemap map;
	EXPECT_TRUE(map.tilemapPath.empty());
	EXPECT_FALSE(map.asset);
}

TEST_F(TilemapComponentFixture, SerializePathOnlyForm) {
	const auto sc = mkShared<Scene>();
	auto entity = sc->createEntityWithUUID(0xBEEF, "TilemapEntity");
	auto& map = entity.addComponent<component::Tilemap>();
	map.tilemapPath = "tilemaps/dungeon_l1.owltilemap";
	// `asset` deliberately left null — the scene loader resolves it.

	const SceneSerializer saver(sc);
	const auto fs = std::filesystem::temp_directory_path() / "tilemap_path_only.yml";
	saver.serialize(fs);

	const auto sc2 = mkShared<Scene>();
	const SceneSerializer loader(sc2);
	ASSERT_TRUE(loader.deserialize(fs));

	const auto entities = sc2->getAllEntities();
	ASSERT_EQ(entities.size(), 1u);
	const auto& restored = entities[0].getComponent<component::Tilemap>();
	EXPECT_EQ(restored.tilemapPath.generic_string(), "tilemaps/dungeon_l1.owltilemap");
	// Asset is left null on load — `Scene::resolveAllTilemapAssets` populates it later.
	EXPECT_FALSE(restored.asset);

	std::filesystem::remove(fs);
}

TEST_F(TilemapComponentFixture, SerializeInlineFallbackForUnsavedAsset) {
	const auto sc = mkShared<Scene>();
	auto entity = sc->createEntityWithUUID(0xBEE2, "InlineTilemapEntity");
	auto& map = entity.addComponent<component::Tilemap>();
	// In-memory asset with no `tilemapPath` mimics a legacy inline tilemap or a freshly
	// authored asset that has not been saved through the editor yet.
	map.asset = mkShared<TilemapAsset>();
	map.asset->tilesetPath = "tilesets/dungeon.owltileset";
	map.asset->width = 4;
	map.asset->height = 3;
	map.asset->cellSize = 0.5f;
	auto& layer = map.asset->addLayer("ground");
	layer.tiles[0] = 5;
	layer.tiles[11] = 11;

	const SceneSerializer saver(sc);
	const auto fs = std::filesystem::temp_directory_path() / "tilemap_inline.yml";
	saver.serialize(fs);

	const auto sc2 = mkShared<Scene>();
	const SceneSerializer loader(sc2);
	ASSERT_TRUE(loader.deserialize(fs));

	const auto entities = sc2->getAllEntities();
	ASSERT_EQ(entities.size(), 1u);
	const auto& restored = entities[0].getComponent<component::Tilemap>();
	EXPECT_TRUE(restored.tilemapPath.empty());
	ASSERT_TRUE(restored.asset);
	EXPECT_EQ(restored.asset->tilesetPath.generic_string(), "tilesets/dungeon.owltileset");
	EXPECT_EQ(restored.asset->width, 4u);
	EXPECT_EQ(restored.asset->height, 3u);
	EXPECT_FLOAT_EQ(restored.asset->cellSize, 0.5f);
	ASSERT_EQ(restored.asset->layers.size(), 1u);
	EXPECT_EQ(restored.asset->getTile(0, 0, 0), 5);
	EXPECT_EQ(restored.asset->getTile(0, 3, 2), 11);

	std::filesystem::remove(fs);
}

TEST_F(TilemapComponentFixture, DeserializeAcceptsLegacyFlatLayout) {
	// Legacy form (pre-asset-extraction) wrote the inline grid data flat under the
	// `Tilemap:` key — without the `inline:` indirection. The component-level
	// deserializer keeps the old layout readable so existing scenes can still load.
	const std::string yaml = R"(tilesetPath: legacy/tileset.owltileset
width: 2
height: 2
cellSize: 1.0
layers:
  - name: only
    tiles: "1,2,3,4"
)";
	const core::Serializer node;
	node.getImpl()->node.reset(YAML::Load(yaml));

	component::Tilemap restored;
	restored.deserialize(node);
	EXPECT_TRUE(restored.tilemapPath.empty());
	ASSERT_TRUE(restored.asset);
	EXPECT_EQ(restored.asset->tilesetPath.generic_string(), "legacy/tileset.owltileset");
	EXPECT_EQ(restored.asset->width, 2u);
	EXPECT_EQ(restored.asset->height, 2u);
	ASSERT_EQ(restored.asset->layers.size(), 1u);
	EXPECT_EQ(restored.asset->getTile(0, 0, 0), 1);
	EXPECT_EQ(restored.asset->getTile(0, 1, 1), 4);
}
