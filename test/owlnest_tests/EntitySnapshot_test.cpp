/**
 * @file EntitySnapshot_test.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "EntitySnapshot.h"
#include "nestTestHelper.h"
#include "testHelper.h"

#include <scene/component/CircleRenderer.h>

#include <string>
#include <vector>

using namespace owl;
using namespace owl::nest;
using namespace owl::nest::test;

using EntitySnapshotTest = NestTest;

TEST_F(EntitySnapshotTest, CaptureRecordsUuidAndYaml) {
	auto entity = m_scene.createEntity("Snap");
	entity.addComponent<scene::component::CircleRenderer>();
	const auto snapshot = EntitySnapshot::capture(entity);
	EXPECT_EQ(snapshot.uuid, entity.getUUID());
	EXPECT_NE(snapshot.yamlData.find("Snap"), std::string::npos);
	EXPECT_NE(snapshot.yamlData.find("CircleRenderer"), std::string::npos);
}

TEST_F(EntitySnapshotTest, RestoreRecreatesEntityWithComponents) {
	auto entity = m_scene.createEntity("Snap");
	setLocalX(entity, 3.f);
	entity.addComponent<scene::component::CircleRenderer>().thickness = 0.25f;
	const auto snapshot = EntitySnapshot::capture(entity);
	m_scene.destroyEntity(entity);
	ASSERT_EQ(m_scene.getEntityCount(), 0u);

	const auto restored = snapshot.restore(m_scene);
	ASSERT_TRUE(restored);
	EXPECT_EQ(restored.getUUID(), snapshot.uuid);
	EXPECT_EQ(restored.getName(), "Snap");
	EXPECT_FLOAT_EQ(localX(restored), 3.f);
	ASSERT_TRUE(restored.hasComponent<scene::component::CircleRenderer>());
	EXPECT_FLOAT_EQ(restored.getComponent<scene::component::CircleRenderer>().thickness, 0.25f);
	EXPECT_EQ(m_scene.getEntityCount(), 1u);
}

TEST_F(EntitySnapshotTest, RestoreOfEmptySnapshotIsInvalid) {
	const EntitySnapshot empty;
	EXPECT_FALSE(empty.restore(m_scene));
	EXPECT_EQ(m_scene.getEntityCount(), 0u);
}

TEST_F(EntitySnapshotTest, RestoreLinksToExistingParent) {
	auto parent = m_scene.createEntity("Parent");
	auto child = m_scene.createEntity("Child");
	m_scene.setParent(child, parent);
	const auto snapshot = EntitySnapshot::capture(child);
	m_scene.destroyEntity(child);
	ASSERT_TRUE(childrenOf(parent).empty());

	const auto restored = snapshot.restore(m_scene);
	ASSERT_TRUE(restored);
	EXPECT_EQ(parentOf(restored), parent.getUUID());
	EXPECT_EQ(childrenOf(parent), std::vector{restored.getUUID()});
}

TEST_F(EntitySnapshotTest, SubtreeCaptureIsBreadthFirst) {
	auto root = m_scene.createEntity("Root");
	auto childA = m_scene.createEntity("A");
	auto childB = m_scene.createEntity("B");
	auto grandChild = m_scene.createEntity("A1");
	m_scene.setParent(childA, root);
	m_scene.setParent(childB, root);
	m_scene.setParent(grandChild, childA);

	const auto snapshot = SubtreeSnapshot::capture(root, m_scene);
	ASSERT_EQ(snapshot.entities.size(), 4u);
	ASSERT_EQ(snapshot.parentUuids.size(), 4u);
	EXPECT_EQ(snapshot.entities[0].uuid, root.getUUID());
	EXPECT_EQ(snapshot.parentUuids[0], core::UUID{0});
	EXPECT_EQ(snapshot.entities[1].uuid, childA.getUUID());
	EXPECT_EQ(snapshot.entities[2].uuid, childB.getUUID());
	EXPECT_EQ(snapshot.entities[3].uuid, grandChild.getUUID());
	EXPECT_EQ(snapshot.parentUuids[3], childA.getUUID());
}

TEST_F(EntitySnapshotTest, SubtreeRestoreRebuildsHierarchyAndWorld) {
	auto root = m_scene.createEntity("Root");
	auto child = m_scene.createEntity("Child");
	auto grandChild = m_scene.createEntity("GrandChild");
	setLocalX(root, 5.f);
	m_scene.setParent(child, root);
	m_scene.setParent(grandChild, child);
	setLocalX(grandChild, 2.f);
	const float grandChildWorld = worldX(m_scene, grandChild);
	const auto snapshot = SubtreeSnapshot::capture(root, m_scene);
	m_scene.destroyEntityWithChildren(root);
	ASSERT_EQ(m_scene.getEntityCount(), 0u);

	const auto restored = snapshot.restore(m_scene);
	ASSERT_TRUE(restored);
	EXPECT_EQ(m_scene.getEntityCount(), 3u);
	const auto restoredChild = m_scene.findEntityByUUID(snapshot.entities[1].uuid);
	const auto restoredGrandChild = m_scene.findEntityByUUID(snapshot.entities[2].uuid);
	ASSERT_TRUE(restoredChild);
	ASSERT_TRUE(restoredGrandChild);
	EXPECT_EQ(parentOf(restoredChild), restored.getUUID());
	EXPECT_EQ(parentOf(restoredGrandChild), restoredChild.getUUID());
	EXPECT_EQ(childrenOf(restored).size(), 1u);
	EXPECT_EQ(childrenOf(restoredChild).size(), 1u);
	EXPECT_FLOAT_EQ(worldX(m_scene, restoredGrandChild), grandChildWorld);
}

TEST_F(EntitySnapshotTest, SubtreeRestoreOfEmptySnapshotIsInvalid) {
	const SubtreeSnapshot empty;
	EXPECT_FALSE(empty.restore(m_scene));
}
