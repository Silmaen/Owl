/**
 * @file ScenePlayer.h
 * @author Silmaen
 * @date 12/30/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Entity.h"
#include "core/Core.h"

namespace owl::scene {

/**
 * @brief Class describing a playre in the scene.
 */
class OWL_API ScenePlayer final {
public:
	/**
	 * @brief Default constructor.
	 */
	ScenePlayer();
	/**
	 * @brief Default destructor.
	 */
	~ScenePlayer();
	/**
	 * @brief Default copy constructor.
	 */
	ScenePlayer(const ScenePlayer&) = default;
	/**
	 * @brief Default move constructor.
	 */
	ScenePlayer(ScenePlayer&&) = default;
	/**
	 * @brief Default copy affectation operator.
	 */
	auto operator=(const ScenePlayer&) -> ScenePlayer& = default;
	/**
	 * @brief Default move affectation operator.
	 */
	auto operator=(ScenePlayer&&) -> ScenePlayer& = default;

	/**
	 * @brief Look the inputs for actions.
	 */
	void parseInputs(const Entity& iPlayer);

	/// The intensity of the linear impulsion.
	float linearImpulse = 0.1f;

	/// The intensity of the jump impulsion.
	float jumpImpulse = 0.2f;

	/// If the player can jump.
	bool canJump = true;

private:
};

}// namespace owl::scene
