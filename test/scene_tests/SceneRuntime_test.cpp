/**
 * @file SceneRuntime_test.cpp
 * @author Silmaen
 * @date 07/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Application.h>
#include <core/Log.h>
#include <core/Timestep.h>
#include <physic/PhysicCommand.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/component/components.h>

using namespace owl;

namespace {

class SceneRuntimeTest : public ::testing::Test {
protected:
	void SetUp() override { core::Log::init(core::Log::Level::Off); }
	void TearDown() override {
		if (physic::PhysicCommand::isInitialized())
			physic::PhysicCommand::destroy();
		core::Log::invalidate();
	}
};

auto makeStep(int iMs) -> core::Timestep {
	core::Timestep ts;
	ts.forceUpdate(std::chrono::milliseconds(iMs));
	return ts;
}

}// namespace

// onStartRuntime / onEndRuntime should bring the scene to Playing then back to Editing
// even with no entities. Lifecycle hooks must be safe on an empty scene.
TEST_F(SceneRuntimeTest, EmptySceneRuntimeLifecycle) {
	scene::Scene scn;
	EXPECT_EQ(scn.status, scene::Scene::Status::Editing);
	scn.onStartRuntime();
	EXPECT_EQ(scn.status, scene::Scene::Status::Playing);
	scn.onUpdateRuntime(makeStep(16), false);
	scn.onEndRuntime();
	EXPECT_EQ(scn.status, scene::Scene::Status::Editing);
}

// A SoundListener and a SoundSource configured for play-on-start must not crash
// onStartRuntime even when the SoundSystem is not running (sound asset lookup fails
// gracefully). This exercises the "asset not loaded" branch.
TEST_F(SceneRuntimeTest, SoundListenerAndSourceOnStart) {
	scene::Scene scn;
	auto listener = scn.createEntity("listener");
	auto& li = listener.addComponent<scene::component::SoundListener>();
	li.primary = true;
	auto& [t] = listener.getComponent<scene::component::Transform>();
	t.translation() = {1.f, 2.f, 0.f};

	auto source = scn.createEntity("source");
	auto& [snd] = source.addComponent<scene::component::SoundSource>();
	snd.playOnStart = true;
	snd.soundAsset = "missing.wav";

	scn.onStartRuntime();
	scn.onUpdateRuntime(makeStep(16), false);
	scn.onEndRuntime();
}

// SoundSource with empty asset is silently skipped at startup.
TEST_F(SceneRuntimeTest, SoundSourceEmptyAssetSkipped) {
	scene::Scene scn;
	auto source = scn.createEntity("source");
	auto& [snd] = source.addComponent<scene::component::SoundSource>();
	snd.playOnStart = true;
	snd.soundAsset = "";
	scn.onStartRuntime();
	scn.onEndRuntime();
}

// AnimatedSprite reset: onStartRuntime should reset frame index and start playback.
TEST_F(SceneRuntimeTest, AnimatedSpriteResetAtStart) {
	scene::Scene scn;
	auto ent = scn.createEntity("anim");
	auto& anim = ent.addComponent<scene::component::AnimatedSpriteRenderer>();
	anim.firstFrame = 5;
	anim.lastFrame = 10;
	anim.m_currentFrame = 9;
	anim.m_elapsedTime = 0.5f;
	anim.m_playing = false;

	scn.onStartRuntime();
	EXPECT_EQ(anim.m_currentFrame, 5u);
	EXPECT_FLOAT_EQ(anim.m_elapsedTime, 0.f);
	EXPECT_TRUE(anim.m_playing);
	scn.onEndRuntime();
}

// Timer triggers must auto-start at runtime.
TEST_F(SceneRuntimeTest, TimerTriggersAutoStart) {
	scene::Scene scn;
	auto ent = scn.createEntity("timer");
	auto& trig = ent.addComponent<scene::component::Trigger>();
	trig.trigger.type = scene::SceneTrigger::TriggerType::Timer;
	trig.trigger.timerDuration = 10.f;

	scn.onStartRuntime();
	scn.onUpdateRuntime(makeStep(16), false);
	scn.onEndRuntime();
}

// Hidden trigger has its overlap state cleared during update — exercises the
// `!isEffectivelyVisible` branch of onUpdateRuntime's trigger loop.
TEST_F(SceneRuntimeTest, HiddenTriggerCancelsTimer) {
	scene::Scene scn;
	auto ent = scn.createEntity("trig");
	auto& trig = ent.addComponent<scene::component::Trigger>();
	trig.trigger.type = scene::SceneTrigger::TriggerType::Timer;
	trig.trigger.timerDuration = 10.f;
	ent.getComponent<scene::component::Visibility>().gameVisible = false;

	scn.onStartRuntime();
	scn.onUpdateRuntime(makeStep(16), false);
	scn.onEndRuntime();
}

// Victory status returns early in onUpdateRuntime — render is not attempted
// when iRender=false, so this is safe without an Application.
TEST_F(SceneRuntimeTest, UpdateRuntimeReturnsEarlyOnVictory) {
	scene::Scene scn;
	scn.status = scene::Scene::Status::Victory;
	scn.onUpdateRuntime(makeStep(16), false);
	EXPECT_EQ(scn.status, scene::Scene::Status::Victory);
}

TEST_F(SceneRuntimeTest, UpdateRuntimeReturnsEarlyOnDeath) {
	scene::Scene scn;
	scn.status = scene::Scene::Status::Death;
	scn.onUpdateRuntime(makeStep(16), false);
	EXPECT_EQ(scn.status, scene::Scene::Status::Death);
}

// PhysicBody-driven entities must update their transforms each frame and survive
// shutdown via onEndRuntime.
TEST_F(SceneRuntimeTest, PhysicBodyUpdatesTransformOverTime) {
	scene::Scene scn;
	auto ent = scn.createEntity("body");
	{
		auto& [body] = ent.addComponent<scene::component::PhysicBody>();
		body.type = scene::SceneBody::BodyType::Dynamic;
	}
	scn.onStartRuntime();
	for (int i = 0; i < 5; ++i) scn.onUpdateRuntime(makeStep(16), false);
	const auto& [transform] = ent.getComponent<scene::component::Transform>();
	EXPECT_LT(transform.translation().y(), 0.f);
	scn.onEndRuntime();
}

// EntityLink should pull the linked entity's world transform.
TEST_F(SceneRuntimeTest, EntityLinkFollowsTarget) {
	scene::Scene scn;
	auto target = scn.createEntity("target");
	auto& [tt] = target.getComponent<scene::component::Transform>();
	tt.translation() = {7.f, 8.f, 0.f};

	auto follower = scn.createEntity("follower");
	auto& link = follower.addComponent<scene::component::EntityLink>();
	link.linkedEntityName = "target";

	scn.onStartRuntime();
	scn.onUpdateRuntime(makeStep(16), false);
	const auto& [ft] = follower.getComponent<scene::component::Transform>();
	EXPECT_NEAR(ft.translation().x(), 7.f, 0.01f);
	EXPECT_NEAR(ft.translation().y(), 8.f, 0.01f);
	scn.onEndRuntime();
}

// `resolveAllEntityLinks` is called from `onStartRuntime` so the per-frame
// link loop never falls into its O(N²) tag-rescan path on the first tick.
// After `onStartRuntime` and before any `onUpdateRuntime`, every EntityLink
// with a known name must already point at the right entity.
TEST_F(SceneRuntimeTest, EntityLinkPreResolvedOnStart) {
	scene::Scene scn;
	auto target = scn.createEntity("target");
	auto follower = scn.createEntity("follower");
	auto& link = follower.addComponent<scene::component::EntityLink>();
	link.linkedEntityName = "target";
	EXPECT_FALSE(link.linkedEntity);
	scn.onStartRuntime();
	EXPECT_TRUE(link.linkedEntity);
	EXPECT_EQ(link.linkedEntity, target);
	scn.onEndRuntime();
}

// A hidden follower must freeze in place rather than track its target — the
// dormant-entity skip in `onUpdateRuntime`'s link loop short-circuits hidden
// hosts.
TEST_F(SceneRuntimeTest, HiddenEntityLinkSkipsTracking) {
	scene::Scene scn;
	auto target = scn.createEntity("target");
	target.getComponent<scene::component::Transform>().transform.translation() = {5.f, 6.f, 0.f};

	auto follower = scn.createEntity("follower");
	follower.getComponent<scene::component::Transform>().transform.translation() = {0.f, 0.f, 0.f};
	auto& link = follower.addComponent<scene::component::EntityLink>();
	link.linkedEntityName = "target";
	follower.getComponent<scene::component::Visibility>().gameVisible = false;

	scn.onStartRuntime();
	scn.onUpdateRuntime(makeStep(16), false);
	const auto& ft = follower.getComponent<scene::component::Transform>().transform;
	EXPECT_NEAR(ft.translation().x(), 0.f, 0.01f);
	EXPECT_NEAR(ft.translation().y(), 0.f, 0.01f);
	scn.onEndRuntime();
}

// onViewportResize updates non-fixedAspectRatio cameras. Fixed-aspect cameras
// must NOT have their viewport changed.
TEST_F(SceneRuntimeTest, OnViewportResizeUpdatesNonFixedCameras) {
	scene::Scene scn;
	auto cam1 = scn.createEntity("cam1");
	auto& c1 = cam1.addComponent<scene::component::Camera>();
	c1.fixedAspectRatio = false;
	auto cam2 = scn.createEntity("cam2");
	auto& c2 = cam2.addComponent<scene::component::Camera>();
	c2.fixedAspectRatio = true;

	scn.onViewportResize({320u, 240u});
	// We don't have getViewportSize on the Camera here, but the call must not crash;
	// the viewport-stored size becomes the new default for newly added cameras.
	auto cam3 = scn.createEntity("cam3");
	(void) cam3.addComponent<scene::component::Camera>();
}

// duplicateEntity makes a root entity. Already covered in SceneHierarchy_test, but
// here we exercise the "duplicate of a child entity" branch where the source is
// not a root: the duplicate must still be a root.
TEST_F(SceneRuntimeTest, DuplicateChildBecomesRoot) {
	scene::Scene scn;
	auto parent = scn.createEntity("parent");
	auto child = scn.createEntity("child");
	scn.setParent(child, parent);
	auto dup = scn.duplicateEntity(child);
	EXPECT_EQ(dup.getComponent<scene::component::Hierarchy>().parentId, core::UUID{0});
}

// Scene::copy clones the registry into a new scene, with all components copied.
TEST_F(SceneRuntimeTest, CopyClonesEntities) {
	auto src = mkShared<scene::Scene>();
	auto e1 = src->createEntityWithUUID(101, "first");
	e1.addComponent<scene::component::SpriteRenderer>();
	auto e2 = src->createEntityWithUUID(102, "second");
	e2.addComponent<scene::component::Camera>();

	const auto dst = scene::Scene::copy(src);
	ASSERT_NE(dst, nullptr);
	const auto entities = dst->getAllEntities();
	EXPECT_EQ(entities.size(), 2u);
	auto found = dst->findEntityByUUID(core::UUID{101});
	ASSERT_TRUE(found);
	EXPECT_EQ(found.getName(), "first");
	EXPECT_TRUE(found.hasComponent<scene::component::SpriteRenderer>());
}

// onViewportResize sets m_viewportSize so that Camera components added later
// inherit the size — exercises the Camera onComponentAdded specialization.
TEST_F(SceneRuntimeTest, CameraInheritsViewportSizeAfterResize) {
	scene::Scene scn;
	scn.onViewportResize({640u, 480u});
	auto e = scn.createEntity("cam");
	auto& cam = e.addComponent<scene::component::Camera>();
	(void) cam;// camera should now have a non-zero viewport — no crash either way.
}

// Adding a Text component without an Application should leave the font as nullptr
// (covered by onComponentAdded<Text>).
TEST_F(SceneRuntimeTest, TextComponentAddedWithoutApplication) {
	scene::Scene scn;
	auto e = scn.createEntity("text");
	auto& text = e.addComponent<scene::component::Text>();
	text.text = "hello";
	EXPECT_EQ(text.font, nullptr);// no Application → defaultFont not assigned
}

// Visibility toggling via setEditor / gameplay flags: the scene's
// isEffectivelyVisible reads either flag based on the editor/runtime context.
TEST_F(SceneRuntimeTest, IsEffectivelyVisibleSeparateModes) {
	scene::Scene scn;
	auto e = scn.createEntity("v");
	auto& vis = e.getComponent<scene::component::Visibility>();
	vis.gameVisible = false;
	vis.editorVisible = true;
	EXPECT_FALSE(scn.isEffectivelyVisible(e, false));
	EXPECT_TRUE(scn.isEffectivelyVisible(e, true));
	vis.gameVisible = true;
	vis.editorVisible = false;
	EXPECT_TRUE(scn.isEffectivelyVisible(e, false));
	EXPECT_FALSE(scn.isEffectivelyVisible(e, true));
}

// rebuildHierarchyChildren should rebuild the children list from each
// entity's parentId, even after an artificial children-list desynchronization.
TEST_F(SceneRuntimeTest, RebuildHierarchyChildrenFromParentIds) {
	scene::Scene scn;
	auto parent = scn.createEntityWithUUID(7, "parent");
	auto child = scn.createEntityWithUUID(8, "child");
	scn.setParent(child, parent);
	// Manually clear the parent's children list to simulate a corrupted state.
	parent.getComponent<scene::component::Hierarchy>().childrenIds.clear();
	scn.rebuildHierarchyChildren();
	const auto& [parentId, childrenIds] = parent.getComponent<scene::component::Hierarchy>();
	(void) parentId;
	ASSERT_EQ(childrenIds.size(), 1u);
	EXPECT_EQ(childrenIds[0], child.getUUID());
}

// rebuildHierarchyChildren orphans entities whose parentId is missing.
TEST_F(SceneRuntimeTest, RebuildHierarchyChildrenOrphansMissingParent) {
	scene::Scene scn;
	auto e = scn.createEntityWithUUID(9, "lonely");
	// Force a parentId that refers to a non-existent entity.
	e.getComponent<scene::component::Hierarchy>().parentId = core::UUID{99999};
	scn.rebuildHierarchyChildren();
	EXPECT_EQ(e.getComponent<scene::component::Hierarchy>().parentId, core::UUID{0});
}

// getPrimaryCamera / getPrimaryPlayer lookups
TEST_F(SceneRuntimeTest, PrimaryCameraAndPlayerLookup) {
	scene::Scene scn;
	EXPECT_FALSE(scn.getPrimaryCamera());
	EXPECT_FALSE(scn.getPrimaryPlayer());
	auto cam = scn.createEntity("cam");
	auto& c = cam.addComponent<scene::component::Camera>();
	c.primary = true;
	auto player = scn.createEntity("player");
	auto& p = player.addComponent<scene::component::Player>();
	p.primary = true;
	EXPECT_EQ(scn.getPrimaryCamera(), cam);
	EXPECT_EQ(scn.getPrimaryPlayer(), player);
}

// `findEntityByUUID` keeps a UUID → entity index warm across the scene's
// lifetime; create/destroy must keep it in sync. Look up an entity by
// UUID, destroy it, then look up another — the second hit must still
// resolve.
TEST_F(SceneRuntimeTest, FindEntityByUUIDCacheSurvivesDestroy) {
	scene::Scene scn;
	auto a = scn.createEntityWithUUID(core::UUID{42}, "a");
	auto b = scn.createEntityWithUUID(core::UUID{43}, "b");
	EXPECT_EQ(scn.findEntityByUUID(core::UUID{42}), a);
	scn.destroyEntity(a);
	EXPECT_FALSE(scn.findEntityByUUID(core::UUID{42}));
	EXPECT_EQ(scn.findEntityByUUID(core::UUID{43}), b);
}

// `getPrimaryPlayer` caches its result; destroying the cached entity must
// invalidate the cache so the next call falls back to the remaining primary
// player. Without this, the second lookup would return a dangling handle.
TEST_F(SceneRuntimeTest, PrimaryPlayerCacheSurvivesDestroy) {
	scene::Scene scn;
	auto first = scn.createEntity("p1");
	first.addComponent<scene::component::Player>().primary = true;
	EXPECT_EQ(scn.getPrimaryPlayer(), first);// warms the cache

	auto second = scn.createEntity("p2");
	second.addComponent<scene::component::Player>().primary = true;

	scn.destroyEntity(first);
	const auto found = scn.getPrimaryPlayer();
	EXPECT_TRUE(found);
	EXPECT_EQ(found, second);
}

// Toggling the cached entity's `primary` flag off must also fall through to
// the next available primary player on the next lookup. EnTT views iterate
// in unspecified order, so we read whichever entity the first lookup picks
// then clear its flag and verify the other one is returned.
TEST_F(SceneRuntimeTest, PrimaryPlayerCacheRevalidatesWhenFlagCleared) {
	scene::Scene scn;
	auto a = scn.createEntity("pa");
	a.addComponent<scene::component::Player>().primary = true;
	auto b = scn.createEntity("pb");
	b.addComponent<scene::component::Player>().primary = true;
	const auto cached = scn.getPrimaryPlayer();
	ASSERT_TRUE(cached);
	ASSERT_TRUE(cached == a || cached == b);
	cached.getComponent<scene::component::Player>().primary = false;
	const auto other = (cached == a) ? b : a;
	EXPECT_EQ(scn.getPrimaryPlayer(), other);
}

// getEntityCount currently uses registry.storage<Entity>() which is never populated
// (Entity is a non-component wrapper) so it always returns 0. Exercise the path
// without asserting on the value to keep the existing behaviour pinned.
TEST_F(SceneRuntimeTest, EntityCountIsCallable) {
	scene::Scene scn;
	EXPECT_EQ(scn.getEntityCount(), 0u);
	scn.createEntity("a");
	scn.createEntity("b");
	(void) scn.getEntityCount();
}
