/**
 * @file WalkQueries.cpp
 * @author Silmaen
 * @date 31/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "scene/nav/WalkQueries.h"

#include "math/matrixCreation.h"
#include "scene/Entity.h"
#include "scene/Scene.h"
#include "scene/component/WalkArea.h"

#include <optional>
#include <vector>

namespace owl::scene::nav {

namespace {

auto apply(const math::mat4& iMatrix, const math::vec2& iPoint) -> math::vec2 {
	const math::vec4 point = iMatrix * math::vec4{iPoint.x(), iPoint.y(), 0.f, 1.f};
	return {point.x(), point.y()};
}

auto hasArea(const Entity& iArea) -> bool { return iArea && iArea.hasComponent<component::WalkArea>(); }

}// namespace

auto findWalkPath(const Scene& iScene, const Entity& iArea, const math::vec2& iFrom, const math::vec2& iTo,
				  std::vector<math::vec2>& oPath) -> bool {
	oPath.clear();
	if (!hasArea(iArea)) {
		OWL_CORE_WARN("findWalkPath: the entity has no WalkArea.")
		return false;
	}
	const math::mat4 world = iScene.getWorldTransform(iArea)();
	const math::mat4 toLocal = math::inverse(world);
	if (!iArea.getComponent<component::WalkArea>().getMesh().findPath(apply(toLocal, iFrom), apply(toLocal, iTo),
																	  oPath))
		return false;
	for (auto& point: oPath) point = apply(world, point);
	return true;
}

auto getWalkClosestPoint(const Scene& iScene, const Entity& iArea, const math::vec2& iPoint)
		-> std::optional<math::vec2> {
	if (!hasArea(iArea))
		return std::nullopt;
	const math::mat4 world = iScene.getWorldTransform(iArea)();
	const auto closest =
			iArea.getComponent<component::WalkArea>().getMesh().getClosestPoint(apply(math::inverse(world), iPoint));
	if (!closest)
		return std::nullopt;
	return apply(world, *closest);
}

auto isOnWalkArea(const Scene& iScene, const Entity& iArea, const math::vec2& iPoint) -> bool {
	if (!hasArea(iArea))
		return false;
	const math::mat4 toLocal = math::inverse(iScene.getWorldTransform(iArea)());
	return iArea.getComponent<component::WalkArea>().getMesh().contains(apply(toLocal, iPoint));
}

auto getWalkScaleAt(const Scene& iScene, const Entity& iArea, const math::vec2& iPoint) -> float {
	if (!hasArea(iArea))
		return 1.f;
	const math::mat4 toLocal = math::inverse(iScene.getWorldTransform(iArea)());
	return iArea.getComponent<component::WalkArea>().getScaleAt(apply(toLocal, iPoint).y());
}

auto walkAgentTo(Scene& ioScene, const Entity& iAgent, const math::vec2& iTarget) -> bool {
	if (!iAgent || !iAgent.hasComponent<component::WalkAgent>()) {
		OWL_CORE_WARN("walkAgentTo: the entity has no WalkAgent.")
		return false;
	}
	auto& agent = iAgent.getComponent<component::WalkAgent>();
	const Entity area = ioScene.findEntityByUUID(agent.area);
	const math::mat4 world = ioScene.getWorldTransform(iAgent)();
	std::vector<math::vec2> path;
	if (!findWalkPath(ioScene, area, {world(0, 3), world(1, 3)}, iTarget, path)) {
		agent.stop();
		return false;
	}
	agent.m_path = std::move(path);
	agent.m_next = 1;
	return true;
}

}// namespace owl::scene::nav
