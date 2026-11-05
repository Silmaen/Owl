/**
 * @file PhysicJoint_test.cpp
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
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

using namespace owl;
using namespace owl::physics;
using namespace owl::scene;

namespace {

void runFrames(Scene& ioScene, const int iFrames) {
	core::Timestep ts;
	ts.forceUpdate(std::chrono::microseconds(16'667));
	for (int f = 0; f < iFrames; ++f) PhysicCommand::frame(ioScene, ts);
}

auto positionOf(const Entity& iEntity) -> math::vec3f {
	return iEntity.getComponent<component::Transform>().transform.translation();
}

auto distance(const math::vec3f& iFirst, const math::vec3f& iSecond) -> float {
	const math::vec3f delta = iFirst - iSecond;
	return std::sqrt(delta.x() * delta.x() + delta.y() * delta.y() + delta.z() * delta.z());
}

class PhysicJointTest : public testing::Test {
protected:
	void SetUp() override { core::Log::init(core::Log::Level::Off); }

	void TearDown() override {
		PhysicCommand::destroy(m_scene);
		core::Log::invalidate();
	}

	auto addBox(const std::string& iName, const math::vec3f& iPosition, const SceneBody::Dimension iDimension)
			-> Entity {
		auto entity = m_scene.createEntity(iName);
		entity.getComponent<component::Transform>().transform.translation() = iPosition;
		auto& [body] = entity.addComponent<component::PhysicBody>();
		body.type = SceneBody::BodyType::Dynamic;
		body.dimension = iDimension;
		return entity;
	}

	auto addJoint(Entity iEntity, const SceneJoint::Type iType) -> SceneJoint& {
		auto& [joint] = iEntity.addComponent<component::PhysicJoint>();
		joint.type = iType;
		return joint;
	}

	Scene m_scene;
};

}// namespace

TEST_F(PhysicJointTest, AHingeToTheWorldSwingsAroundItsAnchor) {
	auto bob = addBox("bob", {2.f, 5.f, 0.f}, SceneBody::Dimension::Plane);
	addJoint(bob, SceneJoint::Type::Hinge).anchor = {-2.f, 0.f, 0.f};
	PhysicCommand::init(m_scene);
	const math::vec3f pivot{0.f, 5.f, 0.f};
	runFrames(m_scene, 30);
	EXPECT_NEAR(distance(positionOf(bob), pivot), 2.f, 0.02f);
	EXPECT_LT(positionOf(bob).y(), 4.f);
	EXPECT_NE(PhysicCommand::getJointPosition(bob), 0.f);
}

TEST_F(PhysicJointTest, ADistanceJointKeepsTwoBodiesApart) {
	auto top = addBox("top", {0.f, 5.f, 0.f}, SceneBody::Dimension::Space);
	top.getComponent<component::PhysicBody>().body.type = SceneBody::BodyType::Static;
	auto weight = addBox("weight", {1.5f, 3.f, 0.f}, SceneBody::Dimension::Space);
	auto& joint = addJoint(weight, SceneJoint::Type::Distance);
	joint.connected = top.getUUID();
	PhysicCommand::init(m_scene);
	const float length = distance(positionOf(weight), positionOf(top));
	runFrames(m_scene, 90);
	EXPECT_NEAR(distance(positionOf(weight), positionOf(top)), length, 0.03f);
	EXPECT_LT(positionOf(weight).x(), 1.4f);
}

TEST_F(PhysicJointTest, AFixedJointMovesTheBodiesAsOne) {
	auto left = addBox("left", {0.f, 5.f, 0.f}, SceneBody::Dimension::Plane);
	auto right = addBox("right", {1.f, 5.f, 0.f}, SceneBody::Dimension::Plane);
	addJoint(right, SceneJoint::Type::Fixed).connected = left.getUUID();
	PhysicCommand::init(m_scene);
	PhysicCommand::setVelocity(left, {3.f, 0.f});
	runFrames(m_scene, 30);
	EXPECT_NEAR(distance(positionOf(left), positionOf(right)), 1.f, 0.01f);
	EXPECT_GT(positionOf(right).x(), 1.5f);
}

TEST_F(PhysicJointTest, ASliderMotorDrivesTheBodyAlongItsAxis) {
	auto cart = addBox("cart", {0.f, 5.f, 0.f}, SceneBody::Dimension::Plane);
	auto& joint = addJoint(cart, SceneJoint::Type::Slider);
	joint.axis = {1.f, 0.f, 0.f};
	joint.motor = true;
	joint.motorSpeed = 2.f;
	joint.motorForce = 1000.f;
	PhysicCommand::init(m_scene);
	runFrames(m_scene, 60);
	PhysicCommand::syncSimulatedTransforms(m_scene);
	EXPECT_NEAR(positionOf(cart).y(), 5.f, 0.01f);
	EXPECT_NEAR(positionOf(cart).x(), 2.f, 0.1f);
	EXPECT_NEAR(PhysicCommand::getJointPosition(cart), positionOf(cart).x(), 0.01f);
	PhysicCommand::setJointMotor(cart, true, -2.f);
	runFrames(m_scene, 60);
	EXPECT_NEAR(positionOf(cart).x(), 0.f, 0.1f);
}

TEST_F(PhysicJointTest, AJointWithoutTheConnectedBodyIsNotBuilt) {
	auto box = addBox("box", {0.f, 5.f, 0.f}, SceneBody::Dimension::Plane);
	addJoint(box, SceneJoint::Type::Hinge).connected = core::UUID{1234};
	PhysicCommand::init(m_scene);
	runFrames(m_scene, 30);
	EXPECT_EQ(box.getComponent<component::PhysicJoint>().joint.jointId, 0u);
	EXPECT_LT(positionOf(box).y(), 4.f);
}

TEST_F(PhysicJointTest, JointsAreRestoredWithTheState) {
	auto bob = addBox("bob", {2.f, 5.f, 0.f}, SceneBody::Dimension::Plane);
	addJoint(bob, SceneJoint::Type::Hinge).anchor = {-2.f, 0.f, 0.f};
	PhysicCommand::init(m_scene);
	runFrames(m_scene, 10);
	const auto state = PhysicCommand::saveState(m_scene);
	runFrames(m_scene, 40);
	PhysicCommand::syncSimulatedTransforms(m_scene);
	const auto first = positionOf(bob);
	ASSERT_TRUE(PhysicCommand::restoreState(m_scene, state));
	runFrames(m_scene, 40);
	PhysicCommand::syncSimulatedTransforms(m_scene);
	EXPECT_EQ(positionOf(bob), first);
}

TEST_F(PhysicJointTest, AJointRemovedFreesTheBody) {
	auto bob = addBox("bob", {2.f, 5.f, 0.f}, SceneBody::Dimension::Plane);
	addJoint(bob, SceneJoint::Type::Fixed);
	PhysicCommand::init(m_scene);
	runFrames(m_scene, 20);
	EXPECT_NEAR(positionOf(bob).y(), 5.f, 0.01f);
	bob.removeComponent<component::PhysicJoint>();
	runFrames(m_scene, 20);
	EXPECT_LT(positionOf(bob).y(), 4.5f);
}

TEST_F(PhysicJointTest, TheJointRoundTrips) {
	auto box = addBox("box", {0.f, 0.f, 0.f}, SceneBody::Dimension::Space);
	auto& joint = addJoint(box, SceneJoint::Type::Slider);
	joint.connected = core::UUID{42};
	joint.anchor = {1.f, 2.f, 3.f};
	joint.axis = {0.f, 1.f, 0.f};
	joint.limited = true;
	joint.lowerLimit = -1.f;
	joint.upperLimit = 2.f;
	joint.motor = true;
	joint.motorSpeed = 0.5f;
	const auto scene = mkShared<Scene>();
	scene->createEntityWithUUID(core::UUID{7}, "copy").addComponent<component::PhysicJoint>(box.getComponent<component::PhysicJoint>());
	const std::string text = SceneSerializer(scene).serializeToString();
	const auto loaded = mkShared<Scene>();
	ASSERT_TRUE(SceneSerializer(loaded).deserializeFromBuffer(std::vector<uint8_t>(text.begin(), text.end()), "mem"));
	const auto copy = loaded->findEntityByUUID(core::UUID{7});
	ASSERT_TRUE(copy.hasComponent<component::PhysicJoint>());
	const auto& read = copy.getComponent<component::PhysicJoint>().joint;
	EXPECT_EQ(read.type, SceneJoint::Type::Slider);
	EXPECT_EQ(read.connected, core::UUID{42});
	EXPECT_EQ(read.anchor, (math::vec3f{1.f, 2.f, 3.f}));
	EXPECT_EQ(read.axis, (math::vec3f{0.f, 1.f, 0.f}));
	EXPECT_TRUE(read.limited);
	EXPECT_FLOAT_EQ(read.upperLimit, 2.f);
	EXPECT_TRUE(read.motor);
	EXPECT_FLOAT_EQ(read.motorSpeed, 0.5f);
}
