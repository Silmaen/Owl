/**
 * @file Steering.cpp
 * @author Silmaen
 * @date 10/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "scene/ai/Steering.h"

#include "math/elementaryFunctions.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <span>

namespace owl::scene::ai {

namespace {

constexpr float g_epsilon = 1e-6f;

auto directionOf(const math::vec3& iVector, const math::vec3& iFallback) -> math::vec3 {
	const float length = iVector.norm();
	return length > g_epsilon ? iVector * (1.f / length) : iFallback;
}

// The heading of the agent: its velocity, else +x.
auto headingOf(const SteeringBody& iAgent) -> math::vec3 { return directionOf(iAgent.velocity, {1.f, 0.f, 0.f}); }

// A direction perpendicular to the heading, on its right on the planes, under the heading in space.
auto sideOf(const math::vec3& iHeading) -> math::vec3 {
	if (std::abs(iHeading.y()) < 1.f - 1e-3f)
		return directionOf(iHeading ^ math::vec3{0.f, 1.f, 0.f}, {0.f, 0.f, 1.f});
	return {1.f, 0.f, 0.f};
}

}// namespace

auto projectOnPlane(const math::vec3& iVector, const SteeringPlane iPlane) -> math::vec3 {
	switch (iPlane) {
		case SteeringPlane::Plane2D:
			return {iVector.x(), iVector.y(), 0.f};
		case SteeringPlane::Ground:
			return {iVector.x(), 0.f, iVector.z()};
		case SteeringPlane::Space:
			break;
	}
	return iVector;
}

auto truncate(const math::vec3& iVector, const float iMax) -> math::vec3 {
	const float length = iVector.norm();
	if (length <= iMax || length <= g_epsilon)
		return iVector;
	return iVector * (iMax / length);
}

auto seek(const SteeringBody& iAgent, const math::vec3& iTarget, const float iMaxSpeed) -> math::vec3 {
	const math::vec3 offset = iTarget - iAgent.position;
	const float distance = offset.norm();
	if (distance <= g_epsilon)
		return -iAgent.velocity;
	return offset * (iMaxSpeed / distance) - iAgent.velocity;
}

auto flee(const SteeringBody& iAgent, const math::vec3& iThreat, const float iMaxSpeed, const float iPanicDistance)
		-> math::vec3 {
	const math::vec3 offset = iAgent.position - iThreat;
	const float distance = offset.norm();
	if (iPanicDistance > 0.f && distance > iPanicDistance)
		return {0.f, 0.f, 0.f};
	const math::vec3 away = directionOf(offset, -headingOf(iAgent));
	return away * iMaxSpeed - iAgent.velocity;
}

auto arrive(const SteeringBody& iAgent, const math::vec3& iTarget, const float iMaxSpeed, const float iSlowRadius)
		-> math::vec3 {
	const math::vec3 offset = iTarget - iAgent.position;
	const float distance = offset.norm();
	if (distance <= g_epsilon)
		return -iAgent.velocity;
	const float speed =
			iSlowRadius > g_epsilon && distance < iSlowRadius ? iMaxSpeed * distance / iSlowRadius : iMaxSpeed;
	return offset * (speed / distance) - iAgent.velocity;
}

auto pursue(const SteeringBody& iAgent, const SteeringBody& iQuarry, const float iMaxSpeed, const float iMaxPrediction)
		-> math::vec3 {
	const float distance = (iQuarry.position - iAgent.position).norm();
	const float speed = iAgent.velocity.norm();
	float prediction = iMaxPrediction;
	if (speed > g_epsilon && distance / speed < iMaxPrediction)
		prediction = distance / speed;
	return seek(iAgent, iQuarry.position + iQuarry.velocity * prediction, iMaxSpeed);
}

auto evade(const SteeringBody& iAgent, const SteeringBody& iThreat, const float iMaxSpeed, const float iMaxPrediction,
		   const float iPanicDistance) -> math::vec3 {
	const float distance = (iThreat.position - iAgent.position).norm();
	if (iPanicDistance > 0.f && distance > iPanicDistance)
		return {0.f, 0.f, 0.f};
	const float speed = iThreat.velocity.norm();
	float prediction = iMaxPrediction;
	if (speed > g_epsilon && distance / speed < iMaxPrediction)
		prediction = distance / speed;
	return flee(iAgent, iThreat.position + iThreat.velocity * prediction, iMaxSpeed, 0.f);
}

auto wander(const SteeringBody& iAgent, WanderState& ioState, const WanderSettings& iSettings,
			const SteeringPlane iPlane, const float iMaxSpeed, const float iSeconds, math::RandomStream& ioRandom)
		-> math::vec3 {
	ioState.heading += ioRandom.nextRange(-1.f, 1.f) * iSettings.jitter * iSeconds;
	if (iPlane == SteeringPlane::Space)
		ioState.pitch = std::clamp(ioState.pitch + ioRandom.nextRange(-0.5f, 0.5f) * iSettings.jitter * iSeconds,
								   -std::numbers::pi_v<float> * 0.4f, std::numbers::pi_v<float> * 0.4f);
	constexpr float twoPi = 2.f * std::numbers::pi_v<float>;
	ioState.heading -= twoPi * std::floor(ioState.heading / twoPi);
	const float cosHeading = math::cos(ioState.heading);
	const float sinHeading = math::sin(ioState.heading);
	math::vec3 offset;
	switch (iPlane) {
		case SteeringPlane::Plane2D:
			offset = {cosHeading, sinHeading, 0.f};
			break;
		case SteeringPlane::Ground:
			offset = {cosHeading, 0.f, sinHeading};
			break;
		case SteeringPlane::Space:
			{
				const float cosPitch = math::cos(ioState.pitch);
				offset = {cosPitch * cosHeading, math::sin(ioState.pitch), cosPitch * sinHeading};
				break;
			}
	}
	const math::vec3 ahead = projectOnPlane(headingOf(iAgent), iPlane);
	const math::vec3 centre = iAgent.position + directionOf(ahead, offset) * iSettings.distance;
	return seek(iAgent, centre + offset * iSettings.radius, iMaxSpeed);
}

auto separation(const SteeringBody& iAgent, const std::span<const SteeringBody> iNeighbours, const float iDistance,
				const float iMaxSpeed) -> math::vec3 {
	math::vec3 push{0.f, 0.f, 0.f};
	bool pushed = false;
	for (const auto& neighbour: iNeighbours) {
		const math::vec3 offset = iAgent.position - neighbour.position;
		const float centres = offset.norm();
		const float gap = centres - iAgent.radius - neighbour.radius;
		if (gap >= iDistance)
			continue;
		const float strength = iDistance > g_epsilon ? std::min(1.f, (iDistance - gap) / iDistance) : 1.f;
		push += directionOf(offset, -sideOf(headingOf(iAgent))) * strength;
		pushed = true;
	}
	if (!pushed)
		return {0.f, 0.f, 0.f};
	return truncate(push, 1.f) * iMaxSpeed - iAgent.velocity;
}

auto alignment(const SteeringBody& iAgent, const std::span<const SteeringBody> iNeighbours, const float iMaxSpeed)
		-> math::vec3 {
	if (iNeighbours.empty())
		return {0.f, 0.f, 0.f};
	math::vec3 heading{0.f, 0.f, 0.f};
	for (const auto& neighbour: iNeighbours) heading += neighbour.velocity;
	const float length = heading.norm();
	if (length <= g_epsilon)
		return {0.f, 0.f, 0.f};
	return heading * (iMaxSpeed / length) - iAgent.velocity;
}

auto cohesion(const SteeringBody& iAgent, const std::span<const SteeringBody> iNeighbours, const float iMaxSpeed)
		-> math::vec3 {
	if (iNeighbours.empty())
		return {0.f, 0.f, 0.f};
	math::vec3 centre{0.f, 0.f, 0.f};
	for (const auto& neighbour: iNeighbours) centre += neighbour.position;
	centre *= 1.f / static_cast<float>(iNeighbours.size());
	return seek(iAgent, centre, iMaxSpeed);
}

auto avoidVolumes(const SteeringBody& iAgent, const std::span<const SteeringBody> iVolumes, const float iLookAhead,
				  const float iMaxSpeed) -> math::vec3 {
	float firstTime = std::numeric_limits<float>::max();
	math::vec3 away{0.f, 0.f, 0.f};
	bool found = false;
	for (const auto& volume: iVolumes) {
		const math::vec3 offset = volume.position - iAgent.position;
		const math::vec3 relative = volume.velocity - iAgent.velocity;
		const float reach = iAgent.radius + volume.radius;
		if (offset.norm() < reach) {
			// Already touching: get out the shortest way, before anything else.
			const math::vec3 out = directionOf(-offset, sideOf(headingOf(iAgent)));
			return out * iMaxSpeed - iAgent.velocity;
		}
		const float closing = relative.normSq();
		if (closing <= g_epsilon)
			continue;
		const float time = -(offset * relative) / closing;
		if (time <= 0.f || time > iLookAhead || time >= firstTime)
			continue;
		const math::vec3 miss = offset + relative * time;
		if (miss.norm() >= reach)
			continue;
		firstTime = time;
		away = directionOf(-miss, sideOf(headingOf(iAgent)));
		found = true;
	}
	if (!found)
		return {0.f, 0.f, 0.f};
	const float urgency = iLookAhead > g_epsilon ? 1.f - firstTime / iLookAhead : 1.f;
	return away * (iMaxSpeed * (0.5f + 0.5f * urgency));
}

}// namespace owl::scene::ai
