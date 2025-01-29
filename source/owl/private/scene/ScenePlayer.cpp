/**
 * @file ScenePlayer.cpp
 * @author Silmaen
 * @date 12/30/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "scene/ScenePlayer.h"

#include "physic/PhysicCommand.h"
#include "input/Input.h"

namespace owl::scene {

ScenePlayer::ScenePlayer() = default;

ScenePlayer::~ScenePlayer() = default;

void ScenePlayer::parseInputs(const Entity& iPlayer) {
	if (input::Input::isKeyPressed(input::key::D)) {
		physic::PhysicCommand::impulse(iPlayer, {linearImpulse, 0});
	}
	if (input::Input::isKeyPressed(input::key::A)) {
		physic::PhysicCommand::impulse(iPlayer, {-linearImpulse, 0});
	}
	if (canJump && input::Input::isKeyPressed(input::key::Space)) {
		auto vel = physic::PhysicCommand::getVelocity(iPlayer);
		if (std::abs(vel.y()) < 0.001f) {// no vertical velocity means on the ground!
			physic::PhysicCommand::impulse(iPlayer, {0, jumpImpulse});
		}
	}
}
}// namespace owl::scene
