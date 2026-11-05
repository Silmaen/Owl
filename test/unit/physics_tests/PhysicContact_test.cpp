/**
 * @file PhysicContact_test.cpp
 * @author Silmaen
 * @date 05/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <physics/PhysicCommand.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/component/components.h>

#include <chrono>
#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

using namespace owl;
using namespace owl::physics;
using namespace owl::scene;

namespace {

void runFrames(Scene& ioScene, const int iFrames, std::vector<PhysicCommand::CollisionEvent>* oEvents = nullptr) {
	core::Timestep ts;
	ts.forceUpdate(std::chrono::microseconds(16'667));
	for (int f = 0; f < iFrames; ++f) {
		PhysicCommand::frame(ioScene, ts);
		for (const auto& event: PhysicCommand::takeCollisionEvents(ioScene))
			if (oEvents != nullptr)
				oEvents->push_back(event);
	}
}

auto addBody(Scene& ioScene, const std::string& iName, const SceneBody::BodyType iType, const math::vec3f& iPosition)
		-> Entity {
	auto entity = ioScene.createEntity(iName);
	entity.getComponent<component::Transform>().transform.translation() = iPosition;
	entity.addComponent<component::PhysicBody>().body.type = iType;
	return entity;
}

auto bodyOf(const Entity& iEntity) -> SceneBody& { return iEntity.getComponent<component::PhysicBody>().body; }

// The component as a saved and loaded scene reads it.
template<typename T>
auto roundTrip(const Entity& iEntity) -> T {
	const auto scene = mkShared<Scene>();
	auto copy = scene->createEntityWithUUID(core::UUID{7}, "copy");
	copy.addComponent<T>(iEntity.getComponent<T>());
	const std::string text = SceneSerializer(scene).serializeToString();
	const auto loaded = mkShared<Scene>();
	std::ignore = SceneSerializer(loaded).deserializeFromBuffer(std::vector<uint8_t>(text.begin(), text.end()), "mem");
	const auto entity = loaded->findEntityByUUID(core::UUID{7});
	return entity && entity.hasComponent<T>() ? entity.getComponent<T>() : T{};
}

auto heightOf(const Entity& iEntity) -> float {
	return iEntity.getComponent<component::Transform>().transform.translation().y();
}

class PhysicContactTest : public testing::Test {
protected:
	void SetUp() override { core::Log::init(core::Log::Level::Off); }

	void TearDown() override {
		PhysicCommand::destroy(m_scene);
		core::Log::invalidate();
	}

	Scene m_scene;
};

}// namespace

TEST_F(PhysicContactTest, MasksLetLayersGoThroughEachOther) {
	auto ground = addBody(m_scene, "ground", SceneBody::BodyType::Static, {0.f, 0.f, 0.f});
	ground.getComponent<component::Transform>().transform.scale() = {10.f, 1.f, 1.f};
	bodyOf(ground).layer = 2;
	auto stopped = addBody(m_scene, "stopped", SceneBody::BodyType::Dynamic, {-2.f, 2.f, 0.f});
	auto ghost = addBody(m_scene, "ghost", SceneBody::BodyType::Dynamic, {2.f, 2.f, 0.f});
	bodyOf(ghost).mask = static_cast<uint8_t>(SceneBody::allLayers & ~(1u << 2u));
	PhysicCommand::init(m_scene);
	runFrames(m_scene, 90);
	EXPECT_NEAR(heightOf(stopped), 1.f, 0.05f);
	EXPECT_LT(heightOf(ghost), -1.f);
}

TEST_F(PhysicContactTest, ABodyLeavingReportsTheEndOfItsContact) {
	auto ground = addBody(m_scene, "ground", SceneBody::BodyType::Static, {0.f, 0.f, 0.f});
	auto box = addBody(m_scene, "box", SceneBody::BodyType::Dynamic, {0.f, 1.5f, 0.f});
	PhysicCommand::init(m_scene);
	std::vector<PhysicCommand::CollisionEvent> events;
	runFrames(m_scene, 60, &events);
	ASSERT_EQ(events.size(), 1u);
	EXPECT_FALSE(events.front().ended);
	EXPECT_FALSE(events.front().sensor);
	events.clear();
	PhysicCommand::setVelocity(box, {0.f, 8.f});
	runFrames(m_scene, 10, &events);
	ASSERT_EQ(events.size(), 1u);
	EXPECT_TRUE(events.front().ended);
	EXPECT_EQ(events.front().entityA == ground.getUUID() ? events.front().entityB : events.front().entityA,
			  box.getUUID());
}

TEST_F(PhysicContactTest, ASensorStopsNothingAndReportsEnterAndExit) {
	auto zone = addBody(m_scene, "zone", SceneBody::BodyType::Static, {0.f, 0.f, 0.f});
	zone.getComponent<component::Transform>().transform.scale() = {4.f, 2.f, 1.f};
	bodyOf(zone).sensor = true;
	auto ball = addBody(m_scene, "ball", SceneBody::BodyType::Dynamic, {0.f, 3.f, 0.f});
	bodyOf(ball).shape = SceneBody::Shape::Sphere;
	PhysicCommand::init(m_scene);
	std::vector<PhysicCommand::CollisionEvent> events;
	runFrames(m_scene, 120, &events);
	EXPECT_LT(heightOf(ball), -3.f);
	ASSERT_EQ(events.size(), 2u);
	EXPECT_TRUE(events[0].sensor);
	EXPECT_FALSE(events[0].ended);
	EXPECT_TRUE(events[1].sensor);
	EXPECT_TRUE(events[1].ended);
}

TEST_F(PhysicContactTest, DestroyingABodyEndsItsContacts) {
	auto zone = addBody(m_scene, "zone", SceneBody::BodyType::Static, {0.f, 0.f, 0.f});
	zone.getComponent<component::Transform>().transform.scale() = {4.f, 4.f, 1.f};
	bodyOf(zone).sensor = true;
	auto box = addBody(m_scene, "box", SceneBody::BodyType::Dynamic, {0.f, 0.f, 0.f});
	PhysicCommand::init(m_scene);
	PhysicCommand::setGravityScale(box, 0.f);
	std::vector<PhysicCommand::CollisionEvent> events;
	runFrames(m_scene, 5, &events);
	ASSERT_EQ(events.size(), 1u);
	events.clear();
	PhysicCommand::destroyBody(zone);
	runFrames(m_scene, 1, &events);
	ASSERT_EQ(events.size(), 1u);
	EXPECT_TRUE(events.front().ended);
	EXPECT_TRUE(events.front().sensor);
}

TEST_F(PhysicContactTest, LayerMaskAndSensorRoundTrip) {
	auto entity = addBody(m_scene, "body", SceneBody::BodyType::Kinematic, {0.f, 0.f, 0.f});
	auto& body = bodyOf(entity);
	body.sensor = true;
	body.layer = 5;
	body.mask = 0x2a;
	const auto read = roundTrip<component::PhysicBody>(entity).body;
	EXPECT_TRUE(read.sensor);
	EXPECT_EQ(read.layer, 5u);
	EXPECT_EQ(read.mask, 0x2au);
	auto plain = addBody(m_scene, "plain", SceneBody::BodyType::Dynamic, {0.f, 0.f, 0.f});
	const auto plainYaml = SceneSerializer::serializeEntityToString(plain);
	EXPECT_EQ(plainYaml.find("sensor"), std::string::npos);
	EXPECT_EQ(plainYaml.find("mask"), std::string::npos);
}
