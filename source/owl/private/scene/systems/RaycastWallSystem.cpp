/**
 * @file RaycastWallSystem.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "EngineSystems.h"

#include "input/Input.h"
#include "physics/PhysicCommand.h"
#include "scene/Entity.h"
#include "scene/Scene.h"
#include "scene/component/components.h"

#include <algorithm>
#include <cmath>

namespace owl::scene::systems {

void updateRaycastDynamicWalls(Scene& ioScene, const float iTimeStep) {
	OWL_PROFILE_FUNCTION()

	// Find primary player position once (used for built-in proximity activation).
	math::vec2 playerWorldXY{0.f, 0.f};
	bool hasPlayer = false;
	if (const Entity player = ioScene.getPrimaryPlayer()) {
		const auto wt = ioScene.getWorldTransform(player);
		playerWorldXY = {wt.translation().x(), wt.translation().y()};
		hasPlayer = true;
	}

	for (const auto view = ioScene.registry.view<component::Transform, component::RaycastDoor>();
		 const auto entity: view) {
		const Entity ent{entity, &ioScene};
		auto& door = view.get<component::RaycastDoor>(entity);
		auto& [transform] = view.get<component::Transform>(entity);

		// Built-in activation (proximity + key edge). `interactionKey == 0` disables.
		if (door.state == component::RaycastDoor::State::Idle && door.interactionKey != 0 && hasPlayer) {
			const float dx = playerWorldXY.x() - transform.translation().x();
			const float dy = playerWorldXY.y() - transform.translation().y();
			const float distSq = dx * dx + dy * dy;
			const bool keyHeld = input::Input::isKeyPressed(door.interactionKey);
			const bool keyEdge = keyHeld && !door.keyHeldLastTick;
			if (keyEdge && distSq <= door.interactionRange * door.interactionRange)
				door.state = component::RaycastDoor::State::Opening;
			door.keyHeldLastTick = keyHeld;
		} else if (door.interactionKey != 0) {
			door.keyHeldLastTick = input::Input::isKeyPressed(door.interactionKey);
		}

		const float prevOffset = door.currentOffset;
		switch (door.state) {
			case component::RaycastDoor::State::Idle:
				break;
			case component::RaycastDoor::State::Opening:
				door.currentOffset += std::max(0.f, door.slideSpeed) * iTimeStep;
				if (door.currentOffset >= 1.f) {
					door.currentOffset = 1.f;
					door.state = component::RaycastDoor::State::Open;
					door.holdTimer = std::max(0.f, door.holdTime);
				}
				break;
			case component::RaycastDoor::State::Open:
				door.holdTimer -= iTimeStep;
				if (door.holdTimer <= 0.f)
					door.state = component::RaycastDoor::State::Closing;
				break;
			case component::RaycastDoor::State::Closing:
				door.currentOffset -= std::max(0.f, door.closeSpeed) * iTimeStep;
				if (door.currentOffset <= 0.f) {
					door.currentOffset = 0.f;
					door.state = component::RaycastDoor::State::Idle;
				}
				break;
		}
		const float delta = door.currentOffset - prevOffset;
		if (std::abs(delta) > 1e-6f) {
			using OD = component::RaycastDoor::OpeningDirection;
			float dx = 0.f;
			float dy = 0.f;
			switch (door.openingDirection) {
				case OD::East:
					dx = 1.f;
					break;
				case OD::West:
					dx = -1.f;
					break;
				case OD::North:
					dy = 1.f;
					break;
				case OD::South:
					dy = -1.f;
					break;
			}
			const math::Transform wt = ioScene.getWorldTransform(ent);
			const float plateX = wt.translation().x() + dx * door.currentOffset;
			const float plateY = wt.translation().y() + dy * door.currentOffset;
			physics::PhysicCommand::setTransform(ent, math::vec2f{plateX, plateY}, wt.rotation().z());
		}
	}

	// Pushwalls — Idle → Moving → Final, one-shot.
	for (const auto view = ioScene.registry.view<component::Transform, component::RaycastPushWall>();
		 const auto entity: view) {
		const Entity ent{entity, &ioScene};
		auto& push = view.get<component::RaycastPushWall>(entity);
		auto& [transform] = view.get<component::Transform>(entity);

		if (push.state == component::RaycastPushWall::State::Idle && push.interactionKey != 0 && hasPlayer) {
			const float dx = playerWorldXY.x() - transform.translation().x();
			const float dy = playerWorldXY.y() - transform.translation().y();
			const float distSq = dx * dx + dy * dy;
			const bool keyHeld = input::Input::isKeyPressed(push.interactionKey);
			if (const bool keyEdge = keyHeld && !push.keyHeldLastTick;
				keyEdge && distSq <= push.interactionRange * push.interactionRange)
				push.state = component::RaycastPushWall::State::Moving;
			push.keyHeldLastTick = keyHeld;
		} else if (push.interactionKey != 0) {
			push.keyHeldLastTick = input::Input::isKeyPressed(push.interactionKey);
		}

		const float prevOffset = push.currentOffset;
		if (push.state == component::RaycastPushWall::State::Moving) {
			push.currentOffset += std::max(0.f, push.slideSpeed) * iTimeStep;
			if (push.currentOffset >= push.slideDistance) {
				push.currentOffset = push.slideDistance;
				push.state = component::RaycastPushWall::State::Final;
			}
		}
		if (const float delta = push.currentOffset - prevOffset; std::abs(delta) > 1e-6f) {
			transform.translation().x() += push.slideDirection.x() * delta;
			transform.translation().y() += push.slideDirection.y() * delta;
			const math::Transform wt = ioScene.getWorldTransform(ent);
			physics::PhysicCommand::setTransform(ent, math::vec2f{wt.translation().x(), wt.translation().y()},
												 wt.rotation().z());
		}
	}
}

}// namespace owl::scene::systems
