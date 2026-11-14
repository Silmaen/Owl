/**
 * @file ReplayBench.cpp
 * @author Silmaen
 * @date 13/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "cases/Cases.h"

#include <core/Timestep.h>
#include <scene/Entity.h>
#include <scene/Replay.h>
#include <scene/WorldHash.h>
#include <scene/component/components.h>

#include <chrono>
#include <cstdint>
#include <format>
#include <tuple>

namespace owl::bench {

namespace {

constexpr uint32_t g_Bodies = 100;
constexpr uint32_t g_Frames = 60;

// A ground and boxes falling on it in rows of 20: the physics of a small 2D game.
auto makeGame(const uint32_t iBodies) -> shared<scene::Scene> {
	auto scn = mkShared<scene::Scene>();
	scn->setRandomSeed(2026);
	auto ground = scn->createEntity("ground");
	ground.getComponent<scene::component::Transform>().transform.scale() = {400.f, 1.f, 1.f};
	ground.addComponent<scene::component::PhysicBody>().body.type = scene::SceneBody::BodyType::Static;
	for (uint32_t i = 0; i < iBodies; ++i) {
		auto box = scn->createEntity("box");
		box.getComponent<scene::component::Transform>().transform.translation() = {
				-15.f + 1.5f * static_cast<float>(i % 20), 2.f + 1.5f * static_cast<float>(i / 20), 0.f};
		box.addComponent<scene::component::PhysicBody>().body.type = scene::SceneBody::BodyType::Dynamic;
	}
	return scn;
}

auto makeFrame() -> core::Timestep {
	core::Timestep frame;
	frame.forceUpdate(std::chrono::microseconds(16'667));
	return frame;
}

void stop(shared<scene::Scene>& ioScene) {
	if (ioScene)
		ioScene->onEndRuntime();
	ioScene.reset();
}

}// namespace

void runReplayBenches(Runner& ioRunner) {
	if (!ioRunner.wants("replay"))
		return;
	const auto frame = makeFrame();
	const auto play = [&frame](scene::Scene& ioScene) -> void {
		for (uint32_t f = 0; f < g_Frames; ++f) ioScene.onUpdateRuntime(frame, false);
	};
	shared<scene::Scene> scn;
	// The same frames live, recorded (inputs each frame, a world hash every 60 ticks) and played back.
	ioRunner.measureWithSetup(
			std::format("replay/live/{}_bodies", g_Bodies), g_Frames,
			[&]() -> void {
				stop(scn);
				scn = makeGame(g_Bodies);
				scn->onStartRuntime();
			},
			[&]() -> void { play(*scn); });
	ioRunner.measureWithSetup(
			std::format("replay/record/{}_bodies", g_Bodies), g_Frames,
			[&]() -> void {
				stop(scn);
				scn = makeGame(g_Bodies);
				std::ignore = scn->setRecordingReplay(true);
				scn->onStartRuntime();
			},
			[&]() -> void { play(*scn); });
	stop(scn);
	auto recorded = makeGame(g_Bodies);
	std::ignore = recorded->setRecordingReplay(true);
	recorded->onStartRuntime();
	play(*recorded);
	const auto replay = recorded->takeRecordedReplay();
	recorded->onEndRuntime();
	if (!replay)
		return;
	const auto timeline = mkShared<const scene::Replay>(*replay);
	ioRunner.measureWithSetup(
			std::format("replay/playback/{}_bodies", g_Bodies), g_Frames,
			[&]() -> void {
				stop(scn);
				scn = makeGame(g_Bodies);
				std::ignore = scn->setPlaybackReplay(timeline);
				scn->onStartRuntime();
			},
			[&]() -> void { play(*scn); });
	stop(scn);
	// The rewind of the editor: a copy of the edited scene, started on the timeline, simulated again up to the frame.
	const auto edited = makeGame(g_Bodies);
	ioRunner.measure(std::format("replay/rewind/{}_frames_{}_bodies", g_Frames, g_Bodies), g_Frames, [&]() -> void {
		auto copy = scene::Scene::copy(edited);
		std::ignore = copy->setPlaybackReplay(timeline);
		std::ignore = copy->setRecordingReplay(true);
		copy->onStartRuntime();
		play(*copy);
		copy->onEndRuntime();
	});
	// The checkpoint of a recording, on a scene of 1000 bodies.
	const auto large = makeGame(1000);
	ioRunner.measure("replay/world_hash/1000_bodies", 1000, [&]() -> void { doNotOptimize(computeWorldHash(*large)); });
}

}// namespace owl::bench
