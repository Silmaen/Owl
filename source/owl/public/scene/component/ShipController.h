/**
 * @file ShipController.h
 * @author Silmaen
 * @date 01/11/2026
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
 *  Flies the entity (its camera is the cockpit) like a spaceship, with inertia and a flight assist.
 *
 * The `owl.ships` system reads the built-in actions: `ship_thrust` pushes along the nose (W / S, the triggers),
 * `ship_roll` rolls (A / D, the bumpers), `ship_strafe` slides sideways and up or down (Q / E, Space / Left-Ctrl, the
 * d-pad), `look` and the captured mouse (unless `mouseSteer` is off: the mouse is then left to the game, an aim) pitch
 * and yaw, `ship_assist` toggles the flight assist (Z, the right stick click). The velocity is kept from frame to frame:
 * with the assist on, dampers stop the motion along every axis no thrust pushes and the rotation once the controls are
 * released, so the ship flies where it points; with the assist off, the ship keeps drifting and spinning. An
 * `angularResponse` of 0 turns at the commanded rate at once and stops on release, with or without the assist. A
 * `throttleRate` above 0 makes the throttle persistent: `ship_thrust` moves it up or down and the ship holds the speed
 * it commands (`m_throttle` times `maxSpeed`) once the controls are released. A script adds its own pitch and yaw
 * command each frame with `m_turnInput` (Lua `entity.set_ship_turn`: steering toward a cursor). A root entity with a
 * `WorldPosition` moves in double precision, the floating origin of the scene follows it.
 */
struct OWL_API ShipController {
	/// Whether the controller flies the entity; off leaves it to scripts.
	bool enabled = true;
	/// Acceleration of the main engine along the nose, in world units per second squared.
	float thrust = 12.f;
	/// Acceleration of the side thrusters (strafe), in world units per second squared.
	float strafeThrust = 6.f;
	/// Top speed, in world units per second; 0 for none.
	float maxSpeed = 150.f;
	/// Pitch and yaw rate of the `look` action at full deflection, in radians per second.
	float turnRate = 1.2f;
	/// Roll rate of the `ship_roll` action at full deflection, in radians per second.
	float rollRate = 1.6f;
	/// Radians per pixel of mouse movement while the cursor is captured.
	float mouseSensitivity = 0.002f;
	/// How fast the rotation rate follows the controls, per second (higher is snappier); 0 follows them at once.
	float angularResponse = 6.f;
	/// Strength of the flight assist dampers, per second: the share of the free velocity removed is 1 - e^(-k t).
	float assistDamping = 1.2f;
	/// Whether the flight assist stabilises the ship; `ship_assist` toggles it in Play.
	bool flightAssist = true;
	/// Whether clicking the view captures the cursor (mouse steering, or the game's own use of the mouse).
	bool captureCursor = true;
	/// Whether the captured mouse pitches and yaws the ship; off leaves the mouse to the game (an aim on the screen).
	bool mouseSteer = true;
	/// Whether pitch is inverted: stick or mouse forward raises the nose (the `look` action and the mouse).
	bool invertPitch = false;
	/**
	 * @brief
	 *  Rate at which the flight assist rolls the wings back level with the world up axis once `ship_roll` is released,
	 *  per second of roll error; 0 keeps the roll where the pilot left it.
	 */
	float levelRate = 0.f;
	/**
	 * @brief
	 *  Arcade flight when above 0: the yaw turns about the world up axis, as a first-person view does, and the nose
	 *  stays within this angle of the horizon, in radians; 0 turns about the body axes in every direction.
	 */
	float maxPitch = 0.f;
	/**
	 * @brief
	 *  Persistent throttle when above 0: how fast `ship_thrust` moves the throttle, per second at full deflection; the
	 *  ship then holds `m_throttle` times `maxSpeed` along its nose. 0 thrusts only while `ship_thrust` is held.
	 */
	float throttleRate = 0.f;
	/// Lowest persistent throttle, from -1 (full reverse) to 0 (no reverse).
	float minThrottle = -1.f;

	/// Runtime: velocity in world space, in world units per second.
	math::vec3 m_velocity{0.f, 0.f, 0.f};
	/// Runtime: rotation rate about the body axes (pitch about X, yaw about Y, roll about Z), in radians per second.
	math::vec3 m_angularVelocity{0.f, 0.f, 0.f};
	/**
	 * @brief
	 *  Runtime: throttle of the last frame, from -1 to 1 (engine effects, the HUD): the `ship_thrust` value, or the
	 *  persistent throttle when `throttleRate` is above 0 (a script may set it).
	 */
	float m_throttle = 0.f;
	/**
	 * @brief
	 *  Runtime: pitch and yaw command of a script for the next frame, as a share of `turnRate` from -1 to 1 (x turns
	 *  right, y lowers the nose, as on the screen, never inverted); added to `look`, cleared once read.
	 */
	math::vec2 m_turnInput{0.f, 0.f};
	/// Runtime: mouse position of the last frame.
	math::vec2 m_lastMouse{0.f, 0.f};
	/// Runtime: whether `m_lastMouse` holds a position.
	bool m_mouseValid = false;

	/**
	 * @brief
	 *  Get the speed of the ship.
	 * @return The norm of the velocity, in world units per second.
	 */
	[[nodiscard]] auto getSpeed() const -> float { return m_velocity.norm(); }

	/**
	 * @brief
	 *  Get the display name for this component.
	 * @return The display name.
	 */
	static auto name() -> const char* { return "Ship Controller"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "ShipController"; }

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
