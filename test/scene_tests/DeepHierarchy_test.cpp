/**
 * @file DeepHierarchy_test.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Timestep.h>
#include <physics/PhysicCommand.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/component/components.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <vector>

using namespace owl;

namespace {

constexpr uint32_t g_chainLength = 1000;

class DeepHierarchyFixture : public testing::Test {
protected:
	void SetUp() override { core::Log::init(core::Log::Level::Off); }

	void TearDown() override {
		if (physics::PhysicCommand::isInitialized())
			physics::PhysicCommand::destroy();
		core::Log::invalidate();
	}
};

auto referenceWorld(const scene::Scene& iScene, const scene::Entity& iEntity) -> math::mat4 {
	math::mat4 world = iEntity.getComponent<scene::component::Transform>().transform();
	core::UUID parentId = iEntity.getComponent<scene::component::Hierarchy>().parentId;
	while (parentId != core::UUID{0}) {
		const scene::Entity parent = iScene.findEntityByUUID(parentId);
		if (!parent)
			break;
		world = parent.getComponent<scene::component::Transform>().transform() * world;
		parentId = parent.getComponent<scene::component::Hierarchy>().parentId;
	}
	return world;
}

// Chain of iLength entities, each one unit to the right of its parent, so entity i sits at world x = i + 1.
auto makeChain(scene::Scene& ioScene, const uint32_t iLength) -> std::vector<scene::Entity> {
	std::vector<scene::Entity> chain;
	chain.reserve(iLength);
	for (uint32_t i = 0; i < iLength; ++i) {
		chain.push_back(ioScene.createEntity("link"));
		if (i > 0)
			ioScene.setParent(chain[i], chain[i - 1]);
	}
	for (auto& link: chain) link.getComponent<scene::component::Transform>().transform.translation().x() = 1.f;
	return chain;
}

auto makeStep() -> core::Timestep {
	core::Timestep step;
	step.forceUpdate(std::chrono::milliseconds(16));
	return step;
}

}// namespace

TEST_F(DeepHierarchyFixture, WorldTransformOfDeepChainLeaf) {
	scene::Scene sc;
	const auto chain = makeChain(sc, g_chainLength);
	EXPECT_NEAR(sc.getWorldTransform(chain.back()).translation().x(), static_cast<float>(g_chainLength), 1e-3f);
	EXPECT_NEAR(sc.getWorldTransform(chain[499]).translation().x(), 500.f, 1e-3f);
}

TEST_F(DeepHierarchyFixture, SetParentOnDeepChainPreservesWorldPosition) {
	scene::Scene sc;
	std::vector<scene::Entity> ents;
	for (uint32_t i = 0; i < g_chainLength; ++i) {
		ents.push_back(sc.createEntity("e"));
		ents.back().getComponent<scene::component::Transform>().transform.translation().x() = static_cast<float>(i);
	}
	for (uint32_t i = 1; i < g_chainLength; ++i) sc.setParent(ents[i], ents[i - 1]);
	uint32_t misplaced = 0;
	for (uint32_t i = 0; i < g_chainLength; ++i) {
		if (std::abs(referenceWorld(sc, ents[i])(0, 3) - static_cast<float>(i)) > 1e-2f)
			++misplaced;
	}
	EXPECT_EQ(misplaced, 0u);
	EXPECT_NEAR(sc.getWorldTransform(ents.back()).translation().x(), static_cast<float>(g_chainLength - 1), 1e-2f);
}

TEST_F(DeepHierarchyFixture, OffFrameAndFrameWorldsMatchReference) {
	scene::Scene sc;
	auto chain = makeChain(sc, 200);
	for (uint32_t i = 0; i < chain.size(); ++i) {
		auto& transform = chain[i].getComponent<scene::component::Transform>().transform;
		transform.rotation().z() = 0.01f * static_cast<float>(i % 7);
		transform.scale() = math::vec3{1.f + 0.001f * static_cast<float>(i % 3), 1.f, 1.f};
	}
	sc.prepareWorldTransforms();
	const auto frameWorlds = sc.getWorldMatrices();
	ASSERT_EQ(frameWorlds.size(), chain.size());
	for (uint32_t i = 0; i < chain.size(); ++i) {
		const math::mat4 expected = referenceWorld(sc, chain[i]);
		const math::Transform offFrame = sc.getWorldTransform(chain[i]);
		const math::mat4& frame = frameWorlds[sc.getWorldIndex(chain[i])];
		EXPECT_NEAR(offFrame.translation().x(), expected(0, 3), 1e-2f) << "entity " << i;
		EXPECT_NEAR(offFrame.translation().y(), expected(1, 3), 1e-2f) << "entity " << i;
		EXPECT_NEAR(frame(0, 3), expected(0, 3), 1e-2f) << "entity " << i;
		EXPECT_NEAR(frame(1, 3), expected(1, 3), 1e-2f) << "entity " << i;
	}
}

TEST_F(DeepHierarchyFixture, FrameWorldsOfDeepChain) {
	scene::Scene sc;
	const auto chain = makeChain(sc, g_chainLength);
	sc.prepareWorldTransforms();
	const auto worlds = sc.getWorldMatrices();
	ASSERT_EQ(worlds.size(), g_chainLength);
	for (const uint32_t i: {0U, 63U, 64U, 65U, 500U, g_chainLength - 1}) {
		const uint32_t slot = sc.getWorldIndex(chain[i]);
		ASSERT_LT(slot, g_chainLength);
		EXPECT_NEAR(worlds[slot](0, 3), static_cast<float>(i + 1), 1e-3f) << "entity " << i;
	}
}

TEST_F(DeepHierarchyFixture, CycleRejectedBeyondSixtyFourLevels) {
	scene::Scene sc;
	const auto chain = makeChain(sc, 100);
	sc.setParent(chain.front(), chain.back());
	EXPECT_EQ(chain.front().getComponent<scene::component::Hierarchy>().parentId, core::UUID{0});
	EXPECT_NEAR(sc.getWorldTransform(chain.back()).translation().x(), 100.f, 1e-3f);
}

TEST_F(DeepHierarchyFixture, CorruptedParentLoopIsDetected) {
	scene::Scene sc;
	const auto chain = makeChain(sc, 100);
	chain.front().getComponent<scene::component::Hierarchy>().parentId = chain.back().getUUID();
	EXPECT_NEAR(sc.getWorldTransform(chain[50]).translation().x(), 1.f, 1e-3f);
	EXPECT_TRUE(sc.isEffectivelyVisible(chain[50], /*iEditorMode=*/false));
	sc.prepareWorldTransforms();
	EXPECT_TRUE(sc.getWorldMatrices().empty());
}

TEST_F(DeepHierarchyFixture, VisibilityInheritedBeyondSixtyFourLevels) {
	scene::Scene sc;
	auto chain = makeChain(sc, 100);
	chain.front().getComponent<scene::component::Visibility>().gameVisible = false;
	EXPECT_FALSE(sc.isEffectivelyVisible(chain.back(), /*iEditorMode=*/false));
	EXPECT_FALSE(sc.isEffectivelyVisible(chain[70], /*iEditorMode=*/false));
	EXPECT_TRUE(sc.isEffectivelyVisible(chain.back(), /*iEditorMode=*/true));
}

TEST_F(DeepHierarchyFixture, CachedVisibilityInheritedDuringUpdate) {
	scene::Scene sc;
	auto hidden = makeChain(sc, 100);
	auto shown = makeChain(sc, 100);
	hidden.front().getComponent<scene::component::Visibility>().gameVisible = false;
	for (auto* link: {&hidden[30], &hidden.back(), &shown[30], &shown.back()}) {
		auto& trigger = link->addComponent<scene::component::Trigger>().trigger;
		trigger.type = scene::SceneTrigger::TriggerType::Timer;
		trigger.timerDuration = 10.f;
	}
	sc.onStartRuntime();
	sc.onUpdateRuntime(makeStep(), false);
	EXPECT_FALSE(hidden[30].getComponent<scene::component::Trigger>().trigger.isTimerRunning());
	EXPECT_FALSE(hidden.back().getComponent<scene::component::Trigger>().trigger.isTimerRunning());
	EXPECT_TRUE(shown[30].getComponent<scene::component::Trigger>().trigger.isTimerRunning());
	EXPECT_TRUE(shown.back().getComponent<scene::component::Trigger>().trigger.isTimerRunning());
	sc.onEndRuntime();
}
