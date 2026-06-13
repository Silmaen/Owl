/**
 * @file VoxelStructure_test.cpp
 * @author Silmaen
 * @date 12/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <data/voxel/VoxelStructure.h>
#include <data/voxel/VoxelWorld.h>

using namespace owl;
using namespace owl::data::voxel;

namespace {
class VoxelStructureFixture : public testing::Test {
protected:
	static void SetUpTestSuite() { core::Log::init(core::Log::Level::Off); }
};

auto makeRegistry() -> BlockRegistry {
	BlockRegistry reg;
	BlockType stone;
	stone.name = "stone";
	stone.setAllFaces(1);
	(void) reg.registerBlock(stone);// id 1
	return reg;
}
}// namespace

TEST_F(VoxelStructureFixture, RoundTrip) {
	VoxelStructure structure;
	structure.size = math::vec3i{2, 2, 2};
	structure.blocks = {1, 0, 0, 2, 3, 0, 0, 4};
	const std::string yaml = structure.serializeToString("tree");
	VoxelStructure loaded;
	ASSERT_TRUE(loaded.deserializeFromString(yaml));
	EXPECT_EQ(loaded.size, (math::vec3i{2, 2, 2}));
	EXPECT_EQ(loaded.blocks, structure.blocks);
	EXPECT_EQ(loaded.at(0, 0, 0), 1u);
	EXPECT_EQ(loaded.at(1, 1, 1), 4u);
}

TEST_F(VoxelStructureFixture, MalformedIsRejected) {
	VoxelStructure structure;
	EXPECT_FALSE(structure.deserializeFromString("not: a structure"));
	EXPECT_TRUE(structure.isEmpty());
}

TEST_F(VoxelStructureFixture, CaptureFromWorldTightBounds) {
	const BlockRegistry reg = makeRegistry();
	VoxelWorld world;
	world.setBlock(math::vec3i{1, 2, 3}, 1);
	world.setBlock(math::vec3i{3, 2, 3}, 1);
	const VoxelStructure structure = VoxelStructure::captureFromWorld(world, reg);
	EXPECT_EQ(structure.size, (math::vec3i{3, 1, 1}));// x spans 1..3, y/z single
	EXPECT_EQ(structure.at(0, 0, 0), 1u);
	EXPECT_EQ(structure.at(1, 0, 0), 0u);// gap stays air
	EXPECT_EQ(structure.at(2, 0, 0), 1u);
}

TEST_F(VoxelStructureFixture, EmptyWorldCaptureIsEmpty) {
	const BlockRegistry reg = makeRegistry();
	const VoxelWorld world;
	const VoxelStructure structure = VoxelStructure::captureFromWorld(world, reg);
	EXPECT_TRUE(structure.isEmpty());
}

TEST_F(VoxelStructureFixture, StampWritesSolidBlocksAtOrigin) {
	VoxelStructure structure;
	structure.size = math::vec3i{2, 1, 1};
	structure.blocks = {7, 0};// solid at local (0,0,0), air at (1,0,0)
	VoxelWorld world;
	structure.stampInto(world, math::vec3i{10, 20, 30});
	EXPECT_EQ(world.getBlock(math::vec3i{10, 20, 30}), 7u);
	EXPECT_EQ(world.getBlock(math::vec3i{11, 20, 30}), g_AirBlock);// air cell not stamped
}
