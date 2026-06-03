/**
 * @file ScenePlayer.cpp
 * @author Silmaen
 * @date 12/30/24
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "scene/ScenePlayer.h"

#include "input/Input.h"
#include "physics/PhysicCommand.h"

namespace owl::scene {

ScenePlayer::ScenePlayer() = default;

void ScenePlayer::parseInputs(const Entity& iPlayer) const {
	if (input::Input::isKeyPressed(input::key::D)) {
		physics::PhysicCommand::impulse(iPlayer, {linearImpulse, 0});
	}
	if (input::Input::isKeyPressed(input::key::A)) {
		physics::PhysicCommand::impulse(iPlayer, {-linearImpulse, 0});
	}
	if (canJump && input::Input::isKeyPressed(input::key::Space)) {
		if (const auto vel = physics::PhysicCommand::getVelocity(iPlayer);
			std::abs(vel.y()) < 0.001f) {// no vertical velocity means on the ground!
			physics::PhysicCommand::impulse(iPlayer, {0, jumpImpulse});
		}
	}
}
}// namespace owl::scene
