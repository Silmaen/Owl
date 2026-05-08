/**
 * @file Tilemap.h
 * @author Silmaen
 * @date 02/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Serializer.h"
#include "math/vectors.h"

#include <filesystem>
#include <string>
#include <vector>

namespace owl::scene {

class TilemapAsset;
}// namespace owl::scene

namespace owl::scene::component {
/// Sentinel index meaning "no tile in this cell". Stored in `TilemapLayer::tiles`.
constexpr int32_t g_EmptyTileIndex = -1;

/**
 * @brief
 *  One layer of a `TilemapAsset`.
 *
 * Holds its own `width × height` grid of tile indices into the asset's
 * shared tileset. Layers stack back-to-front in the order they are stored on
 * the asset (index 0 renders first, last index renders on top). Each layer
 * has an independent visibility flag and parallax factor used at render time.
 *
 * The parallax factor is a per-axis multiplier applied to the camera position
 * when the layer is drawn: `(1, 1)` is the default (moves with the world),
 * `(0.5, 0.5)` scrolls slower than the world (typical background), `(0, 0)`
 * is camera-locked (sky / fullscreen overlay).
 *
 * Lives inside `scene::component::*` to keep the type in the existing
 * include path; the actual data container is `scene::TilemapAsset`.
 */
struct OWL_API TilemapLayer {
	/// Designer-friendly layer name (e.g. "background", "ground", "props").
	std::string name;
	/// Whether the layer is rendered.
	bool visible = true;
	/// Per-axis parallax factor (1 = move with world, 0 = camera-locked).
	math::vec2 parallax{1.f, 1.f};
	/// Row-major flat grid of tile indices into the tileset; `g_EmptyTileIndex` = empty.
	std::vector<int32_t> tiles;
};

/**
 * @brief
 *  Reference to a `TilemapAsset` (`.owltilemap`) attached to an entity.
 *
 * Since v0.2.0 the grid data (width / height / layers / tileset) lives in a
 * standalone `scene::TilemapAsset` so the same level can be reused across
 * scenes and authored in a dedicated tilemap document. This component is the
 * scene-side handle: a relative path on disk plus the runtime-resolved
 * asset shared pointer. The path is what gets serialized; the asset is
 * resolved on scene load (mirrors the texture-loading pattern used by
 * `SpriteRenderer`).
 *
 * When the path is empty but `asset` is non-null (e.g. an in-memory tilemap
 * built programmatically by tests, or a legacy inline tilemap that has not
 * yet been saved through the editor), the asset's data is serialized inline
 * under the `inline:` key for back-compat. The supported path is the
 * preferred form for new scenes.
 *
 * On-disk form (preferred):
 * ```yaml
 * Tilemap:
 *   tilemapPath: assets/tilemaps/dungeon_l1.owltilemap
 * ```.
 *
 * On-disk form (legacy / in-memory fallback):
 * ```yaml
 * Tilemap:
 *   inline:
 *     tilesetPath: assets/tilesets/dungeon.owltileset
 *     width: 32
 *     height: 16
 *     cellSize: 1.0
 *     layers:
 *       - { name: ground, tiles: "0,0,1,1,..." }
 * ```.
 */
struct OWL_API Tilemap {
	/// Relative path (from the project root) to the `.owltilemap` asset.
	std::filesystem::path tilemapPath;
	/// Resolved asset (runtime only — built from the path or from inline data on load).
	shared<scene::TilemapAsset> asset;

	/**
	 * @brief
	 *  Get the class title.
	 * @return The class title.
	 */
	static auto name() -> const char* { return "Tilemap"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "Tilemap"; }

	/**
	 * @brief
	 *  Write this component to a YAML context.
	 * @param[in] iOut The YAML context.
	 */
	void serialize(const core::Serializer& iOut) const;

	/**
	 * @brief
	 *  Read this component from a YAML node.
	 *
	 * Accepts both the preferred `tilemapPath`-only form and the legacy inline
	 * form (either flat or nested under `inline:`). Inline data is moved into
	 * a fresh in-memory `TilemapAsset`; the path is left empty and the asset
	 * is considered "unsaved" until the user gives it a path through the
	 * tilemap editor.
	 * @param[in] iNode The YAML node to read.
	 */
	void deserialize(const core::Serializer& iNode);
};

}// namespace owl::scene::component
