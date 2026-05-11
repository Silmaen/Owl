/**
 * @file RaycastPushWall.h
 * @author Silmaen
 * @date 11/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Serializer.h"
#include "input/KeyCodes.h"
#include "scene/Tileset.h"

namespace owl::scene::component {
/**
 * @brief
 *  Wolfenstein-style pushwall rendered by `RendererRaycast::drawDynamicWalls`.
 *
 * A pushwall behaves like a one-shot door: it slides once along
 * `slideDirection` for `slideDistance` cells and then stays there forever.
 * Shares the dynamic-wall rendering and physics infrastructure with
 * `RaycastDoor`, but the state machine is simpler (Idle → Moving → Final).
 * Use it to reveal secret passages or to gate progress with a permanent
 * displacement.
 *
 * `Transform.translation.xy` is the rest world position; while moving, the
 * rendered AABB and the kinematic Box2D body are offset by `currentOffset`
 * along `slideDirection`.
 */
struct OWL_API RaycastPushWall {
	/**
	 * @brief
	 *  Lifecycle state of the pushwall.
	 */
	enum struct State : uint8_t {
		Idle,///< Ready to be triggered.
		Moving,///< Sliding toward the final position.
		Final,///< Reached the final position, no further animation.
	};

	/**
	 * @brief
	 *  Path to the `.owltileset` asset that provides the wall texture.
	 *
	 * Pointing this at the same tileset the world's tilemap uses lets the engine
	 * reuse the same `shared<Tileset>` (and its atlas texture) — no double-load.
	 * The block is rendered with the tile at `tileIndex`.
	 */
	std::string tilesetPath;
	/// Tile index sampled from the atlas for the block faces.
	uint32_t tileIndex = 0;
	/// Resolved tileset asset (runtime — populated by `Scene::resolveAllTilemapAssets`).
	shared<Tileset> tileset;
	/// Slide axis in world cell coordinates (unit vector recommended).
	math::vec2 slideDirection{1.0f, 0.0f};
	/// Total travel distance along `slideDirection` from rest to Final, in cells.
	float slideDistance = 2.0f;
	/// Slide speed, in cells per second.
	float slideSpeed = 1.0f;
	/**
	 * @brief
	 *  Engine-handled activation key, as a GLFW key code.
	 *
	 * Set to `0` to disable the built-in handler and drive activation from
	 * Lua via `pushwall.activate(entity)`.
	 */
	input::KeyCode interactionKey = input::key::E;
	/// Maximum distance (cells) between player and pushwall centre for the built-in key to fire.
	float interactionRange = 1.5f;

	/// Runtime state.
	State state = State::Idle;
	/// Current slide progress, in cells. `0` at rest, `slideDistance` at Final.
	float currentOffset = 0.0f;
	/// Edge-detection latch: `interactionKey` was held last tick.
	bool keyHeldLastTick = false;
	/**
	 * @brief
	 *  Internal index into `PhysicCommand`'s body table for the kinematic block.
	 *
	 * Auto-assigned by `PhysicCommand::init` when the pushwall has no explicit
	 * `PhysicBody` component. `0` means no auto-body.
	 */
	uint64_t bodyId = 0;

	/**
	 * @brief
	 *  Get the class title.
	 * @return The class title.
	 */
	static auto name() -> const char* { return "Raycast PushWall"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "RaycastPushWall"; }

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
