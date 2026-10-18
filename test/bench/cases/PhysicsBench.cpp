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
#include <string>
#include <utility>

namespace owl::bench {

namespace {

constexpr uint32_t g_FramesPerSample = 60;

auto makeBoxScene(const uint32_t iBodies, const uint32_t iWorkers) -> shared<scene::Scene> {
	auto scn = mkShared<scene::Scene>();
	scn->getPhysicsSettings().workerCount = iWorkers;
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
	for (const auto& [frameUs, rate]: {std::pair<int64_t, const char*>{16'667, "60hz"}, {6'944, "144hz"}}) {
		core::Timestep frame;
		frame.forceUpdate(std::chrono::microseconds(frameUs));
		shared<scene::Scene> scn;
		ioRunner.measureWithSetup(
				std::format("physics/frame_empty/{}", rate), g_FramesPerSample,
				[&]() -> void {
					scn = mkShared<scene::Scene>();
					physics::PhysicCommand::init(*scn);
				},
				[&]() -> void {
					for (uint32_t f = 0; f < g_FramesPerSample; ++f) physics::PhysicCommand::frame(*scn, frame);
				});
	}
	for (const uint32_t count: {100U, 1000U, 5000U}) {
		shared<scene::Scene> scn;
		ioRunner.measureWithSetup(
				std::format("physics/init/{}_bodies", count), count, [&]() -> void { scn = makeBoxScene(count, 1); },
				[&]() -> void { physics::PhysicCommand::init(*scn); });
		for (const uint32_t workers: {1U, 2U, 4U, 8U}) {
			const std::string suffix = workers == 1 ? std::string{} : std::format("_mt{}", workers);
			ioRunner.measureWithSetup(
					std::format("physics/step_falling{}/{}_bodies", suffix, count), g_FramesPerSample,
					[&]() -> void {
						scn = makeBoxScene(count, workers);
						physics::PhysicCommand::init(*scn);
					},
					[&]() -> void {
						for (uint32_t f = 0; f < g_FramesPerSample; ++f) physics::PhysicCommand::frame(*scn, step);
					});
			ioRunner.measureWithSetup(
					std::format("physics/step_settled{}/{}_bodies", suffix, count), g_FramesPerSample,
					[&]() -> void {
						scn = makeBoxScene(count, workers);
						physics::PhysicCommand::init(*scn);
						for (uint32_t f = 0; f < 600; ++f) physics::PhysicCommand::frame(*scn, step);
					},
					[&]() -> void {
						for (uint32_t f = 0; f < g_FramesPerSample; ++f) physics::PhysicCommand::frame(*scn, step);
					});
		}
	}
}

}// namespace owl::bench
