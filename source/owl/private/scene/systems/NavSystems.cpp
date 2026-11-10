/**
 * @file NavSystems.cpp
 * @author Silmaen
 * @date 08/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "EngineSystems.h"
#include "math/elementaryFunctions.h"
#include "scene/Entity.h"
#include "scene/Scene.h"
#include "scene/component/components.h"
#include "scene/nav/Avoidance.h"
#include "scene/nav/NavBake.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <tuple>
#include <vector>

namespace owl::scene::systems {

namespace {

constexpr float g_minSpeed = 0.05f;

struct Walker {
	Entity entity;
	component::NavAgent* agent = nullptr;
	const nav::NavMesh* mesh = nullptr;
	math::vec3 position;
	math::vec2 preferred;
	bool onLink = false;
};

auto planar(const math::vec3& iPoint) -> math::vec2 { return {iPoint.x(), iPoint.z()}; }

auto isOnLink(const component::NavAgent& iAgent) -> bool {
	return iAgent.m_next > 0 && iAgent.m_next < iAgent.m_path.size() && iAgent.m_path[iAgent.m_next - 1].link != -1;
}

auto findAnimator(const Scene& iScene, const Entity& iEntity) -> component::Animator* {
	if (iEntity.hasComponent<component::Animator>())
		return &iEntity.getComponent<component::Animator>();
	for (const auto child: iEntity.getComponent<component::Hierarchy>().childrenIds) {
		if (const auto entity = iScene.findEntityByUUID(child); entity && entity.hasComponent<component::Animator>())
			return &entity.getComponent<component::Animator>();
	}
	return nullptr;
}

auto findVolume(const Scene& iScene, const component::NavAgent& iAgent, const math::vec3& iPosition)
		-> const component::NavMeshVolume* {
	const Entity entity = iAgent.volume != core::UUID{0} ? iScene.findEntityByUUID(iAgent.volume)
														 : nav::findNavVolume(iScene, iPosition);
	if (!entity || !entity.hasComponent<component::NavMeshVolume>())
		return nullptr;
	const auto& volume = entity.getComponent<component::NavMeshVolume>();
	return volume.isBaked() ? &volume : nullptr;
}

// Locate the agent on its mesh and find the path it was asked for; false when it cannot walk this tick.
auto prepare(const Scene& iScene, Walker& ioWalker) -> bool {
	auto& agent = *ioWalker.agent;
	const auto* volume = findVolume(iScene, agent, ioWalker.position);
	if (volume == nullptr)
		return false;
	ioWalker.mesh = &volume->getMesh();
	const auto& mesh = *ioWalker.mesh;
	if (agent.m_triangle < 0 || std::cmp_greater_equal(agent.m_triangle, mesh.getTriangles().size())) {
		const auto nearest = mesh.findNearest(ioWalker.position);
		if (!nearest.has_value())
			return false;
		agent.m_triangle = static_cast<int32_t>(nearest->triangle);
		if (!isOnLink(agent))
			ioWalker.position = nearest->position;
	}
	if (agent.m_pendingTarget) {
		agent.m_pendingTarget = false;
		agent.m_linkProgress = 0.f;
		if (!mesh.findPath(ioWalker.position, agent.m_target, agent.m_path)) {
			OWL_CORE_WARN_ONCE("NavAgent: '{}' found no path to its goal.", ioWalker.entity.getName())
			agent.stop();
		} else {
			agent.m_next = 1;
		}
	}
	return true;
}

// Velocity towards the next corner, slowing down on the last one so as not to overshoot it.
void computePreferred(Walker& ioWalker, const float iSeconds) {
	const auto& agent = *ioWalker.agent;
	ioWalker.preferred = {0.f, 0.f};
	if (agent.m_next == 0 || agent.m_next >= agent.m_path.size())
		return;
	ioWalker.onLink = isOnLink(agent);
	const math::vec2 offset = planar(agent.m_path[agent.m_next].position) - planar(ioWalker.position);
	const float distance = offset.norm();
	if (distance <= 1e-6f)
		return;
	const bool last = agent.m_next + 1 == agent.m_path.size();
	const float speed = last ? std::min(agent.speed, distance / std::max(iSeconds, 1e-4f)) : agent.speed;
	ioWalker.preferred = offset * (speed / distance);
}

void crossLink(Walker& ioWalker, const float iSeconds) {
	auto& agent = *ioWalker.agent;
	const math::vec3& start = agent.m_path[agent.m_next - 1].position;
	const math::vec3& end = agent.m_path[agent.m_next].position;
	const float length = (end - start).norm();
	agent.m_linkProgress += agent.speed * iSeconds;
	if (length <= 1e-6f || agent.m_linkProgress >= length) {
		ioWalker.position = end;
		agent.m_linkProgress = 0.f;
		agent.m_triangle = -1;
		++agent.m_next;
		if (agent.m_next >= agent.m_path.size())
			agent.stop();
		agent.m_velocity = {0.f, 0.f};
		return;
	}
	const math::vec3 direction = (end - start) * (1.f / length);
	ioWalker.position = start + direction * agent.m_linkProgress;
	agent.m_velocity = planar(direction) * agent.speed;
}

void walk(Walker& ioWalker, const math::vec2& iVelocity, const float iSeconds) {
	auto& agent = *ioWalker.agent;
	const math::vec2 change = iVelocity - agent.m_velocity;
	const float limit = agent.acceleration * iSeconds;
	agent.m_velocity = change.norm() > limit ? agent.m_velocity + change.normalized() * limit : iVelocity;
	if (agent.m_velocity.norm() > agent.speed)
		agent.m_velocity = agent.m_velocity.normalized() * agent.speed;
	if (!agent.isMoving() && agent.m_velocity.norm() < g_minSpeed)
		agent.m_velocity = {0.f, 0.f};
	const math::vec3 target{ioWalker.position.x() + agent.m_velocity.x() * iSeconds, ioWalker.position.y(),
							ioWalker.position.z() + agent.m_velocity.y() * iSeconds};
	const auto moved =
			ioWalker.mesh->moveAlongSurface({ioWalker.position, static_cast<uint32_t>(agent.m_triangle)}, target);
	ioWalker.position = moved.position;
	agent.m_triangle = static_cast<int32_t>(moved.triangle);
	while (agent.m_next > 0 && agent.m_next < agent.m_path.size()) {
		const bool last = agent.m_next + 1 == agent.m_path.size();
		const bool beforeLink = agent.m_path[agent.m_next].link != -1;
		const float reach = last || beforeLink ? std::max(agent.stoppingDistance, 0.02f) : std::max(agent.radius, 0.1f);
		if ((planar(agent.m_path[agent.m_next].position) - planar(ioWalker.position)).norm() > reach)
			break;
		if (beforeLink)
			ioWalker.position = agent.m_path[agent.m_next].position;
		++agent.m_next;
		if (agent.m_next >= agent.m_path.size()) {
			agent.stop();
			agent.m_velocity = {0.f, 0.f};
		}
	}
}

void place(const Scene& iScene, const Walker& iWalker, const float iSeconds) {
	const auto& agent = *iWalker.agent;
	setWorldPosition(iScene, iWalker.entity, iWalker.position);
	const float speed = agent.m_velocity.norm();
	if (agent.turn && speed > g_minSpeed) {
		iWalker.entity.getComponent<component::Transform>().transform.rotation().y() =
				math::atan2(agent.m_velocity.x(), agent.m_velocity.y());
	}
	if (agent.speedParameter.empty() || iSeconds <= 0.f)
		return;
	if (auto* animator = findAnimator(iScene, iWalker.entity); animator != nullptr && animator->player.isReady())
		std::ignore = animator->player.setParameter(animator->machine, agent.speedParameter, speed);
}

void trackObstacles(Scene& ioScene, const float iSeconds, std::vector<nav::AvoidanceBody>& oBodies) {
	std::vector<std::tuple<uint64_t, entt::entity>> sorted;
	for (const auto handle: ioScene.registry.view<component::NavObstacle, component::Transform, component::ID>())
		sorted.emplace_back(static_cast<uint64_t>(ioScene.registry.get<component::ID>(handle).id), handle);
	std::ranges::sort(sorted);
	for (const auto& [uuid, handle]: sorted) {
		const Entity entity{handle, &ioScene};
		auto& obstacle = entity.getComponent<component::NavObstacle>();
		const math::vec3 position = getWorldPosition(ioScene, entity);
		if (obstacle.m_started && iSeconds > 0.f)
			obstacle.m_velocity = planar(position - obstacle.m_lastPosition) * (1.f / iSeconds);
		obstacle.m_lastPosition = position;
		obstacle.m_started = true;
		oBodies.push_back({.position = planar(position),
						   .velocity = obstacle.m_velocity,
						   .preferred = obstacle.m_velocity,
						   .radius = obstacle.radius,
						   .maxSpeed = obstacle.m_velocity.norm(),
						   .responsive = false});
	}
}

}// namespace

void tickNavAgents(Scene& ioScene, const SystemContext& iContext) {
	OWL_PROFILE_FUNCTION()

	const float seconds = iContext.timeStep.getSeconds();
	const auto volumes = ioScene.registry.view<component::NavMeshVolume>();
	if (volumes.begin() == volumes.end())
		return;
	const auto links = nav::collectNavLinks(ioScene);
	for (const auto handle: volumes) volumes.get<component::NavMeshVolume>(handle).setLinks(links);

	std::vector<std::tuple<uint64_t, entt::entity>> sorted;
	for (const auto handle: ioScene.registry.view<component::NavAgent, component::Transform, component::ID>())
		sorted.emplace_back(static_cast<uint64_t>(ioScene.registry.get<component::ID>(handle).id), handle);
	std::ranges::sort(sorted);
	std::vector<Walker> walkers;
	walkers.reserve(sorted.size());
	for (const auto& [uuid, handle]: sorted) {
		const Entity entity{handle, &ioScene};
		Walker walker{.entity = entity,
					  .agent = &entity.getComponent<component::NavAgent>(),
					  .mesh = nullptr,
					  .position = getWorldPosition(ioScene, entity),
					  .preferred = {},
					  .onLink = false};
		if (prepare(ioScene, walker))
			walkers.push_back(walker);
	}

	std::vector<nav::AvoidanceBody> bodies;
	bodies.reserve(walkers.size());
	for (auto& walker: walkers) {
		computePreferred(walker, seconds);
		bodies.push_back({.position = planar(walker.position),
						  .velocity = walker.agent->m_velocity,
						  .preferred = walker.preferred,
						  .radius = walker.agent->radius,
						  .maxSpeed = walker.agent->speed,
						  .responsive = walker.agent->avoidance && !walker.onLink});
	}
	trackObstacles(ioScene, seconds, bodies);
	const auto velocities = nav::computeAvoidance(bodies, {}, seconds);

	for (size_t index = 0; index < walkers.size(); ++index) {
		auto& walker = walkers[index];
		if (walker.onLink)
			crossLink(walker, seconds);
		else
			walk(walker, walker.agent->avoidance ? velocities[index] : walker.preferred, seconds);
		place(ioScene, walker, seconds);
	}
}

}// namespace owl::scene::systems
