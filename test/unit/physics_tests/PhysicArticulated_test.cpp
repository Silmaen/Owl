/**
 * @file PhysicArticulated_test.cpp
 * @author Silmaen
 * @date 09/11/2026
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
#include <filesystem>
#include <string>

using namespace owl;
using namespace owl::physics;
using namespace owl::scene;

namespace {

void runFrames(Scene& ioScene, const int iFrames) {
	core::Timestep ts;
	ts.forceUpdate(std::chrono::microseconds(16'667));
	for (int f = 0; f < iFrames; ++f) PhysicCommand::frame(ioScene, ts);
}

class PhysicArticulatedTest : public testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		OWL_REQUIRE_MODULE(PHYSICS);
	}

	void TearDown() override {
		PhysicCommand::destroy(m_scene);
		core::Log::invalidate();
	}

	auto addBox(const std::string& iName, const math::vec3f& iPosition, const SceneBody::Dimension iDimension,
				const SceneBody::BodyType iType = SceneBody::BodyType::Dynamic) -> Entity {
		auto entity = m_scene.createEntity(iName);
		entity.getComponent<component::Transform>().transform.translation() = iPosition;
		auto& [body] = entity.addComponent<component::PhysicBody>();
		body.type = iType;
		body.dimension = iDimension;
		return entity;
	}

	// A unit box hanging from a static anchor by a distance joint breaking above `iBreakForce`.
	auto hang(const float iBreakForce) -> Entity {
		const auto anchor = addBox("anchor", {0.f, 5.f, 0.f}, SceneBody::Dimension::Plane, SceneBody::BodyType::Static);
		auto weight = addBox("weight", {0.f, 3.f, 0.f}, SceneBody::Dimension::Plane);
		auto& [joint] = weight.addComponent<component::PhysicJoint>();
		joint.type = SceneJoint::Type::Distance;
		joint.connected = anchor.getUUID();
		joint.breakForce = iBreakForce;
		return weight;
	}

	Scene m_scene;
};

}// namespace

TEST_F(PhysicArticulatedTest, aJointBeyondItsBreakForceBreaksAndIsReported) {
	const auto weight = hang(5.f);
	PhysicCommand::init(m_scene);
	runFrames(m_scene, 10);
	EXPECT_EQ(weight.getComponent<component::PhysicJoint>().joint.jointId, 0u);
	const auto breaks = PhysicCommand::takeJointBreaks(m_scene);
	ASSERT_EQ(breaks.size(), 1u);
	EXPECT_EQ(breaks.front().entity, weight.getUUID());
	EXPECT_GT(breaks.front().force, 5.f);
	EXPECT_TRUE(PhysicCommand::takeJointBreaks(m_scene).empty());
	runFrames(m_scene, 30);
	EXPECT_LT(weight.getComponent<component::Transform>().transform.translation().y(), 2.f);
}

TEST_F(PhysicArticulatedTest, aJointWithinItsThresholdsHolds) {
	const auto weight = hang(50.f);
	weight.getComponent<component::PhysicJoint>().joint.breakTorque = 50.f;
	PhysicCommand::init(m_scene);
	runFrames(m_scene, 60);
	EXPECT_NE(weight.getComponent<component::PhysicJoint>().joint.jointId, 0u);
	EXPECT_TRUE(PhysicCommand::takeJointBreaks(m_scene).empty());
	EXPECT_NEAR(weight.getComponent<component::Transform>().transform.translation().y(), 3.f, 0.05f);
}

TEST_F(PhysicArticulatedTest, aForceOffTheCentreTurnsTheBody) {
	m_scene.getPhysicsSettings().gravity = {0.f, 0.f, 0.f};
	const auto centred = addBox("centred", {0.f, 0.f, 0.f}, SceneBody::Dimension::Space);
	const auto pushed = addBox("pushed", {10.f, 0.f, 0.f}, SceneBody::Dimension::Space);
	const auto spun = addBox("spun", {20.f, 0.f, 0.f}, SceneBody::Dimension::Space);
	PhysicCommand::init(m_scene);
	for (int tick = 0; tick < 10; ++tick) {
		PhysicCommand::addForce(centred, {0.f, 10.f, 0.f});
		PhysicCommand::addForceAtPoint(pushed, {0.f, 10.f, 0.f}, {10.5f, 0.f, 0.f});
		PhysicCommand::addTorque(spun, {0.f, 0.f, 5.f});
		PhysicCommand::step(m_scene);
	}
	EXPECT_GT(PhysicCommand::getVelocity3D(centred).y(), 0.5f);
	EXPECT_NEAR(PhysicCommand::getVelocity3D(pushed).y(), PhysicCommand::getVelocity3D(centred).y(), 1e-4f);
	PhysicCommand::syncSimulatedTransforms(m_scene);
	EXPECT_NEAR(centred.getComponent<component::Transform>().transform.rotation().z(), 0.f, 1e-5f);
	EXPECT_GT(pushed.getComponent<component::Transform>().transform.rotation().z(), 0.01f);
	EXPECT_GT(spun.getComponent<component::Transform>().transform.rotation().z(), 0.01f);
	EXPECT_NEAR(PhysicCommand::getVelocity3D(spun).y(), 0.f, 1e-6f);
}

TEST_F(PhysicArticulatedTest, aPartMergesIntoItsAncestorAndBreaksOff) {
	m_scene.getPhysicsSettings().gravity = {0.f, 0.f, 0.f};
	const auto hull = addBox("hull", {0.f, 0.f, 0.f}, SceneBody::Dimension::Plane);
	auto wing = addBox("wing", {2.f, 0.f, 0.f}, SceneBody::Dimension::Plane);
	wing.getComponent<component::PhysicBody>().body.part = true;
	m_scene.setParent(wing, hull);
	PhysicCommand::init(m_scene);
	EXPECT_EQ(wing.getComponent<component::PhysicBody>().body.bodyId, 0u);
	EXPECT_EQ(PhysicCommand::getAssemblyRoot(wing), hull.getUUID());
	EXPECT_EQ(PhysicCommand::getAssemblyRoot(hull), core::UUID{0});
	EXPECT_NEAR(PhysicCommand::getMass(hull), 2.f, 1e-3f);
	EXPECT_NEAR(PhysicCommand::getMass(wing), 2.f, 1e-3f);
	EXPECT_NEAR(PhysicCommand::getCenterOfMass(hull).x(), 1.f, 1e-3f);
	// A force on the part pushes the whole assembly at the part: it moves and turns.
	for (int tick = 0; tick < 10; ++tick) {
		PhysicCommand::addForce(wing, {0.f, 20.f, 0.f});
		PhysicCommand::step(m_scene);
	}
	EXPECT_GT(PhysicCommand::getVelocity(hull).y(), 0.5f);
	PhysicCommand::detach(wing);
	EXPECT_FALSE(wing.getComponent<component::PhysicBody>().body.part);
	EXPECT_NE(wing.getComponent<component::PhysicBody>().body.bodyId, 0u);
	EXPECT_EQ(PhysicCommand::getAssemblyRoot(wing), core::UUID{0});
	EXPECT_NEAR(PhysicCommand::getMass(hull), 1.f, 1e-3f);
	EXPECT_NEAR(PhysicCommand::getMass(wing), 1.f, 1e-3f);
	// The part flies on at the speed its point had, faster than the hull's centre since the assembly turns.
	EXPECT_GT(PhysicCommand::getVelocity(wing).y(), PhysicCommand::getVelocity(hull).y());
	EXPECT_TRUE(PhysicCommand::attach(wing));
	EXPECT_EQ(PhysicCommand::getAssemblyRoot(wing), hull.getUUID());
	EXPECT_NEAR(PhysicCommand::getMass(hull), 2.f, 1e-3f);
	EXPECT_TRUE(PhysicCommand::attach(wing));
}

TEST_F(PhysicArticulatedTest, aPartWithoutAncestorBodyKeepsItsOwn) {
	auto loose = addBox("loose", {0.f, 0.f, 0.f}, SceneBody::Dimension::Plane);
	loose.getComponent<component::PhysicBody>().body.part = true;
	PhysicCommand::init(m_scene);
	EXPECT_NE(loose.getComponent<component::PhysicBody>().body.bodyId, 0u);
	EXPECT_FALSE(PhysicCommand::attach(loose));
	EXPECT_FALSE(loose.getComponent<component::PhysicBody>().body.part);
}

TEST_F(PhysicArticulatedTest, removingAPartOrItsRootRebuildsTheAssembly) {
	m_scene.getPhysicsSettings().gravity = {0.f, 0.f, 0.f};
	const auto hull = addBox("hull", {0.f, 0.f, 0.f}, SceneBody::Dimension::Space);
	auto first = addBox("first", {1.f, 0.f, 0.f}, SceneBody::Dimension::Space);
	auto second = addBox("second", {-1.f, 0.f, 0.f}, SceneBody::Dimension::Space);
	for (auto part: {first, second}) {
		part.getComponent<component::PhysicBody>().body.part = true;
		m_scene.setParent(part, hull);
	}
	PhysicCommand::init(m_scene);
	EXPECT_NEAR(PhysicCommand::getMass(hull), 3.f, 1e-3f);
	first.removeComponent<component::PhysicBody>();
	PhysicCommand::step(m_scene);
	EXPECT_NEAR(PhysicCommand::getMass(hull), 2.f, 1e-3f);
	auto hullCopy = hull;
	hullCopy.removeComponent<component::PhysicBody>();
	PhysicCommand::step(m_scene);
	EXPECT_NE(second.getComponent<component::PhysicBody>().body.bodyId, 0u);
	EXPECT_NEAR(PhysicCommand::getMass(second), 1.f, 1e-3f);
}

TEST_F(PhysicArticulatedTest, aJointOnAPartHoldsTheAssembly) {
	const auto hull = addBox("hull", {0.f, 5.f, 0.f}, SceneBody::Dimension::Plane);
	auto hook = addBox("hook", {1.f, 5.f, 0.f}, SceneBody::Dimension::Plane);
	hook.getComponent<component::PhysicBody>().body.part = true;
	m_scene.setParent(hook, hull);
	hook.addComponent<component::PhysicJoint>().joint.type = SceneJoint::Type::Hinge;
	PhysicCommand::init(m_scene);
	EXPECT_NE(hook.getComponent<component::PhysicJoint>().joint.jointId, 0u);
	runFrames(m_scene, 30);
	// Hung at the hook, the assembly swings under it instead of falling.
	const auto& place = hull.getComponent<component::Transform>().transform.translation();
	EXPECT_NEAR(std::hypot(place.x() - 1.f, place.y() - 5.f), 1.f, 0.05f);
	PhysicCommand::detach(hook);
	EXPECT_NE(hook.getComponent<component::PhysicJoint>().joint.jointId, 0u);
}

TEST_F(PhysicArticulatedTest, solverIterationsAndBreakThresholdsAreSaved) {
	m_scene.getPhysicsSettings().velocitySteps = 30;
	m_scene.getPhysicsSettings().positionSteps = 500;
	auto weight = hang(12.5f);
	weight.getComponent<component::PhysicJoint>().joint.breakTorque = 3.f;
	weight.getComponent<component::PhysicBody>().body.part = true;
	const auto file = std::filesystem::temp_directory_path() / "owl_articulated_test.owl";
	const auto scene = mkShared<Scene>();
	scene->getPhysicsSettings() = m_scene.getPhysicsSettings();
	auto copy = scene->createEntityWithUUID(weight.getUUID(), "weight");
	copy.addComponent<component::PhysicBody>() = weight.getComponent<component::PhysicBody>();
	copy.addComponent<component::PhysicJoint>() = weight.getComponent<component::PhysicJoint>();
	ASSERT_TRUE(SceneSerializer(scene).serialize(file));
	const auto loaded = mkShared<Scene>();
	ASSERT_TRUE(SceneSerializer(loaded).deserialize(file));
	std::filesystem::remove(file);
	EXPECT_EQ(loaded->getPhysicsSettings().velocitySteps, 30u);
	EXPECT_EQ(loaded->getPhysicsSettings().positionSteps, PhysicsSettings::maxSolverIterations);
	const auto entity = loaded->findEntityByUUID(weight.getUUID());
	ASSERT_TRUE(entity);
	EXPECT_FLOAT_EQ(entity.getComponent<component::PhysicJoint>().joint.breakForce, 12.5f);
	EXPECT_FLOAT_EQ(entity.getComponent<component::PhysicJoint>().joint.breakTorque, 3.f);
	EXPECT_TRUE(entity.getComponent<component::PhysicBody>().body.part);
	PhysicCommand::init(m_scene);
	EXPECT_EQ(PhysicCommand::getSettings(m_scene).positionSteps, PhysicsSettings::maxSolverIterations);
}
