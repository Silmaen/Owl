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
		if (!ioPatrol.waypoints.empty() || ioPatrol.m_target == ioPatrol.m_position)
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

void updateFollowTargets(Scene& ioScene, [[maybe_unused]] const SystemContext& iContext) {
	OWL_PROFILE_FUNCTION()

	for (const auto view = ioScene.registry.view<component::FollowTarget, component::Transform>();
		 const auto handle: view) {
		auto& follow = view.get<component::FollowTarget>(handle);
		if (!follow.enabled || follow.target == core::UUID{0})
			continue;
		const Entity target = ioScene.findEntityByUUID(follow.target);
		if (!target || target == Entity{handle, &ioScene})
			continue;
		auto& transform = view.get<component::Transform>(handle).transform;
		if (!follow.m_captured) {
			follow.m_baseYaw = transform.rotation().y();
			follow.m_captured = true;
		}
		const auto world = ioScene.getWorldTransform(target);
		transform.translation() = world.translation() + follow.offset;
		if (follow.followYaw)
			transform.rotation().y() = world.rotation().y() + follow.m_baseYaw;
	}
}

void applyDayNightCycles(Scene& ioScene, const bool iRuntime) {
	OWL_PROFILE_FUNCTION()

	const auto smooth = [](const float iEdge0, const float iEdge1, const float iX) -> float {
		const float t = std::clamp((iX - iEdge0) / (iEdge1 - iEdge0), 0.f, 1.f);
		return t * t * (3.f - 2.f * t);
	};
	const auto mix = [](const math::vec3& iA, const math::vec3& iB, const float iT) -> math::vec3 {
		return iA * (1.f - iT) + iB * iT;
	};
	for (const auto view = ioScene.registry.view<component::DayNightCycle>(); const auto handle: view) {
		auto& cycle = view.get<component::DayNightCycle>(handle);
		const math::vec3 sun = cycle.getSunDirection();
		if (iRuntime) {
			const float wanted = cycle.nightExposure * (1.f - smooth(-0.15f, 0.05f, sun.y()));
			const auto cameras = ioScene.registry.view<component::Camera>();
			for (const auto cameraHandle: cameras) {
				auto& camera = cameras.get<component::Camera>(cameraHandle).camera;
				auto settings = camera.getPostProcess();
				settings.exposure += wanted - cycle.m_appliedExposure;
				camera.setPostProcess(settings);
			}
			cycle.m_appliedExposure = wanted;
		}
		if (auto* sky = ioScene.registry.try_get<component::ProceduralSky>(handle); sky != nullptr)
			sky->sunDirection = sun;
		if (cycle.sun == core::UUID{0})
			continue;
		const Entity lightEntity = ioScene.findEntityByUUID(cycle.sun);
		if (!lightEntity || !lightEntity.hasComponent<component::Light>())
			continue;
		auto& light = lightEntity.getComponent<component::Light>();
		const float noonHeight =
				std::sin(std::clamp(cycle.maxElevation, 1.f, 90.f) * std::numbers::pi_v<float> / 180.f);
		const bool byDay = sun.y() >= 0.f;
		const math::vec3 source = byDay ? sun : sun * -1.f;
		const float height = source.y();
		const float rise = smooth(-0.02f, 0.12f, height) * std::clamp(height / noonHeight, 0.05f, 1.f);
		if (byDay) {
			light.intensity = cycle.sunIlluminance * rise;
			light.color = mix(cycle.sunsetColor, cycle.sunColor, smooth(0.f, 0.35f, height));
		} else {
			light.intensity = cycle.moonIlluminance * rise;
			light.color = cycle.moonColor;
		}
		const math::vec3 travel = source * -1.f;
		auto& rotation = lightEntity.getComponent<component::Transform>().transform.rotation();
		rotation = {std::asin(std::clamp(travel.y(), -1.f, 1.f)), std::atan2(-travel.x(), -travel.z()), 0.f};
	}
}

void updateDayNightCycles(Scene& ioScene, const SystemContext& iContext) {
	OWL_PROFILE_FUNCTION()

	const float seconds = iContext.timeStep.getSeconds();
	for (const auto view = ioScene.registry.view<component::DayNightCycle>(); const auto handle: view) {
		auto& cycle = view.get<component::DayNightCycle>(handle);
		if (!cycle.enabled)
			continue;
		cycle.timeOfDay = std::fmod(cycle.timeOfDay + seconds * 24.f / std::max(cycle.dayLength, 1.f), 24.f);
	}
	applyDayNightCycles(ioScene, true);
}

}// namespace owl::scene::systems
