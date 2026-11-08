/**
 * @file GravityField.cpp
 * @author Silmaen
 * @date 06/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "GravityField.h"

#include "scene/Entity.h"
#include "scene/Scene.h"
#include "scene/component/ID.h"

#include <algorithm>
#include <cstdint>

namespace owl::physics {

namespace {

auto toDouble(const math::vec3f& iVector) -> math::vec3d {
	return {static_cast<double>(iVector.x()), static_cast<double>(iVector.y()), static_cast<double>(iVector.z())};
}

}// namespace

void gatherFieldSources(const scene::Scene& iScene, const std::function<math::vec3f(core::UUID)>& iVelocityOf,
						std::vector<FieldSource>& oSources) {
	oSources.clear();
	// The world transform reads the parents through the scene; nothing is written.
	auto& scene = const_cast<scene::Scene&>(iScene);
	for (const auto [handle, source, id]:
		 scene.registry.view<scene::component::GravitySource, scene::component::ID>().each()) {
		if (!source.enabled)
			continue;
		oSources.push_back({.owner = id.id,
							.position = scene.getWorldTransform(scene::Entity{handle, &scene}).translation(),
							.velocity = iVelocityOf ? iVelocityOf(id.id) : math::vec3f{0.f, 0.f, 0.f},
							.field = source});
	}
	std::ranges::sort(oSources, {},
					  [](const FieldSource& iSource) -> uint64_t { return static_cast<uint64_t>(iSource.owner); });
}

auto fieldAcceleration(const std::span<const FieldSource> iSources, const math::vec3f& iPoint,
					   const core::UUID iExclude) -> math::vec3f {
	math::vec3f total{0.f, 0.f, 0.f};
	for (const auto& source: iSources)
		if (source.owner != iExclude)
			total += source.field.getAcceleration(iPoint - source.position);
	return total;
}

auto railsArc(const std::span<const FieldSource> iSources, const math::vec3f& iWorldGravity, const math::vec3f& iPoint,
			  const core::UUID iExclude, const float iScale, const bool iFields) -> RailsArc {
	math::vec3f uniform = iWorldGravity;
	std::optional<size_t> strongest;
	float strongestPull = 0.f;
	for (size_t index = 0; iFields && index < iSources.size(); ++index) {
		const auto& source = iSources[index];
		if (source.owner == iExclude)
			continue;
		const math::vec3f pull = source.field.getAcceleration(iPoint - source.position);
		if (source.field.type == scene::component::GravitySource::Type::Uniform) {
			uniform += pull;
			continue;
		}
		if (const float magnitude = pull.normSq(); magnitude > strongestPull) {
			strongestPull = magnitude;
			strongest = index;
		}
	}
	const auto scale = static_cast<double>(iScale);
	if (strongest && strongestPull > uniform.normSq())
		return {.source = strongest,
				.acceleration = {0.0, 0.0, 0.0},
				.gravitationalParameter = iSources[*strongest].field.getGravitationalParameter() * scale};
	return {.source = std::nullopt, .acceleration = toDouble(uniform) * scale, .gravitationalParameter = 0.0};
}

auto followArc(const RailsArc& iArc, const std::span<const FieldSource> iSources, const math::OrbitState& iRelative,
			   const double iSeconds) -> math::OrbitState {
	if (!iArc.source)
		return math::propagateUniform(iRelative, iArc.acceleration, iSeconds);
	const auto surface = static_cast<double>(
			std::max(iSources[*iArc.source].field.radius, scene::component::GravitySource::minRadius));
	// Under the surface the conic means nothing: the body rests where it is.
	if (iRelative.position.normSq() < surface * surface)
		return {.position = iRelative.position, .velocity = {0.0, 0.0, 0.0}};
	auto moved = math::propagateKepler(iRelative, iArc.gravitationalParameter, iSeconds);
	if (const double distance = moved.position.norm(); distance < surface && distance > 0.0)
		moved = {.position = moved.position * (surface / distance), .velocity = {0.0, 0.0, 0.0}};
	return moved;
}

auto predictPath(const std::span<const FieldSource> iSources, const PathSettings& iSettings,
				 const math::vec3f& iPosition, const math::vec3f& iVelocity, const uint32_t iSteps)
		-> std::vector<math::vec3f> {
	std::vector<math::vec3f> path;
	path.reserve(iSteps);
	const uint32_t subSteps = std::max(iSettings.subSteps, 1u);
	const float subSeconds = iSettings.stepSeconds / static_cast<float>(subSteps);
	math::vec3f position = iPosition;
	math::vec3f velocity = iVelocity;
	const auto flatten = [&iSettings](math::vec3f& ioVector) -> void {
		if (iSettings.plane)
			ioVector.z() = 0.f;
	};
	flatten(velocity);
	for (uint32_t step = 0; step < iSteps; ++step) {
		// The solver reads the fields once per step and keeps the force over its collision steps.
		math::vec3f acceleration = iSettings.worldGravity;
		if (iSettings.fields)
			acceleration += fieldAcceleration(iSources, position, iSettings.self);
		acceleration *= iSettings.gravityScale;
		flatten(acceleration);
		for (uint32_t sub = 0; sub < subSteps; ++sub) {
			velocity += acceleration * subSeconds;
			position += velocity * subSeconds;
		}
		path.push_back(position);
	}
	return path;
}

}// namespace owl::physics
