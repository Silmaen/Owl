/**
 * @file FrameBench.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "cases/Cases.h"

#include <physics/PhysicCommand.h>
#include <renderer/CameraEditor.h>
#include <renderer/Renderer2D.h>
#include <scene/Entity.h>
#include <scene/component/components.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <string>
#include <tuple>

namespace owl::bench {

namespace {

auto makeStep() -> core::Timestep {
	core::Timestep step;
	step.forceUpdate(std::chrono::microseconds(16667));
	return step;
}

void runEditorFrames(Runner& ioRunner) {
	const renderer::CameraEditor camera;
	for (const auto& [count, shape, label]:
		 {std::tuple{1000U, Shape::Flat, "flat"}, std::tuple{10000U, Shape::Flat, "flat"},
		  std::tuple{10000U, Shape::Forest, "forest"}, std::tuple{1000U, Shape::Chain, "chain"}}) {
		const std::string name = std::format("frame/editor_update/{}{}", label, count);
		if (!ioRunner.wants(name))
			continue;
		const auto scn = makeSpriteScene(count, shape);
		scn->onViewportResize({1280, 720});
		const auto step = makeStep();
		ioRunner.measure(name, count, [&]() -> void { scn->onUpdateEditor(step, camera); });
		ioRunner.metric(name + "_quads", renderer::Renderer2D::getStats().quadCount, "quads (last frame)");
	}
}

// The same frames with the render preparation on the scheduler and on the calling thread only, to compare them.
void runLargeEditorFrames(Runner& ioRunner) {
	const renderer::CameraEditor camera;
	for (const auto& [count, shape, label]:
		 {std::tuple{50000U, Shape::Flat, "flat"}, std::tuple{50000U, Shape::Forest, "forest"}}) {
		for (const bool moving: {false, true}) {
			for (const bool parallel: {true, false}) {
				const std::string name = std::format("frame/editor_update/{}{}{}{}", moving ? "moving_" : "", label,
													 count, parallel ? "" : "_serial");
				if (!ioRunner.wants(name))
					continue;
				const auto scn = makeSpriteScene(count, shape);
				if (!parallel)
					scn->setParallelMinEntities(std::numeric_limits<size_t>::max());
				scn->onViewportResize({1280, 720});
				const auto entities = scn->getAllEntities();
				const auto step = makeStep();
				float offset = 0.f;
				ioRunner.measure(name, count, [&]() -> void {
					if (moving) {
						offset += 0.001f;
						for (const auto& ent: entities)
							ent.getComponent<scene::component::Transform>().transform.translation().y() = offset;
					}
					scn->onUpdateEditor(step, camera);
				});
			}
		}
	}
}

void runRuntimeFrames(Runner& ioRunner) {
	for (const uint32_t count: {1000U, 10000U}) {
		for (const bool render: {false, true}) {
			const std::string name = std::format("frame/runtime_update/{}flat{}", render ? "render_" : "logic_", count);
			if (!ioRunner.wants(name))
				continue;
			const auto scn = makeSpriteScene(count, Shape::Flat);
			auto cam = scn->createEntity("camera");
			auto& camComp = cam.addComponent<scene::component::Camera>();
			camComp.primary = true;
			scn->onViewportResize({1280, 720});
			scn->onStartRuntime();
			const auto step = makeStep();
			ioRunner.measure(name, count, [&]() -> void { scn->onUpdateRuntime(step, render); });
			scn->onEndRuntime();
		}
	}
}

void runCatchUpFrame(Runner& ioRunner) {
	constexpr uint32_t count = 1000;
	const std::string name = std::format("frame/runtime_update/logic_ticks4_flat{}", count);
	if (!ioRunner.wants(name))
		return;
	const auto scn = makeSpriteScene(count, Shape::Flat);
	scn->onStartRuntime();
	// Four 60 Hz ticks in one frame: the cost of the fixed-tick block when a frame falls behind.
	core::Timestep frame;
	frame.forceUpdate(std::chrono::microseconds(66'667));
	ioRunner.measure(name, count, [&]() -> void { scn->onUpdateRuntime(frame, false); });
	ioRunner.metric(name + "_ticks", scn->getSimulationClock().getFrameTickCount(), "ticks (last frame)");
	scn->onEndRuntime();
}

}// namespace

void runFrameBenches(Runner& ioRunner) {
	runEditorFrames(ioRunner);
	runLargeEditorFrames(ioRunner);
	runRuntimeFrames(ioRunner);
	runCatchUpFrame(ioRunner);
}

}// namespace owl::bench
