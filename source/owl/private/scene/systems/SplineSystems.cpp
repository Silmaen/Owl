/**
 * @file SplineSystems.cpp
 * @author Silmaen
 * @date 30/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "EngineSystems.h"
#include "math/Transform.h"
#include "math/matrixCreation.h"
#include "scene/Entity.h"
#include "scene/Scene.h"
#include "scene/component/components.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

namespace owl::scene::systems {

namespace {

auto unit(const math::vec3& iVector) -> math::vec3 {
	const float length = iVector.norm();
	return length > std::numeric_limits<float>::epsilon() ? iVector * (1.f / length) : math::vec3{0.f, 0.f, 0.f};
}

auto transformDirection(const math::mat4& iMatrix, const math::vec3& iDirection) -> math::vec3 {
	const math::vec4 direction = iMatrix * math::vec4{iDirection.x(), iDirection.y(), iDirection.z(), 0.f};
	return {direction.x(), direction.y(), direction.z()};
}

auto transformPoint(const math::mat4& iMatrix, const math::vec3& iPoint) -> math::vec3 {
	const math::vec4 point = iMatrix * math::vec4{iPoint.x(), iPoint.y(), iPoint.z(), 1.f};
	return {point.x(), point.y(), point.z()};
}

void advance(component::SplineFollower& ioFollower, const float iLength, const bool iClosed, const float iSeconds) {
	using Mode = component::SplineFollower::Mode;
	if (!ioFollower.m_started) {
		ioFollower.m_distance = ioFollower.startDistance;
		ioFollower.m_direction = 1.f;
		ioFollower.m_travelled = 0.f;
		ioFollower.m_started = true;
		return;
	}
	if (iLength <= 0.f)
		return;
	float step = ioFollower.speed * ioFollower.m_direction * iSeconds;
	switch (ioFollower.mode) {
		case Mode::Once:
			if (iClosed) {
				const float left = std::max(0.f, iLength - ioFollower.m_travelled);
				step = std::clamp(step, -left, left);
				ioFollower.m_travelled += std::abs(step);
				ioFollower.m_distance = std::fmod(std::fmod(ioFollower.m_distance + step, iLength) + iLength, iLength);
			} else {
				ioFollower.m_distance = std::clamp(ioFollower.m_distance + step, 0.f, iLength);
			}
			break;
		case Mode::Loop:
			ioFollower.m_distance = std::fmod(std::fmod(ioFollower.m_distance + step, iLength) + iLength, iLength);
			break;
		case Mode::PingPong:
			ioFollower.m_distance += step;
			if (iClosed) {
				ioFollower.m_distance = std::fmod(std::fmod(ioFollower.m_distance, iLength) + iLength, iLength);
			} else if (ioFollower.m_distance > iLength) {
				ioFollower.m_distance = std::max(0.f, 2.f * iLength - ioFollower.m_distance);
				ioFollower.m_direction = -ioFollower.m_direction;
			} else if (ioFollower.m_distance < 0.f) {
				ioFollower.m_distance = std::min(iLength, -ioFollower.m_distance);
				ioFollower.m_direction = -ioFollower.m_direction;
			}
			break;
	}
}

// World frame of a point of the path: -Z along the curve in 3D, +X along it on a 2D path (up along Z).
auto orientedFrame(const math::vec3& iPosition, const math::vec3& iTangent, const math::vec3& iUp) -> math::mat4 {
	math::mat4 frame = math::identity<float, 4>();
	const bool flat = std::abs(iUp.z()) > 0.9f;
	math::vec3 axisX;
	math::vec3 axisY;
	math::vec3 axisZ;
	if (flat) {
		axisZ = {0.f, 0.f, 1.f};
		axisX = unit({iTangent.x(), iTangent.y(), 0.f});
		axisY = axisZ ^ axisX;
	} else {
		axisZ = iTangent * -1.f;
		axisX = unit(iUp ^ axisZ);
		axisY = axisZ ^ axisX;
	}
	if (axisX.normSq() <= 0.f)
		return math::translate(math::identity<float, 4>(), iPosition);
	for (size_t row = 0; row < 3; ++row) {
		frame(row, 0) = axisX[row];
		frame(row, 1) = axisY[row];
		frame(row, 2) = axisZ[row];
		frame(row, 3) = iPosition[row];
	}
	return frame;
}

auto parentWorld(const Scene& iScene, const Entity& iEntity) -> math::mat4 {
	const auto parentId = iEntity.getComponent<component::Hierarchy>().parentId;
	if (parentId != core::UUID{0}) {
		if (const auto parent = iScene.findEntityByUUID(parentId); parent)
			return iScene.getWorldTransform(parent)();
	}
	return math::identity<float, 4>();
}

}// namespace

void updateSplineFollowers(Scene& ioScene, const SystemContext& iContext) {
	OWL_PROFILE_FUNCTION()

	const float seconds = iContext.timeStep.getSeconds();
	for (const auto view = ioScene.registry.view<component::SplineFollower, component::Transform>();
		 const auto handle: view) {
		auto& follower = view.get<component::SplineFollower>(handle);
		if (!follower.enabled || follower.spline == core::UUID{0})
			continue;
		const Entity entity{handle, &ioScene};
		const Entity splineEntity = ioScene.findEntityByUUID(follower.spline);
		if (!splineEntity || splineEntity == entity || !splineEntity.hasComponent<component::Spline>())
			continue;
		const auto& spline = splineEntity.getComponent<component::Spline>();
		advance(follower, spline.curve.getLength(), spline.curve.isClosed(), seconds);
		const auto sample = spline.sample(follower.m_distance, follower.lane, follower.offset);
		const math::mat4 world = ioScene.getWorldTransform(splineEntity)();
		const math::vec3 position = transformPoint(world, sample.position);
		auto& transform = view.get<component::Transform>(handle).transform;
		const math::mat4 toLocal = math::inverse(parentWorld(ioScene, entity));
		if (!follower.orient || sample.tangent.normSq() <= 0.f) {
			transform.translation() = transformPoint(toLocal, position);
			continue;
		}
		const math::mat4 frame = orientedFrame(position, unit(transformDirection(world, sample.tangent)),
											   unit(transformDirection(world, spline.up)));
		const math::Transform local{toLocal * frame};
		transform.translation() = local.translation();
		transform.rotation() = local.rotation();
	}
}

}// namespace owl::scene::systems
