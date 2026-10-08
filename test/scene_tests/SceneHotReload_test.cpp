/**
 * @file SceneHotReload_test.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/TilemapAsset.h>
#include <scene/Tileset.h>
#include <scene/component/LuaScript.h>
#include <scene/component/Tilemap.h>

#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>

using namespace owl;
using namespace owl::scene;

namespace {

void writeText(const std::filesystem::path& iPath, const std::string& iContent) {
	std::ofstream file(iPath, std::ios::binary | std::ios::trunc);
	file << iContent;
}

auto makeDir(const std::string& iName) -> std::filesystem::path {
	const auto dir = std::filesystem::temp_directory_path() / iName;
	std::filesystem::remove_all(dir);
	std::filesystem::create_directories(dir);
	return dir;
}

auto scriptVersion(const int iVersion) -> std::string {
	return std::format("version = {}\ncreated = 0\nfunction on_create()\n  created = created + 1\nend\n", iVersion);
}

}// namespace

TEST(SceneHotReload, ScriptIsReplacedAndKeepsItsProperties) {
	core::Log::init(core::Log::Level::Off);
	const auto dir = makeDir("owl_scene_hotreload_lua");
	const auto file = dir / "mover.lua";
	writeText(file, scriptVersion(1));

	auto scn = mkShared<Scene>();
	auto entity = scn->createEntity("Mover");
	auto& comp = entity.addComponent<component::LuaScript>();
	comp.scriptPath = file.string();
	comp.properties.push_back({.name = "speed", .type = script::ScriptPropertyType::Float, .value = 2.0f});
	scn->onStartRuntime();
	ASSERT_NE(comp.instance, nullptr);
	EXPECT_EQ(comp.instance->getPropertyInt("version").value_or(0), 1);
	comp.instance->setProperty("speed", 9.0f);

	writeText(file, scriptVersion(2));
	EXPECT_TRUE(scn->onAssetFileChanged(file));
	auto& reloaded = entity.getComponent<component::LuaScript>();
	ASSERT_NE(reloaded.instance, nullptr);
	EXPECT_EQ(reloaded.instance->getPropertyInt("version").value_or(0), 2);
	EXPECT_EQ(reloaded.instance->getPropertyInt("created").value_or(0), 1);
	EXPECT_NEAR(reloaded.instance->getPropertyFloat("speed").value_or(0.f), 9.0f, 1e-5f);

	writeText(file, "version = = 3\n");
	const auto* kept = reloaded.instance.get();
	EXPECT_TRUE(scn->onAssetFileChanged(file));
	EXPECT_EQ(entity.getComponent<component::LuaScript>().instance.get(), kept);
	EXPECT_EQ(kept->getPropertyInt("version").value_or(0), 2);

	EXPECT_FALSE(scn->onAssetFileChanged(dir / "other.lua"));
	scn->onEndRuntime();
	std::filesystem::remove_all(dir);
	core::Log::invalidate();
}

TEST(SceneHotReload, TilesetAndTilemapAreDroppedForAResolve) {
	core::Log::init(core::Log::Level::Off);
	const auto dir = makeDir("owl_scene_hotreload_tiles");
	const auto tilesetFile = dir / "dungeon.owltileset";
	const auto tilemapFile = dir / "level.owltilemap";
	writeText(tilesetFile, "placeholder");
	writeText(tilemapFile, "placeholder");

	auto scn = mkShared<Scene>();
	auto entity = scn->createEntity("Map");
	auto& tilemap = entity.addComponent<component::Tilemap>();
	tilemap.tilemapPath = tilemapFile;
	tilemap.asset = mkShared<TilemapAsset>();
	tilemap.asset->tilesetPath = tilesetFile;
	tilemap.asset->tileset = mkShared<Tileset>();

	EXPECT_FALSE(scn->onAssetFileChanged(dir / "unrelated.owltileset"));
	EXPECT_NE(tilemap.asset->tileset, nullptr);
	EXPECT_TRUE(scn->onAssetFileChanged(tilesetFile));
	EXPECT_EQ(tilemap.asset->tileset, nullptr);
	EXPECT_TRUE(scn->onAssetFileChanged(tilemapFile));
	EXPECT_EQ(tilemap.asset, nullptr);
	EXPECT_FALSE(scn->onAssetFileChanged(dir / "texture.png"));

	std::filesystem::remove_all(dir);
	core::Log::invalidate();
}
