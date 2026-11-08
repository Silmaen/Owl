/**
 * @file Avoidance.cpp
 * @author Silmaen
 * @date 08/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "scene/nav/Avoidance.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <span>
#include <vector>

namespace owl::scene::nav {

namespace {

constexpr float g_epsilon = 1e-5f;
// Share of its preferred speed under which a body is blocked.
constexpr float g_blockedShare = 0.25f;
// Turn of the preferred velocity of a blocked body, in radians.
constexpr float g_detourAngle = 0.6f;

// A half-plane of allowed velocities: left of `direction` through `point`.
struct Line {
	math::vec2 point;
	math::vec2 direction;
};

auto det(const math::vec2& iA, const math::vec2& iB) -> float { return iA.x() * iB.y() - iA.y() * iB.x(); }

auto dot(const math::vec2& iA, const math::vec2& iB) -> float { return iA.x() * iB.x() + iA.y() * iB.y(); }

// Best velocity on line `iLine` inside the speed disc and the previous lines; false when there is none.
auto solveOnLine(const std::span<const Line> iLines, const size_t iLine, const float iRadius,
				 const math::vec2& iPreferred, const bool iDirectionOpt, math::vec2& oResult) -> bool {
	const Line& line = iLines[iLine];
	const float dotProduct = dot(line.point, line.direction);
	const float discriminant = dotProduct * dotProduct + iRadius * iRadius - line.point.normSq();
	if (discriminant < 0.f)
		return false;
	const float root = std::sqrt(discriminant);
	float tLeft = -dotProduct - root;
	float tRight = -dotProduct + root;
	for (size_t index = 0; index < iLine; ++index) {
		const float denominator = det(line.direction, iLines[index].direction);
		const float numerator = det(iLines[index].direction, line.point - iLines[index].point);
		if (std::abs(denominator) <= g_epsilon) {
			if (numerator < 0.f)
				return false;
			continue;
		}
		const float t = numerator / denominator;
		if (denominator >= 0.f)
			tRight = std::min(tRight, t);
		else
			tLeft = std::max(tLeft, t);
		if (tLeft > tRight)
			return false;
	}
	if (iDirectionOpt) {
		oResult = line.point + line.direction * (dot(iPreferred, line.direction) > 0.f ? tRight : tLeft);
		return true;
	}
	const float t = std::clamp(dot(line.direction, iPreferred - line.point), tLeft, tRight);
	oResult = line.point + line.direction * t;
	return true;
}

// Velocity nearest to the preferred one inside the speed disc and every line; returns the first line that failed.
auto solve(const std::span<const Line> iLines, const float iRadius, const math::vec2& iPreferred,
		   const bool iDirectionOpt, math::vec2& oResult) -> size_t {
	if (iDirectionOpt) {
		oResult = iPreferred * iRadius;
	} else if (iPreferred.normSq() > iRadius * iRadius) {
		oResult = iPreferred.normalized() * iRadius;
	} else {
		oResult = iPreferred;
	}
	for (size_t index = 0; index < iLines.size(); ++index) {
		if (det(iLines[index].direction, iLines[index].point - oResult) <= 0.f)
			continue;
		const math::vec2 previous = oResult;
		if (!solveOnLine(iLines, index, iRadius, iPreferred, iDirectionOpt, oResult)) {
			oResult = previous;
			return index;
		}
	}
	return iLines.size();
}

// When no velocity satisfies every line: the one that least violates the lines from `iFailed` on.
void solveDense(const std::span<const Line> iLines, const size_t iFailed, const float iRadius, math::vec2& ioResult) {
	float distance = 0.f;
	std::vector<Line> projected;
	for (size_t index = iFailed; index < iLines.size(); ++index) {
		if (det(iLines[index].direction, iLines[index].point - ioResult) <= distance)
			continue;
		projected.clear();
		for (size_t other = 0; other < index; ++other) {
			Line line;
			const float determinant = det(iLines[index].direction, iLines[other].direction);
			if (std::abs(determinant) <= g_epsilon) {
				if (dot(iLines[index].direction, iLines[other].direction) > 0.f)
					continue;
				line.point = (iLines[index].point + iLines[other].point) * 0.5f;
			} else {
				line.point =
						iLines[index].point +
						iLines[index].direction *
								(det(iLines[other].direction, iLines[index].point - iLines[other].point) / determinant);
			}
			line.direction = (iLines[other].direction - iLines[index].direction).normalized();
			projected.push_back(line);
		}
		const math::vec2 previous = ioResult;
		const math::vec2 normal{-iLines[index].direction.y(), iLines[index].direction.x()};
		if (solve(projected, iRadius, normal, true, ioResult) < projected.size())
			ioResult = previous;
		distance = det(iLines[index].direction, iLines[index].point - ioResult);
	}
}

auto solveVelocity(const std::span<const Line> iLines, const float iMaxSpeed, const math::vec2& iPreferred)
		-> math::vec2 {
	math::vec2 result;
	if (const size_t failed = solve(iLines, iMaxSpeed, iPreferred, false, result); failed < iLines.size())
		solveDense(iLines, failed, iMaxSpeed, result);
	return result;
}

auto rotate(const math::vec2& iVector, const float iAngle) -> math::vec2 {
	const float cosine = std::cos(iAngle);
	const float sine = std::sin(iAngle);
	return {iVector.x() * cosine - iVector.y() * sine, iVector.x() * sine + iVector.y() * cosine};
}

auto orcaLine(const AvoidanceBody& iBody, const AvoidanceBody& iOther, const float iInvHorizon,
			  const float iInvTimeStep) -> Line {
	const math::vec2 relativePosition = iOther.position - iBody.position;
	const math::vec2 relativeVelocity = iBody.velocity - iOther.velocity;
	const float distSq = relativePosition.normSq();
	const float combinedRadius = iBody.radius + iOther.radius;
	const float combinedRadiusSq = combinedRadius * combinedRadius;
	Line line;
	math::vec2 correction;
	if (distSq > combinedRadiusSq) {
		const math::vec2 w = relativeVelocity - relativePosition * iInvHorizon;
		const float wLengthSq = w.normSq();
		const float dotProduct = dot(w, relativePosition);
		if (dotProduct < 0.f && dotProduct * dotProduct > combinedRadiusSq * wLengthSq) {
			const float wLength = std::sqrt(wLengthSq);
			const math::vec2 unitW = wLength > g_epsilon ? w / wLength : math::vec2{1.f, 0.f};
			line.direction = {unitW.y(), -unitW.x()};
			correction = unitW * (combinedRadius * iInvHorizon - wLength);
		} else {
			const float leg = std::sqrt(distSq - combinedRadiusSq);
			if (det(relativePosition, w) > 0.f) {
				line.direction = math::vec2{relativePosition.x() * leg - relativePosition.y() * combinedRadius,
											relativePosition.x() * combinedRadius + relativePosition.y() * leg} /
								 distSq;
			} else {
				line.direction = math::vec2{relativePosition.x() * leg + relativePosition.y() * combinedRadius,
											-relativePosition.x() * combinedRadius + relativePosition.y() * leg} /
								 -distSq;
			}
			correction = line.direction * dot(relativeVelocity, line.direction) - relativeVelocity;
		}
	} else {
		const math::vec2 w = relativeVelocity - relativePosition * iInvTimeStep;
		const float wLength = w.norm();
		const math::vec2 unitW = wLength > g_epsilon ? w / wLength : math::vec2{1.f, 0.f};
		line.direction = {unitW.y(), -unitW.x()};
		correction = unitW * (combinedRadius * iInvTimeStep - wLength);
	}
	const float share = iOther.responsive ? 0.5f : 1.f;
	line.point = iBody.velocity + correction * share;
	return line;
}

}// namespace

auto computeAvoidance(const std::span<const AvoidanceBody> iBodies, const AvoidanceSettings& iSettings,
					  const float iTimeStep) -> std::vector<math::vec2> {
	OWL_PROFILE_FUNCTION()

	std::vector<math::vec2> velocities(iBodies.size());
	if (iBodies.empty())
		return velocities;
	const float invHorizon = 1.f / std::max(iSettings.timeHorizon, g_epsilon);
	const float invTimeStep = 1.f / std::max(iTimeStep, g_epsilon);
	std::vector<Line> lines;
	for (size_t index = 0; index < iBodies.size(); ++index) {
		const auto& body = iBodies[index];
		if (!body.responsive) {
			velocities[index] = body.velocity;
			continue;
		}
		lines.clear();
		for (size_t other = 0; other < iBodies.size(); ++other) {
			if (other == index)
				continue;
			const float reach = iSettings.neighbourDistance + iBodies[other].radius;
			if ((iBodies[other].position - body.position).normSq() > reach * reach)
				continue;
			lines.push_back(orcaLine(body, iBodies[other], invHorizon, invTimeStep));
		}
		math::vec2 result = solveVelocity(lines, body.maxSpeed, body.preferred);
		// Two bodies face to face block each other: the blocked one tries again bearing right, as both do.
		if (result.normSq() < body.preferred.normSq() * g_blockedShare * g_blockedShare)
			result = solveVelocity(lines, body.maxSpeed, rotate(body.preferred, -g_detourAngle));
		velocities[index] = result;
	}
	return velocities;
}

}// namespace owl::scene::nav
