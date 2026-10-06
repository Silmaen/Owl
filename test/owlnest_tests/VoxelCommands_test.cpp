/**
 * @file VoxelCommands_test.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "commands/VoxelCommands.h"
#include "nestTestHelper.h"
#include "testHelper.h"

#include <scene/component/VoxelWorld.h>

#include <utility>
#include <vector>

using namespace owl;
using namespace owl::nest;
using namespace owl::nest::commands;
using namespace owl::nest::test;

namespace {

class VoxelCommandsTest : public NestTest {
protected:
	void SetUp() override {
		NestTest::SetUp();
		m_entity = m_scene.createEntity("World");
		auto& voxel = m_entity.addComponent<scene::component::VoxelWorld>();
		data::voxel::BlockType stone;
		stone.name = "stone";
		stone.solid = true;
		m_stone = voxel.registry.registerBlock(stone);
		data::voxel::BlockType dirt;
		dirt.name = "dirt";
		dirt.solid = true;
		m_dirt = voxel.registry.registerBlock(dirt);
	}

	[[nodiscard]] auto world() const -> data::voxel::VoxelWorld& {
		return m_entity.getComponent<scene::component::VoxelWorld>().world;
	}

	// Apply the edits to the world (as the brush does) and return the matching command.
	auto paint(std::vector<VoxelBlockEdit> iEdits) -> uniq<VoxelEditCommand> {
		for (const auto& edit: iEdits) world().setBlock(edit.coord, edit.after, edit.afterMeta);
		return mkUniq<VoxelEditCommand>(m_entity.getUUID(), std::move(iEdits), "Paint");
	}

	scene::Entity m_entity;
	data::voxel::BlockId m_stone = 0;
	data::voxel::BlockId m_dirt = 0;
};

}// namespace

TEST_F(VoxelCommandsTest, EditUndoRedo) {
	const math::vec3i first{1, 2, 3};
	const math::vec3i second{-4, 0, 7};
	m_undo.push(paint({{.coord = first, .before = data::voxel::g_AirBlock, .after = m_stone},
					   {.coord = second, .before = data::voxel::g_AirBlock, .after = m_dirt}}));
	EXPECT_EQ(m_undo.undoDescription(), "Paint");
	ASSERT_EQ(world().getBlock(first), m_stone);

	m_undo.undo(m_scene);
	EXPECT_EQ(world().getBlock(first), data::voxel::g_AirBlock);
	EXPECT_EQ(world().getBlock(second), data::voxel::g_AirBlock);
	EXPECT_EQ(m_undo.lastSelectionHint(), m_entity.getUUID());

	m_undo.redo(m_scene);
	EXPECT_EQ(world().getBlock(first), m_stone);
	EXPECT_EQ(world().getBlock(second), m_dirt);
}

TEST_F(VoxelCommandsTest, UndoReplaysEditsInReverseOrder) {
	const math::vec3i coord{0, 0, 0};
	m_undo.push(paint({{.coord = coord, .before = data::voxel::g_AirBlock, .after = m_stone},
					   {.coord = coord, .before = m_stone, .after = m_dirt}}));
	ASSERT_EQ(world().getBlock(coord), m_dirt);
	m_undo.undo(m_scene);
	EXPECT_EQ(world().getBlock(coord), data::voxel::g_AirBlock);
}

TEST_F(VoxelCommandsTest, StrokeMergesIntoOneStep) {
	const math::vec3i first{0, 0, 0};
	const math::vec3i second{1, 0, 0};
	m_undo.push(paint({{.coord = first, .before = data::voxel::g_AirBlock, .after = m_stone}}));
	m_undo.push(paint({{.coord = second, .before = data::voxel::g_AirBlock, .after = m_stone}}));
	m_undo.undo(m_scene);
	EXPECT_FALSE(m_undo.canUndo());
	EXPECT_EQ(world().getBlock(first), data::voxel::g_AirBlock);
	EXPECT_EQ(world().getBlock(second), data::voxel::g_AirBlock);
}

TEST_F(VoxelCommandsTest, MetadataIsRestored) {
	const math::vec3i coord{2, 2, 2};
	constexpr data::voxel::PackedMeta meta = 3;
	m_undo.push(paint({{.coord = coord,
						.before = data::voxel::g_AirBlock,
						.after = m_stone,
						.beforeMeta = data::voxel::g_DefaultMeta,
						.afterMeta = meta}}));
	ASSERT_EQ(world().getMeta(coord), meta);
	m_undo.undo(m_scene);
	EXPECT_EQ(world().getMeta(coord), data::voxel::g_DefaultMeta);
	m_undo.redo(m_scene);
	EXPECT_EQ(world().getMeta(coord), meta);
}

TEST_F(VoxelCommandsTest, MissingEntityIsNoOp) {
	auto command = mkUniq<VoxelEditCommand>(
			core::UUID{}, std::vector<VoxelBlockEdit>{{.coord = {0, 0, 0}, .before = 0, .after = m_stone}}, "Paint");
	command->redo(m_scene);
	command->undo(m_scene);
	EXPECT_EQ(world().getBlock({0, 0, 0}), data::voxel::g_AirBlock);
}
