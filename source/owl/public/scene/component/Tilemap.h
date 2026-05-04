/**
 * @file Tilemap.h
 * @author Silmaen
 * @date 02/05/2026
 * Copyright © 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Serializer.h"
#include "math/vectors.h"

#include <filesystem>
#include <string>
#include <vector>

namespace owl::scene {
class Tileset;
}// namespace owl::scene

namespace owl::scene::component {

/// Sentinel index meaning "no tile in this cell". Stored in `TilemapLayer::tiles`.
constexpr int32_t g_EmptyTileIndex = -1;

/**
 * @brief One layer of a `Tilemap`.
 *
 * Holds its own `width × height` grid of tile indices into the parent tilemap's
 * tileset. Layers stack back-to-front in the order they are stored on the
 * component (index 0 renders first, last index renders on top). Each layer
 * has an independent visibility flag and parallax factor used at render time.
 *
 * The parallax factor is a per-axis multiplier applied to the camera position
 * when the layer is drawn: `(1, 1)` is the default (moves with the world),
 * `(0.5, 0.5)` scrolls slower than the world (typical background), `(0, 0)`
 * is camera-locked (sky / fullscreen overlay).
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
 * @brief Grid of tiles using a shared `Tileset` asset.
 *
 * Replaces a fan of individual sprite entities with a single ECS component holding
 * a `width × height` grid (in cells) and one or more `TilemapLayer` entries.
 * Tiles are referenced by their 0-based row-major index into the tileset; cells
 * containing `g_EmptyTileIndex` are skipped during draw and physics generation.
 *
 * The tileset is referenced by relative path on disk and resolved at load time
 * into the runtime `tileset` shared pointer (mirrors the texture-loading
 * pattern used by `SpriteRenderer`).
 *
 * On-disk layout (excerpt):
 * ```yaml
 * Tilemap:
 *   tilesetPath: assets/tilesets/dungeon.owltileset
 *   width: 32
 *   height: 16
 *   cellSize: 1.0
 *   layers:
 *     - name: ground
 *       visible: true
 *       parallax: [1.0, 1.0]
 *       tiles: "0,0,1,1,...,-1,-1"   # comma-separated row-major
 *     - name: props
 *       ...
 * ```
 *
 * Tile data is emitted as a comma-separated string of integers to keep YAML
 * compact (a 64×64 grid is 4096 entries — far too noisy as a YAML sequence).
 */
struct OWL_API Tilemap {
	/// Relative path (from the project root) to the `.owltileset` asset.
	std::filesystem::path tilesetPath;
	/// Resolved tileset (runtime only, not serialized).
	shared<scene::Tileset> tileset;
	/// Grid width in cells (>= 1).
	uint32_t width = 16;
	/// Grid height in cells (>= 1).
	uint32_t height = 16;
	/// World-space size of one cell (uniform; tile aspect is handled by the renderer).
	float cellSize = 1.f;
	/// Ordered list of layers (back-to-front).
	std::vector<TilemapLayer> layers;

	/**
	 * @brief Resize all existing layers to a new grid shape, padding with empty.
	 *
	 * Existing tile data is preserved for cells that fit in the new bounds; new
	 * cells are filled with `g_EmptyTileIndex`. Width / height are clamped to
	 * at least 1.
	 * @param[in] iWidth New grid width (>= 1).
	 * @param[in] iHeight New grid height (>= 1).
	 */
	void resize(uint32_t iWidth, uint32_t iHeight);

	/**
	 * @brief Append a new empty layer with the given name.
	 *
	 * The new layer is created `width × height` cells, all empty, visible, and
	 * with `(1, 1)` parallax. Returns a reference to the new layer.
	 * @param[in] iName The layer name.
	 * @return The newly created layer.
	 */
	auto addLayer(const std::string& iName) -> TilemapLayer&;

	/**
	 * @brief Get a tile index from a layer (no bounds check on layer index).
	 * @param[in] iLayer The 0-based layer index.
	 * @param[in] iX Cell x (0-based, left to right).
	 * @param[in] iY Cell y (0-based, top to bottom).
	 * @return The tile index, or `g_EmptyTileIndex` for out-of-grid cells.
	 */
	[[nodiscard]] auto getTile(uint32_t iLayer, uint32_t iX, uint32_t iY) const -> int32_t;

	/**
	 * @brief Set a tile in a layer, growing the layer's storage if needed.
	 *
	 * Out-of-grid cells are silently ignored. Out-of-range layer indices throw
	 * `std::out_of_range` via vector access — callers must verify the layer
	 * exists before calling.
	 * @param[in] iLayer The 0-based layer index.
	 * @param[in] iX Cell x.
	 * @param[in] iY Cell y.
	 * @param[in] iValue Tile index, or `g_EmptyTileIndex` to clear.
	 */
	void setTile(uint32_t iLayer, uint32_t iX, uint32_t iY, int32_t iValue);

	/**
	 * @brief Get the class title.
	 * @return The class title.
	 */
	static auto name() -> const char* { return "Tilemap"; }

	/**
	 * @brief Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "Tilemap"; }

	/**
	 * @brief Write this component to a YAML context.
	 * @param[in] iOut The YAML context.
	 */
	void serialize(const core::Serializer& iOut) const;

	/**
	 * @brief Read this component from a YAML node.
	 * @param[in] iNode The YAML node to read.
	 */
	void deserialize(const core::Serializer& iNode);
};

}// namespace owl::scene::component
