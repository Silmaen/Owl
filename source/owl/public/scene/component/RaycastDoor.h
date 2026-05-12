/**
 * @file RaycastDoor.h
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
 *  Wolfenstein-style sliding door — a 1×1 cell whose internal **plate** slides
 *  exactly one cell (plus 1 pixel) into the adjacent wall pocket along one of
 *  the four cardinal directions.
 *
 * Visual breakdown of the door's cell cube:
 *   - The cube's two **inside faces perpendicular to the opening direction**
 *     are textured with `lateralTexture` (the jamb / pocket-side wall). They
 *     have no thickness — they're just the inner colouring of the cube — and
 *     **never animate**.
 *   - A thin **plate** perpendicular to the plate normal slides along the
 *     opening direction. The plate's surface normal is **perpendicular** to
 *     the opening direction (a north-opening door has plate surfaces facing
 *     east and west), so the player approaches the door head-on along the
 *     axis perpendicular to the opening direction.
 *   - The plate's `faceTexture` is flipped so that U=1 always sits on the
 *     opening-direction side; both sides of the plate therefore show the
 *     same "this is where the door retracts to" cue.
 *
 * Slide length is always 1 cell plus 1 pixel (≈ 1/64 cell) — that extra
 * pixel guarantees the plate is fully behind the pocket-side wall when
 * `currentOffset = 1.0`, even after texture filtering. No `slideDistance`
 * knob is exposed for that reason.
 *
 * The entity's `Transform.translation` (XY) marks the door's **cell centre**;
 * `Transform.scale` is intentionally ignored — the cell is always one tile.
 * The kinematic Box2D body, when present, tracks the plate position so the
 * player collides with the actual moving surface.
 *
 * Activation matches `RaycastPushWall`: built-in proximity-and-key
 * (`interactionKey`, `interactionRange`) with a Lua override when the key
 * is set to `0`. State machine: Idle → Opening → Open (`holdTime` seconds)
 * → Closing → Idle.
 */
struct OWL_API RaycastDoor {
	/**
	 * @brief
	 *  Cardinal direction the plate retracts toward when the door opens.
	 *
	 * The opening direction determines (i) the slide axis (`North`/`South`
	 * → Y axis, `East`/`West` → X axis), (ii) which cube faces carry the
	 * lateral texture (the faces with normals along the slide axis), and
	 * (iii) where the U=1 edge of the plate texture sits (always on the
	 * opening-direction side).
	 */
	enum struct OpeningDirection : uint8_t {
		North,///< Plate slides toward +Y; laterals on the cell's N and S inside faces.
		South,///< Plate slides toward −Y; laterals on the cell's N and S inside faces.
		East,///< Plate slides toward +X; laterals on the cell's E and W inside faces.
		West,///< Plate slides toward −X; laterals on the cell's E and W inside faces.
	};

	/**
	 * @brief
	 *  Lifecycle state of the door's slide animation.
	 */
	enum struct State : uint8_t {
		Idle,///< Closed and ready to open.
		Opening,///< Animating from closed toward open.
		Open,///< Fully open, holding for `holdTime` before closing.
		Closing,///< Animating from open back to closed.
	};

	/**
	 * @brief
	 *  Path to the `.owltileset` asset providing both face and lateral textures.
	 *
	 * Pointing this at the same tileset the world's tilemap uses lets the engine
	 * reuse the same `shared<Tileset>` (and its atlas texture) — no double-load.
	 * The plate texture is sampled at `faceTileIndex` in the atlas, the lateral
	 * at `lateralTileIndex`.
	 */
	std::string tilesetPath;
	/// Tile index for the moving plate (`faceTexture`). U=1 always sits on the opening-direction side.
	uint32_t faceTileIndex = 0;
	/// Tile index for the lateral jamb texture sampled on the two cube inside faces perpendicular to the opening direction.
	uint32_t lateralTileIndex = 0;
	/// Resolved tileset asset (runtime — populated by `Scene::resolveAllTilemapAssets`).
	shared<Tileset> tileset;
	/// Cardinal direction the plate retracts to when opening.
	OpeningDirection openingDirection = OpeningDirection::North;
	/// Open animation speed, in cells per second.
	float slideSpeed = 2.0f;
	/// Seconds the door stays Open before automatically Closing.
	float holdTime = 2.0f;
	/// Close animation speed, in cells per second.
	float closeSpeed = 2.0f;
	/**
	 * @brief
	 *  Engine-handled activation key, as a GLFW key code.
	 *
	 * When the primary player is within `interactionRange` cells of the door's
	 * **current** centre and presses this key, the door auto-activates. Set
	 * to `0` to disable the built-in handler entirely and drive activation
	 * from Lua via `door.activate(entity)`.
	 */
	input::KeyCode interactionKey = input::key::E;
	/// Maximum distance (cells) between the player centre and the door centre for the built-in key to fire.
	float interactionRange = 1.5f;

	/// Runtime state — not authored, advanced by `Scene::updateRaycastDynamicWalls`.
	State state = State::Idle;
	/// Current slide progress, in cells. `0` at closed, `1.0` at open (the renderer adds the +1-pixel margin).
	float currentOffset = 0.0f;
	/// Hold timer (seconds remaining in the Open state).
	float holdTimer = 0.0f;
	/// Edge-detection latch: `interactionKey` was held last tick.
	bool keyHeldLastTick = false;
	/**
	 * @brief
	 *  Internal index into `PhysicCommand`'s body table for the kinematic plate body.
	 *
	 * Auto-assigned by `PhysicCommand::init` when the door has no explicit
	 * `PhysicBody` component. `0` means no auto-body (the user added their own
	 * `PhysicBody`, or physics has not been initialised yet).
	 */
	uint64_t bodyId = 0;

	/**
	 * @brief
	 *  Get the class title.
	 * @return The class title.
	 */
	static auto name() -> const char* { return "Raycast Door"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "RaycastDoor"; }

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
