/**
 * @file ScenePlayer.cpp
 * @author Silmaen
 * @date 12/30/24
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "scene/ScenePlayer.h"

#include "physics/PhysicCommand.h"

#include <cmath>

namespace owl::scene {

ScenePlayer::ScenePlayer() = default;

void ScenePlayer::parseInputs(const Entity& iPlayer, const input::ActionFrame& iActions) const {
	if (const float horizontal = iActions.getVector("move").x(); std::abs(horizontal) > 0.f)
		physics::PhysicCommand::impulse(iPlayer, {horizontal * linearImpulse, 0});
	if (canJump && iActions.isHeld("jump")) {
		if (const auto vel = physics::PhysicCommand::getVelocity(iPlayer);
			std::abs(vel.y()) < 0.001f) {// no vertical velocity means on the ground!
			physics::PhysicCommand::impulse(iPlayer, {0, jumpImpulse});
		}
	}
}
}// namespace owl::scene
