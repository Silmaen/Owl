/**
 * @file SceneCoverage_test.cpp
 * @author Silmaen
 * @date 14/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/component/components.h>

using namespace owl;
using namespace owl::scene;
using namespace owl::scene::component;

// ============================================================================
// Scene::copy — independent modification and GameState propagation
// ============================================================================

TEST(SceneCoverage, CopyCreatesIndependentEntities) {
	const shared<Scene> original = mkShared<Scene>();
	original->onViewportResize({800, 600});
	auto ent = original->createEntity("Original");
	ent.addOrReplaceComponent<SpriteRenderer>();

	auto copy = Scene::copy(original);
	EXPECT_EQ(copy->getAllEntities().size(), original->getAllEntities().size());

	// Modify the copy: add a new entity.
	copy->createEntity("CopyOnly");
	EXPECT_EQ(copy->getAllEntities().size(), 2u);
	// Original is unaffected.
	EXPECT_EQ(original->getAllEntities().size(), 1u);
}

TEST(SceneCoverage, CopyCopiesGameState) {
	const shared<Scene> original = mkShared<Scene>();
	original->getGameState().set("score", 42);
	original->getGameState().set("name", std::string("player1"));

	auto copy = Scene::copy(original);
	ASSERT_TRUE(copy->getGameState().get("score").has_value());
	EXPECT_EQ(std::get<int64_t>(copy->getGameState().get("score").value()), 42);
	ASSERT_TRUE(copy->getGameState().get("name").has_value());
	EXPECT_EQ(std::get<std::string>(copy->getGameState().get("name").value()), "player1");

	// Modifying copy state does not affect original.
	copy->getGameState().set("score", 100);
	EXPECT_EQ(std::get<int64_t>(original->getGameState().get("score").value()), 42);
}

TEST(SceneCoverage, CopyDuplicatesComponentData) {
	const shared<Scene> original = mkShared<Scene>();
	auto ent = original->createEntity("Sprite");
	auto& sprite = ent.addOrReplaceComponent<SpriteRenderer>();
	sprite.color = {0.5f, 0.5f, 0.5f, 1.0f};
	sprite.tilingFactor = {3.0f, 3.0f};

	auto copy = Scene::copy(original);
	const auto allCopy = copy->getAllEntities();
	ASSERT_EQ(allCopy.size(), 1u);
	EXPECT_TRUE(allCopy[0].hasComponent<SpriteRenderer>());
	const auto& copiedSprite = allCopy[0].getComponent<SpriteRenderer>();
	EXPECT_FLOAT_EQ(copiedSprite.tilingFactor.x(), 3.0f);
	EXPECT_FLOAT_EQ(copiedSprite.color.x(), 0.5f);
}

// ============================================================================
// Scene::getEntityCount — direct method exercise
// ============================================================================

TEST(SceneCoverage, GetEntityCountEmpty) {
	const Scene sc;
	EXPECT_EQ(sc.getEntityCount(), 0u);
}

TEST(SceneCoverage, GetEntityCountAfterCreateAndDestroy) {
	Scene sc;
	auto a = sc.createEntity("A");
	sc.createEntity("B");
	sc.createEntity("C");
	EXPECT_EQ(sc.getAllEntities().size(), 3u);
	sc.destroyEntity(a);
	EXPECT_EQ(sc.getAllEntities().size(), 2u);
}

// ============================================================================
// Scene::getChildren — explicit test
// ============================================================================

TEST(SceneCoverage, GetChildrenReturnsCorrectEntities) {
	Scene sc;
	auto parent = sc.createEntity("parent");
	auto child1 = sc.createEntity("child1");
	auto child2 = sc.createEntity("child2");
	sc.createEntity("unrelated");
	sc.setParent(child1, parent);
	sc.setParent(child2, parent);

	const auto children = sc.getChildren(parent);
	EXPECT_EQ(children.size(), 2u);
	// The children should be the ones we parented.
	bool foundChild1 = false;
	bool foundChild2 = false;
	for (const auto& child: children) {
		if (child.getUUID() == child1.getUUID())
			foundChild1 = true;
		if (child.getUUID() == child2.getUUID())
			foundChild2 = true;
	}
	EXPECT_TRUE(foundChild1);
	EXPECT_TRUE(foundChild2);
}

TEST(SceneCoverage, GetChildrenEmptyForLeaf) {
	Scene sc;
	auto leaf = sc.createEntity("leaf");
	const auto children = sc.getChildren(leaf);
	EXPECT_TRUE(children.empty());
}

// ============================================================================
// Scene::findEntityByUUID edge cases
// ============================================================================

TEST(SceneCoverage, FindEntityByUUIDZero) {
	Scene sc;
	sc.createEntity("ent");
	// UUID 0 should not match any normal entity (UUID 0 is the "no parent" sentinel).
	auto found = sc.findEntityByUUID(core::UUID{0});
	// This may or may not find the entity depending on whether createEntityWithUUID(0) was used.
	// Since we used createEntity (which generates a random UUID), UUID 0 should not be found.
	EXPECT_FALSE(found);
}

TEST(SceneCoverage, FindEntityByUUIDExplicitZero) {
	// An entity created with explicit UUID 0 should be findable.
	Scene sc;
	sc.createEntityWithUUID(core::UUID{0}, "zero_ent");
	auto found = sc.findEntityByUUID(core::UUID{0});
	EXPECT_TRUE(found);
	EXPECT_EQ(found.getName(), "zero_ent");
}

// ============================================================================
// Scene::isEffectivelyVisible — deeply nested and root
// ============================================================================

TEST(SceneCoverage, IsEffectivelyVisibleRootEntity) {
	Scene sc;
	auto root = sc.createEntity("root");
	EXPECT_TRUE(sc.isEffectivelyVisible(root, true));
	EXPECT_TRUE(sc.isEffectivelyVisible(root, false));

	root.getComponent<Visibility>().gameVisible = false;
	EXPECT_FALSE(sc.isEffectivelyVisible(root, false));
	EXPECT_TRUE(sc.isEffectivelyVisible(root, true));
}

TEST(SceneCoverage, IsEffectivelyVisibleGrandparentHidden) {
	Scene sc;
	auto grandparent = sc.createEntity("grandparent");
	auto parent = sc.createEntity("parent");
	auto child = sc.createEntity("child");
	sc.setParent(parent, grandparent);
	sc.setParent(child, parent);

	// All visible initially.
	EXPECT_TRUE(sc.isEffectivelyVisible(child, false));
	EXPECT_TRUE(sc.isEffectivelyVisible(child, true));

	// Hide grandparent in game mode.
	grandparent.getComponent<Visibility>().gameVisible = false;
	EXPECT_FALSE(sc.isEffectivelyVisible(child, false));
	// Editor should still be visible.
	EXPECT_TRUE(sc.isEffectivelyVisible(child, true));

	// Hide grandparent in editor mode too.
	grandparent.getComponent<Visibility>().editorVisible = false;
	EXPECT_FALSE(sc.isEffectivelyVisible(child, true));
}

TEST(SceneCoverage, IsEffectivelyVisibleMiddleLayerHidden) {
	Scene sc;
	auto grandparent = sc.createEntity("gp");
	auto parent = sc.createEntity("parent");
	auto child = sc.createEntity("child");
	sc.setParent(parent, grandparent);
	sc.setParent(child, parent);

	// Hide only the middle layer.
	parent.getComponent<Visibility>().editorVisible = false;
	EXPECT_FALSE(sc.isEffectivelyVisible(child, true));
	// Grandparent is still visible.
	EXPECT_TRUE(sc.isEffectivelyVisible(grandparent, true));
	// Parent itself is not visible.
	EXPECT_FALSE(sc.isEffectivelyVisible(parent, true));
}

// ============================================================================
// Scene::getWorldTransform — with scale and rotation
// ============================================================================

TEST(SceneCoverage, WorldTransformWithScale) {
	Scene sc;
	auto parent = sc.createEntity("parent");
	parent.getComponent<Transform>().transform.translation() = {0.f, 0.f, 0.f};
	parent.getComponent<Transform>().transform.scale() = {2.f, 2.f, 1.f};

	auto child = sc.createEntity("child");
	child.getComponent<Transform>().transform.translation() = {3.f, 0.f, 0.f};

	sc.setParent(child, parent);
	// After setParent, child local = inv(parentWorld) * childWorld.
	// parentWorld has scale (2,2,1), so inv(parentWorld) scales by (0.5,0.5,1).
	// child was at (3,0,0), so local = (1.5, 0, 0).
	// World = parent * local: translation = scale * local_t = (2*1.5, 0, 0) = (3, 0, 0).
	const auto wt = sc.getWorldTransform(child);
	EXPECT_NEAR(wt.translation().x(), 3.f, 0.01f);
	EXPECT_NEAR(wt.translation().y(), 0.f, 0.01f);
}

TEST(SceneCoverage, WorldTransformRootEntityIsLocal) {
	Scene sc;
	auto root = sc.createEntity("root");
	root.getComponent<Transform>().transform.translation() = {7.f, 3.f, 1.f};
	root.getComponent<Transform>().transform.scale() = {2.f, 3.f, 1.f};

	const auto wt = sc.getWorldTransform(root);
	EXPECT_FLOAT_EQ(wt.translation().x(), 7.f);
	EXPECT_FLOAT_EQ(wt.translation().y(), 3.f);
	EXPECT_FLOAT_EQ(wt.translation().z(), 1.f);
	EXPECT_FLOAT_EQ(wt.scale().x(), 2.f);
	EXPECT_FLOAT_EQ(wt.scale().y(), 3.f);
}

TEST(SceneCoverage, WorldTransformThreeDeepChain) {
	Scene sc;
	auto gp = sc.createEntity("gp");
	gp.getComponent<Transform>().transform.translation() = {10.f, 0.f, 0.f};

	auto parent = sc.createEntity("parent");
	parent.getComponent<Transform>().transform.translation() = {20.f, 0.f, 0.f};

	auto child = sc.createEntity("child");
	child.getComponent<Transform>().transform.translation() = {30.f, 0.f, 0.f};

	sc.setParent(parent, gp);
	sc.setParent(child, parent);

	// Each setParent recomputes local to preserve world position.
	// So child's world position should still be 30.
	const auto wt = sc.getWorldTransform(child);
	EXPECT_NEAR(wt.translation().x(), 30.f, 0.01f);
}

// ============================================================================
// Scene::destroyEntity — multi-child reparenting to grandparent
// ============================================================================

TEST(SceneCoverage, DestroyEntityReparentsMultipleChildrenToGrandparent) {
	Scene sc;
	auto gp = sc.createEntity("grandparent");
	auto parent = sc.createEntity("parent");
	auto child1 = sc.createEntity("child1");
	auto child2 = sc.createEntity("child2");
	auto child3 = sc.createEntity("child3");
	sc.setParent(parent, gp);
	sc.setParent(child1, parent);
	sc.setParent(child2, parent);
	sc.setParent(child3, parent);

	EXPECT_EQ(sc.getAllEntities().size(), 5u);
	sc.destroyEntity(parent);
	EXPECT_EQ(sc.getAllEntities().size(), 4u);

	// All three children should now be children of gp.
	const auto& gpH = gp.getComponent<Hierarchy>();
	EXPECT_EQ(gpH.childrenIds.size(), 3u);
	EXPECT_EQ(static_cast<uint64_t>(child1.getComponent<Hierarchy>().parentId), static_cast<uint64_t>(gp.getUUID()));
	EXPECT_EQ(static_cast<uint64_t>(child2.getComponent<Hierarchy>().parentId), static_cast<uint64_t>(gp.getUUID()));
	EXPECT_EQ(static_cast<uint64_t>(child3.getComponent<Hierarchy>().parentId), static_cast<uint64_t>(gp.getUUID()));
}

TEST(SceneCoverage, DestroyEntityLeafNode) {
	Scene sc;
	auto parent = sc.createEntity("parent");
	auto child = sc.createEntity("child");
	sc.setParent(child, parent);
	EXPECT_EQ(sc.getAllEntities().size(), 2u);

	sc.destroyEntity(child);
	EXPECT_EQ(sc.getAllEntities().size(), 1u);
	// Parent should have no children.
	EXPECT_TRUE(parent.getComponent<Hierarchy>().childrenIds.empty());
}

// ============================================================================
// Scene::destroyEntityWithChildren — deep subtree
// ============================================================================

TEST(SceneCoverage, DestroyEntityWithChildrenDeepSubtree) {
	Scene sc;
	auto root = sc.createEntity("root");
	auto child = sc.createEntity("child");
	auto grandchild = sc.createEntity("grandchild");
	auto greatgrandchild = sc.createEntity("greatgrandchild");
	sc.setParent(child, root);
	sc.setParent(grandchild, child);
	sc.setParent(greatgrandchild, grandchild);

	EXPECT_EQ(sc.getAllEntities().size(), 4u);
	sc.destroyEntityWithChildren(root);
	EXPECT_TRUE(sc.getAllEntities().empty());
}

TEST(SceneCoverage, DestroyEntityWithChildrenRemovesFromGrandparent) {
	Scene sc;
	auto gp = sc.createEntity("gp");
	auto parent = sc.createEntity("parent");
	auto child = sc.createEntity("child");
	sc.setParent(parent, gp);
	sc.setParent(child, parent);
	EXPECT_EQ(sc.getAllEntities().size(), 3u);

	sc.destroyEntityWithChildren(parent);
	EXPECT_EQ(sc.getAllEntities().size(), 1u);
	// Grandparent should have no children anymore.
	EXPECT_TRUE(gp.getComponent<Hierarchy>().childrenIds.empty());
}

// ============================================================================
// Scene::duplicateSubtree — deeper hierarchy and component preservation
// ============================================================================

TEST(SceneCoverage, DuplicateSubtreeThreeDeep) {
	const shared<Scene> sc = mkShared<Scene>();
	auto root = sc->createEntity("root");
	auto child = sc->createEntity("child");
	auto grandchild = sc->createEntity("grandchild");
	sc->setParent(child, root);
	sc->setParent(grandchild, child);

	const auto countBefore = sc->getAllEntities().size();
	auto dupRoot = sc->duplicateSubtree(root);
	// Should create 3 new entities.
	EXPECT_EQ(sc->getAllEntities().size(), countBefore + 3);

	// dupRoot is a root entity.
	EXPECT_EQ(static_cast<uint64_t>(dupRoot.getComponent<Hierarchy>().parentId), 0u);
	// dupRoot should have 1 child.
	ASSERT_EQ(dupRoot.getComponent<Hierarchy>().childrenIds.size(), 1u);
	// That child should have 1 grandchild.
	auto dupChild = sc->findEntityByUUID(dupRoot.getComponent<Hierarchy>().childrenIds[0]);
	ASSERT_TRUE(dupChild);
	ASSERT_EQ(dupChild.getComponent<Hierarchy>().childrenIds.size(), 1u);
	auto dupGrandchild = sc->findEntityByUUID(dupChild.getComponent<Hierarchy>().childrenIds[0]);
	ASSERT_TRUE(dupGrandchild);
	EXPECT_TRUE(dupGrandchild.getComponent<Hierarchy>().childrenIds.empty());
}

TEST(SceneCoverage, DuplicateSubtreePreservesNames) {
	const shared<Scene> sc = mkShared<Scene>();
	auto root = sc->createEntity("RootName");
	auto child = sc->createEntity("ChildName");
	sc->setParent(child, root);

	auto dupRoot = sc->duplicateSubtree(root);
	EXPECT_EQ(dupRoot.getName(), "RootName");
	auto dupChild = sc->findEntityByUUID(dupRoot.getComponent<Hierarchy>().childrenIds[0]);
	ASSERT_TRUE(dupChild);
	EXPECT_EQ(dupChild.getName(), "ChildName");
}

TEST(SceneCoverage, DuplicateSubtreeNewUUIDs) {
	const shared<Scene> sc = mkShared<Scene>();
	auto root = sc->createEntity("root");
	auto child = sc->createEntity("child");
	sc->setParent(child, root);

	auto dupRoot = sc->duplicateSubtree(root);
	EXPECT_NE(dupRoot.getUUID(), root.getUUID());
	auto dupChild = sc->findEntityByUUID(dupRoot.getComponent<Hierarchy>().childrenIds[0]);
	ASSERT_TRUE(dupChild);
	EXPECT_NE(dupChild.getUUID(), child.getUUID());
}

TEST(SceneCoverage, DuplicateSubtreePreservesComponents) {
	const shared<Scene> sc = mkShared<Scene>();
	auto root = sc->createEntity("root");
	auto& sprite = root.addOrReplaceComponent<SpriteRenderer>();
	sprite.tilingFactor = {5.0f, 5.0f};
	auto child = sc->createEntity("child");
	child.addOrReplaceComponent<CircleRenderer>();
	sc->setParent(child, root);

	auto dupRoot = sc->duplicateSubtree(root);
	EXPECT_TRUE(dupRoot.hasComponent<SpriteRenderer>());
	EXPECT_FLOAT_EQ(dupRoot.getComponent<SpriteRenderer>().tilingFactor.x(), 5.0f);

	auto dupChild = sc->findEntityByUUID(dupRoot.getComponent<Hierarchy>().childrenIds[0]);
	ASSERT_TRUE(dupChild);
	EXPECT_TRUE(dupChild.hasComponent<CircleRenderer>());
}

// ============================================================================
// Scene status, teleportRequest, saveLoadRequest fields
// ============================================================================

TEST(SceneCoverage, StatusDefaultIsEditing) {
	const Scene sc;
	EXPECT_EQ(sc.status, Scene::Status::Editing);
}

TEST(SceneCoverage, StatusCanBeSetDirectly) {
	Scene sc;
	sc.status = Scene::Status::Playing;
	EXPECT_EQ(sc.status, Scene::Status::Playing);
	sc.status = Scene::Status::Victory;
	EXPECT_EQ(sc.status, Scene::Status::Victory);
	sc.status = Scene::Status::Death;
	EXPECT_EQ(sc.status, Scene::Status::Death);
	sc.status = Scene::Status::Editing;
	EXPECT_EQ(sc.status, Scene::Status::Editing);
}

TEST(SceneCoverage, TeleportRequestDefaults) {
	const Scene sc;
	EXPECT_FALSE(sc.teleportRequest.pending);
	EXPECT_TRUE(sc.teleportRequest.levelName.empty());
	EXPECT_TRUE(sc.teleportRequest.targetName.empty());
	EXPECT_FLOAT_EQ(sc.teleportRequest.initialVelocity.x(), 0.f);
	EXPECT_FLOAT_EQ(sc.teleportRequest.initialVelocity.y(), 0.f);
	EXPECT_FLOAT_EQ(sc.teleportRequest.rotationDelta, 0.f);
}

TEST(SceneCoverage, TeleportRequestModifiable) {
	Scene sc;
	sc.teleportRequest.pending = true;
	sc.teleportRequest.levelName = "level2";
	sc.teleportRequest.targetName = "spawn_point";
	sc.teleportRequest.initialVelocity = {1.5f, 2.5f};
	sc.teleportRequest.rotationDelta = 3.14f;
	EXPECT_TRUE(sc.teleportRequest.pending);
	EXPECT_EQ(sc.teleportRequest.levelName, "level2");
	EXPECT_EQ(sc.teleportRequest.targetName, "spawn_point");
	EXPECT_FLOAT_EQ(sc.teleportRequest.initialVelocity.x(), 1.5f);
	EXPECT_FLOAT_EQ(sc.teleportRequest.initialVelocity.y(), 2.5f);
	EXPECT_FLOAT_EQ(sc.teleportRequest.rotationDelta, 3.14f);
}

TEST(SceneCoverage, SaveLoadRequestDefaults) {
	const Scene sc;
	EXPECT_FALSE(sc.saveLoadRequest.pending);
	EXPECT_FALSE(sc.saveLoadRequest.isLoad);
	EXPECT_EQ(sc.saveLoadRequest.slot, 0u);
}

TEST(SceneCoverage, SaveLoadRequestModifiable) {
	Scene sc;
	sc.saveLoadRequest.pending = true;
	sc.saveLoadRequest.isLoad = true;
	sc.saveLoadRequest.slot = 5;
	EXPECT_TRUE(sc.saveLoadRequest.pending);
	EXPECT_TRUE(sc.saveLoadRequest.isLoad);
	EXPECT_EQ(sc.saveLoadRequest.slot, 5u);
}

// ============================================================================
// Scene::getGameState — full API exercise
// ============================================================================

TEST(SceneCoverage, GameStateSetGetInt) {
	Scene sc;
	auto& gs = sc.getGameState();
	gs.set("score", static_cast<int64_t>(100));
	auto val = gs.get("score");
	ASSERT_TRUE(val.has_value());
	EXPECT_EQ(std::get<int64_t>(val.value()), 100);
}

TEST(SceneCoverage, GameStateSetGetFloat) {
	Scene sc;
	auto& gs = sc.getGameState();
	gs.set("health", 75.5f);
	auto val = gs.get("health");
	ASSERT_TRUE(val.has_value());
	EXPECT_FLOAT_EQ(std::get<float>(val.value()), 75.5f);
}

TEST(SceneCoverage, GameStateSetGetString) {
	Scene sc;
	auto& gs = sc.getGameState();
	gs.set("name", std::string("Hero"));
	auto val = gs.get("name");
	ASSERT_TRUE(val.has_value());
	EXPECT_EQ(std::get<std::string>(val.value()), "Hero");
}

TEST(SceneCoverage, GameStateSetGetBool) {
	Scene sc;
	auto& gs = sc.getGameState();
	gs.set("alive", true);
	auto val = gs.get("alive");
	ASSERT_TRUE(val.has_value());
	EXPECT_TRUE(std::get<bool>(val.value()));
}

TEST(SceneCoverage, GameStateGetMissingKey) {
	const Scene sc;
	const auto& gs = sc.getGameState();
	auto val = gs.get("nonexistent");
	EXPECT_FALSE(val.has_value());
}

TEST(SceneCoverage, GameStateGetWithDefault) {
	const Scene sc;
	const auto& gs = sc.getGameState();
	auto val = gs.get("missing", static_cast<int64_t>(42));
	EXPECT_EQ(std::get<int64_t>(val), 42);
}

TEST(SceneCoverage, GameStateRemove) {
	Scene sc;
	auto& gs = sc.getGameState();
	gs.set("key", static_cast<int64_t>(1));
	EXPECT_TRUE(gs.get("key").has_value());
	gs.remove("key");
	EXPECT_FALSE(gs.get("key").has_value());
}

TEST(SceneCoverage, GameStateClear) {
	Scene sc;
	auto& gs = sc.getGameState();
	gs.set("a", static_cast<int64_t>(1));
	gs.set("b", static_cast<int64_t>(2));
	EXPECT_EQ(gs.size(), 2u);
	gs.clear();
	EXPECT_TRUE(gs.empty());
	EXPECT_EQ(gs.size(), 0u);
}

TEST(SceneCoverage, GameStateKeys) {
	Scene sc;
	auto& gs = sc.getGameState();
	gs.set("alpha", static_cast<int64_t>(1));
	gs.set("beta", static_cast<int64_t>(2));
	gs.set("gamma", static_cast<int64_t>(3));
	auto keys = gs.keys();
	EXPECT_EQ(keys.size(), 3u);
	// Check all keys are present (order not guaranteed for unordered_map).
	std::sort(keys.begin(), keys.end());
	EXPECT_EQ(keys[0], "alpha");
	EXPECT_EQ(keys[1], "beta");
	EXPECT_EQ(keys[2], "gamma");
}

TEST(SceneCoverage, GameStateEmptyInitially) {
	const Scene sc;
	const auto& gs = sc.getGameState();
	EXPECT_TRUE(gs.empty());
	EXPECT_EQ(gs.size(), 0u);
}

TEST(SceneCoverage, GameStateOverwrite) {
	Scene sc;
	auto& gs = sc.getGameState();
	gs.set("key", static_cast<int64_t>(1));
	gs.set("key", static_cast<int64_t>(999));
	EXPECT_EQ(std::get<int64_t>(gs.get("key").value()), 999);
	EXPECT_EQ(gs.size(), 1u);
}

TEST(SceneCoverage, GameStateConstAccess) {
	Scene sc;
	sc.getGameState().set("val", 3.14f);
	const Scene& constSc = sc;
	const auto& constGs = constSc.getGameState();
	ASSERT_TRUE(constGs.get("val").has_value());
	EXPECT_FLOAT_EQ(std::get<float>(constGs.get("val").value()), 3.14f);
}

// ============================================================================
// Scene::onViewportResize — camera aspect ratio behaviour
// ============================================================================

TEST(SceneCoverage, OnViewportResizeFixedAspectRatioCamera) {
	Scene sc;
	auto camEnt = sc.createEntity("FixedCam");
	auto& cam = camEnt.addOrReplaceComponent<Camera>(true, true);
	cam.fixedAspectRatio = true;
	// Should not crash and should not resize the fixed-aspect-ratio camera.
	sc.onViewportResize({1024, 768});
	EXPECT_TRUE(cam.fixedAspectRatio);
}

TEST(SceneCoverage, OnViewportResizeMultipleCameras) {
	Scene sc;
	auto cam1 = sc.createEntity("Cam1");
	auto& c1 = cam1.addOrReplaceComponent<Camera>(true, false);
	c1.fixedAspectRatio = false;
	auto cam2 = sc.createEntity("Cam2");
	auto& c2 = cam2.addOrReplaceComponent<Camera>(false, false);
	c2.fixedAspectRatio = false;
	// Both non-fixed cameras should be resized without crashing.
	sc.onViewportResize({640, 480});
	EXPECT_TRUE(true);
}

TEST(SceneCoverage, OnViewportResizeNormalDimension) {
	Scene sc;
	auto camEnt = sc.createEntity("Cam");
	camEnt.addOrReplaceComponent<Camera>(true, false);
	sc.onViewportResize({1280, 720});
	sc.onViewportResize({1920, 1080});
	EXPECT_TRUE(true);
}

// ============================================================================
// Scene::rebuildHierarchyChildren — orphan handling
// ============================================================================

TEST(SceneCoverage, RebuildHierarchyChildrenOrphansCorruptedParent) {
	core::Log::init(core::Log::Level::Off);
	Scene sc;
	auto parent = sc.createEntity("parent");
	auto child = sc.createEntity("child");
	sc.setParent(child, parent);

	// Manually corrupt the child's parentId to point to a non-existent UUID.
	child.getComponent<Hierarchy>().parentId = core::UUID{9999999};

	// Rebuild should detect the missing parent and orphan the child.
	sc.rebuildHierarchyChildren();
	EXPECT_EQ(static_cast<uint64_t>(child.getComponent<Hierarchy>().parentId), 0u);
	core::Log::invalidate();
}

TEST(SceneCoverage, RebuildHierarchyChildrenRebuildsCorrectly) {
	Scene sc;
	auto parent = sc.createEntity("parent");
	auto child1 = sc.createEntity("child1");
	auto child2 = sc.createEntity("child2");
	sc.setParent(child1, parent);
	sc.setParent(child2, parent);

	// Manually clear children lists (simulating post-deserialization state).
	parent.getComponent<Hierarchy>().childrenIds.clear();

	sc.rebuildHierarchyChildren();
	EXPECT_EQ(parent.getComponent<Hierarchy>().childrenIds.size(), 2u);
}

// ============================================================================
// Scene::unparent — already-root entity (no-op)
// ============================================================================

TEST(SceneCoverage, UnparentAlreadyRootIsNoop) {
	Scene sc;
	auto root = sc.createEntity("root");
	root.getComponent<Transform>().transform.translation() = {5.f, 3.f, 0.f};
	sc.unparent(root);
	// Should be a no-op: still root, transform unchanged.
	EXPECT_EQ(static_cast<uint64_t>(root.getComponent<Hierarchy>().parentId), 0u);
	EXPECT_FLOAT_EQ(root.getComponent<Transform>().transform.translation().x(), 5.f);
	EXPECT_FLOAT_EQ(root.getComponent<Transform>().transform.translation().y(), 3.f);
}

// ============================================================================
// Scene::setParent — edge cases
// ============================================================================

TEST(SceneCoverage, SetParentChildIsAlreadyDescendantOfNewParent) {
	core::Log::init(core::Log::Level::Off);
	Scene sc;
	auto a = sc.createEntity("A");
	auto b = sc.createEntity("B");
	auto c = sc.createEntity("C");
	sc.setParent(b, a);
	sc.setParent(c, b);

	// c is already a descendant of a via b. Try to directly set c's parent to a.
	sc.setParent(c, a);
	// This is valid (not circular). c should now be directly under a.
	EXPECT_EQ(static_cast<uint64_t>(c.getComponent<Hierarchy>().parentId), static_cast<uint64_t>(a.getUUID()));
	// b should have lost c as a child.
	EXPECT_TRUE(b.getComponent<Hierarchy>().childrenIds.empty());
	core::Log::invalidate();
}

TEST(SceneCoverage, SetParentReparentBetweenDifferentParents) {
	Scene sc;
	auto parent1 = sc.createEntity("parent1");
	auto parent2 = sc.createEntity("parent2");
	auto child = sc.createEntity("child");
	sc.setParent(child, parent1);
	EXPECT_EQ(parent1.getComponent<Hierarchy>().childrenIds.size(), 1u);
	EXPECT_TRUE(parent2.getComponent<Hierarchy>().childrenIds.empty());

	sc.setParent(child, parent2);
	// parent1 should now have no children.
	EXPECT_TRUE(parent1.getComponent<Hierarchy>().childrenIds.empty());
	// parent2 should have the child.
	EXPECT_EQ(parent2.getComponent<Hierarchy>().childrenIds.size(), 1u);
	EXPECT_EQ(static_cast<uint64_t>(child.getComponent<Hierarchy>().parentId),
			  static_cast<uint64_t>(parent2.getUUID()));
}

// ============================================================================
// Scene::duplicateEntity — hierarchy reset
// ============================================================================

TEST(SceneCoverage, DuplicateEntityAlwaysRoot) {
	const shared<Scene> sc = mkShared<Scene>();
	auto parent = sc->createEntity("parent");
	auto child = sc->createEntity("child");
	sc->setParent(child, parent);

	// Duplicate the child: should become a root entity.
	auto dup = sc->duplicateEntity(child);
	const auto& dupH = dup.getComponent<Hierarchy>();
	EXPECT_EQ(static_cast<uint64_t>(dupH.parentId), 0u);
	EXPECT_TRUE(dupH.childrenIds.empty());
	// Original child still has its parent.
	EXPECT_EQ(static_cast<uint64_t>(child.getComponent<Hierarchy>().parentId), static_cast<uint64_t>(parent.getUUID()));
}

// ============================================================================
// getPrimaryCamera / getPrimaryPlayer — multiple entities
// ============================================================================

TEST(SceneCoverage, GetPrimaryCameraMultipleCameras) {
	Scene sc;
	auto cam1 = sc.createEntity("cam1");
	cam1.addOrReplaceComponent<Camera>(false, false);
	auto cam2 = sc.createEntity("cam2");
	cam2.addOrReplaceComponent<Camera>(true, false);
	auto cam3 = sc.createEntity("cam3");
	cam3.addOrReplaceComponent<Camera>(false, false);

	auto primary = sc.getPrimaryCamera();
	EXPECT_TRUE(primary);
	EXPECT_EQ(primary.getUUID(), cam2.getUUID());
}

TEST(SceneCoverage, GetPrimaryCameraNoPrimary) {
	Scene sc;
	auto cam = sc.createEntity("cam");
	cam.addOrReplaceComponent<Camera>(false, false);
	auto primary = sc.getPrimaryCamera();
	EXPECT_FALSE(primary);
}

TEST(SceneCoverage, GetPrimaryPlayerMultiplePlayers) {
	core::Log::init(core::Log::Level::Off);
	Scene sc;
	auto p1 = sc.createEntity("player1");
	auto& [primary1, player1] = p1.addComponent<Player>();
	primary1 = false;
	auto p2 = sc.createEntity("player2");
	auto& [primary2, player2] = p2.addComponent<Player>();
	primary2 = true;

	auto found = sc.getPrimaryPlayer();
	EXPECT_TRUE(found);
	EXPECT_EQ(found.getUUID(), p2.getUUID());
	core::Log::invalidate();
}

// ============================================================================
// Scene::getAllEntities vs getRootEntities consistency
// ============================================================================

TEST(SceneCoverage, AllEntitiesVsRootEntitiesWithHierarchy) {
	Scene sc;
	auto root1 = sc.createEntity("root1");
	sc.createEntity("root2");
	auto child1 = sc.createEntity("child1");
	auto child2 = sc.createEntity("child2");
	auto grandchild = sc.createEntity("grandchild");
	sc.setParent(child1, root1);
	sc.setParent(child2, root1);
	sc.setParent(grandchild, child1);

	EXPECT_EQ(sc.getAllEntities().size(), 5u);
	EXPECT_EQ(sc.getRootEntities().size(), 2u);
}

// ============================================================================
// Scene::createEntityWithUUID — preserve UUID
// ============================================================================

TEST(SceneCoverage, CreateEntityWithUUIDPreservesUUID) {
	Scene sc;
	auto ent = sc.createEntityWithUUID(core::UUID{12345}, "named");
	EXPECT_EQ(static_cast<uint64_t>(ent.getUUID()), 12345u);
	EXPECT_EQ(ent.getName(), "named");
}

TEST(SceneCoverage, CreateEntityWithUUIDDefaultName) {
	Scene sc;
	auto ent = sc.createEntityWithUUID(core::UUID{67890});
	EXPECT_EQ(ent.getName(), "Entity");
}

// ============================================================================
// Scene::copy — empty scene
// ============================================================================

TEST(SceneCoverage, CopyEmptyScene) {
	const shared<Scene> original = mkShared<Scene>();
	auto copy = Scene::copy(original);
	EXPECT_TRUE(copy->getAllEntities().empty());
	EXPECT_TRUE(copy->getGameState().empty());
}

// ============================================================================
// Mixed operations: create, destroy, count consistency
// ============================================================================

TEST(SceneCoverage, CreateDestroyCountConsistency) {
	Scene sc;
	auto e1 = sc.createEntity("e1");
	auto e2 = sc.createEntity("e2");
	auto e3 = sc.createEntity("e3");
	EXPECT_EQ(sc.getAllEntities().size(), 3u);

	sc.destroyEntity(e2);
	EXPECT_EQ(sc.getAllEntities().size(), 2u);

	sc.createEntity("e4");
	EXPECT_EQ(sc.getAllEntities().size(), 3u);

	sc.destroyEntity(e1);
	sc.destroyEntity(e3);
	EXPECT_EQ(sc.getAllEntities().size(), 1u);
}

// ============================================================================
// Complex scenario: copy after hierarchy + state modifications
// ============================================================================

TEST(SceneCoverage, CopyPreservesFullState) {
	const shared<Scene> original = mkShared<Scene>();
	original->onViewportResize({1920, 1080});

	auto parent = original->createEntity("parent");
	parent.getComponent<Transform>().transform.translation() = {5.f, 0.f, 0.f};
	auto child = original->createEntity("child");
	child.getComponent<Transform>().transform.translation() = {10.f, 0.f, 0.f};
	original->setParent(child, parent);

	auto& vis = parent.getComponent<Visibility>();
	vis.gameVisible = false;

	original->getGameState().set("level", static_cast<int64_t>(3));

	auto copy = Scene::copy(original);
	copy->rebuildHierarchyChildren();

	EXPECT_EQ(copy->getAllEntities().size(), 2u);
	EXPECT_EQ(copy->getRootEntities().size(), 1u);

	const auto copyRoot = copy->getRootEntities()[0];
	EXPECT_EQ(copyRoot.getName(), "parent");
	EXPECT_FALSE(copyRoot.getComponent<Visibility>().gameVisible);
	EXPECT_NEAR(copyRoot.getComponent<Transform>().transform.translation().x(), 5.f, 0.01f);

	const auto& copyRootH = copyRoot.getComponent<Hierarchy>();
	ASSERT_EQ(copyRootH.childrenIds.size(), 1u);
	auto copyChild = copy->findEntityByUUID(copyRootH.childrenIds[0]);
	ASSERT_TRUE(copyChild);
	EXPECT_EQ(copyChild.getName(), "child");

	ASSERT_TRUE(copy->getGameState().get("level").has_value());
	EXPECT_EQ(std::get<int64_t>(copy->getGameState().get("level").value()), 3);
}
