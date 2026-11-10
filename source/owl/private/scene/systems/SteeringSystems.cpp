/**
 * @file SteeringSystems.cpp
 * @author Silmaen
 * @date 10/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "EngineSystems.h"
#include "math/elementaryFunctions.h"
#include "scene/Entity.h"
#include "scene/Scene.h"
#include "scene/ai/Steering.h"
#include "scene/component/components.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <tuple>
#include <vector>

namespace owl::scene::systems {

namespace {

constexpr float g_minSpeed = 0.05f;
// The behaviours ask for a change of velocity; the agent makes it in a quarter of a second (then `maxAcceleration`).
constexpr float g_responseRate = 4.f;

using ai::SteeringBody;
using component::SteeringAgent;

struct Mover {
	Entity entity;
	SteeringAgent* agent = nullptr;
	math::vec3 position;
	math::vec3 velocity;
	uint64_t cell = 0;
};

// Cells of the neighbour grid: 21 bits per axis, enough for ±1e6 cells.
auto cellOf(const math::vec3& iPosition, const float iCellSize) -> std::array<int64_t, 3> {
	return {static_cast<int64_t>(std::floor(iPosition.x() / iCellSize)),
			static_cast<int64_t>(std::floor(iPosition.y() / iCellSize)),
			static_cast<int64_t>(std::floor(iPosition.z() / iCellSize))};
}

auto cellKey(const std::array<int64_t, 3>& iCell) -> uint64_t {
	constexpr int64_t bias = 1 << 20;
	constexpr uint64_t mask = (1u << 21u) - 1u;
	return (static_cast<uint64_t>(iCell[0] + bias) & mask) << 42u |
		   (static_cast<uint64_t>(iCell[1] + bias) & mask) << 21u | (static_cast<uint64_t>(iCell[2] + bias) & mask);
}

// Velocity of an entity: its own steering or navigation velocity, else read from its moves between two ticks.
auto targetVelocity(SteeringAgent& ioAgent, const Entity& iTarget, const math::vec3& iPosition, const float iSeconds)
		-> math::vec3 {
	math::vec3 velocity{0.f, 0.f, 0.f};
	if (iTarget.hasComponent<SteeringAgent>()) {
		velocity = iTarget.getComponent<SteeringAgent>().m_velocity;
	} else if (iTarget.hasComponent<component::NavAgent>()) {
		const auto& nav = iTarget.getComponent<component::NavAgent>().m_velocity;
		velocity = {nav.x(), 0.f, nav.y()};
	} else if (ioAgent.m_targetTracked == iTarget.getUUID() && iSeconds > 0.f) {
		velocity = (iPosition - ioAgent.m_targetLast) * (1.f / iSeconds);
	}
	ioAgent.m_targetLast = iPosition;
	ioAgent.m_targetTracked = iTarget.getUUID();
	return velocity;
}

// A body seen from an agent: its offset from the agent laid on the plane of the agent.
auto seen(const Mover& iAgent, const math::vec3& iPosition, const math::vec3& iVelocity, const float iRadius)
		-> SteeringBody {
	const auto plane = iAgent.agent->plane;
	return {.position = iAgent.position + ai::projectOnPlane(iPosition - iAgent.position, plane),
			.velocity = ai::projectOnPlane(iVelocity, plane),
			.radius = iRadius};
}

// The world point of the formation slot and the velocity of the leader, when the leader exists.
auto formationSlot(const Scene& iScene, const SteeringAgent& iAgent)
		-> std::optional<std::tuple<math::vec3, math::vec3>> {
	if (iAgent.leader == core::UUID{0})
		return std::nullopt;
	const Entity leader = iScene.findEntityByUUID(iAgent.leader);
	if (!leader)
		return std::nullopt;
	const math::mat4 world = iScene.getWorldTransform(leader)();
	// The slot follows the turn of the leader, not its scale.
	std::array<math::vec3, 3> axes;
	for (size_t column = 0; column < axes.size(); ++column)
		axes[column] = math::vec3{world(0, column), world(1, column), world(2, column)}.normalized();
	const math::vec3 origin{world(0, 3), world(1, 3), world(2, 3)};
	const math::vec3 point = origin + axes[0] * iAgent.slot.x() + axes[1] * iAgent.slot.y() + axes[2] * iAgent.slot.z();
	math::vec3 velocity{0.f, 0.f, 0.f};
	if (leader.hasComponent<SteeringAgent>())
		velocity = leader.getComponent<SteeringAgent>().m_velocity;
	else if (leader.hasComponent<component::NavAgent>()) {
		const auto& nav = leader.getComponent<component::NavAgent>().m_velocity;
		velocity = {nav.x(), 0.f, nav.y()};
	}
	return std::tuple{point, velocity};
}

void turn(const Entity& iEntity, const SteeringAgent& iAgent, const math::vec3& iVelocity) {
	const float speed = iVelocity.norm();
	if (!iAgent.turn || speed <= g_minSpeed)
		return;
	auto& rotation = iEntity.getComponent<component::Transform>().transform.rotation();
	switch (iAgent.plane) {
		case ai::SteeringPlane::Plane2D:
			rotation.z() = math::atan2(iVelocity.y(), iVelocity.x());
			break;
		case ai::SteeringPlane::Ground:
			rotation.y() = math::atan2(iVelocity.x(), iVelocity.z());
			break;
		case ai::SteeringPlane::Space:
			rotation.y() = math::atan2(iVelocity.x(), iVelocity.z());
			rotation.x() = -math::asin(std::clamp(iVelocity.y() / speed, -1.f, 1.f));
			rotation.z() = 0.f;
			break;
	}
}

class Steering {
public:
	explicit Steering(Scene& ioScene, const float iSeconds) : m_scene{ioScene}, m_seconds{iSeconds} {}

	void gather() {
		std::vector<std::tuple<uint64_t, entt::entity>> sorted;
		for (const auto handle: m_scene.registry.view<SteeringAgent, component::Transform, component::ID>()) {
			if (!m_scene.registry.get<SteeringAgent>(handle).enabled || isSimulationDemoted(m_scene, handle))
				continue;
			sorted.emplace_back(static_cast<uint64_t>(m_scene.registry.get<component::ID>(handle).id), handle);
		}
		std::ranges::sort(sorted);
		m_movers.reserve(sorted.size());
		for (const auto& [uuid, handle]: sorted) {
			const Entity entity{handle, &m_scene};
			auto& agent = entity.getComponent<SteeringAgent>();
			m_movers.push_back({.entity = entity,
								.agent = &agent,
								.position = getWorldPosition(m_scene, entity),
								.velocity = agent.m_velocity,
								.cell = 0});
			const float reach =
					std::max(agent.neighbourRadius, 2.f * agent.radius + 2.f * agent.maxSpeed * agent.lookAhead);
			m_cellSize = std::max(m_cellSize, reach);
		}
		for (auto& mover: m_movers) mover.cell = cellKey(cellOf(mover.position, m_cellSize));
		m_grid.reserve(m_movers.size());
		for (uint32_t index = 0; index < m_movers.size(); ++index) m_grid.emplace_back(m_movers[index].cell, index);
		std::ranges::sort(m_grid);
		for (const auto view = m_scene.registry.view<component::NavObstacle, component::Transform>();
			 const auto handle: view) {
			const Entity entity{handle, &m_scene};
			const auto& obstacle = view.get<component::NavObstacle>(handle);
			m_obstacles.push_back({.position = getWorldPosition(m_scene, entity),
								   .velocity = {obstacle.m_velocity.x(), 0.f, obstacle.m_velocity.y()},
								   .radius = obstacle.radius});
		}
		// Obstacles are few and the view order is not the UUID order: sort them so the sums stay bit-exact.
		std::ranges::sort(m_obstacles, [](const SteeringBody& iLeft, const SteeringBody& iRight) -> bool {
			return std::tuple{iLeft.position.x(), iLeft.position.y(), iLeft.position.z(), iLeft.radius} <
				   std::tuple{iRight.position.x(), iRight.position.y(), iRight.position.z(), iRight.radius};
		});
	}

	// New velocity of every mover, read from the positions and velocities of the last tick only.
	auto steer() -> std::vector<math::vec3> {
		std::vector<math::vec3> velocities;
		velocities.reserve(m_movers.size());
		auto& random = m_scene.getRandomStream("owl.steering");
		for (const auto& mover: m_movers) velocities.push_back(steerOne(mover, random));
		return velocities;
	}

	void move(const std::vector<math::vec3>& iVelocities) const {
		for (size_t index = 0; index < m_movers.size(); ++index) {
			const auto& mover = m_movers[index];
			mover.agent->m_velocity = iVelocities[index];
			setWorldPosition(m_scene, mover.entity, mover.position + iVelocities[index] * m_seconds);
			turn(mover.entity, *mover.agent, iVelocities[index]);
		}
	}

private:
	auto steerOne(const Mover& iMover, math::RandomStream& ioRandom) -> math::vec3 {
		auto& agent = *iMover.agent;
		const auto& weights = agent.weights;
		const float speed = agent.maxSpeed;
		const SteeringBody self{.position = iMover.position, .velocity = iMover.velocity, .radius = agent.radius};
		collectAround(iMover);

		math::vec3 force{0.f, 0.f, 0.f};
		if (weights.seek > 0.f || weights.flee > 0.f || weights.arrive > 0.f || weights.pursue > 0.f ||
			weights.evade > 0.f) {
			const SteeringBody quarry = targetOf(iMover);
			if (weights.seek > 0.f)
				force += ai::seek(self, quarry.position, speed) * weights.seek;
			if (weights.flee > 0.f)
				force += ai::flee(self, quarry.position, speed, agent.panicDistance) * weights.flee;
			if (weights.arrive > 0.f)
				force += ai::arrive(self, quarry.position, speed, agent.slowRadius) * weights.arrive;
			if (weights.pursue > 0.f)
				force += ai::pursue(self, quarry, speed, agent.prediction) * weights.pursue;
			if (weights.evade > 0.f)
				force += ai::evade(self, quarry, speed, agent.prediction, agent.panicDistance) * weights.evade;
		}
		if (weights.wander > 0.f)
			force += ai::wander(self, agent.m_wander, agent.wanderSettings, agent.plane, speed, m_seconds, ioRandom) *
					 weights.wander;
		if (weights.separation > 0.f)
			force += ai::separation(self, m_flock, agent.separationDistance, speed) * weights.separation;
		if (weights.alignment > 0.f)
			force += ai::alignment(self, m_flock, speed) * weights.alignment;
		if (weights.cohesion > 0.f)
			force += ai::cohesion(self, m_flock, speed) * weights.cohesion;
		if (weights.avoidance > 0.f)
			force += ai::avoidVolumes(self, m_volumes, agent.lookAhead, speed) * weights.avoidance;
		if (weights.formation > 0.f) {
			if (const auto slot = formationSlot(m_scene, agent); slot.has_value()) {
				const auto& [point, leaderVelocity] = *slot;
				const SteeringBody onPlane = seen(iMover, point, leaderVelocity, 0.f);
				force += (ai::arrive(self, onPlane.position, speed, agent.slowRadius) + onPlane.velocity) *
						 weights.formation;
			}
		}
		force = ai::truncate(ai::projectOnPlane(force * g_responseRate, agent.plane), agent.maxAcceleration);
		math::vec3 velocity = ai::truncate(iMover.velocity + force * m_seconds, speed);
		velocity = ai::projectOnPlane(velocity, agent.plane);
		if (velocity.norm() < g_minSpeed && force.norm() < g_minSpeed)
			velocity = {0.f, 0.f, 0.f};
		return velocity;
	}

	[[nodiscard]] auto targetOf(const Mover& iMover) const -> SteeringBody {
		auto& agent = *iMover.agent;
		if (agent.target != core::UUID{0}) {
			if (const Entity target = m_scene.findEntityByUUID(agent.target); target) {
				const math::vec3 position = getWorldPosition(m_scene, target);
				const math::vec3 velocity = targetVelocity(agent, target, position, m_seconds);
				return seen(iMover, position, velocity, 0.f);
			}
		}
		return seen(iMover, agent.targetPoint, {0.f, 0.f, 0.f}, 0.f);
	}

	// The flock members and the volumes around a mover, from the 27 cells around its own.
	void collectAround(const Mover& iMover) {
		m_flock.clear();
		m_volumes.clear();
		const auto& agent = *iMover.agent;
		const auto centre = cellOf(iMover.position, m_cellSize);
		m_nearby.clear();
		for (int64_t dx = -1; dx <= 1; ++dx) {
			for (int64_t dy = -1; dy <= 1; ++dy) {
				for (int64_t dz = -1; dz <= 1; ++dz) {
					const uint64_t key = cellKey({centre[0] + dx, centre[1] + dy, centre[2] + dz});
					const auto first = std::ranges::lower_bound(m_grid, std::tuple{key, uint32_t{0}});
					for (auto it = first; it != m_grid.end() && std::get<0>(*it) == key; ++it)
						m_nearby.push_back(std::get<1>(*it));
				}
			}
		}
		// The movers in UUID order whatever the cell they sit in: the sums stay the same on every target.
		std::ranges::sort(m_nearby);
		for (const auto index: m_nearby) {
			const auto& other = m_movers[index];
			if (other.entity == iMover.entity)
				continue;
			const SteeringBody body = seen(iMover, other.position, other.velocity, other.agent->radius);
			m_volumes.push_back(body);
			if (other.agent->flock == agent.flock && (body.position - iMover.position).norm() < agent.neighbourRadius)
				m_flock.push_back(body);
		}
		for (const auto& obstacle: m_obstacles)
			m_volumes.push_back(seen(iMover, obstacle.position, obstacle.velocity, obstacle.radius));
	}

	// The scene.
	Scene& m_scene;
	// Duration of the tick.
	float m_seconds;
	// The agents that move this tick, in UUID order.
	std::vector<Mover> m_movers;
	// Side of a cell of the neighbour grid: the longest reach of an agent.
	float m_cellSize = 1.f;
	// Cell and index of every mover, sorted.
	std::vector<std::tuple<uint64_t, uint32_t>> m_grid;
	// `NavObstacle`s as volumes.
	std::vector<SteeringBody> m_obstacles;
	// Movers in the cells around the current one.
	std::vector<uint32_t> m_nearby;
	// Flock members of the current mover.
	std::vector<SteeringBody> m_flock;
	// Volumes around the current mover.
	std::vector<SteeringBody> m_volumes;
};

}// namespace

void tickSteeringAgents(Scene& ioScene, const SystemContext& iContext) {
	OWL_PROFILE_FUNCTION()

	const float seconds = iContext.timeStep.getSeconds();
	if (seconds <= 0.f)
		return;
	const auto view = ioScene.registry.view<SteeringAgent>();
	if (view.begin() == view.end())
		return;
	Steering steering{ioScene, seconds};
	steering.gather();
	steering.move(steering.steer());
}

}// namespace owl::scene::systems
