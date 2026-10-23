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
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <tuple>
#include <utility>
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

// Distance along the ray to its entry in the box, or none when it misses it.
auto rayHit(const math::vec3& iOrigin, const math::vec3& iDirection, const math::box3f& iBox) -> std::optional<float> {
	float near = 0.f;
	float far = std::numeric_limits<float>::max();
	for (size_t axis = 0; axis < 3; ++axis) {
		if (std::abs(iDirection[axis]) < 1e-8f) {
			if (iOrigin[axis] < iBox.min()[axis] || iOrigin[axis] > iBox.max()[axis])
				return std::nullopt;
			continue;
		}
		float t0 = (iBox.min()[axis] - iOrigin[axis]) / iDirection[axis];
		float t1 = (iBox.max()[axis] - iOrigin[axis]) / iDirection[axis];
		if (t0 > t1)
			std::swap(t0, t1);
		near = std::max(near, t0);
		far = std::min(far, t1);
		if (near > far)
			return std::nullopt;
	}
	return near;
}

auto contains(const math::box3f& iBox, const math::vec3& iPoint) -> bool {
	constexpr float margin = 1e-3f;
	for (size_t axis = 0; axis < 3; ++axis) {
		if (iPoint[axis] < iBox.min()[axis] - margin || iPoint[axis] > iBox.max()[axis] + margin)
			return false;
	}
	return true;
}

auto merged(const math::box3f& iA, const math::box3f& iB) -> math::box3f {
	return {math::vec3{std::min(iA.min().x(), iB.min().x()), std::min(iA.min().y(), iB.min().y()),
					   std::min(iA.min().z(), iB.min().z())},
			math::vec3{std::max(iA.max().x(), iB.max().x()), std::max(iA.max().y(), iB.max().y()),
					   std::max(iA.max().z(), iB.max().z())}};
}

// The box an interaction trigger is aimed at by: the world boxes of the meshes of its subtree, else its transform.
auto interactionBox(const Scene& iScene, const Entity& iEntity, std::vector<uint32_t>& ioIds) -> math::box3f {
	const auto world = iScene.getWorldTransform(iEntity);
	const math::vec3 half = world.scale() * 0.5f;
	math::box3f box{world.translation() - half, world.translation() + half};
	const auto& index = iScene.getMeshSpatialIndex();
	const auto root = static_cast<entt::entity>(iEntity);
	if (const auto id = static_cast<uint32_t>(entt::to_entity(root)); index.contains(id))
		box = index.getBox(id);
	if (!iEntity.hasComponent<component::Hierarchy>() ||
		iEntity.getComponent<component::Hierarchy>().childrenIds.empty())
		return box;
	ioIds.clear();
	index.query(math::box3f{box.min() - math::vec3{2.f, 2.f, 2.f}, box.max() + math::vec3{2.f, 2.f, 2.f}}, ioIds);
	for (const uint32_t id: ioIds) {
		const auto handle = iScene.getMeshSpatialEntity(id);
		if (handle != root && handle != entt::null && iScene.registry.valid(handle) && iScene.isInSubtree(handle, root))
			box = merged(box, index.getBox(id));
	}
	return box;
}

// The nearest interaction trigger the view ray hits within reach, before any other mesh box.
auto findAimedInteraction(Scene& ioScene, const math::vec3& iEye, const math::vec3& iForward, const float iReach,
						  std::vector<uint32_t>& ioIds) -> std::optional<std::pair<Entity, math::box3f>> {
	std::optional<std::pair<Entity, math::box3f>> best;
	float bestDistance = iReach;
	for (const auto [handle, trigger]: ioScene.registry.view<component::Trigger>().each()) {
		if (trigger.trigger.type != SceneTrigger::TriggerType::Interaction)
			continue;
		const Entity entity{handle, &ioScene};
		if (!ioScene.isEffectivelyVisible(entity, false))
			continue;
		const auto box = interactionBox(ioScene, entity, ioIds);
		if (const auto distance = rayHit(iEye, iForward, box); distance && *distance <= bestDistance) {
			bestDistance = *distance;
			best = std::pair{entity, box};
		}
	}
	if (!best)
		return std::nullopt;
	const math::vec3 end = iEye + iForward * bestDistance;
	const math::vec3 aimedPoint = end;
	const math::vec3 low{std::min(iEye.x(), end.x()), std::min(iEye.y(), end.y()), std::min(iEye.z(), end.z())};
	const math::vec3 high{std::max(iEye.x(), end.x()), std::max(iEye.y(), end.y()), std::max(iEye.z(), end.z())};
	ioIds.clear();
	const auto& index = ioScene.getMeshSpatialIndex();
	index.query(math::box3f{low, high}, ioIds);
	for (const uint32_t id: ioIds) {
		const auto handle = ioScene.getMeshSpatialEntity(id);
		if (handle == entt::null || !ioScene.registry.valid(handle) ||
			ioScene.registry.all_of<component::Trigger>(handle) ||
			ioScene.isInSubtree(handle, static_cast<entt::entity>(best->first)))
			continue;
		// World boxes are coarser than meshes: one holding the aimed point (a hearth around a key) does not hide it.
		const auto& box = index.getBox(id);
		if (contains(box, aimedPoint))
			continue;
		if (const auto distance = rayHit(iEye, iForward, box);
			distance && *distance > 0.f && *distance < bestDistance - 1e-3f)
			return std::nullopt;
	}
	return best;
}

auto hasMeshInSubtree(const Scene& iScene, const Entity& iRoot) -> bool {
	const auto root = static_cast<entt::entity>(iRoot);
	const auto meshes = iScene.registry.view<component::MeshRenderer>();
	return std::ranges::any_of(meshes, [&](const auto iHandle) -> bool { return iScene.isInSubtree(iHandle, root); });
}

auto isWorldLoaded(const Scene& iScene) -> bool {
	bool anyMesh = false;
	for (const auto [handle, renderer]: iScene.registry.view<component::MeshRenderer>().each()) {
		std::ignore = handle;
		if (renderer.meshPath.empty())
			continue;
		if (!renderer.mesh)
			return false;
		anyMesh = true;
	}
	return !anyMesh || iScene.getMeshSpatialIndex().size() > 0;
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
	ioScene.setInteractionHighlight(std::nullopt);
	ioScene.setOutline(std::nullopt);
	for (const auto handle: view) {
		auto [transform, controller] = view.get<component::Transform, component::FirstPersonController>(handle);
		if (!controller.enabled)
			continue;
		auto& eye = transform.transform;
		// The transform holds the view: a script or the editor turning the entity turns the walker.
		controller.m_yaw = eye.rotation().y();
		controller.m_pitch = eye.rotation().x();
		// The same aiming crosshair as the voxel player: interactions are used by aiming at them.
		ioScene.setShowCrosshair(true);
		turnView(controller, eye, actions, dt, cursorCaptured);
		if (!controller.m_worldReady) {
			controller.m_worldReady = isWorldLoaded(ioScene);
			if (!controller.m_worldReady)
				continue;
		}
		const float sy = std::sin(controller.m_yaw);
		const float cy = std::cos(controller.m_yaw);
		const math::vec2 steer = actions.getVector("move");
		const math::vec3 wish = math::vec3{-sy, 0.f, -cy} * -steer.y() + math::vec3{cy, 0.f, -sy} * steer.x();
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
		const float cp = std::cos(controller.m_pitch);
		const math::vec3 forward{-cp * std::sin(controller.m_yaw), std::sin(controller.m_pitch),
								 -cp * std::cos(controller.m_yaw)};
		const auto aimed = findAimedInteraction(ioScene, eye.translation(), forward, controller.reach, ids);
		controller.m_target = aimed ? aimed->first.getUUID() : core::UUID{0};
		const bool hasMesh = aimed && hasMeshInSubtree(ioScene, aimed->first);
		ioScene.setOutline(hasMesh ? std::optional{Scene::Outline{.root = aimed->first.getUUID()}} : std::nullopt);
		ioScene.setInteractionHighlight(aimed && !hasMesh ? std::optional{aimed->second} : std::nullopt);
	}
}

}// namespace owl::scene::systems
