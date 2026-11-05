/**
 * @file ViewportJoint.cpp
 * @author Silmaen
 * @date 05/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "EditorLayer.h"
#include "Viewport.h"
#include "document/SceneDocument.h"

#include <renderer/Renderer2D.h>
#include <scene/component/PhysicJoint.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <utility>

namespace owl::nest::panel {

namespace {

auto toWorld(const math::mat4& iWorld, const math::vec3& iPoint, const float iW) -> math::vec3 {
	const math::vec4 point = iWorld * math::vec4{iPoint.x(), iPoint.y(), iPoint.z(), iW};
	return {point.x(), point.y(), point.z()};
}

void drawCross(const math::vec3& iCenter, const float iSize, const math::vec4& iColor, const int iEntityId) {
	for (const auto& offset:
		 std::array{math::vec3{iSize, 0.f, 0.f}, math::vec3{0.f, iSize, 0.f}, math::vec3{0.f, 0.f, iSize}})
		renderer::Renderer2D::drawLine(
				{.point1 = iCenter - offset, .point2 = iCenter + offset, .color = iColor, .entityId = iEntityId});
}

}// namespace

void Viewport::renderJointOverlay(const shared<scene::Scene>& iScene) const {
	if (m_parent == nullptr || mp_document == nullptr || mp_document->state() != SceneDocument::State::Edit)
		return;
	const scene::Entity selected = m_parent->getSelectedEntity();
	if (!selected || !selected.hasComponent<scene::component::PhysicJoint>())
		return;
	const auto& joint = selected.getComponent<scene::component::PhysicJoint>().joint;
	const auto id = static_cast<int>(static_cast<uint32_t>(selected));
	const math::mat4 world = iScene->getWorldTransform(selected)();
	constexpr math::vec4 anchorColor{1.f, 0.8f, 0.25f, 1.f};
	constexpr math::vec4 linkColor{0.4f, 0.8f, 1.f, 1.f};
	constexpr math::vec4 axisColor{1.f, 0.4f, 0.4f, 1.f};
	const math::vec3 anchor = toWorld(world, joint.anchor, 1.f);
	const float size = (m_isometric ? 0.15f : getLightMarkerRadius(anchor)) * 0.6f;
	drawCross(anchor, size, anchorColor, id);
	// The joint pulls towards the connected body: its anchor for a distance joint, its origin otherwise.
	math::vec3 target = joint.type == scene::SceneJoint::Type::Distance ? joint.connectedAnchor : anchor;
	if (const scene::Entity other = iScene->findEntityByUUID(joint.connected); other) {
		const math::mat4 otherWorld = iScene->getWorldTransform(other)();
		target = toWorld(otherWorld,
						 joint.type == scene::SceneJoint::Type::Distance ? joint.connectedAnchor
																		 : math::vec3{0.f, 0.f, 0.f},
						 1.f);
	}
	renderer::Renderer2D::drawLine({.point1 = anchor, .point2 = target, .color = linkColor, .entityId = id});
	drawCross(target, size * 0.6f, linkColor, id);
	if (joint.type != scene::SceneJoint::Type::Hinge && joint.type != scene::SceneJoint::Type::Slider)
		return;
	math::vec3 axis = toWorld(world, joint.axis, 0.f);
	if (const float length = std::sqrt(axis * axis); length > 0.f)
		axis = axis * (size * 4.f / length);
	renderer::Renderer2D::drawLine(
			{.point1 = anchor - axis, .point2 = anchor + axis, .color = axisColor, .entityId = id});
}

}// namespace owl::nest::panel
