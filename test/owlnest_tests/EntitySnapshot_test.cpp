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

TEST_F(EntitySnapshotTest, RestoreExistingEntityIsInPlace) {
	auto parent = m_scene.createEntity("Parent");
	auto child = m_scene.createEntity("Child");
	m_scene.setParent(child, parent);
	const auto snapshot = EntitySnapshot::capture(parent);
	const auto handle = static_cast<entt::entity>(parent);
	setLocalX(parent, 6.f);
	parent.addComponent<scene::component::CircleRenderer>();

	const auto restored = snapshot.restore(m_scene);
	EXPECT_EQ(static_cast<entt::entity>(restored), handle);
	EXPECT_FLOAT_EQ(localX(parent), 0.f);
	EXPECT_FALSE(parent.hasComponent<scene::component::CircleRenderer>());
	EXPECT_EQ(childrenOf(parent), std::vector{child.getUUID()});
	EXPECT_EQ(m_scene.getEntityCount(), 2u);
}

TEST_F(EntitySnapshotTest, HierarchySlotRoundTrip) {
	auto parent = m_scene.createEntity("Parent");
	auto first = m_scene.createEntity("First");
	auto second = m_scene.createEntity("Second");
	auto third = m_scene.createEntity("Third");
	m_scene.setParent(first, parent);
	m_scene.setParent(second, parent);
	m_scene.setParent(third, parent);
	const auto slot = HierarchySlot::capture(second, m_scene);
	EXPECT_EQ(slot.parentUuid, parent.getUUID());
	EXPECT_EQ(slot.siblingIndex, 1u);

	m_scene.unparent(second);
	setLocalX(second, 4.f);
	slot.restore(second, m_scene);
	EXPECT_EQ(childrenOf(parent), (std::vector{first.getUUID(), second.getUUID(), third.getUUID()}));
	EXPECT_EQ(parentOf(second), parent.getUUID());
	EXPECT_FLOAT_EQ(localX(second), 4.f);
}

TEST_F(EntitySnapshotTest, HierarchySlotClampsIndexAndHandlesMissingParent) {
	auto parent = m_scene.createEntity("Parent");
	auto child = m_scene.createEntity("Child");
	HierarchySlot{.parentUuid = parent.getUUID(), .siblingIndex = 42}.restore(child, m_scene);
	EXPECT_EQ(childrenOf(parent), std::vector{child.getUUID()});
	HierarchySlot{.parentUuid = core::UUID{}, .siblingIndex = 0}.restore(child, m_scene);
	EXPECT_EQ(parentOf(child), core::UUID{0});
	EXPECT_TRUE(childrenOf(parent).empty());
}

TEST_F(EntitySnapshotTest, SubtreeRestoreInPlaceRepairsLinksAndOrder) {
	auto root = m_scene.createEntity("Root");
	auto childA = m_scene.createEntity("A");
	auto childB = m_scene.createEntity("B");
	m_scene.setParent(childA, root);
	m_scene.setParent(childB, root);
	setLocalX(childA, 1.f);
	const auto before = sceneState(m_scene);
	const auto snapshot = SubtreeSnapshot::capture(root, m_scene);
	const auto handle = static_cast<entt::entity>(childA);

	m_scene.unparent(childA);
	setLocalX(childA, 9.f);
	m_scene.setParent(childA, root);

	snapshot.restore(m_scene);
	EXPECT_EQ(static_cast<entt::entity>(m_scene.findEntityByUUID(childA.getUUID())), handle);
	EXPECT_EQ(sceneState(m_scene), before);
}
