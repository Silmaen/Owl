/**
 * @file PhysicCommand.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "physics/PhysicCommand.h"

#include "physics/GravityField.h"
#include "scene/Entity.h"
#include "scene/component/PhysicBody.h"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

// Built instead of the Jolt implementation when OWL_MODULE_PHYSICS is OFF: the API stays, nothing simulates.

namespace owl::physics {

void PhysicCommand::init([[maybe_unused]] scene::Scene& ioScene) {
	static bool sWarned = false;
	if (!sWarned) {
		OWL_CORE_WARN("Physics: Not built in (OWL_MODULE_PHYSICS=OFF), bodies stay where they are.")
		sWarned = true;
	}
}

void PhysicCommand::destroy([[maybe_unused]] scene::Scene& ioScene) {}

auto PhysicCommand::isInitialized([[maybe_unused]] const scene::Scene& iScene) -> bool { return false; }

void PhysicCommand::step([[maybe_unused]] scene::Scene& ioScene) {}

void PhysicCommand::writeFrameTransforms([[maybe_unused]] scene::Scene& ioScene) {}

void PhysicCommand::frame([[maybe_unused]] scene::Scene& ioScene, [[maybe_unused]] const core::Timestep& iTimestep) {}

auto PhysicCommand::getSettings([[maybe_unused]] const scene::Scene& iScene) -> PhysicsSettings {
	return PhysicsSettings{};
}

auto PhysicCommand::getWorkerCount([[maybe_unused]] const scene::Scene& iScene) -> uint32_t { return 0; }

auto PhysicCommand::getLastFrameStepCount([[maybe_unused]] const scene::Scene& iScene) -> uint32_t { return 0; }

auto PhysicCommand::getInterpolationAlpha([[maybe_unused]] const scene::Scene& iScene) -> float { return 1.f; }

void PhysicCommand::syncSimulatedTransforms([[maybe_unused]] scene::Scene& ioScene) {}

void PhysicCommand::shiftOrigin([[maybe_unused]] scene::Scene& ioScene, [[maybe_unused]] const math::vec2f& iShift) {}

auto PhysicCommand::takeCollisionEvents([[maybe_unused]] scene::Scene& ioScene) -> std::vector<CollisionEvent> {
	return {};
}

void PhysicCommand::destroyBody([[maybe_unused]] const scene::Entity& iEntity) {}

void PhysicCommand::impulse([[maybe_unused]] const scene::Entity& iEntity,
							[[maybe_unused]] const math::vec2f& iImpulse) {}

auto PhysicCommand::getVelocity([[maybe_unused]] const scene::Entity& iEntity) -> math::vec2f { return {0.f, 0.f}; }

void PhysicCommand::setTransform([[maybe_unused]] const scene::Entity& iEntity,
								 [[maybe_unused]] const math::vec2f& iPosition,
								 [[maybe_unused]] const float iRotation) {}

void PhysicCommand::setVelocity([[maybe_unused]] const scene::Entity& iEntity,
								[[maybe_unused]] const math::vec2f& iVelocity) {}

auto PhysicCommand::saveState([[maybe_unused]] const scene::Scene& iScene) -> std::vector<uint8_t> { return {}; }

auto PhysicCommand::restoreState([[maybe_unused]] scene::Scene& ioScene,
								 [[maybe_unused]] const std::span<const uint8_t> iState) -> bool {
	return false;
}

void PhysicCommand::impulse3D([[maybe_unused]] const scene::Entity& iEntity,
							  [[maybe_unused]] const math::vec3f& iImpulse) {}

auto PhysicCommand::getVelocity3D([[maybe_unused]] const scene::Entity& iEntity) -> math::vec3f {
	return {0.f, 0.f, 0.f};
}

void PhysicCommand::setVelocity3D([[maybe_unused]] const scene::Entity& iEntity,
								  [[maybe_unused]] const math::vec3f& iVelocity) {}

void PhysicCommand::setTransform3D([[maybe_unused]] const scene::Entity& iEntity,
								   [[maybe_unused]] const math::vec3f& iPosition,
								   [[maybe_unused]] const math::vec3f& iRotation) {}

void PhysicCommand::setGravityScale([[maybe_unused]] const scene::Entity& iEntity,
									[[maybe_unused]] const float iScale) {}

auto PhysicCommand::getSnapshot([[maybe_unused]] const scene::Entity& iEntity) -> PhysicsSnapshot { return {}; }

void PhysicCommand::applySnapshot([[maybe_unused]] const scene::Entity& iEntity,
								  [[maybe_unused]] const PhysicsSnapshot& iSnapshot) {}

auto PhysicCommand::raycast([[maybe_unused]] const scene::Scene& iScene, [[maybe_unused]] const math::vec3f& iOrigin,
							[[maybe_unused]] const math::vec3f& iDirection, [[maybe_unused]] const float iDistance,
							[[maybe_unused]] const uint8_t iMask) -> std::optional<QueryHit> {
	return std::nullopt;
}

auto PhysicCommand::shapeCast([[maybe_unused]] const scene::Scene& iScene, [[maybe_unused]] const QueryShape& iShape,
							  [[maybe_unused]] const math::vec3f& iOrigin,
							  [[maybe_unused]] const math::vec3f& iDirection, [[maybe_unused]] const float iDistance,
							  [[maybe_unused]] const uint8_t iMask) -> std::optional<QueryHit> {
	return std::nullopt;
}

auto PhysicCommand::overlap([[maybe_unused]] const scene::Scene& iScene, [[maybe_unused]] const QueryShape& iShape,
							[[maybe_unused]] const math::vec3f& iCentre, [[maybe_unused]] const uint8_t iMask)
		-> std::vector<core::UUID> {
	return {};
}

void PhysicCommand::setJointMotor([[maybe_unused]] const scene::Entity& iEntity, [[maybe_unused]] const bool iEnabled,
								  [[maybe_unused]] const float iSpeed) {}

auto PhysicCommand::getJointPosition([[maybe_unused]] const scene::Entity& iEntity) -> float { return 0.f; }

auto PhysicCommand::getGravityAt(const scene::Scene& iScene, const math::vec3f& iPoint) -> math::vec3f {
	std::vector<FieldSource> sources;
	gatherFieldSources(iScene, {}, sources);
	return iScene.getPhysicsSettings().clamped().gravity + fieldAcceleration(sources, iPoint, core::UUID{0});
}

auto PhysicCommand::predictTrajectory(const scene::Entity& iEntity, const uint32_t iSteps) -> std::vector<math::vec3f> {
	if (!iEntity || !iEntity.hasComponent<scene::component::PhysicBody>())
		return {};
	const auto& sbody = iEntity.getComponent<scene::component::PhysicBody>().body;
	if (sbody.type != scene::SceneBody::BodyType::Dynamic)
		return {};
	const auto& scene = *iEntity.getScene();
	std::vector<FieldSource> sources;
	gatherFieldSources(scene, {}, sources);
	const auto settings = scene.getPhysicsSettings().clamped();
	const PathSettings path{.worldGravity = settings.gravity,
							.gravityScale = sbody.gravityScale,
							.fields = sbody.gravityFields,
							.plane = sbody.dimension == scene::SceneBody::Dimension::Plane,
							.stepSeconds = static_cast<float>(scene.getSimulationClock().getStepSeconds()),
							.subSteps = settings.solverSubSteps,
							.self = iEntity.getUUID()};
	return predictPath(sources, path, scene.getWorldTransform(iEntity).translation(), {0.f, 0.f, 0.f},
					   std::min(iSteps, maxPredictionSteps));
}

auto PhysicCommand::isOnRails([[maybe_unused]] const scene::Scene& iScene) -> bool { return false; }

void PhysicCommand::setSimulated([[maybe_unused]] const scene::Entity& iEntity, [[maybe_unused]] const bool iSimulated,
								 [[maybe_unused]] const math::vec3f& iVelocity) {}

auto PhysicCommand::isSimulated([[maybe_unused]] const scene::Entity& iEntity) -> bool { return true; }

auto PhysicCommand::followRails([[maybe_unused]] const scene::Entity& iEntity, const math::vec3f& iVelocity,
								[[maybe_unused]] const float iSeconds) -> math::vec3f {
	return iVelocity;
}

}// namespace owl::physics
