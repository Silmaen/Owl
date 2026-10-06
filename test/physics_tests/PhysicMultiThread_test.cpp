/**
 * @file PhysicMultiThread_test.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <physics/PhysicCommand.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/component/components.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

using namespace owl;
using namespace owl::physics;
using namespace owl::scene;

namespace {

auto makeFrame(const int64_t iMicroseconds) -> core::Timestep {
	core::Timestep ts;
	ts.forceUpdate(std::chrono::microseconds(iMicroseconds));
	return ts;
}

struct RunResult {
	std::vector<math::vec3f> positions;
	uint32_t workers = 0;
	size_t collisions = 0;
};

auto simulatePile(const uint32_t iWorkers) -> RunResult {
	Scene scene;
	scene.getPhysicsSettings().workerCount = iWorkers;
	auto ground = scene.createEntity("ground");
	ground.getComponent<component::Transform>().transform.scale() = {120.f, 1.f, 1.f};
	ground.addComponent<component::PhysicBody>().body.type = SceneBody::BodyType::Static;
	std::vector<Entity> boxes;
	for (uint32_t i = 0; i < 600; ++i) {
		auto box = scene.createEntity("box");
		box.getComponent<component::Transform>().transform.translation() = {
				-45.f + 1.5f * static_cast<float>(i % 60), 2.f + 1.5f * static_cast<float>(i / 60), 0.f};
		box.addComponent<component::PhysicBody>().body.type = SceneBody::BodyType::Dynamic;
		boxes.push_back(box);
	}
	RunResult result;
	PhysicCommand::init(&scene);
	result.workers = PhysicCommand::getWorkerCount();
	for (int f = 0; f < 240; ++f) {
		PhysicCommand::frame(makeFrame(16'667));
		result.collisions += PhysicCommand::takeCollisionEvents().size();
	}
	PhysicCommand::syncSimulatedTransforms();
	for (const auto& box: boxes)
		result.positions.push_back(box.getComponent<component::Transform>().transform.translation());
	PhysicCommand::destroy();
	return result;
}

}// namespace

TEST(PhysicMultiThread, SolverGivesTheSingleThreadResult) {
	core::Log::init(core::Log::Level::Off);
	const auto mono = simulatePile(1);
	const auto multi = simulatePile(4);
	const auto again = simulatePile(4);
	EXPECT_EQ(mono.workers, 1u);
	EXPECT_EQ(multi.workers, 4u);
	ASSERT_EQ(mono.positions.size(), multi.positions.size());
	// The solve order of a 600-box pile depends on the worker count: same count, same bits; mono, within 1 cm.
	float maxGap = 0.f;
	for (size_t i = 0; i < mono.positions.size(); ++i) {
		EXPECT_EQ(multi.positions[i], again.positions[i]) << "box " << i;
		maxGap = std::max({maxGap, std::abs(mono.positions[i].x() - multi.positions[i].x()),
						   std::abs(mono.positions[i].y() - multi.positions[i].y())});
	}
	EXPECT_LT(maxGap, 1e-2f);
	EXPECT_EQ(mono.collisions, multi.collisions);
	EXPECT_GT(mono.collisions, 0u);
	core::Log::invalidate();
}

TEST(PhysicMultiThread, WorkerCountSettings) {
	core::Log::init(core::Log::Level::Off);
	PhysicsSettings settings;
	EXPECT_EQ(settings.getEffectiveWorkerCount(10), 1u);
	EXPECT_GE(settings.getEffectiveWorkerCount(PhysicsSettings::autoThreadingMinBodies), 1u);
	EXPECT_LE(settings.getEffectiveWorkerCount(PhysicsSettings::autoThreadingMinBodies),
			  PhysicsSettings::maxAutoWorkerCount);
	settings.workerCount = 1;
	EXPECT_EQ(settings.getEffectiveWorkerCount(0), 1u);
	settings.workerCount = 1000;
	EXPECT_EQ(settings.getEffectiveWorkerCount(0), PhysicsSettings::maxWorkerCount);
	EXPECT_EQ(settings.clamped().workerCount, PhysicsSettings::maxWorkerCount);
	EXPECT_EQ(PhysicCommand::getWorkerCount(), 0u);
	Scene scene;
	scene.getPhysicsSettings().workerCount = 2;
	PhysicCommand::init(&scene);
	EXPECT_EQ(PhysicCommand::getWorkerCount(), 2u);
	PhysicCommand::frame(makeFrame(16'667));
	scene.getPhysicsSettings().workerCount = 3;
	PhysicCommand::init(&scene);
	EXPECT_EQ(PhysicCommand::getWorkerCount(), 3u);
	PhysicCommand::frame(makeFrame(16'667));
	PhysicCommand::destroy();
	core::Log::invalidate();
}
