/**
 * @file LuaVoxelBindings_test.cpp
 * @author Silmaen
 * @date 24/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <data/voxel/Block.h>
#include <math/vectors.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/component/components.h>
#include <script/EntityHandle.h>
#include <script/ScriptInstance.h>

#include <cstdint>
#include <format>
#include <string>
#include <vector>

using namespace owl;
using namespace owl::script;

namespace {

/// A scene with a voxel world of two blocks (`stone` = 1, `gold` = 2) over a 4x1x4 stone slab, a voxel player aiming
/// at the block (1, 0, 1) from above and a script running `iBody` on creation.
class LuaVoxel : public testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_scene = mkShared<scene::Scene>();
		m_world = m_scene->createEntity("World");
		auto& voxels = m_world.addComponent<scene::component::VoxelWorld>();
		data::voxel::BlockType stone;
		stone.name = "stone";
		data::voxel::BlockType gold;
		gold.name = "gold";
		m_stone = voxels.registry.registerBlock(stone);
		m_gold = voxels.registry.registerBlock(gold);
		for (int32_t x = 0; x < 4; ++x)
			for (int32_t z = 0; z < 4; ++z) voxels.world.setBlock({x, 0, z}, m_stone);
		m_player = m_scene->createEntity("Player");
		auto& player = m_player.addComponent<scene::component::VoxelPlayer>();
		player.hasTarget = true;
		player.targetBlock = {1, 0, 1};
		player.targetNormal = {0, 1, 0};
		player.placeBlock = m_stone;
	}

	void TearDown() override { core::Log::invalidate(); }

	/// Run a script on creation and return the instance holding its globals.
	auto run(const std::string& iBody) -> shared<ScriptInstance> {
		const std::string text = std::format("world = {}\nplayer = {}\nfunction on_create()\n{}\nend\n",
											 static_cast<int64_t>(getEntityHandle(*m_scene, m_world)),
											 static_cast<int64_t>(getEntityHandle(*m_scene, m_player)), iBody);
		const std::vector<uint8_t> data(text.begin(), text.end());
		auto instance = mkShared<ScriptInstance>();
		instance->setScene(m_scene.get());
		EXPECT_TRUE(instance->createFromBuffer(data, "voxel_test", static_cast<uint64_t>(m_world.getUUID())));
		instance->onCreate();
		return instance;
	}

	auto voxels() -> scene::component::VoxelWorld& { return m_world.getComponent<scene::component::VoxelWorld>(); }

	shared<scene::Scene> m_scene;
	scene::Entity m_world;
	scene::Entity m_player;
	data::voxel::BlockId m_stone = 0;
	data::voxel::BlockId m_gold = 0;
};

}// namespace

TEST_F(LuaVoxel, aBlockIsReadAtItsPosition) {
	const auto instance = run("solid = voxel.get_block(world, 1, 0, 1)\n"
							  "above = voxel.get_block(world, 1, 1, 1)\n"
							  "far = voxel.get_block(world, 900, 0, 900)\n"
							  "none = voxel.get_block(player, 1, 0, 1)");
	EXPECT_EQ(instance->getPropertyInt("solid").value_or(-1), m_stone);
	EXPECT_EQ(instance->getPropertyInt("above").value_or(-1), 0);
	EXPECT_EQ(instance->getPropertyInt("far").value_or(-1), 0);
	EXPECT_EQ(instance->getPropertyInt("none").value_or(-1), 0);
}

TEST_F(LuaVoxel, aBlockIsWrittenAndBroken) {
	const auto instance = run("placed = voxel.set_block(world, 1, 1, 1, 2)\n"
							  "broken = voxel.set_block(world, 0, 0, 0, 0)\n"
							  "unknown = voxel.set_block(world, 2, 1, 2, 77)\n"
							  "negative = voxel.set_block(world, 2, 1, 2, -1)\n"
							  "no_world = voxel.set_block(player, 2, 1, 2, 1)");
	EXPECT_TRUE(instance->getPropertyBool("placed").value_or(false));
	EXPECT_TRUE(instance->getPropertyBool("broken").value_or(false));
	EXPECT_FALSE(instance->getPropertyBool("unknown").value_or(true));
	EXPECT_FALSE(instance->getPropertyBool("negative").value_or(true));
	EXPECT_FALSE(instance->getPropertyBool("no_world").value_or(true));
	EXPECT_EQ(voxels().world.getBlock({1, 1, 1}), m_gold);
	EXPECT_EQ(voxels().world.getBlock({0, 0, 0}), data::voxel::g_AirBlock);
	EXPECT_EQ(voxels().world.getBlock({2, 1, 2}), data::voxel::g_AirBlock);
}

TEST_F(LuaVoxel, anEditOfAProceduralWorldIsKeptForTheSave) {
	voxels().proceduralTerrain = true;
	const auto instance = run("inside = voxel.set_block(world, 1, 0, 1, 2)\n"
							  "outside = voxel.set_block(world, 900, 0, 900, 2)");
	EXPECT_TRUE(instance->getPropertyBool("inside").value_or(false));
	EXPECT_FALSE(instance->getPropertyBool("outside").value_or(true));
	EXPECT_EQ(voxels().editedChunks.size(), 1u);
}

TEST_F(LuaVoxel, theBlocksOfThePaletteHaveANameAndAnId) {
	const auto instance = run("name = voxel.get_block_name(world, 2)\n"
							  "air = voxel.get_block_name(world, 0) == nil\n"
							  "unknown = voxel.get_block_name(world, 9) == nil\n"
							  "gold = voxel.find_block(world, 'gold')\n"
							  "missing = voxel.find_block(world, 'diamond')");
	EXPECT_EQ(instance->getPropertyString("name").value_or(""), "gold");
	EXPECT_TRUE(instance->getPropertyBool("air").value_or(false));
	EXPECT_TRUE(instance->getPropertyBool("unknown").value_or(false));
	EXPECT_EQ(instance->getPropertyInt("gold").value_or(-1), m_gold);
	EXPECT_EQ(instance->getPropertyInt("missing").value_or(-1), 0);
}

TEST_F(LuaVoxel, theAimGivesTheBlockAndTheFace) {
	const auto instance = run("x, y, z, nx, ny, nz = voxel.get_target(player)\n"
							  "no_player = voxel.get_target(world) == nil");
	EXPECT_EQ(instance->getPropertyInt("x").value_or(-1), 1);
	EXPECT_EQ(instance->getPropertyInt("y").value_or(-1), 0);
	EXPECT_EQ(instance->getPropertyInt("z").value_or(-1), 1);
	EXPECT_EQ(instance->getPropertyInt("nx").value_or(-1), 0);
	EXPECT_EQ(instance->getPropertyInt("ny").value_or(-1), 1);
	EXPECT_EQ(instance->getPropertyInt("nz").value_or(-1), 0);
	EXPECT_TRUE(instance->getPropertyBool("no_player").value_or(false));
}

TEST_F(LuaVoxel, noAimGivesNothing) {
	m_player.getComponent<scene::component::VoxelPlayer>().hasTarget = false;
	const auto instance = run("missed = voxel.get_target(player) == nil");
	EXPECT_TRUE(instance->getPropertyBool("missed").value_or(false));
}

TEST_F(LuaVoxel, thePlacedBlockIsChosen) {
	const auto instance = run("before = voxel.get_place_block(player)\n"
							  "done = voxel.set_place_block(player, 2)\n"
							  "after = voxel.get_place_block(player)\n"
							  "air = voxel.set_place_block(player, 0)\n"
							  "no_player = voxel.set_place_block(world, 2)\n"
							  "none = voxel.get_place_block(world) == nil");
	EXPECT_EQ(instance->getPropertyInt("before").value_or(-1), m_stone);
	EXPECT_TRUE(instance->getPropertyBool("done").value_or(false));
	EXPECT_EQ(instance->getPropertyInt("after").value_or(-1), m_gold);
	EXPECT_FALSE(instance->getPropertyBool("air").value_or(true));
	EXPECT_FALSE(instance->getPropertyBool("no_player").value_or(true));
	EXPECT_TRUE(instance->getPropertyBool("none").value_or(false));
	EXPECT_EQ(m_player.getComponent<scene::component::VoxelPlayer>().placeBlock, m_gold);
}
