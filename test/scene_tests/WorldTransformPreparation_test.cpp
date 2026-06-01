/**
 * @file WorldTransformPreparation_test.cpp
 * @author Silmaen
 * @date 01/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <renderer/gpu/RenderCommand.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/component/components.h>

using namespace owl;

namespace {
class WorldTransformPreparationFixture : public testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		renderer::gpu::RenderCommand::create(renderer::gpu::RenderAPI::Type::Null);
	}

	void TearDown() override {
		renderer::gpu::RenderCommand::invalidate();
		core::Log::invalidate();
	}
};
}// namespace

TEST_F(WorldTransformPreparationFixture, EmptySceneIsSafe) {
	scene::Scene sc;
	sc.prepareWorldTransforms();
	const auto orphan = sc.createEntity("late");
	// The orphan was added after prepare, so it has no slot yet.
	EXPECT_EQ(sc.getWorldIndex(orphan), std::numeric_limits<uint32_t>::max());
}

TEST_F(WorldTransformPreparationFixture, FlatSceneAssignsConsecutiveSlots) {
	scene::Scene sc;
	const auto a = sc.createEntity("a");
	const auto b = sc.createEntity("b");
	const auto c = sc.createEntity("c");
	sc.prepareWorldTransforms();

	const auto ia = sc.getWorldIndex(a);
	const auto ib = sc.getWorldIndex(b);
	const auto ic = sc.getWorldIndex(c);
	EXPECT_LT(ia, 3u);
	EXPECT_LT(ib, 3u);
	EXPECT_LT(ic, 3u);
	EXPECT_NE(ia, ib);
	EXPECT_NE(ib, ic);
	EXPECT_NE(ia, ic);
	EXPECT_NE(sc.getWorldsBuffer(), nullptr);
}

TEST_F(WorldTransformPreparationFixture, ParentVisitedBeforeChild) {
	scene::Scene sc;
	const auto parent = sc.createEntity("parent");
	const auto child = sc.createEntity("child");
	const auto grandchild = sc.createEntity("grandchild");
	sc.setParent(child, parent);
	sc.setParent(grandchild, child);
	sc.prepareWorldTransforms();

	const auto pIdx = sc.getWorldIndex(parent);
	const auto cIdx = sc.getWorldIndex(child);
	const auto gIdx = sc.getWorldIndex(grandchild);
	EXPECT_LT(pIdx, cIdx);
	EXPECT_LT(cIdx, gIdx);
}

TEST_F(WorldTransformPreparationFixture, WorldTransformCacheMatchesCpuWalk) {
	scene::Scene sc;
	const auto parent = sc.createEntity("parent");
	const auto child = sc.createEntity("child");
	// Set hierarchy first, then translations — setParent() reabsorbs the child's
	// world pose into local coordinates, so any translation written before the
	// reparent would be reinterpreted afterwards.
	sc.setParent(child, parent);
	parent.getComponent<scene::component::Transform>().transform.translation() = math::vec3{2.f, 3.f, 0.f};
	child.getComponent<scene::component::Transform>().transform.translation() = math::vec3{1.f, 0.f, 0.f};

	sc.prepareWorldTransforms();
	const auto childWorld = sc.getWorldTransform(child);
	// World translation = parent.translation + child.local_translation.
	EXPECT_NEAR(childWorld.translation().x(), 3.f, 1e-5f);
	EXPECT_NEAR(childWorld.translation().y(), 3.f, 1e-5f);
	EXPECT_NEAR(childWorld.translation().z(), 0.f, 1e-5f);
}

TEST_F(WorldTransformPreparationFixture, RepeatedPrepareIsIdempotent) {
	scene::Scene sc;
	const auto a = sc.createEntity("a");
	sc.prepareWorldTransforms();
	const auto firstIdx = sc.getWorldIndex(a);
	sc.prepareWorldTransforms();
	const auto secondIdx = sc.getWorldIndex(a);
	EXPECT_EQ(firstIdx, secondIdx);
}
