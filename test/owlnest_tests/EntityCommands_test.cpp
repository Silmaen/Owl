/**
 * @file EntityCommands_test.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "commands/EntityCommands.h"
#include "nestTestHelper.h"
#include "testHelper.h"

#include <scene/component/CircleRenderer.h>

#include <vector>

using namespace owl;
using namespace owl::nest;
using namespace owl::nest::commands;
using namespace owl::nest::test;

using EntityCommandsTest = NestTest;

TEST_F(EntityCommandsTest, CreateUndoRedo) {
	const auto initial = sceneState(m_scene);
	auto entity = m_scene.createEntity("Created");
	setLocalX(entity, 2.f);
	const auto uuid = entity.getUUID();
	const auto created = sceneState(m_scene);
	m_undo.push(mkUniq<CreateEntityCommand>(entity));
	EXPECT_EQ(m_undo.undoDescription(), "Create 'Created'");

	m_undo.undo(m_scene);
	EXPECT_FALSE(m_scene.findEntityByUUID(uuid));
	EXPECT_EQ(sceneState(m_scene), initial);

	m_undo.redo(m_scene);
	const auto redone = m_scene.findEntityByUUID(uuid);
	ASSERT_TRUE(redone);
	EXPECT_EQ(m_undo.lastSelectionHint(), uuid);
	EXPECT_EQ(sceneState(m_scene), created);
}

TEST_F(EntityCommandsTest, CreateUndoKeepsLaterEditsForRedo) {
	auto entity = m_scene.createEntity("Created");
	const auto uuid = entity.getUUID();
	m_undo.push(mkUniq<CreateEntityCommand>(entity));
	setLocalX(entity, 9.f);
	m_undo.undo(m_scene);
	m_undo.redo(m_scene);
	const auto redone = m_scene.findEntityByUUID(uuid);
	ASSERT_TRUE(redone);
	EXPECT_FLOAT_EQ(localX(redone), 9.f);
}

TEST_F(EntityCommandsTest, DeleteLeafUndoRedo) {
	auto entity = m_scene.createEntity("Leaf");
	entity.addComponent<scene::component::CircleRenderer>().thickness = 0.5f;
	const auto uuid = entity.getUUID();
	const auto before = sceneState(m_scene);

	m_undo.execute(mkUniq<DeleteEntityCommand>(entity, m_scene), m_scene);
	EXPECT_FALSE(m_scene.findEntityByUUID(uuid));
	EXPECT_EQ(m_scene.getEntityCount(), 0u);

	m_undo.undo(m_scene);
	EXPECT_EQ(sceneState(m_scene), before);
	EXPECT_EQ(m_undo.lastSelectionHint(), uuid);

	m_undo.redo(m_scene);
	EXPECT_FALSE(m_scene.findEntityByUUID(uuid));
}

TEST_F(EntityCommandsTest, DeleteChildUndoRestoresParentLink) {
	auto parent = m_scene.createEntity("Parent");
	auto child = m_scene.createEntity("Child");
	setLocalX(parent, 5.f);
	m_scene.setParent(child, parent);
	const auto childUuid = child.getUUID();
	const auto before = sceneState(m_scene);

	m_undo.execute(mkUniq<DeleteEntityCommand>(child, m_scene), m_scene);
	EXPECT_TRUE(childrenOf(parent).empty());

	m_undo.undo(m_scene);
	const auto restored = m_scene.findEntityByUUID(childUuid);
	ASSERT_TRUE(restored);
	EXPECT_EQ(parentOf(restored), parent.getUUID());
	EXPECT_EQ(childrenOf(parent), std::vector{childUuid});
	EXPECT_EQ(sceneState(m_scene), before);
}

TEST_F(EntityCommandsTest, DeleteParentUndoReattachesChildren) {
	auto parent = m_scene.createEntity("Parent");
	auto childA = m_scene.createEntity("A");
	auto childB = m_scene.createEntity("B");
	m_scene.setParent(childA, parent);
	m_scene.setParent(childB, parent);
	const auto parentUuid = parent.getUUID();

	m_undo.execute(mkUniq<DeleteEntityCommand>(parent, m_scene), m_scene);
	EXPECT_EQ(parentOf(childA), core::UUID{0});
	EXPECT_EQ(parentOf(childB), core::UUID{0});

	m_undo.undo(m_scene);
	const auto restored = m_scene.findEntityByUUID(parentUuid);
	ASSERT_TRUE(restored);
	EXPECT_EQ(parentOf(childA), parentUuid);
	EXPECT_EQ(parentOf(childB), parentUuid);
	EXPECT_EQ(childrenOf(restored), (std::vector{childA.getUUID(), childB.getUUID()}));
}

TEST_F(EntityCommandsTest, DeleteParentKeepsChildrenWorld) {
	auto parent = m_scene.createEntity("Parent");
	auto child = m_scene.createEntity("Child");
	setLocalX(parent, 5.f);
	m_scene.setParent(child, parent);
	ASSERT_FLOAT_EQ(worldX(m_scene, child), 0.f);

	m_undo.execute(mkUniq<DeleteEntityCommand>(parent, m_scene), m_scene);
	EXPECT_FLOAT_EQ(worldX(m_scene, child), 0.f);
}

TEST_F(EntityCommandsTest, DeleteParentUndoRestoresScene) {
	auto grandParent = m_scene.createEntity("GrandParent");
	auto sibling = m_scene.createEntity("Sibling");
	auto parent = m_scene.createEntity("Parent");
	auto child = m_scene.createEntity("Child");
	setLocalX(grandParent, 1.f);
	setLocalX(parent, 5.f);
	m_scene.setParent(parent, grandParent);
	m_scene.setParent(sibling, grandParent);
	m_scene.setParent(child, parent);
	setLocalX(child, 2.f);
	const auto before = sceneState(m_scene);

	m_undo.execute(mkUniq<DeleteEntityCommand>(parent, m_scene), m_scene);
	m_undo.undo(m_scene);
	EXPECT_EQ(sceneState(m_scene), before);
}

TEST_F(EntityCommandsTest, DeleteSubtreeUndoRedo) {
	auto anchor = m_scene.createEntity("Anchor");
	auto root = m_scene.createEntity("Root");
	auto child = m_scene.createEntity("Child");
	auto grandChild = m_scene.createEntity("GrandChild");
	m_scene.setParent(root, anchor);
	m_scene.setParent(child, root);
	m_scene.setParent(grandChild, child);
	const auto before = sceneState(m_scene);

	m_undo.execute(mkUniq<DeleteSubtreeCommand>(root, m_scene), m_scene);
	EXPECT_EQ(m_scene.getEntityCount(), 1u);
	EXPECT_TRUE(childrenOf(anchor).empty());
	EXPECT_EQ(m_undo.undoDescription(), "Delete 'Root' with children");

	m_undo.undo(m_scene);
	EXPECT_EQ(m_scene.getEntityCount(), 4u);
	EXPECT_EQ(sceneState(m_scene), before);

	m_undo.redo(m_scene);
	EXPECT_EQ(m_scene.getEntityCount(), 1u);
}

TEST_F(EntityCommandsTest, DuplicateUndoRedo) {
	auto original = m_scene.createEntity("Original");
	setLocalX(original, 4.f);
	const auto duplicate = m_scene.duplicateEntity(original);
	const auto duplicateUuid = duplicate.getUUID();
	const auto duplicated = sceneState(m_scene);
	m_undo.push(mkUniq<DuplicateEntityCommand>(original, duplicate));
	EXPECT_EQ(m_undo.undoDescription(), "Duplicate 'Original'");

	m_undo.undo(m_scene);
	EXPECT_FALSE(m_scene.findEntityByUUID(duplicateUuid));
	EXPECT_TRUE(m_scene.findEntityByUUID(original.getUUID()));

	m_undo.redo(m_scene);
	EXPECT_EQ(sceneState(m_scene), duplicated);
}

TEST_F(EntityCommandsTest, DuplicateSubtreeUndoRedo) {
	auto root = m_scene.createEntity("Root");
	auto child = m_scene.createEntity("Child");
	m_scene.setParent(child, root);
	const auto duplicateRoot = m_scene.duplicateSubtree(root);
	const auto duplicated = sceneState(m_scene);
	ASSERT_EQ(m_scene.getEntityCount(), 4u);
	m_undo.push(mkUniq<DuplicateSubtreeCommand>(root, duplicateRoot, m_scene));

	m_undo.undo(m_scene);
	EXPECT_EQ(m_scene.getEntityCount(), 2u);

	m_undo.redo(m_scene);
	EXPECT_EQ(m_scene.getEntityCount(), 4u);
	EXPECT_EQ(sceneState(m_scene), duplicated);
}

TEST_F(EntityCommandsTest, DeleteSubtreeUndoRestoresSiblingSlot) {
	auto anchor = m_scene.createEntity("Anchor");
	auto first = m_scene.createEntity("First");
	auto root = m_scene.createEntity("Root");
	auto last = m_scene.createEntity("Last");
	auto child = m_scene.createEntity("Child");
	m_scene.setParent(first, anchor);
	m_scene.setParent(root, anchor);
	m_scene.setParent(last, anchor);
	m_scene.setParent(child, root);
	const auto before = sceneState(m_scene);

	m_undo.execute(mkUniq<DeleteSubtreeCommand>(root, m_scene), m_scene);
	m_undo.undo(m_scene);
	EXPECT_EQ(sceneState(m_scene), before);
}

TEST_F(EntityCommandsTest, DeleteParentRedoUndoTwice) {
	auto parent = m_scene.createEntity("Parent");
	auto child = m_scene.createEntity("Child");
	setLocalX(parent, 5.f);
	m_scene.setParent(child, parent);
	setLocalX(child, 1.f);
	const auto before = sceneState(m_scene);
	m_undo.execute(mkUniq<DeleteEntityCommand>(parent, m_scene), m_scene);
	const auto deleted = sceneState(m_scene);
	m_undo.undo(m_scene);
	m_undo.redo(m_scene);
	EXPECT_EQ(sceneState(m_scene), deleted);
	m_undo.undo(m_scene);
	EXPECT_EQ(sceneState(m_scene), before);
}
