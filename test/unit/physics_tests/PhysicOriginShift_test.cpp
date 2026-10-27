/**
 * @file PhysicOriginShift_test.cpp
 * @author Silmaen
 * @date 27/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <physics/PhysicCommand.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/component/components.h>

#include <chrono>
#include <cstddef>
#include <optional>
#include <vector>

using namespace owl;
using namespace owl::physics;
using namespace owl::scene;

namespace {

class PhysicOriginShiftTest : public ::testing::Test {
protected:
	void SetUp() override { core::Log::init(core::Log::Level::Off); }

	void TearDown() override { core::Log::invalidate(); }
};

constexpr math::vec3d g_shift{30.0, -12.0, 0.0};

auto makeFrame() -> core::Timestep {
	core::Timestep ts;
	ts.forceUpdate(std::chrono::microseconds(16667));
	return ts;
}

auto makeScene(Scene& ioScene) -> std::vector<Entity> {
	auto ground = ioScene.createEntity("ground");
	ground.getComponent<component::Transform>().transform.scale() = {40.f, 1.f, 1.f};
	ground.addComponent<component::PhysicBody>().body.type = SceneBody::BodyType::Static;
	std::vector<Entity> boxes;
	for (int i = 0; i < 4; ++i) {
		auto box = ioScene.createEntity("box");
		box.getComponent<component::Transform>().transform.translation() = {static_cast<float>(i) * 1.5f - 2.f,
																			2.f + static_cast<float>(i), 0.f};
		box.addComponent<component::PhysicBody>().body.type = SceneBody::BodyType::Dynamic;
		boxes.push_back(box);
	}
	return boxes;
}

auto translations(const std::vector<Entity>& iBoxes) -> std::vector<math::vec3> {
	std::vector<math::vec3> out;
	for (const auto& box: iBoxes) out.push_back(box.getComponent<component::Transform>().transform.translation());
	return out;
}

struct Run {
	std::vector<math::vec3> beforeShift;
	std::vector<math::vec3> afterShift;
	std::vector<math::vec3> end;
};

// Falls the boxes for a while, optionally moves the world origin in the middle of a frame interval, and falls on.
auto simulate(const std::optional<math::vec3d>& iShift) -> Run {
	Scene scene;
	const auto boxes = makeScene(scene);
	PhysicCommand::init(scene);
	for (int frame = 0; frame < 40; ++frame) PhysicCommand::frame(scene, makeFrame());
	Run run;
	run.beforeShift = translations(boxes);
	if (iShift)
		scene.shiftWorldOrigin(*iShift);
	run.afterShift = translations(boxes);
	for (int frame = 0; frame < 40; ++frame) PhysicCommand::frame(scene, makeFrame());
	run.end = translations(boxes);
	PhysicCommand::destroy(scene);
	return run;
}

}// namespace

TEST_F(PhysicOriginShiftTest, TheBodiesCarryOnFromWhereTheShiftLeftThem) {
	const auto reference = simulate(std::nullopt);
	const auto shifted = simulate(g_shift);
	const math::vec3 shift{static_cast<float>(g_shift.x()), static_cast<float>(g_shift.y()), 0.f};
	ASSERT_EQ(reference.end.size(), shifted.end.size());
	for (std::size_t i = 0; i < reference.end.size(); ++i) {
		EXPECT_NEAR(shifted.afterShift[i].x(), shifted.beforeShift[i].x() - shift.x(), 1e-4f) << i;
		EXPECT_NEAR(shifted.afterShift[i].y(), shifted.beforeShift[i].y() - shift.y(), 1e-4f) << i;
		// Float rounding differs once translated, so the two falls agree within a millimetre, not bit for bit.
		EXPECT_NEAR(shifted.end[i].x() + shift.x(), reference.end[i].x(), 1e-3f) << i;
		EXPECT_NEAR(shifted.end[i].y() + shift.y(), reference.end[i].y(), 1e-3f) << i;
	}
}

TEST_F(PhysicOriginShiftTest, AWorldlessSceneShiftsWithoutPhysics) {
	Scene scene;
	auto box = scene.createEntity("box");
	box.addComponent<component::PhysicBody>().body.type = SceneBody::BodyType::Dynamic;
	EXPECT_NO_THROW(PhysicCommand::shiftOrigin(scene, {5.f, 5.f}));
	EXPECT_FALSE(PhysicCommand::isInitialized(scene));
}
