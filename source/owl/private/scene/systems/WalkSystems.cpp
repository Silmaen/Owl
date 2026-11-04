/**
 * @file WalkSystems.cpp
 * @author Silmaen
 * @date 31/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "EngineSystems.h"
#include "math/elementaryFunctions.h"
#include "math/matrixCreation.h"
#include "scene/Entity.h"
#include "scene/Scene.h"
#include "scene/component/components.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <tuple>

namespace owl::scene::systems {

namespace {

auto transformPoint(const math::mat4& iMatrix, const math::vec3& iPoint) -> math::vec3 {
	const math::vec4 point = iMatrix * math::vec4{iPoint.x(), iPoint.y(), iPoint.z(), 1.f};
	return {point.x(), point.y(), point.z()};
}

auto parentWorld(const Scene& iScene, const Entity& iEntity) -> math::mat4 {
	const auto parentId = iEntity.getComponent<component::Hierarchy>().parentId;
	if (parentId != core::UUID{0}) {
		if (const auto parent = iScene.findEntityByUUID(parentId); parent)
			return iScene.getWorldTransform(parent)();
	}
	return math::identity<float, 4>();
}

// The Animator of the entity, else of its first child holding one.
auto findAnimator(const Scene& iScene, const Entity& iEntity) -> component::Animator* {
	if (iEntity.hasComponent<component::Animator>())
		return &iEntity.getComponent<component::Animator>();
	for (const auto child: iEntity.getComponent<component::Hierarchy>().childrenIds) {
		if (const auto entity = iScene.findEntityByUUID(child); entity && entity.hasComponent<component::Animator>())
			return &entity.getComponent<component::Animator>();
	}
	return nullptr;
}

// Walk the corners left by a distance; returns the distance walked.
auto walkPath(component::WalkAgent& ioAgent, math::vec2& ioPosition, float iDistance, math::vec2& oDirection) -> float {
	float walked = 0.f;
	while (iDistance > 0.f && ioAgent.isMoving()) {
		const math::vec2 target = ioAgent.m_path[ioAgent.m_next];
		const math::vec2 offset = target - ioPosition;
		const float length = offset.norm();
		if (length > std::numeric_limits<float>::epsilon())
			oDirection = offset * (1.f / length);
		if (length <= iDistance) {
			ioPosition = target;
			iDistance -= length;
			walked += length;
			++ioAgent.m_next;
			continue;
		}
		ioPosition = ioPosition + oDirection * iDistance;
		walked += iDistance;
		iDistance = 0.f;
	}
	if (!ioAgent.isMoving())
		ioAgent.stop();
	return walked;
}

void updateAgent(Scene& ioScene, const Entity& iEntity, component::WalkAgent& ioAgent, const float iSeconds) {
	auto& transform = iEntity.getComponent<component::Transform>().transform;
	if (!ioAgent.m_started) {
		ioAgent.m_baseScale = transform.scale();
		ioAgent.m_baseScale.x() = std::abs(ioAgent.m_baseScale.x());
		ioAgent.m_started = true;
	}
	const Entity areaEntity = ioScene.findEntityByUUID(ioAgent.area);
	if (!areaEntity || areaEntity == iEntity || !areaEntity.hasComponent<component::WalkArea>())
		return;
	const auto& area = areaEntity.getComponent<component::WalkArea>();
	const math::mat4 areaWorld = ioScene.getWorldTransform(areaEntity)();
	const math::mat4 toArea = math::inverse(areaWorld);
	const math::mat4 world = ioScene.getWorldTransform(iEntity)();
	math::vec3 position{world(0, 3), world(1, 3), world(2, 3)};
	math::vec2 planar{position.x(), position.y()};
	const float before = transformPoint(toArea, position).y();
	const float speed = ioAgent.speed * (ioAgent.scaleSpeed ? area.getScaleAt(before) : 1.f);
	math::vec2 direction{0.f, 0.f};
	const float walked = walkPath(ioAgent, planar, speed * iSeconds, direction);
	ioAgent.m_currentSpeed = iSeconds > 0.f ? walked / iSeconds : 0.f;
	position = {planar.x(), planar.y(), position.z()};
	const float height = transformPoint(toArea, position).y();
	if (area.depthSort)
		position.z() = areaWorld(2, 3) + area.getDepthAt(height);
	transform.translation() = transformPoint(math::inverse(parentWorld(ioScene, iEntity)), position);
	if (std::abs(direction.x()) > 1e-3f)
		ioAgent.m_sideways = direction.x() < 0.f ? -1.f : 1.f;
	const float factor = area.getScaleAt(height);
	math::vec3 scale = ioAgent.m_baseScale * factor;
	if (ioAgent.facing == component::WalkAgent::Facing::MirrorX)
		scale.x() *= ioAgent.m_sideways;
	transform.scale() = scale;
	if (ioAgent.facing == component::WalkAgent::Facing::TurnY && direction.normSq() > 0.f)
		transform.rotation().y() = math::atan2(direction.x(), -direction.y());
	if (ioAgent.speedParameter.empty())
		return;
	if (auto* animator = findAnimator(ioScene, iEntity); animator != nullptr && animator->player.isReady())
		std::ignore = animator->player.setParameter(animator->machine, ioAgent.speedParameter,
													ioAgent.m_currentSpeed / std::max(factor, 0.01f));
}

void updateMask(const Scene& iScene, const Entity& iEntity, const component::WalkBehind& iMask) {
	const Entity areaEntity = iScene.findEntityByUUID(iMask.area);
	if (!areaEntity || !areaEntity.hasComponent<component::WalkArea>())
		return;
	const auto& area = areaEntity.getComponent<component::WalkArea>();
	if (!area.depthSort)
		return;
	const math::mat4 world = iScene.getWorldTransform(iEntity)();
	const math::vec3 position{world(0, 3), world(1, 3),
							  iScene.getWorldTransform(areaEntity)()(2, 3) + area.getDepthAt(iMask.baseline)};
	auto& transform = iEntity.getComponent<component::Transform>().transform;
	transform.translation() = transformPoint(math::inverse(parentWorld(iScene, iEntity)), position);
}

}// namespace

void updateWalkAgents(Scene& ioScene, const SystemContext& iContext) {
	OWL_PROFILE_FUNCTION()

	const float seconds = iContext.timeStep.getSeconds();
	for (const auto view = ioScene.registry.view<component::WalkAgent, component::Transform>(); const auto handle: view)
		updateAgent(ioScene, Entity{handle, &ioScene}, view.get<component::WalkAgent>(handle), seconds);
	for (const auto view = ioScene.registry.view<component::WalkBehind, component::Transform>();
		 const auto handle: view)
		updateMask(ioScene, Entity{handle, &ioScene}, view.get<component::WalkBehind>(handle));
}

}// namespace owl::scene::systems
