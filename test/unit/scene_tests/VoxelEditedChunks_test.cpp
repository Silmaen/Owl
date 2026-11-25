/**
 * @file VoxelEditedChunks_test.cpp
 * @author Silmaen
 * @date 24/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <core/Serializer.h>
#include <core/SerializerImpl.h>
#include <data/voxel/Block.h>
#include <data/voxel/Chunk.h>
#include <math/vectors.h>
#include <scene/Entity.h>
#include <scene/SaveManager.h>
#include <scene/Scene.h>
#include <scene/component/VoxelWorld.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>

using namespace owl;
using namespace owl::scene;

namespace {

// A procedural world of one block type with the chunks (0, 0, 0) and (1, 0, 0) streamed in, filled with stone.
auto makeProceduralWorld(component::VoxelWorld& oWorld) -> data::voxel::BlockId {
	data::voxel::BlockType stone;
	stone.name = "stone";
	const auto id = oWorld.registry.registerBlock(stone);
	oWorld.proceduralTerrain = true;
	oWorld.world.getOrCreateChunk({0, 0, 0})->fill(id);
	oWorld.world.getOrCreateChunk({1, 0, 0})->fill(id);
	return id;
}

auto roundTrip(const component::VoxelWorld& iSource) -> component::VoxelWorld {
	const core::Serializer out;
	out.getImpl()->emitter << YAML::BeginMap;
	iSource.serialize(out);
	out.getImpl()->emitter << YAML::EndMap;
	const core::YamlDocument document{std::string{out.getImpl()->emitter.c_str()}};
	component::VoxelWorld restored;
	const core::Serializer in;
	in.getImpl()->node = document.getRoot()[component::VoxelWorld::key()];
	restored.deserialize(in);
	return restored;
}

class VoxelEditedChunksTest : public ::testing::Test {
protected:
	void SetUp() override { core::Log::init(core::Log::Level::Off); }
	void TearDown() override { core::Log::invalidate(); }
};

}// namespace

TEST_F(VoxelEditedChunksTest, AChunkKeyGivesItsCoordinateBack) {
	for (const math::vec3i coord: {math::vec3i{0, 0, 0}, math::vec3i{-3, 7, 12}, math::vec3i{4096, -2, -4096}})
		EXPECT_EQ(component::VoxelWorld::chunkCoordinate(component::VoxelWorld::chunkKey(coord)), coord);
}

TEST_F(VoxelEditedChunksTest, AnEditMarksTheChunkOfAProceduralWorld) {
	component::VoxelWorld world;
	makeProceduralWorld(world);
	EXPECT_TRUE(world.editBlock({3, 4, 5}, data::voxel::g_AirBlock));
	EXPECT_EQ(world.world.getBlock({3, 4, 5}), data::voxel::g_AirBlock);
	EXPECT_EQ(world.editedChunks.size(), 1u);
	EXPECT_TRUE(world.editedChunks.contains(component::VoxelWorld::chunkKey({0, 0, 0})));
}

TEST_F(VoxelEditedChunksTest, AnEditOutsideTheStreamedChunksIsRefused) {
	component::VoxelWorld world;
	const auto stone = makeProceduralWorld(world);
	EXPECT_FALSE(world.editBlock({500, 4, 5}, stone));
	EXPECT_FALSE(world.world.hasChunk(data::voxel::worldToChunk({500, 4, 5})));
	EXPECT_TRUE(world.editedChunks.empty());
}

TEST_F(VoxelEditedChunksTest, AnAuthoredWorldIsEditedAnywhere) {
	component::VoxelWorld world;
	const data::voxel::BlockType stone;
	const auto id = world.registry.registerBlock(stone);
	EXPECT_TRUE(world.editBlock({500, 4, 5}, id));
	EXPECT_EQ(world.world.getBlock({500, 4, 5}), id);
	EXPECT_TRUE(world.editedChunks.empty());
}

TEST_F(VoxelEditedChunksTest, OnlyTheEditedChunksOfAProceduralWorldAreWritten) {
	component::VoxelWorld world;
	const auto stone = makeProceduralWorld(world);
	ASSERT_TRUE(world.editBlock({3, 4, 5}, data::voxel::g_AirBlock));
	const auto restored = roundTrip(world);
	EXPECT_TRUE(restored.proceduralTerrain);
	EXPECT_EQ(restored.world.chunkCount(), 1u);
	EXPECT_EQ(restored.world.getBlock({3, 4, 5}), data::voxel::g_AirBlock);
	EXPECT_EQ(restored.world.getBlock({3, 4, 6}), stone);
	EXPECT_TRUE(restored.editedChunks.contains(component::VoxelWorld::chunkKey({0, 0, 0})));
}

TEST_F(VoxelEditedChunksTest, AnEmptiedChunkIsWrittenToo) {
	component::VoxelWorld world;
	makeProceduralWorld(world);
	world.world.getChunk({0, 0, 0})->fill(data::voxel::g_AirBlock);
	world.world.setBlock({1, 1, 1}, 1);
	ASSERT_TRUE(world.editBlock({1, 1, 1}, data::voxel::g_AirBlock));
	const auto restored = roundTrip(world);
	EXPECT_TRUE(restored.world.hasChunk({0, 0, 0}));
	EXPECT_EQ(restored.editedChunks.size(), 1u);
}

TEST_F(VoxelEditedChunksTest, AnUneditedProceduralWorldWritesNoChunk) {
	component::VoxelWorld world;
	makeProceduralWorld(world);
	const auto restored = roundTrip(world);
	EXPECT_EQ(restored.world.chunkCount(), 0u);
	EXPECT_TRUE(restored.editedChunks.empty());
}

TEST_F(VoxelEditedChunksTest, RegenerateForgetsTheEdits) {
	component::VoxelWorld world;
	makeProceduralWorld(world);
	ASSERT_TRUE(world.editBlock({3, 4, 5}, data::voxel::g_AirBlock));
	world.regenerate();
	EXPECT_EQ(world.world.chunkCount(), 0u);
	EXPECT_TRUE(world.editedChunks.empty());
}

TEST_F(VoxelEditedChunksTest, ASaveKeepsTheEditsOfAProceduralWorld) {
	SaveManager::setGameName("OwlVoxelEdits_" +
							 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
	auto scene = mkShared<Scene>();
	auto& world = scene->createEntity("World").addComponent<component::VoxelWorld>();
	const auto stone = makeProceduralWorld(world);
	ASSERT_TRUE(world.editBlock({3, 4, 5}, data::voxel::g_AirBlock));
	ASSERT_TRUE(world.editBlock({20, 1, 1}, data::voxel::g_AirBlock));
	ASSERT_TRUE(SaveManager::save(1, scene, "scenes/voxel.owl"));

	auto loaded = mkShared<Scene>();
	ASSERT_TRUE(SaveManager::load(1, loaded).success);
	const auto view = loaded->registry.view<component::VoxelWorld>();
	ASSERT_NE(view.begin(), view.end());
	const auto& restored = view.get<component::VoxelWorld>(view.front());
	EXPECT_EQ(restored.world.getBlock({3, 4, 5}), data::voxel::g_AirBlock);
	EXPECT_EQ(restored.world.getBlock({20, 1, 1}), data::voxel::g_AirBlock);
	EXPECT_EQ(restored.world.getBlock({20, 1, 2}), stone);
	EXPECT_EQ(restored.editedChunks.size(), 2u);
	std::filesystem::remove_all(SaveManager::getSaveDirectory().parent_path());
}
