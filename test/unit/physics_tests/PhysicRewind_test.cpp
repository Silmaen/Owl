/**
 * @file PhysicRewind_test.cpp
 * @author Silmaen
 * @date 04/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <physics/PhysicCommand.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/component/components.h>

#include <cstdint>
#include <span>
#include <utility>
#include <vector>

using namespace owl;
using namespace owl::physics;
using namespace owl::scene;

namespace {

// Positions and rotations of the bodies, then the collision pairs begun during the frames.
struct Trace {
	std::vector<math::vec3f> poses;
	std::vector<std::pair<uint64_t, uint64_t>> collisions;

	auto operator==(const Trace&) const -> bool = default;
};

// A 2D box pile hit by a 2D disc, and 3D bodies tumbling on a 3D ground beside it.
class PhysicRewindTest : public testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		auto ground = m_scene.createEntity("ground");
		ground.getComponent<component::Transform>().transform.scale() = {40.f, 1.f, 10.f};
		ground.addComponent<component::PhysicBody>().body.type = SceneBody::BodyType::Static;
		for (uint32_t i = 0; i < 12; ++i) {
			const uint32_t row = i / 4;
			addBody(SceneBody::Dimension::Plane, SceneBody::Shape::Box,
					{static_cast<float>(i % 4) * 1.05f - 2.f, 1.f + static_cast<float>(row) * 1.05f, 0.f});
		}
		auto disc = addBody(SceneBody::Dimension::Plane, SceneBody::Shape::Sphere, {-9.f, 2.f, 0.f});
		auto ball = addBody(SceneBody::Dimension::Space, SceneBody::Shape::Sphere, {8.f, 3.f, 0.f});
		auto block = addBody(SceneBody::Dimension::Space, SceneBody::Shape::Box, {10.f, 2.f, 1.f});
		block.getComponent<component::Transform>().transform.rotation() = {0.4f, 0.2f, 0.7f};
		PhysicCommand::init(m_scene);
		PhysicCommand::setVelocity(disc, {12.f, 0.f});
		PhysicCommand::setVelocity3D(ball, {1.f, 0.f, -2.f});
		PhysicCommand::setVelocity3D(block, {-1.f, 0.f, 0.5f});
	}

	void TearDown() override {
		PhysicCommand::destroy(m_scene);
		core::Log::invalidate();
	}

	auto addBody(const SceneBody::Dimension iDimension, const SceneBody::Shape iShape, const math::vec3f& iPosition)
			-> Entity {
		auto entity = m_scene.createEntity("body");
		entity.getComponent<component::Transform>().transform.translation() = iPosition;
		auto& [body] = entity.addComponent<component::PhysicBody>();
		body.type = SceneBody::BodyType::Dynamic;
		body.dimension = iDimension;
		body.shape = iShape;
		m_bodies.push_back(entity);
		return entity;
	}

	// One fixed step per tick: the clock of the scene is not part of the physics state.
	auto play(const int iTicks) -> Trace {
		Trace trace;
		for (int t = 0; t < iTicks; ++t) {
			PhysicCommand::step(m_scene);
			for (const auto& [entityA, entityB]: PhysicCommand::takeCollisionEvents(m_scene))
				trace.collisions.emplace_back(static_cast<uint64_t>(entityA), static_cast<uint64_t>(entityB));
		}
		PhysicCommand::syncSimulatedTransforms(m_scene);
		for (const auto& body: m_bodies) {
			const auto& transform = body.getComponent<component::Transform>().transform;
			trace.poses.push_back(transform.translation());
			trace.poses.push_back(transform.rotation());
		}
		return trace;
	}

	Scene m_scene;
	std::vector<Entity> m_bodies;
};

}// namespace

TEST_F(PhysicRewindTest, RestoredWorldReplaysTheSameBitsAndEvents) {
	std::ignore = play(30);
	const auto state = PhysicCommand::saveState(m_scene);
	ASSERT_FALSE(state.empty());
	const auto first = play(90);
	EXPECT_FALSE(first.collisions.empty());
	ASSERT_TRUE(PhysicCommand::restoreState(m_scene, state));
	EXPECT_EQ(play(90), first);
	ASSERT_TRUE(PhysicCommand::restoreState(m_scene, state));
	EXPECT_EQ(play(90), first);
}

TEST_F(PhysicRewindTest, AStateNeedsTheBodiesItWasSavedWith) {
	std::ignore = play(10);
	const auto state = PhysicCommand::saveState(m_scene);
	PhysicCommand::destroyBody(m_bodies.front());
	EXPECT_FALSE(PhysicCommand::restoreState(m_scene, state));
}

TEST_F(PhysicRewindTest, MalformedStatesAreRefused) {
	const auto state = PhysicCommand::saveState(m_scene);
	ASSERT_GT(state.size(), 16u);
	EXPECT_FALSE(PhysicCommand::restoreState(m_scene, {}));
	EXPECT_FALSE(PhysicCommand::restoreState(m_scene, std::span(state).first(state.size() / 2)));
	auto longer = state;
	longer.push_back(0);
	EXPECT_FALSE(PhysicCommand::restoreState(m_scene, longer));
	EXPECT_TRUE(PhysicCommand::restoreState(m_scene, state));
}

TEST(PhysicRewind, WithoutAWorldThereIsNoState) {
	core::Log::init(core::Log::Level::Off);
	Scene scene;
	EXPECT_TRUE(PhysicCommand::saveState(scene).empty());
	EXPECT_FALSE(PhysicCommand::restoreState(scene, std::vector<uint8_t>{1, 2, 3}));
	core::Log::invalidate();
}
