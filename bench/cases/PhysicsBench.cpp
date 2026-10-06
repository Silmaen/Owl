/**
 * @file PhysicsBench.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "cases/Cases.h"

#include <physics/PhysicCommand.h>
#include <scene/Entity.h>
#include <scene/component/components.h>

#include <chrono>
#include <cstdint>
#include <format>

namespace owl::bench {

namespace {

constexpr uint32_t g_FramesPerSample = 60;

auto makeBoxScene(const uint32_t iBodies) -> shared<scene::Scene> {
	auto scn = mkShared<scene::Scene>();
	auto ground = scn->createEntity("ground");
	auto& groundTr = ground.getComponent<scene::component::Transform>().transform;
	groundTr.scale() = {400.f, 1.f, 1.f};
	ground.addComponent<scene::component::PhysicBody>().body.type = scene::SceneBody::BodyType::Static;
	for (uint32_t i = 0; i < iBodies; ++i) {
		auto box = scn->createEntity("box");
		auto& tr = box.getComponent<scene::component::Transform>().transform;
		tr.translation() = {-150.f + 1.5f * static_cast<float>(i % 200), 2.f + 1.5f * static_cast<float>(i / 200), 0.f};
		box.addComponent<scene::component::PhysicBody>().body.type = scene::SceneBody::BodyType::Dynamic;
	}
	return scn;
}

auto makeStep() -> core::Timestep {
	core::Timestep step;
	step.forceUpdate(std::chrono::microseconds(16667));
	return step;
}

}// namespace

void runPhysicsBenches(Runner& ioRunner) {
	if (!ioRunner.wants("physics"))
		return;
	const auto step = makeStep();
	for (const uint32_t count: {100U, 1000U, 5000U}) {
		shared<scene::Scene> scn;
		ioRunner.measureWithSetup(
				std::format("physics/init/{}_bodies", count), count,
				[&]() -> void {
					if (physics::PhysicCommand::isInitialized())
						physics::PhysicCommand::destroy();
					scn = makeBoxScene(count);
				},
				[&]() -> void { physics::PhysicCommand::init(scn.get()); });
		ioRunner.measureWithSetup(
				std::format("physics/step_falling/{}_bodies", count), g_FramesPerSample,
				[&]() -> void {
					if (physics::PhysicCommand::isInitialized())
						physics::PhysicCommand::destroy();
					scn = makeBoxScene(count);
					physics::PhysicCommand::init(scn.get());
				},
				[&]() -> void {
					for (uint32_t f = 0; f < g_FramesPerSample; ++f) physics::PhysicCommand::frame(step);
				});
		ioRunner.measureWithSetup(
				std::format("physics/step_settled/{}_bodies", count), g_FramesPerSample,
				[&]() -> void {
					if (physics::PhysicCommand::isInitialized())
						physics::PhysicCommand::destroy();
					scn = makeBoxScene(count);
					physics::PhysicCommand::init(scn.get());
					for (uint32_t f = 0; f < 600; ++f) physics::PhysicCommand::frame(step);
				},
				[&]() -> void {
					for (uint32_t f = 0; f < g_FramesPerSample; ++f) physics::PhysicCommand::frame(step);
				});
		if (physics::PhysicCommand::isInitialized())
			physics::PhysicCommand::destroy();
	}
}

}// namespace owl::bench
