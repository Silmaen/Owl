/**
 * @file HierarchyCommands_test.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "commands/HierarchyCommands.h"
#include "nestTestHelper.h"
#include "testHelper.h"

#include <vector>

using namespace owl;
using namespace owl::nest;
using namespace owl::nest::commands;
using namespace owl::nest::test;

using HierarchyCommandsTest = NestTest;

TEST_F(HierarchyCommandsTest, ReparentUndoRedo) {
	auto parent = m_scene.createEntity("Parent");
	auto child = m_scene.createEntity("Child");
	setLocalX(parent, 5.f);
	setLocalX(child, 3.f);
	const auto before = sceneState(m_scene);

	m_undo.execute(mkUniq<ReparentCommand>(child, parent.getUUID()), m_scene);
	EXPECT_EQ(m_undo.undoDescription(), "Reparent 'Child'");
	EXPECT_EQ(parentOf(child), parent.getUUID());
	EXPECT_FLOAT_EQ(worldX(m_scene, child), 3.f);
	EXPECT_FLOAT_EQ(localX(child), -2.f);

	m_undo.undo(m_scene);
	EXPECT_EQ(parentOf(child), core::UUID{0});
	EXPECT_TRUE(childrenOf(parent).empty());
	EXPECT_EQ(sceneState(m_scene), before);

	m_undo.redo(m_scene);
	EXPECT_EQ(parentOf(child), parent.getUUID());
	EXPECT_FLOAT_EQ(worldX(m_scene, child), 3.f);
}

TEST_F(HierarchyCommandsTest, ReparentBetweenParentsUndoRestoresOldParent) {
	auto oldParent = m_scene.createEntity("Old");
	auto newParent = m_scene.createEntity("New");
	auto child = m_scene.createEntity("Child");
	setLocalX(oldParent, 1.f);
	setLocalX(newParent, 10.f);
	m_scene.setParent(child, oldParent);
	setLocalX(child, 2.f);
	const auto before = sceneState(m_scene);

	m_undo.execute(mkUniq<ReparentCommand>(child, newParent.getUUID()), m_scene);
	EXPECT_EQ(childrenOf(newParent), std::vector{child.getUUID()});
	EXPECT_TRUE(childrenOf(oldParent).empty());
	EXPECT_FLOAT_EQ(worldX(m_scene, child), 3.f);

	m_undo.undo(m_scene);
	EXPECT_EQ(parentOf(child), oldParent.getUUID());
	EXPECT_FLOAT_EQ(localX(child), 2.f);
	EXPECT_EQ(sceneState(m_scene), before);
}

TEST_F(HierarchyCommandsTest, UnparentUndoRedo) {
	auto parent = m_scene.createEntity("Parent");
	auto child = m_scene.createEntity("Child");
	setLocalX(parent, 5.f);
	m_scene.setParent(child, parent);
	setLocalX(child, 2.f);
	const auto before = sceneState(m_scene);

	m_undo.execute(mkUniq<UnparentCommand>(child), m_scene);
	EXPECT_EQ(m_undo.undoDescription(), "Unparent 'Child'");
	EXPECT_EQ(parentOf(child), core::UUID{0});
	EXPECT_FLOAT_EQ(localX(child), 7.f);

	m_undo.undo(m_scene);
	EXPECT_EQ(parentOf(child), parent.getUUID());
	EXPECT_FLOAT_EQ(localX(child), 2.f);
	EXPECT_EQ(sceneState(m_scene), before);

	m_undo.redo(m_scene);
	EXPECT_EQ(parentOf(child), core::UUID{0});
	EXPECT_FLOAT_EQ(worldX(m_scene, child), 7.f);
}

TEST_F(HierarchyCommandsTest, ReparentOfMissingEntityIsNoOp) {
	auto parent = m_scene.createEntity("Parent");
	auto child = m_scene.createEntity("Child");
	auto command = mkUniq<ReparentCommand>(child, parent.getUUID());
	m_scene.destroyEntity(child);
	command->redo(m_scene);
	command->undo(m_scene);
	EXPECT_TRUE(childrenOf(parent).empty());
}
