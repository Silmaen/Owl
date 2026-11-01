/**
 * @file ShipSystem.cpp
 * @author Silmaen
 * @date 01/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "EngineSystems.h"

#include "app/Application.h"
#include "input/ActionFrame.h"
#include "input/Input.h"
#include "scene/Entity.h"
#include "scene/Scene.h"
#include "scene/component/Hierarchy.h"
#include "scene/component/ShipController.h"
#include "scene/component/Transform.h"
#include "scene/component/WorldPosition.h"
#include "window/Window.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace owl::scene::systems {

namespace {

// Columns of a rotation matrix: the right, up and back axes of the body in world space.
using Basis = std::array<math::vec3, 3>;

constexpr float g_deadZone = 1e-3f;

// The basis of the `Transform` rotation (Rz * Ry * Rx, the order of `math::Transform`).
auto toBasis(const math::vec3& iEuler) -> Basis {
	const float sx = std::sin(iEuler.x());
	const float cx = std::cos(iEuler.x());
	const float sy = std::sin(iEuler.y());
	const float cy = std::cos(iEuler.y());
	const float sz = std::sin(iEuler.z());
	const float cz = std::cos(iEuler.z());
	return {math::vec3{cz * cy, sz * cy, -sy}, math::vec3{cz * sy * sx - sz * cx, sz * sy * sx + cz * cx, cy * sx},
			math::vec3{cz * sy * cx + sz * sx, sz * sy * cx - cz * sx, cy * cx}};
}

auto toEuler(const Basis& iBasis) -> math::vec3 {
	const float sy = std::clamp(-iBasis[0].z(), -1.f, 1.f);
	const float y = std::asin(sy);
	if (std::abs(std::cos(y)) < 1e-5f)
		return {std::atan2(-iBasis[2].y(), iBasis[1].y()), y, 0.f};
	return {std::atan2(iBasis[1].z(), iBasis[2].z()), y, std::atan2(iBasis[0].y(), iBasis[0].x())};
}

// Rodrigues rotation of a vector about the axis-angle vector.
auto rotated(const math::vec3& iVector, const math::vec3& iAxisAngle) -> math::vec3 {
	const float angle = iAxisAngle.norm();
	if (angle < 1e-9f)
		return iVector;
	const math::vec3 axis = iAxisAngle * (1.f / angle);
	const float c = std::cos(angle);
	return iVector * c + (axis ^ iVector) * std::sin(angle) + axis * ((axis * iVector) * (1.f - c));
}

// Turn the body by angles about its own axes (pitch about right, yaw about up, roll about back).
void turn(Basis& ioBasis, const math::vec3& iBodyAngles) {
	const math::vec3 axisAngle =
			ioBasis[0] * iBodyAngles.x() + ioBasis[1] * iBodyAngles.y() + ioBasis[2] * iBodyAngles.z();
	for (auto& axis: ioBasis) axis = rotated(axis, axisAngle);
	// Gram-Schmidt keeps the basis orthonormal against the drift of the float rotations.
	ioBasis[2] = ioBasis[2].normalized();
	ioBasis[0] = (ioBasis[1] ^ ioBasis[2]).normalized();
	ioBasis[1] = ioBasis[2] ^ ioBasis[0];
}

auto readMouse(component::ShipController& ioShip, const bool iCaptured) -> math::vec2 {
	if (!iCaptured) {
		ioShip.m_mouseValid = false;
		return {0.f, 0.f};
	}
	const math::vec2 mouse = input::Input::getMousePos();
	const math::vec2 delta = ioShip.m_mouseValid ? mouse - ioShip.m_lastMouse : math::vec2{0.f, 0.f};
	ioShip.m_lastMouse = mouse;
	ioShip.m_mouseValid = true;
	return delta;
}

void steer(component::ShipController& ioShip, Basis& ioBasis, const input::ActionFrame& iActions,
		   const math::vec2& iMouse, const float iDt) {
	const math::vec2 look = iActions.getVector("look");
	const math::vec3 wanted{-look.y() * ioShip.turnRate, -look.x() * ioShip.turnRate,
							-iActions.getValue("ship_roll") * ioShip.rollRate};
	auto& rate = ioShip.m_angularVelocity;
	if (ioShip.flightAssist) {
		const float keep = std::exp(-ioShip.angularResponse * iDt);
		rate = wanted + (rate - wanted) * keep;
	} else {
		rate += wanted * (ioShip.angularResponse * iDt);
		const math::vec3 limit{ioShip.turnRate, ioShip.turnRate, ioShip.rollRate};
		for (size_t axis = 0; axis < 3; ++axis) rate[axis] = std::clamp(rate[axis], -limit[axis], limit[axis]);
	}
	const math::vec3 mouseTurn{-iMouse.y() * ioShip.mouseSensitivity, -iMouse.x() * ioShip.mouseSensitivity, 0.f};
	turn(ioBasis, rate * iDt + mouseTurn);
}

void accelerate(component::ShipController& ioShip, const Basis& iBasis, const input::ActionFrame& iActions,
				const float iDt) {
	const float throttle = std::clamp(iActions.getValue("ship_thrust"), -1.f, 1.f);
	const math::vec2 strafe = iActions.getVector("ship_strafe");
	ioShip.m_throttle = throttle;
	const math::vec3 forward = -iBasis[2];
	auto& velocity = ioShip.m_velocity;
	velocity += (forward * (throttle * ioShip.thrust) + iBasis[0] * (strafe.x() * ioShip.strafeThrust) +
				 iBasis[1] * (strafe.y() * ioShip.strafeThrust)) *
				iDt;
	if (ioShip.flightAssist) {
		// The dampers remove the motion along every body axis no thruster pushes.
		const float keep = std::exp(-ioShip.assistDamping * iDt);
		const std::array<bool, 3> pushed{std::abs(strafe.x()) > g_deadZone, std::abs(strafe.y()) > g_deadZone,
										 std::abs(throttle) > g_deadZone};
		for (size_t axis = 0; axis < 3; ++axis) {
			if (pushed[axis])
				continue;
			const float along = velocity * iBasis[axis];
			velocity -= iBasis[axis] * (along * (1.f - keep));
		}
	}
	if (const float speed = velocity.norm(); ioShip.maxSpeed > 0.f && speed > ioShip.maxSpeed)
		velocity *= ioShip.maxSpeed / speed;
}

auto isRoot(const Scene& iScene, const entt::entity iHandle) -> bool {
	const auto* hierarchy = iScene.registry.try_get<component::Hierarchy>(iHandle);
	return hierarchy == nullptr || hierarchy->parentId == core::UUID{0};
}

// A root with a world position moves in double: the float translation is placed again from it, never accumulated.
void move(Scene& ioScene, const entt::entity iHandle, math::vec3& ioLocal, const math::vec3& iDelta) {
	auto* world = ioScene.registry.try_get<component::WorldPosition>(iHandle);
	if (world == nullptr || !isRoot(ioScene, iHandle)) {
		ioLocal += iDelta;
		return;
	}
	const auto& origin = ioScene.getWorldOrigin();
	if (world->m_synced && ioLocal != world->m_syncedLocal)
		world->position = origin + math::vec3d{static_cast<double>(ioLocal.x()), static_cast<double>(ioLocal.y()),
											   static_cast<double>(ioLocal.z())};
	world->position += math::vec3d{static_cast<double>(iDelta.x()), static_cast<double>(iDelta.y()),
								   static_cast<double>(iDelta.z())};
	const math::vec3d relative = world->position - origin;
	ioLocal = {static_cast<float>(relative.x()), static_cast<float>(relative.y()), static_cast<float>(relative.z())};
	world->m_synced = true;
	world->m_syncedLocal = ioLocal;
	world->m_syncedPosition = world->position;
}

}// namespace

void updateShips(Scene& ioScene, const SystemContext& iContext) {
	OWL_PROFILE_FUNCTION()

	const float dt = iContext.timeStep.getSeconds();
	if (dt <= 0.f)
		return;
	const auto view = ioScene.registry.view<component::Transform, component::ShipController>();
	if (view.begin() == view.end())
		return;
	const bool cursorCaptured = app::Application::instanced() &&
								app::Application::get().getWindow().getCursorMode() == window::CursorMode::Disabled;
	const auto& actions = ioScene.getActions();
	for (const auto handle: view) {
		auto [transform, ship] = view.get<component::Transform, component::ShipController>(handle);
		if (!ship.enabled)
			continue;
		if (actions.isJustPressed("ship_assist"))
			ship.flightAssist = !ship.flightAssist;
		auto& pose = transform.transform;
		// The transform holds the attitude: a script or the editor turning the entity turns the ship.
		Basis basis = toBasis(pose.rotation());
		steer(ship, basis, actions, readMouse(ship, cursorCaptured && ship.captureCursor), dt);
		pose.rotation() = toEuler(basis);
		accelerate(ship, basis, actions, dt);
		move(ioScene, handle, pose.translation(), ship.m_velocity * dt);
	}
}

}// namespace owl::scene::systems
