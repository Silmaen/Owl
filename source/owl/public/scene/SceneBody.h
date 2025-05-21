/**
 * @file SceneBody.h
 * @author Silmaen
 * @date 12/30/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "math/vectors.h"
#include "core/Core.h"

namespace owl::scene {

/**
 * @brief Class describing a physical body in the scene.
 */
class OWL_API SceneBody final{
public:
	/**
	 * @brief Default constructor.
	 */
	SceneBody();
	/**
	 * @brief Default destructor.
	 */
	~SceneBody();
	/**
	 * @brief Default copy constructor.
	 */
	SceneBody(const SceneBody&) = default;
	/**
	 * @brief Default move constructor.
	 */
	SceneBody(SceneBody&&) = default;
	/**
	 * @brief Default copy affectation operator.
	 */
	auto operator=(const SceneBody&) -> SceneBody& = default;
	/**
	 * @brief Default move affectation operator.
	 */
	auto operator=(SceneBody&&) -> SceneBody& = default;

	/// The type of physical body
	enum struct BodyType : uint8_t {
		Static,///< Static but affected by physic.
		Dynamic,///< Position dynamically computed.
		Kinematic///< Kinematic position.
	};
	/// The type of body.
	BodyType type = BodyType::Dynamic;

	/// If the body can rotate.
	bool fixedRotation = false;
	/// Internal body index.
	uint64_t bodyId = 0;
	/// The size of the collider.
	math::vec3f colliderSize{1, 1, 1};
	/// The density of the material.
	float density{1.f};
	/// The restitution coefficient in case of contact.
	float restitution{0};
	/// The friction coefficient for contact.
	float friction{0.5f};

};

}// namespace owl::scene
