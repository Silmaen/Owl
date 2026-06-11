/**
 * @file VoxelPlayer.h
 * @author Silmaen
 * @date 08/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/Serializer.h"
#include "data/voxel/Block.h"
#include "math/vectors.h"

namespace owl::scene::component {

/**
 * @brief
 *  First-person grounded player for voxel worlds (walk / run / jump with gravity and AABB-vs-voxel collision).
 *
 * Placed on a camera entity, it makes the scene drive the entity transform each
 * runtime frame: WASD moves on the horizontal plane relative to the yaw, the
 * arrow keys look around, Left-Shift runs and Space jumps. The body is an
 * axis-aligned box (`halfExtents`); the entity transform is its centre (the eye).
 * Collision is resolved against every `VoxelWorld` solid block — the player does
 * not edit the world. Tunable speeds / gravity are authored; the motion state
 * (velocity, yaw, pitch, grounded) lives at runtime and is not serialized.
 */
struct OWL_API VoxelPlayer {
	/// Walking speed in blocks per second.
	float walkSpeed = 5.f;
	/// Running speed (while Left-Shift is held) in blocks per second.
	float runSpeed = 9.f;
	/// Upward speed imparted by a jump in blocks per second.
	float jumpSpeed = 8.5f;
	/// Downward gravity acceleration in blocks per second squared.
	float gravity = 24.f;
	/// Look (rotation) speed in radians per second for the arrow keys.
	float lookSpeed = 1.5f;
	/// Mouse-look sensitivity in radians per pixel (used while the cursor is captured).
	float mouseSensitivity = 0.0025f;
	/// Horizontal fly speed in blocks per second (fly mode).
	float flySpeed = 12.f;
	/// Speed multiplier applied in fly mode while super-speed is on.
	float superSpeedMultiplier = 4.f;
	/// Half the player box size on each axis (collision AABB; the transform is its centre).
	math::vec3 halfExtents{0.4f, 0.9f, 0.4f};
	/// Maximum reach in blocks for breaking / placing (the targeting ray length).
	float reach = 5.f;
	/// Block id placed on right-click (resolved against the targeted world's registry).
	data::voxel::BlockId placeBlock = 1;
	/// When true, clicking the viewport captures (hides / locks) the cursor for mouse-look; off keeps a normal cursor.
	bool captureCursor = false;

	/// Vertical velocity (runtime; not serialized).
	float velocityY = 0.f;
	/// Yaw angle in radians (runtime; mirrors the transform rotation).
	float yaw = 0.f;
	/// Pitch angle in radians (runtime; mirrors the transform rotation).
	float pitch = 0.f;
	/// True while resting on solid ground (runtime; gates jumping).
	bool grounded = false;
	/// True once the initial transform has been read into yaw/pitch (runtime).
	bool initialized = false;
	/// True while in free-fly mode (no gravity, vertical on Space/Shift); toggled by double-tapping Space.
	bool flyMode = false;
	/// True while super-speed is on (fly mode only); toggled by 'J'.
	bool superSpeed = false;
	/// Last cursor position for mouse-look delta (runtime).
	math::vec2 lastMouse{0.f, 0.f};
	/// Whether `lastMouse` holds a valid sample (reset when the cursor is released, to avoid a look jump).
	bool mouseValid = false;
	/// Countdown for the Space double-tap window in seconds (runtime).
	float doubleTapTimer = 0.f;
	/// Edge-detection latch for the Space key (runtime).
	bool spaceWasPressed = false;
	/// Edge-detection latch for the 'J' key (runtime).
	bool superSpeedWasPressed = false;
	/// Edge-detection latch for the break (left mouse) button (runtime).
	bool breakWasPressed = false;
	/// Edge-detection latch for the place (right mouse) button (runtime).
	bool placeWasPressed = false;
	/// True while the targeting ray hits a block this frame (runtime; drives the highlight).
	bool hasTarget = false;
	/// World-grid coordinates of the currently targeted block (runtime; valid when `hasTarget`).
	math::vec3i targetBlock{0, 0, 0};

	/**
	 * @brief
	 *  Get the display name for this component.
	 * @return The display name.
	 */
	static auto name() -> const char* { return "Voxel Player"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "VoxelPlayer"; }

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
