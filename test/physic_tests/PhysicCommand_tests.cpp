#include "testHelper.h"

#include <physic/PhysicCommand.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneBody.h>
#include <scene/component/components.h>

using namespace owl::core;
using namespace owl::physic;
using namespace owl::scene;

TEST(PhysicCommand, Creation) {
	Log::init(Log::Level::Off);
	Scene scene;
	// create a scene with physic

	auto b1 = scene.createEntity("body1");
	auto& phy1 = b1.addComponent<component::PhysicBody>();
	phy1.body.type = SceneBody::BodyType::Dynamic;
	phy1.body.restitution = 1.0f;
	auto& [transform1] = b1.getComponent<component::Transform>();
	transform1.translation().x() = 15.0f;

	auto b2 = scene.createEntity("body2");
	auto& phy2 = b2.addComponent<component::PhysicBody>();
	phy2.body.type = SceneBody::BodyType::Static;
	phy2.body.restitution = 1.0f;
	auto& [transform2] = b2.getComponent<component::Transform>();
	transform2.translation().x() = -15.0f;

	auto b3 = scene.createEntity("body3");
	auto& phy3 = b3.addComponent<component::PhysicBody>();
	phy3.body.type = SceneBody::BodyType::Kinematic;
	auto& [transform3] = b3.getComponent<component::Transform>();
	transform3.translation().x() = 0.0f;

	Timestep ts;
	ts.forceUpdate(std::chrono::milliseconds(100));
	EXPECT_FALSE(PhysicCommand::isInitialized());
	PhysicCommand::frame(ts);
	PhysicCommand::init(nullptr);
	EXPECT_FALSE(PhysicCommand::isInitialized());
	PhysicCommand::init(&scene);
	EXPECT_TRUE(PhysicCommand::isInitialized());
	PhysicCommand::init(&scene);
	EXPECT_TRUE(PhysicCommand::isInitialized());
	PhysicCommand::frame(ts);
	// test the new positions...
	EXPECT_EQ(transform1.translation().y(), -0.0613125041f);
	EXPECT_EQ(transform2.translation().y(), 0.0f);
	EXPECT_EQ(transform3.translation().y(), 0.0f);

	PhysicCommand::destroy();
	EXPECT_FALSE(PhysicCommand::isInitialized());
	Log::invalidate();
}


TEST(PhysicCommand, badImpulse) {
	Log::init(Log::Level::Off);
	// Single body scene...
	Scene scene;
	auto b1 = scene.createEntity("body1");
	// uninitialized.
	PhysicCommand::impulse(b1, {0, 15});
	EXPECT_EQ(PhysicCommand::getVelocity(b1), owl::math::vec2f(0, 0));

	// initialization
	PhysicCommand::init(&scene);

	// void entity
	PhysicCommand::impulse({}, {0, 15});
	EXPECT_EQ(PhysicCommand::getVelocity({}), owl::math::vec2f(0, 0));

	// entity without physics
	PhysicCommand::impulse(b1, {0, 15});
	EXPECT_EQ(PhysicCommand::getVelocity(b1), owl::math::vec2f(0, 0));

	{
		auto& [body] = b1.addComponent<component::PhysicBody>();
		body.type = SceneBody::BodyType::Static;
		body.restitution = 1.0f;
		auto& [transform1] = b1.getComponent<component::Transform>();
		transform1.translation().x() = 15.0f;
	}

	// static body!
	PhysicCommand::impulse(b1, {0, 15});
	EXPECT_EQ(PhysicCommand::getVelocity(b1), owl::math::vec2f(0, 0));

	Log::invalidate();
}


TEST(PhysicCommand, Impulse) {
	Log::init(Log::Level::Off);
	// Single body scene...
	Scene scene;
	auto b1 = scene.createEntity("body1");
	{
		auto& [body] = b1.addComponent<component::PhysicBody>();
		body.type = SceneBody::BodyType::Dynamic;
		body.restitution = 1.0f;
		auto& [transform1] = b1.getComponent<component::Transform>();
		transform1.translation().x() = 15.0f;
	}

	PhysicCommand::init(&scene);
	PhysicCommand::impulse(b1, {0, 15});
	Timestep ts;
	ts.forceUpdate(std::chrono::milliseconds(100));
	PhysicCommand::frame(ts);
	EXPECT_NEAR(PhysicCommand::getVelocity(b1).y(), 14.019001, 0.001);

	Log::invalidate();
}

// A dynamic body with `gravityScale = 0` must not accumulate any velocity from world gravity.
// This is the API the top-down `world_player.lua` uses to opt out of gravity entirely.
TEST(PhysicCommand, GravityScaleZeroCancelsFalling) {
	Log::init(Log::Level::Off);
	Scene scene;
	auto b1 = scene.createEntity("body1");
	{
		auto& [body] = b1.addComponent<component::PhysicBody>();
		body.type = SceneBody::BodyType::Dynamic;
	}
	PhysicCommand::init(&scene);
	PhysicCommand::setGravityScale(b1, 0.f);
	Timestep ts;
	ts.forceUpdate(std::chrono::milliseconds(500));
	for (int i = 0; i < 10; ++i)
		PhysicCommand::frame(ts);
	// Vertical velocity must stay at 0 — Box2D world gravity has no effect on this body.
	EXPECT_NEAR(PhysicCommand::getVelocity(b1).y(), 0.0, 1e-3);
	Log::invalidate();
}

// `setGravityScale` must be a no-op (no crash) for the cases we silently swallow elsewhere:
// uninitialized engine, null entity, no physic body, or a static body. Dynamic bodies
// must be registered in the physics world (i.e. created BEFORE `init`) before we can
// dereference their `bodyId` — that path is covered by `GravityScaleZeroCancelsFalling`.
TEST(PhysicCommand, GravityScaleNoOpEdgeCases) {
	Log::init(Log::Level::Off);
	Scene scene;
	auto noBody = scene.createEntity("nobody");
	auto staticBody = scene.createEntity("static");
	{
		auto& [body] = staticBody.addComponent<component::PhysicBody>();
		body.type = SceneBody::BodyType::Static;
	}

	// Uninitialized: warns but does not crash.
	PhysicCommand::setGravityScale(noBody, 0.f);
	PhysicCommand::init(&scene);
	// Null entity.
	PhysicCommand::setGravityScale({}, 0.f);
	// Entity without PhysicBody.
	PhysicCommand::setGravityScale(noBody, 0.f);
	// Static body — silently ignored (only Dynamic is meaningful).
	PhysicCommand::setGravityScale(staticBody, 0.f);
	Log::invalidate();
}
