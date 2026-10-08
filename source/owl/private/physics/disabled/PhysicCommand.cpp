/**
 * @file PhysicCommand.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "physics/PhysicCommand.h"

#include <cstdint>
#include <vector>

// Built instead of the Box2D implementation when OWL_MODULE_PHYSICS is OFF: the API stays, nothing simulates.

namespace owl::physics {

// Empty: no physics world without the physics module.
class PhysicCommand::Impl {};

shared<PhysicCommand::Impl> PhysicCommand::m_impl = nullptr;
scene::Scene* PhysicCommand::m_scene = nullptr;

PhysicCommand::PhysicCommand() = default;

void PhysicCommand::init([[maybe_unused]] scene::Scene* iScene) {
	static bool sWarned = false;
	if (!sWarned) {
		OWL_CORE_WARN("Physic: not built in (OWL_MODULE_PHYSICS=OFF), bodies stay where they are.")
		sWarned = true;
	}
}

void PhysicCommand::destroy() {}

void PhysicCommand::releaseScene([[maybe_unused]] const scene::Scene* iScene) {}

auto PhysicCommand::isInitialized() -> bool { return false; }

void PhysicCommand::frame([[maybe_unused]] const core::Timestep& iTimestep) {}

auto PhysicCommand::getSettings() -> PhysicsSettings { return PhysicsSettings{}; }

auto PhysicCommand::getWorkerCount() -> uint32_t { return 0; }

auto PhysicCommand::getLastFrameStepCount() -> uint32_t { return 0; }

auto PhysicCommand::getInterpolationAlpha() -> float { return 1.f; }

void PhysicCommand::syncSimulatedTransforms() {}

auto PhysicCommand::takeCollisionEvents() -> std::vector<CollisionEvent> { return {}; }

void PhysicCommand::destroyBody([[maybe_unused]] const scene::Entity& iEntity) {}

void PhysicCommand::impulse([[maybe_unused]] const scene::Entity& iEntity,
							[[maybe_unused]] const math::vec2f& iImpulse) {}

auto PhysicCommand::getVelocity([[maybe_unused]] const scene::Entity& iEntity) -> math::vec2f { return {0.f, 0.f}; }

void PhysicCommand::setTransform([[maybe_unused]] const scene::Entity& iEntity,
								 [[maybe_unused]] const math::vec2f& iPosition,
								 [[maybe_unused]] const float iRotation) {}

void PhysicCommand::setVelocity([[maybe_unused]] const scene::Entity& iEntity,
								[[maybe_unused]] const math::vec2f& iVelocity) {}

void PhysicCommand::setGravityScale([[maybe_unused]] const scene::Entity& iEntity,
									[[maybe_unused]] const float iScale) {}

auto PhysicCommand::getSnapshot([[maybe_unused]] const scene::Entity& iEntity) -> PhysicsSnapshot { return {}; }

void PhysicCommand::applySnapshot([[maybe_unused]] const scene::Entity& iEntity,
								  [[maybe_unused]] const PhysicsSnapshot& iSnapshot) {}

}// namespace owl::physics
