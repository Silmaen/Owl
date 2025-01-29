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
	owl::core::Log::init(owl::core::Log::Level::Off);
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
	owl::core::Log::invalidate();
}


TEST(PhysicCommand, badImpulse) {
	Log::init(owl::core::Log::Level::Off);
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
	Log::init(owl::core::Log::Level::Off);
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
