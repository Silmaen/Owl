/**
 * @file SceneHierarchy_test.cpp
 * @author Silmaen
 * @date 04/08/26
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/component/components.h>

using namespace owl::scene;
using namespace owl::scene::component;

TEST(SceneHierarchy, HierarchyAutoAdded) {
	Scene sc;
	auto ent = sc.createEntity("test");
	EXPECT_TRUE(ent.hasComponent<Hierarchy>());
	const auto& h = ent.getComponent<Hierarchy>();
	EXPECT_EQ(static_cast<uint64_t>(h.parentId), 0u);
	EXPECT_TRUE(h.childrenIds.empty());
}

TEST(SceneHierarchy, SetParent) {
	Scene sc;
	auto parent = sc.createEntity("parent");
	auto child = sc.createEntity("child");
	sc.setParent(child, parent);
	const auto& childH = child.getComponent<Hierarchy>();
	const auto& parentH = parent.getComponent<Hierarchy>();
	EXPECT_EQ(static_cast<uint64_t>(childH.parentId), static_cast<uint64_t>(parent.getUUID()));
	ASSERT_EQ(parentH.childrenIds.size(), 1u);
	EXPECT_EQ(static_cast<uint64_t>(parentH.childrenIds[0]), static_cast<uint64_t>(child.getUUID()));
}

TEST(SceneHierarchy, Unparent) {
	Scene sc;
	auto parent = sc.createEntity("parent");
	parent.getComponent<Transform>().transform.translation() = {5.f, 0.f, 0.f};
	auto child = sc.createEntity("child");
	child.getComponent<Transform>().transform.translation() = {3.f, 0.f, 0.f};
	sc.setParent(child, parent);
	// child's local transform should have been recomputed.
	sc.unparent(child);
	// After unparent, child should be root and keep its world position.
	const auto& childH = child.getComponent<Hierarchy>();
	EXPECT_EQ(static_cast<uint64_t>(childH.parentId), 0u);
	// The parent should have no children.
	const auto& parentH = parent.getComponent<Hierarchy>();
	EXPECT_TRUE(parentH.childrenIds.empty());
}

TEST(SceneHierarchy, CircularReferencePrevention) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	Scene sc;
	auto a = sc.createEntity("A");
	auto b = sc.createEntity("B");
	auto c = sc.createEntity("C");
	sc.setParent(b, a);
	sc.setParent(c, b);
	// Try to set A's parent to C -> circular, should be refused.
	sc.setParent(a, c);
	const auto& aH = a.getComponent<Hierarchy>();
	EXPECT_EQ(static_cast<uint64_t>(aH.parentId), 0u);
}

TEST(SceneHierarchy, SelfParentPrevention) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	Scene sc;
	auto ent = sc.createEntity("self");
	sc.setParent(ent, ent);
	const auto& h = ent.getComponent<Hierarchy>();
	EXPECT_EQ(static_cast<uint64_t>(h.parentId), 0u);
}

TEST(SceneHierarchy, WorldTransformSimple) {
	Scene sc;
	auto parent = sc.createEntity("parent");
	parent.getComponent<Transform>().transform.translation() = {5.f, 0.f, 0.f};
	auto child = sc.createEntity("child");
	child.getComponent<Transform>().transform.translation() = {3.f, 0.f, 0.f};
	sc.setParent(child, parent);
	const auto worldTransform = sc.getWorldTransform(child);
	// Parent at (5,0,0), child was at (3,0,0) before reparent.
	// setParent recomputed child local = inverse(parentWorld) * childWorld = (3-5, 0, 0) = (-2, 0, 0)
	// world = parent * local = (5, 0, 0) + (-2, 0, 0) = (3, 0, 0)
	EXPECT_NEAR(worldTransform.translation().x(), 3.f, 0.001f);
	EXPECT_NEAR(worldTransform.translation().y(), 0.f, 0.001f);
}

TEST(SceneHierarchy, WorldTransformNested) {
	Scene sc;
	auto gp = sc.createEntity("grandparent");
	gp.getComponent<Transform>().transform.translation() = {10.f, 0.f, 0.f};
	auto p = sc.createEntity("parent");
	p.getComponent<Transform>().transform.translation() = {5.f, 0.f, 0.f};
	auto c = sc.createEntity("child");
	c.getComponent<Transform>().transform.translation() = {2.f, 0.f, 0.f};
	sc.setParent(p, gp);
	sc.setParent(c, p);
	const auto worldTransform = sc.getWorldTransform(c);
	// gp at (10,0,0), p was at (5,0,0) -> p local = (-5,0,0), p world = (10-5=5,0,0)
	// c was at (2,0,0) -> c local = inverse(pWorld) * (2,0,0) = (2-5=-3,0,0), c world = (5-3=2,0,0)
	EXPECT_NEAR(worldTransform.translation().x(), 2.f, 0.001f);
}

TEST(SceneHierarchy, EffectiveVisibility) {
	Scene sc;
	auto parent = sc.createEntity("parent");
	auto child = sc.createEntity("child");
	sc.setParent(child, parent);
	// Both visible initially.
	EXPECT_TRUE(sc.isEffectivelyVisible(child, true));
	EXPECT_TRUE(sc.isEffectivelyVisible(child, false));
	// Hide parent in editor.
	parent.getComponent<Visibility>().editorVisible = false;
	EXPECT_FALSE(sc.isEffectivelyVisible(child, true));
	EXPECT_TRUE(sc.isEffectivelyVisible(child, false));
	// Hide parent in game.
	parent.getComponent<Visibility>().gameVisible = false;
	EXPECT_FALSE(sc.isEffectivelyVisible(child, false));
}

TEST(SceneHierarchy, OrphanOnDestroyRoot) {
	Scene sc;
	auto parent = sc.createEntity("parent");
	parent.getComponent<Transform>().transform.translation() = {5.f, 0.f, 0.f};
	auto child1 = sc.createEntity("child1");
	auto child2 = sc.createEntity("child2");
	sc.setParent(child1, parent);
	sc.setParent(child2, parent);
	sc.destroyEntity(parent);
	// Parent was root, so children become root entities.
	EXPECT_EQ(static_cast<uint64_t>(child1.getComponent<Hierarchy>().parentId), 0u);
	EXPECT_EQ(static_cast<uint64_t>(child2.getComponent<Hierarchy>().parentId), 0u);
}

TEST(SceneHierarchy, ReparentToGrandparentOnDestroy) {
	Scene sc;
	auto grandparent = sc.createEntity("grandparent");
	auto parent = sc.createEntity("parent");
	auto child = sc.createEntity("child");
	sc.setParent(parent, grandparent);
	sc.setParent(child, parent);
	sc.destroyEntity(parent);
	// Child should now be a child of grandparent.
	EXPECT_EQ(static_cast<uint64_t>(child.getComponent<Hierarchy>().parentId),
			  static_cast<uint64_t>(grandparent.getUUID()));
	const auto& gpChildren = grandparent.getComponent<Hierarchy>().childrenIds;
	EXPECT_EQ(gpChildren.size(), 1u);
	EXPECT_EQ(static_cast<uint64_t>(gpChildren[0]), static_cast<uint64_t>(child.getUUID()));
}

TEST(SceneHierarchy, CascadeDelete) {
	Scene sc;
	auto parent = sc.createEntity("parent");
	auto child1 = sc.createEntity("child1");
	auto child2 = sc.createEntity("child2");
	sc.setParent(child1, parent);
	sc.setParent(child2, parent);
	EXPECT_EQ(sc.getAllEntities().size(), 3u);
	sc.destroyEntityWithChildren(parent);
	EXPECT_TRUE(sc.getAllEntities().empty());
}

TEST(SceneHierarchy, DuplicateEntityIsRoot) {
	const owl::shared<Scene> sc = owl::mkShared<Scene>();
	auto parent = sc->createEntity("parent");
	auto child = sc->createEntity("child");
	sc->setParent(child, parent);
	auto dup = sc->duplicateEntity(child);
	const auto& dupH = dup.getComponent<Hierarchy>();
	EXPECT_EQ(static_cast<uint64_t>(dupH.parentId), 0u);
	EXPECT_TRUE(dupH.childrenIds.empty());
}

TEST(SceneHierarchy, DuplicateSubtree) {
	const owl::shared<Scene> sc = owl::mkShared<Scene>();
	auto parent = sc->createEntity("parent");
	auto child = sc->createEntity("child");
	sc->setParent(child, parent);
	const auto countBefore = sc->getAllEntities().size();
	auto dupRoot = sc->duplicateSubtree(parent);
	// Should have created 2 new entities (parent + child).
	EXPECT_EQ(sc->getAllEntities().size(), countBefore + 2);
	// Duplicate root is a root entity.
	EXPECT_EQ(static_cast<uint64_t>(dupRoot.getComponent<Hierarchy>().parentId), 0u);
	// Duplicate root should have 1 child.
	EXPECT_EQ(dupRoot.getComponent<Hierarchy>().childrenIds.size(), 1u);
	// The child should reference the duplicate root.
	const auto dupChildId = dupRoot.getComponent<Hierarchy>().childrenIds[0];
	const auto dupChild = sc->findEntityByUUID(dupChildId);
	EXPECT_TRUE(dupChild);
	EXPECT_EQ(static_cast<uint64_t>(dupChild.getComponent<Hierarchy>().parentId),
			  static_cast<uint64_t>(dupRoot.getUUID()));
}

TEST(SceneHierarchy, RootEntities) {
	Scene sc;
	auto root1 = sc.createEntity("root1");
	sc.createEntity("root2");
	auto child = sc.createEntity("child");
	sc.setParent(child, root1);
	const auto roots = sc.getRootEntities();
	EXPECT_EQ(roots.size(), 2u);
}

TEST(SceneHierarchy, FindByUUID) {
	Scene sc;
	auto ent = sc.createEntity("findme");
	const auto uuid = ent.getUUID();
	auto found = sc.findEntityByUUID(uuid);
	EXPECT_TRUE(found);
	EXPECT_EQ(found.getName(), "findme");
	auto notFound = sc.findEntityByUUID(owl::core::UUID{999999});
	EXPECT_FALSE(notFound);
}

TEST(SceneHierarchy, ReparentPreservesWorldPosition) {
	Scene sc;
	auto parent1 = sc.createEntity("parent1");
	parent1.getComponent<Transform>().transform.translation() = {10.f, 0.f, 0.f};
	auto parent2 = sc.createEntity("parent2");
	parent2.getComponent<Transform>().transform.translation() = {20.f, 0.f, 0.f};
	auto child = sc.createEntity("child");
	child.getComponent<Transform>().transform.translation() = {15.f, 0.f, 0.f};
	sc.setParent(child, parent1);
	// child world should still be ~15.
	auto worldBefore = sc.getWorldTransform(child);
	EXPECT_NEAR(worldBefore.translation().x(), 15.f, 0.001f);
	// Reparent to parent2.
	sc.setParent(child, parent2);
	auto worldAfter = sc.getWorldTransform(child);
	// World position should be preserved.
	EXPECT_NEAR(worldAfter.translation().x(), 15.f, 0.001f);
}

TEST(SceneHierarchy, SceneCopyPreservesHierarchy) {
	const owl::shared<Scene> sc = owl::mkShared<Scene>();
	auto parent = sc->createEntity("parent");
	auto child = sc->createEntity("child");
	sc->setParent(child, parent);
	auto sc2 = Scene::copy(sc);
	sc2->rebuildHierarchyChildren();
	const auto roots = sc2->getRootEntities();
	EXPECT_EQ(roots.size(), 1u);
	EXPECT_EQ(roots[0].getName(), "parent");
	const auto& parentH = roots[0].getComponent<Hierarchy>();
	EXPECT_EQ(parentH.childrenIds.size(), 1u);
}

TEST(SceneHierarchy, SerializationRoundTrip) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	const auto rootPath = owl::test::getRootPath();
	const auto filePath = rootPath / "test" / "scene_tests" / "test_hierarchy.owl";
	{
		const owl::shared<Scene> sc = owl::mkShared<Scene>();
		auto parent = sc->createEntity("parent");
		parent.getComponent<Transform>().transform.translation() = {5.f, 0.f, 0.f};
		auto child = sc->createEntity("child");
		child.getComponent<Transform>().transform.translation() = {3.f, 0.f, 0.f};
		sc->setParent(child, parent);
		const SceneSerializer serializer(sc);
		serializer.serialize(filePath);
	}
	{
		const owl::shared<Scene> sc = owl::mkShared<Scene>();
		const SceneSerializer serializer(sc);
		EXPECT_TRUE(serializer.deserialize(filePath));
		const auto roots = sc->getRootEntities();
		EXPECT_EQ(roots.size(), 1u);
		EXPECT_EQ(roots[0].getName(), "parent");
		const auto& parentH = roots[0].getComponent<Hierarchy>();
		EXPECT_EQ(parentH.childrenIds.size(), 1u);
	}
	std::filesystem::remove(filePath);
}

TEST(SceneHierarchy, BackwardCompat) {
	// An entity without Hierarchy key should default to root.
	Scene sc;
	auto ent = sc.createEntity("old_entity");
	const auto& h = ent.getComponent<Hierarchy>();
	EXPECT_EQ(static_cast<uint64_t>(h.parentId), 0u);
	EXPECT_TRUE(h.childrenIds.empty());
}
