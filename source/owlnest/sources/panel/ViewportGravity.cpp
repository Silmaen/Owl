/**
 * @file ViewportGravity.cpp
 * @author Silmaen
 * @date 06/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "EditorLayer.h"
#include "Viewport.h"

#include <physics/PhysicCommand.h>
#include <renderer/Renderer2D.h>
#include <scene/component/GravitySource.h>
#include <scene/component/PhysicBody.h>
#include <scene/component/SimulationLod.h>

#include <cmath>
#include <cstdint>
#include <numbers>

namespace owl::nest::panel {

namespace {

// Ticks of the predicted path drawn for the selected body.
constexpr uint32_t g_PathSteps = 600;
// Segments of a drawn circle.
constexpr uint32_t g_RingSegments = 64;

// A circle in the XY plane, around the entity.
void drawRing(const math::vec3& iCentre, const float iRadius, const math::vec4& iColor, const int iEntityId) {
	if (iRadius <= 0.f)
		return;
	math::vec3 previous{iCentre.x() + iRadius, iCentre.y(), iCentre.z()};
	for (uint32_t segment = 1; segment <= g_RingSegments; ++segment) {
		const float angle =
				2.f * std::numbers::pi_v<float> * static_cast<float>(segment) / static_cast<float>(g_RingSegments);
		const math::vec3 next{iCentre.x() + iRadius * std::cos(angle), iCentre.y() + iRadius * std::sin(angle),
							  iCentre.z()};
		renderer::Renderer2D::drawLine({.point1 = previous, .point2 = next, .color = iColor, .entityId = iEntityId});
		previous = next;
	}
}

}// namespace

void Viewport::renderGravityOverlay(const shared<scene::Scene>& iScene) const {
	if (m_parent == nullptr)
		return;
	const scene::Entity selected = m_parent->getSelectedEntity();
	if (!selected)
		return;
	const auto id = static_cast<int>(static_cast<uint32_t>(selected));
	const math::vec3 centre = iScene->getWorldTransform(selected).translation();
	constexpr math::vec4 surfaceColor{0.55f, 0.4f, 1.f, 1.f};
	constexpr math::vec4 reachColor{0.55f, 0.4f, 1.f, 0.45f};
	constexpr math::vec4 pathColor{1.f, 0.85f, 0.3f, 1.f};
	constexpr math::vec4 lodColor{0.3f, 0.9f, 0.6f, 0.8f};
	if (selected.hasComponent<scene::component::GravitySource>()) {
		const auto& source = selected.getComponent<scene::component::GravitySource>();
		if (source.type == scene::component::GravitySource::Type::Uniform) {
			const float length = source.direction.norm();
			if (length > 0.f)
				renderer::Renderer2D::drawLine({.point1 = centre,
												.point2 = centre + source.direction * (2.f / length),
												.color = surfaceColor,
												.entityId = id});
		} else
			drawRing(centre, source.radius, surfaceColor, id);
		drawRing(centre, source.range, reachColor, id);
	}
	if (selected.hasComponent<scene::component::SimulationLod>()) {
		const auto& lod = selected.getComponent<scene::component::SimulationLod>();
		drawRing(centre, lod.radius, lodColor, id);
		drawRing(centre, lod.radius + lod.hysteresis, lodColor * 0.6f, id);
	}
	if (!selected.hasComponent<scene::component::PhysicBody>())
		return;
	const auto path = physics::PhysicCommand::predictTrajectory(selected, g_PathSteps);
	math::vec3 previous = centre;
	for (const auto& point: path) {
		renderer::Renderer2D::drawLine({.point1 = previous, .point2 = point, .color = pathColor, .entityId = id});
		previous = point;
	}
}

}// namespace owl::nest::panel
