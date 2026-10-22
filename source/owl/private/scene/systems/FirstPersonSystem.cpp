/**
 * @file FirstPersonSystem.cpp
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "EngineSystems.h"

#include "app/Application.h"
#include "data/geometry/BoxCollision.h"
#include "input/ActionFrame.h"
#include "input/Input.h"
#include "scene/Entity.h"
#include "scene/Scene.h"
#include "scene/component/components.h"
#include "window/Window.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace owl::scene::systems {

namespace {

void turnView(component::FirstPersonController& ioController, math::Transform& ioTransform,
			  const input::ActionFrame& iActions, const float iDt, const bool iCursorCaptured) {
	if (iCursorCaptured) {
		const math::vec2 mouse = input::Input::getMousePos();
		if (ioController.m_mouseValid) {
			ioController.m_yaw -= (mouse.x() - ioController.m_lastMouse.x()) * ioController.mouseSensitivity;
			ioController.m_pitch -= (mouse.y() - ioController.m_lastMouse.y()) * ioController.mouseSensitivity;
		}
		ioController.m_lastMouse = mouse;
		ioController.m_mouseValid = true;
	} else {
		ioController.m_mouseValid = false;
	}
	const math::vec2 look = iActions.getVector("look");
	ioController.m_yaw -= look.x() * ioController.lookSpeed * iDt;
	ioController.m_pitch -= look.y() * ioController.lookSpeed * iDt;
	ioController.m_pitch = std::clamp(ioController.m_pitch, -1.5f, 1.5f);
	ioTransform.rotation() = math::vec3{ioController.m_pitch, ioController.m_yaw, 0.f};
}

void gatherObstacles(const Scene& iScene, const math::box3f& iReach, std::vector<uint32_t>& ioIds,
					 std::vector<math::box3f>& ioBoxes) {
	ioIds.clear();
	ioBoxes.clear();
	const auto& index = iScene.getMeshSpatialIndex();
	index.query(iReach, ioIds);
	for (const uint32_t id: ioIds) {
		const auto handle = iScene.getMeshSpatialEntity(id);
		if (handle == entt::null || !iScene.registry.valid(handle) ||
			iScene.registry.all_of<component::Trigger>(handle))
			continue;
		ioBoxes.push_back(index.getBox(id));
	}
}

}// namespace

void updateFirstPersonControllers(Scene& ioScene, const SystemContext& iContext) {
	OWL_PROFILE_FUNCTION()

	const float dt = iContext.timeStep.getSeconds();
	if (dt <= 0.f)
		return;
	const auto view = ioScene.registry.view<component::Transform, component::FirstPersonController>();
	if (view.begin() == view.end())
		return;
	const bool cursorCaptured = app::Application::instanced() &&
								app::Application::get().getWindow().getCursorMode() == window::CursorMode::Disabled;
	const auto& actions = ioScene.getActions();
	std::vector<uint32_t> ids;
	std::vector<math::box3f> boxes;
	for (const auto handle: view) {
		auto [transform, controller] = view.get<component::Transform, component::FirstPersonController>(handle);
		if (!controller.enabled)
			continue;
		auto& eye = transform.transform;
		if (!controller.m_initialized) {
			controller.m_yaw = eye.rotation().y();
			controller.m_pitch = eye.rotation().x();
			controller.m_initialized = true;
		}
		turnView(controller, eye, actions, dt, cursorCaptured);
		const float sy = std::sin(controller.m_yaw);
		const float cy = std::cos(controller.m_yaw);
		const math::vec2 steer = actions.getVector("move");
		const math::vec3 wish = math::vec3{-sy, 0.f, -cy} * steer.y() + math::vec3{cy, 0.f, -sy} * steer.x();
		const float length = std::sqrt(wish.x() * wish.x() + wish.z() * wish.z());
		const float speed = actions.isHeld("sprint") ? controller.runSpeed : controller.walkSpeed;
		math::vec3 velocity{0.f, 0.f, 0.f};
		if (length > 0.f) {
			velocity.x() = wish.x() / length * speed * std::min(length, 1.f);
			velocity.z() = wish.z() / length * speed * std::min(length, 1.f);
		}
		if (controller.m_grounded && controller.jumpSpeed > 0.f && actions.isJustPressed("jump"))
			controller.m_velocityY = controller.jumpSpeed;
		controller.m_velocityY -= controller.gravity * dt;
		velocity.y() = controller.m_velocityY;
		const math::vec3 half = controller.getHalfExtents();
		const math::vec3 body = eye.translation() + controller.getBodyOffset();
		const math::vec3 delta = velocity * dt;
		const math::vec3 margin{std::abs(delta.x()) + half.x() + 0.5f, std::abs(delta.y()) + half.y() + 0.5f,
								std::abs(delta.z()) + half.z() + 0.5f};
		gatherObstacles(ioScene, math::box3f{body - margin, body + margin}, ids, boxes);
		const auto moved = data::geometry::moveBoxAmong(body, half, delta, boxes, controller.stepHeight);
		eye.translation() = moved.position - controller.getBodyOffset();
		controller.m_grounded = moved.onGround;
		if (moved.onGround || moved.hitCeiling)
			controller.m_velocityY = 0.f;
	}
}

}// namespace owl::scene::systems
