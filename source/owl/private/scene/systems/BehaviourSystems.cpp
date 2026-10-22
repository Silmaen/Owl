/**
 * @file BehaviourSystems.cpp
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "EngineSystems.h"

#include "physics/PhysicCommand.h"
#include "scene/Entity.h"
#include "scene/Scene.h"
#include "scene/component/components.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>

namespace owl::scene::systems {

namespace {

// Whether a body of the entity is simulated: such an entity moves through the physics step, not the transform.
auto isSimulatedBody(const Scene& iScene, const Entity& iEntity) -> bool {
	return iEntity.hasComponent<component::PhysicBody>() && physics::PhysicCommand::isInitialized(iScene);
}

// Hashed value in [0, 1) of a seed and a counter.
auto unitHash(const float iSeed, const uint32_t iCounter) -> float {
	const double x =
			std::sin(static_cast<double>(iSeed) * 12.9898 + static_cast<double>(iCounter) * 78.233) * 43758.5453;
	return static_cast<float>(x - std::floor(x));
}

void stepOscillator(const Entity& iEntity, component::Oscillator& ioOscillator, const float iSeconds) {
	auto& translation = iEntity.getComponent<component::Transform>().transform.translation();
	if (!ioOscillator.m_captured) {
		ioOscillator.m_basePosition = translation;
		ioOscillator.m_captured = true;
	}
	ioOscillator.m_time += iSeconds;
	translation = ioOscillator.m_basePosition + ioOscillator.offset * ioOscillator.share(ioOscillator.m_time);
}

void tickOscillator(const Entity& iEntity, component::Oscillator& ioOscillator, const float iSeconds) {
	const auto& translation = iEntity.getComponent<component::Transform>().transform.translation();
	if (!ioOscillator.m_captured) {
		ioOscillator.m_basePosition = translation;
		ioOscillator.m_captured = true;
	}
	ioOscillator.m_time += iSeconds;
	const auto target = ioOscillator.m_basePosition + ioOscillator.offset * ioOscillator.share(ioOscillator.m_time);
	const auto velocity = (target - translation) / std::max(iSeconds, 1e-4f);
	physics::PhysicCommand::setVelocity(iEntity, {velocity.x(), velocity.y()});
}

void startPatrol(const Entity& iEntity, component::Patrol& ioPatrol) {
	const auto& transform = iEntity.getComponent<component::Transform>().transform;
	ioPatrol.m_basePosition = transform.translation();
	ioPatrol.m_baseScaleX = std::abs(transform.scale().x());
	ioPatrol.m_position = ioPatrol.m_basePosition;
	if (!ioPatrol.waypoints.empty()) {
		ioPatrol.m_position = {ioPatrol.waypoints.front().x(), ioPatrol.waypoints.front().y(),
							   ioPatrol.m_basePosition.z()};
		ioPatrol.m_index = std::min<std::size_t>(1, ioPatrol.waypoints.size() - 1);
	}
	ioPatrol.m_target = ioPatrol.m_position;
	ioPatrol.m_captured = true;
}

void advanceWaypoint(component::Patrol& ioPatrol) {
	const std::size_t count = ioPatrol.waypoints.size();
	if (count < 2)
		return;
	if (ioPatrol.mode == component::Patrol::Mode::Loop) {
		ioPatrol.m_index = (ioPatrol.m_index + 1) % count;
	} else if (!ioPatrol.m_reverse) {
		if (ioPatrol.m_index + 1 < count) {
			++ioPatrol.m_index;
		} else {
			ioPatrol.m_reverse = true;
			--ioPatrol.m_index;
		}
	} else if (ioPatrol.m_index > 0) {
		--ioPatrol.m_index;
	} else {
		ioPatrol.m_reverse = false;
		++ioPatrol.m_index;
	}
}

// The point the patrol walks to now: the waypoint of the route, or a random point of the wander disk.
auto nextTarget(component::Patrol& ioPatrol) -> math::vec3 {
	if (!ioPatrol.waypoints.empty()) {
		const auto& waypoint = ioPatrol.waypoints[ioPatrol.m_index];
		return {waypoint.x(), waypoint.y(), ioPatrol.m_basePosition.z()};
	}
	const float angle = 2.f * std::numbers::pi_v<float> * unitHash(ioPatrol.seed, ioPatrol.m_choices * 2);
	const float radius = ioPatrol.wanderRadius * std::sqrt(unitHash(ioPatrol.seed, ioPatrol.m_choices * 2 + 1));
	++ioPatrol.m_choices;
	return {ioPatrol.m_basePosition.x() + radius * std::cos(angle),
			ioPatrol.m_basePosition.y() + radius * std::sin(angle), ioPatrol.m_basePosition.z()};
}

void stepPatrol(const Entity& iEntity, component::Patrol& ioPatrol, const float iSeconds) {
	if (!ioPatrol.m_captured)
		startPatrol(iEntity, ioPatrol);
	const bool canMove = !ioPatrol.waypoints.empty() || ioPatrol.wanderRadius > 0.f;
	bool moving = false;
	float directionX = 0.f;
	if (canMove && ioPatrol.m_wait > 0.f) {
		ioPatrol.m_wait -= iSeconds;
	} else if (canMove) {
		if (ioPatrol.waypoints.empty() && ioPatrol.m_target == ioPatrol.m_position)
			ioPatrol.m_target = nextTarget(ioPatrol);
		else if (!ioPatrol.waypoints.empty())
			ioPatrol.m_target = nextTarget(ioPatrol);
		const auto toTarget = ioPatrol.m_target - ioPatrol.m_position;
		const float distance = std::sqrt(toTarget.x() * toTarget.x() + toTarget.y() * toTarget.y());
		const float step = ioPatrol.speed * iSeconds;
		if (distance <= step) {
			ioPatrol.m_position = ioPatrol.m_target;
			ioPatrol.m_wait = ioPatrol.waitTime;
			advanceWaypoint(ioPatrol);
		} else {
			ioPatrol.m_position = ioPatrol.m_position + toTarget * (step / distance);
			directionX = toTarget.x();
			moving = true;
		}
	}
	if (moving)
		ioPatrol.m_walked += iSeconds;
	auto& transform = iEntity.getComponent<component::Transform>().transform;
	const float hop = moving ? ioPatrol.hopHeight * std::abs(std::sin(ioPatrol.m_walked * ioPatrol.hopRate)) : 0.f;
	transform.translation() = {ioPatrol.m_position.x(), ioPatrol.m_position.y() + hop, ioPatrol.m_position.z()};
	if (ioPatrol.faceDirection && directionX != 0.f)
		transform.scale().x() = directionX < 0.f ? -ioPatrol.m_baseScaleX : ioPatrol.m_baseScaleX;
}

}// namespace

void updateOscillators(Scene& ioScene, const SystemContext& iContext) {
	OWL_PROFILE_FUNCTION()

	for (const auto view = ioScene.registry.view<component::Oscillator, component::Transform>();
		 const auto handle: view) {
		const Entity entity{handle, &ioScene};
		auto& oscillator = view.get<component::Oscillator>(handle);
		if (oscillator.enabled && !isSimulatedBody(ioScene, entity))
			stepOscillator(entity, oscillator, iContext.timeStep.getSeconds());
	}
}

void tickOscillators(Scene& ioScene, const SystemContext& iContext) {
	OWL_PROFILE_FUNCTION()

	for (const auto view = ioScene.registry.view<component::Oscillator, component::Transform>();
		 const auto handle: view) {
		const Entity entity{handle, &ioScene};
		auto& oscillator = view.get<component::Oscillator>(handle);
		if (oscillator.enabled && isSimulatedBody(ioScene, entity))
			tickOscillator(entity, oscillator, iContext.timeStep.getSeconds());
	}
}

void updatePatrols(Scene& ioScene, const SystemContext& iContext) {
	OWL_PROFILE_FUNCTION()

	for (const auto view = ioScene.registry.view<component::Patrol, component::Transform>(); const auto handle: view) {
		auto& patrol = view.get<component::Patrol>(handle);
		if (patrol.enabled)
			stepPatrol(Entity{handle, &ioScene}, patrol, iContext.timeStep.getSeconds());
	}
}

void updateDrifts(Scene& ioScene, const SystemContext& iContext) {
	OWL_PROFILE_FUNCTION()

	math::vec3 camera{0.f, 0.f, 0.f};
	if (const Entity primary = ioScene.getPrimaryCamera(); primary)
		camera = ioScene.getWorldTransform(primary).translation();
	const float seconds = iContext.timeStep.getSeconds();
	for (const auto view = ioScene.registry.view<component::Drift, component::Transform>(); const auto handle: view) {
		auto& drift = view.get<component::Drift>(handle);
		if (!drift.enabled)
			continue;
		auto& translation = view.get<component::Transform>(handle).transform.translation();
		if (!drift.m_captured) {
			drift.m_basePosition = translation;
			drift.m_captured = true;
		}
		drift.m_drift = drift.m_drift + drift.velocity * seconds;
		for (std::size_t axis = 0; axis < 3; ++axis) {
			if (drift.range[axis] > 0.f && drift.m_drift[axis] > drift.range[axis])
				drift.m_drift[axis] = -drift.range[axis];
		}
		translation = {drift.m_basePosition.x() + drift.m_drift.x() + camera.x() * drift.cameraFollow,
					   drift.m_basePosition.y() + drift.m_drift.y() + camera.y() * drift.cameraFollow,
					   drift.m_basePosition.z() + drift.m_drift.z()};
	}
}

}// namespace owl::scene::systems
