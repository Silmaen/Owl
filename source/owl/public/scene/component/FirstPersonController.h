/**
 * @file FirstPersonController.h
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/Serializer.h"
#include "math/vectors.h"

namespace owl::scene::component {

/**
 * @brief
 *  Walks the entity (its camera at eye height) on foot through a 3D scene: the `move` action walks, `sprint` runs,
 *  `look` and the captured mouse turn the view, `jump` jumps, gravity pulls down.
 *
 * The `owl.first_person` system moves a body box of `radius` and `height`, the eye `eyeHeight` above its feet, against
 * the world boxes of the `MeshRenderer` entities (those with a `Trigger` let it through, a box lower than
 * `stepHeight` is climbed). The `Transform` of the entity is the eye: its rotation holds the pitch and the yaw.
 * Triggers of the scene see the body box in 3D.
 */
struct OWL_API FirstPersonController {
	/// Whether the controller moves the entity; off leaves it to scripts.
	bool enabled = true;
	/// Walking speed, in world units per second.
	float walkSpeed = 2.2f;
	/// Running speed (`sprint` held), in world units per second.
	float runSpeed = 4.f;
	/// Speed of the `look` action, in radians per second.
	float lookSpeed = 1.8f;
	/// Radians per pixel of mouse movement while the cursor is captured.
	float mouseSensitivity = 0.0025f;
	/// Whether clicking the view captures the cursor for mouse look.
	bool captureCursor = true;
	/// Height of the eye above the feet, in world units.
	float eyeHeight = 1.6f;
	/// Height of the body box, in world units.
	float height = 1.75f;
	/// Half width of the body box, in world units.
	float radius = 0.25f;
	/// Highest step climbed without jumping, in world units.
	float stepHeight = 0.35f;
	/// Downward acceleration, in world units per second squared.
	float gravity = 9.81f;
	/// Upward speed of a jump, in world units per second; 0 for none.
	float jumpSpeed = 3.5f;

	/// Runtime: yaw and pitch were read from the transform.
	bool m_initialized = false;
	/// Runtime: heading, in radians about Y.
	float m_yaw = 0.f;
	/// Runtime: pitch, in radians about X.
	float m_pitch = 0.f;
	/// Runtime: vertical speed.
	float m_velocityY = 0.f;
	/// Runtime: whether the feet rest on a box.
	bool m_grounded = false;
	/// Runtime: mouse position of the last frame.
	math::vec2 m_lastMouse{0.f, 0.f};
	/// Runtime: whether `m_lastMouse` holds a position.
	bool m_mouseValid = false;

	/**
	 * @brief
	 *  Get the half size of the body box.
	 * @return The half extents.
	 */
	[[nodiscard]] auto getHalfExtents() const -> math::vec3 { return {radius, height * 0.5f, radius}; }

	/**
	 * @brief
	 *  Get the offset from the eye to the centre of the body box.
	 * @return The offset, along Y.
	 */
	[[nodiscard]] auto getBodyOffset() const -> math::vec3 { return {0.f, height * 0.5f - eyeHeight, 0.f}; }

	/**
	 * @brief
	 *  Get the display name for this component.
	 * @return The display name.
	 */
	static auto name() -> const char* { return "First Person Controller"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "FirstPersonController"; }

	/**
	 * @brief
	 *  Write this component to a YAML context.
	 * @param[in] iOut The YAML context.
	 */
	void serialize(const core::Serializer& iOut) const;

	/**
	 * @brief
	 *  Read this component from a YAML node.
	 * @param[in] iNode The YAML node to read.
	 */
	void deserialize(const core::Serializer& iNode);
};

}// namespace owl::scene::component
