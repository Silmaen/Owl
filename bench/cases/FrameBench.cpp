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
#include <cstdint>
#include <format>
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

}// namespace

void runFrameBenches(Runner& ioRunner) {
	runEditorFrames(ioRunner);
	runRuntimeFrames(ioRunner);
}

}// namespace owl::bench
