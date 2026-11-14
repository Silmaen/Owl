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
#include <cmath>
#include <cstdint>
#include <format>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace owl::bench {

namespace {

constexpr uint32_t g_FramesPerSample = 60;

auto makeBoxScene(const uint32_t iBodies, const uint32_t iWorkers, const uint32_t iCollisionSteps = 0)
		-> shared<scene::Scene> {
	auto scn = mkShared<scene::Scene>();
	scn->getPhysicsSettings().workerCount = iWorkers;
	if (iCollisionSteps > 0)
		scn->getPhysicsSettings().solverSubSteps = iCollisionSteps;
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


auto makeFrame(const int64_t iMicroseconds) -> core::Timestep {
	core::Timestep frame;
	frame.forceUpdate(std::chrono::microseconds(iMicroseconds));
	return frame;
}

auto addBody(scene::Scene& ioScene, const math::vec3f& iPosition, const scene::SceneBody::Dimension iDimension,
			 const scene::SceneBody::Shape iShape, const math::vec3f& iSize) -> scene::Entity {
	auto entity = ioScene.createEntity("body");
	entity.getComponent<scene::component::Transform>().transform.translation() = iPosition;
	auto& [body] = entity.addComponent<scene::component::PhysicBody>();
	body.type = scene::SceneBody::BodyType::Dynamic;
	body.dimension = iDimension;
	body.shape = iShape;
	body.colliderSize = iSize;
	return entity;
}

// Bodies in space on near-circular orbits around four point sources, no world gravity.
constexpr uint32_t g_OrbitBodies = 100;
constexpr uint32_t g_Sources = 4;

auto makeOrbitScene() -> std::pair<shared<scene::Scene>, scene::Entity> {
	auto scn = mkShared<scene::Scene>();
	scn->getPhysicsSettings().gravity = {0.f, 0.f, 0.f};
	for (uint32_t s = 0; s < g_Sources; ++s) {
		auto source = scn->createEntity("planet");
		source.getComponent<scene::component::Transform>().transform.translation() = {200.f * static_cast<float>(s),
																					  0.f, 0.f};
		auto& field = source.addComponent<scene::component::GravitySource>();
		field.type = scene::component::GravitySource::Type::Point;
		field.strength = 10.f;
		field.radius = 5.f;
	}
	scene::Entity first;
	for (uint32_t i = 0; i < g_OrbitBodies; ++i) {
		const float angle = 0.37f * static_cast<float>(i);
		const float orbit = 20.f + 0.5f * static_cast<float>(i / g_Sources);
		const math::vec3f centre{200.f * static_cast<float>(i % g_Sources), 0.f, 0.f};
		auto ball = addBody(*scn, centre + math::vec3f{orbit * std::cos(angle), orbit * std::sin(angle), 0.f},
							scene::SceneBody::Dimension::Space, scene::SceneBody::Shape::Sphere, {0.5f, 0.5f, 0.5f});
		if (i == 0)
			first = ball;
	}
	return {scn, first};
}

// Gives every orbiting body the speed of a circular orbit around its source (Point: mu = strength * radius^2).
void launchOrbits(scene::Scene& ioScene) {
	const auto view = ioScene.registry.view<scene::component::PhysicBody, scene::component::Transform>();
	for (const auto e: view) {
		const math::vec3f position = view.get<scene::component::Transform>(e).transform.translation();
		const math::vec3f centre{200.f * std::round(position.x() / 200.f), 0.f, 0.f};
		const math::vec3f offset = position - centre;
		const float speed = std::sqrt(250.f / offset.norm());
		physics::PhysicCommand::setVelocity3D(scene::Entity{e, &ioScene},
											  math::vec3f{-offset.y(), offset.x(), 0.3f}.normalized() * speed);
	}
}

// Chains of hinged 2D links hanging from a static anchor, swinging from horizontal.
constexpr uint32_t g_Chains = 10;
constexpr uint32_t g_Links = 20;

auto makeChainScene() -> shared<scene::Scene> {
	auto scn = mkShared<scene::Scene>();
	for (uint32_t c = 0; c < g_Chains; ++c) {
		const float x0 = 30.f * static_cast<float>(c);
		auto anchor = scn->createEntity("anchor");
		anchor.getComponent<scene::component::Transform>().transform.translation() = {x0, 20.f, 0.f};
		anchor.addComponent<scene::component::PhysicBody>().body.type = scene::SceneBody::BodyType::Static;
		core::UUID previous = anchor.getUUID();
		for (uint32_t l = 0; l < g_Links; ++l) {
			auto link = addBody(*scn, {x0 + 0.5f + static_cast<float>(l), 20.f, 0.f},
								scene::SceneBody::Dimension::Plane, scene::SceneBody::Shape::Box, {0.9f, 0.2f, 1.f});
			auto& [joint] = link.addComponent<scene::component::PhysicJoint>();
			joint.type = scene::SceneJoint::Type::Hinge;
			joint.connected = previous;
			joint.anchor = {-0.5f, 0.f, 0.f};
			previous = link.getUUID();
		}
	}
	return scn;
}

// Rigid assemblies of a hull and its parts (one compound body each), falling on a ground.
constexpr uint32_t g_Assemblies = 50;
constexpr uint32_t g_Parts = 8;

auto makeAssemblyScene() -> std::pair<shared<scene::Scene>, scene::Entity> {
	auto scn = mkShared<scene::Scene>();
	auto ground = scn->createEntity("ground");
	ground.getComponent<scene::component::Transform>().transform.scale() = {400.f, 1.f, 1.f};
	ground.addComponent<scene::component::PhysicBody>().body.type = scene::SceneBody::BodyType::Static;
	scene::Entity firstPart;
	for (uint32_t a = 0; a < g_Assemblies; ++a) {
		const math::vec3f origin{-150.f + 6.f * static_cast<float>(a), 3.f + static_cast<float>(a % 3), 0.f};
		auto hull = addBody(*scn, origin, scene::SceneBody::Dimension::Plane, scene::SceneBody::Shape::Box,
							{1.f, 1.f, 1.f});
		for (uint32_t p = 0; p < g_Parts; ++p) {
			// The parent keeps the world pose of a part: it is placed in the world, next to its hull.
			auto part = addBody(*scn,
								origin + math::vec3f{0.75f + 0.5f * static_cast<float>(p % 4),
													 0.5f * static_cast<float>(p / 4) - 0.25f, 0.f},
								scene::SceneBody::Dimension::Plane, scene::SceneBody::Shape::Box, {0.5f, 0.5f, 1.f});
			part.getComponent<scene::component::PhysicBody>().body.part = true;
			scn->setParent(part, hull);
			if (a == 0 && p == 0)
				firstPart = part;
		}
	}
	return {scn, firstPart};
}

// Ends the runtime of a scene started by `onStartRuntime`, if any.
void stopRuntime(shared<scene::Scene>& ioScene) {
	if (ioScene)
		ioScene->onEndRuntime();
	ioScene.reset();
}

// The 0.7 simulation features (rollback, fields, warp, joints, parts); each case builds its own scene for any filter.
void runSimulationBenches(Runner& ioRunner) {
	const auto step = makeStep();
	const auto run = [&step](scene::Scene& ioScene) -> void {
		for (uint32_t f = 0; f < g_FramesPerSample; ++f) physics::PhysicCommand::frame(ioScene, step);
	};
	shared<scene::Scene> scn;
	// Rollback: the snapshot of a world of 1000 falling boxes, and its restore.
	std::vector<uint8_t> state;
	const auto fallFor = [&](const uint32_t iFrames) -> void {
		scn = makeBoxScene(1000, 1);
		physics::PhysicCommand::init(*scn);
		for (uint32_t f = 0; f < iFrames; ++f) physics::PhysicCommand::frame(*scn, step);
	};
	ioRunner.measureWithSetup(
			"physics/rollback/save_state/1000_bodies", 1000, [&]() -> void { fallFor(30); },
			[&]() -> void { state = physics::PhysicCommand::saveState(*scn); });
	ioRunner.measureWithSetup(
			"physics/rollback/restore_state/1000_bodies", 1000,
			[&]() -> void {
				fallFor(30);
				state = physics::PhysicCommand::saveState(*scn);
				for (uint32_t f = 0; f < 10; ++f) physics::PhysicCommand::frame(*scn, step);
			},
			[&]() -> void { doNotOptimize(physics::PhysicCommand::restoreState(*scn, state)); });
	// Gravity fields: the step of bodies pulled by point sources, and a trajectory prediction through them.
	scene::Entity ball;
	const auto orbit = [&]() -> void {
		std::tie(scn, ball) = makeOrbitScene();
		physics::PhysicCommand::init(*scn);
		launchOrbits(*scn);
	};
	ioRunner.measureWithSetup(std::format("physics/gravity/fields_step/{}_bodies_{}_sources", g_OrbitBodies, g_Sources),
							  g_FramesPerSample, orbit, [&]() -> void { run(*scn); });
	constexpr uint32_t predictionSteps = 1000;
	if (const auto name = std::format("physics/gravity/predict/{}_steps_{}_sources", predictionSteps, g_Sources);
		ioRunner.wants(name)) {
		orbit();
		ioRunner.measure(name, predictionSteps, [&]() -> void {
			doNotOptimize(physics::PhysicCommand::predictTrajectory(ball, predictionSteps));
		});
	}
	scn.reset();
	// Time warp: frames at x80 (the bodies on their arcs), and the hand-off between the solver and the rails.
	constexpr uint32_t warpFrames = 10;
	const auto frame = makeFrame(16'667);
	shared<scene::Scene> warped;
	const auto startOrbits = [&]() -> void {
		stopRuntime(warped);
		std::tie(warped, ball) = makeOrbitScene();
		warped->onStartRuntime();
		launchOrbits(*warped);
	};
	ioRunner.measureWithSetup(
			std::format("physics/time_warp/rails_frame/{}_bodies", g_OrbitBodies), warpFrames,
			[&]() -> void {
				startOrbits();
				warped->setTimeScale(80.f);
				warped->onUpdateRuntime(frame, false);
			},
			[&]() -> void {
				for (uint32_t f = 0; f < warpFrames; ++f) warped->onUpdateRuntime(frame, false);
			});
	ioRunner.measureWithSetup(std::format("physics/time_warp/hand_off/{}_bodies", g_OrbitBodies), warpFrames,
							  startOrbits, [&]() -> void {
								  for (uint32_t f = 0; f < warpFrames; ++f) {
									  warped->setTimeScale(f % 2 == 0 ? 80.f : 1.f);
									  warped->onUpdateRuntime(frame, false);
								  }
							  });
	stopRuntime(warped);
	// Articulated bodies: hinged chains swinging down.
	ioRunner.measureWithSetup(
			std::format("physics/articulated/chain_step/{}x{}_links", g_Chains, g_Links), g_FramesPerSample,
			[&]() -> void {
				scn = makeChainScene();
				physics::PhysicCommand::init(*scn);
			},
			[&]() -> void { run(*scn); });
	// Compound bodies: assemblies of parts falling, and the rebuild of an assembly when a part leaves and comes back.
	scene::Entity part;
	const auto assemble = [&]() -> void {
		std::tie(scn, part) = makeAssemblyScene();
		physics::PhysicCommand::init(*scn);
	};
	ioRunner.measureWithSetup(std::format("physics/compound/assembly_step/{}x{}_parts", g_Assemblies, g_Parts),
							  g_FramesPerSample, assemble, [&]() -> void { run(*scn); });
	if (const auto name = std::format("physics/compound/detach_attach/{}_parts", g_Parts); ioRunner.wants(name)) {
		assemble();
		ioRunner.measure(name, 1, [&]() -> void {
			physics::PhysicCommand::detach(part);
			doNotOptimize(physics::PhysicCommand::attach(part));
		});
	}
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
	// A dense 2D scene tuned down to one collision step per tick (the default runs two).
	for (const uint32_t count: {1000U}) {
		shared<scene::Scene> scn;
		ioRunner.measureWithSetup(
				std::format("physics/step_falling_cs1/{}_bodies", count), g_FramesPerSample,
				[&]() -> void {
					scn = makeBoxScene(count, 1, 1);
					physics::PhysicCommand::init(*scn);
				},
				[&]() -> void {
					for (uint32_t f = 0; f < g_FramesPerSample; ++f) physics::PhysicCommand::frame(*scn, step);
				});
	}
	runSimulationBenches(ioRunner);
}

}// namespace owl::bench
