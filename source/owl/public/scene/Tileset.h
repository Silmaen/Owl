/**
 * @file Tileset.h
 * @author Silmaen
 * @date 02/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "math/vectors.h"
#include "renderer/gpu/Texture.h"

#include <array>
#include <filesystem>
#include <string>

namespace owl::scene {

/**
 * @brief Per-tile metadata stored alongside a `Tileset`.
 *
 * One entry per tile slot in the atlas (row-major from top-left). Holds
 * authoring-time annotations that drive runtime behaviour: collision flag for
 * physics generation, optional designer-friendly name shown in the editor's
 * tile palette.
 */
struct OWL_API TileMeta {
	/// Whether this tile produces a static collider when present in a tilemap.
	bool collidable = false;
	/// Optional human-readable name (shown in the tile palette tooltip).
	std::string name;
};

/**
 * @brief Reusable tile atlas asset (`.owltileset`).
 *
 * A `Tileset` pairs an atlas texture with a fixed-size grid (`tileWidth × tileHeight`
 * cells, `columns × rows` slots) and one `TileMeta` per slot. Tilemaps reference
 * tiles into a tileset via row-major 0-based indices; index `-1` (any negative)
 * marks an empty cell.
 *
 * The on-disk format is YAML with the extension `.owltileset`:
 * ```yaml
 * Tileset: <name>
 * Version: 1
 * texture: <texture serialized name>     # optional
 * tileWidth: 32
 * tileHeight: 32
 * columns: 8
 * rows: 8
 * tiles:                                  # optional, only entries that differ from default
 *   - { index: 5, collidable: true, name: "wall" }
 *   - { index: 12, collidable: true }
 * ```
 *
 * Default `TileMeta` (every flag false, empty name) is implied for any slot
 * not listed under `tiles:` so authoring stays sparse — only special tiles
 * are written.
 */
class OWL_API Tileset final {
public:
	Tileset() = default;
	~Tileset() = default;
	Tileset(const Tileset&) = default;
	Tileset(Tileset&&) = default;
	auto operator=(const Tileset&) -> Tileset& = default;
	auto operator=(Tileset&&) -> Tileset& = default;

	/// @brief File extension used by `.owltileset` assets (with leading dot).
	static auto fileExtension() -> const char* { return ".owltileset"; }

	/// Atlas texture (may be null while authoring).
	shared<renderer::gpu::Texture2D> texture;
	/// Tile width in atlas pixels (>= 1).
	uint32_t tileWidth = 32;
	/// Tile height in atlas pixels (>= 1).
	uint32_t tileHeight = 32;
	/// Number of columns in the atlas (>= 1).
	uint32_t columns = 1;
	/// Number of rows in the atlas (>= 1).
	uint32_t rows = 1;
	/// Sampler filter applied to the atlas texture after load. `Nearest` for
	/// pixel-art / raycaster atlases (no filtering, no mipmaps); `Linear`
	/// (the default) for everything else.
	renderer::gpu::FilterMode filterMode = renderer::gpu::FilterMode::Linear;
	/// Per-tile metadata, row-major. Resized to `columns * rows` on load and on `resize()`.
	std::vector<TileMeta> tiles;

	/**
	 * @brief Total number of tile slots (`columns * rows`).
	 * @return The slot count.
	 */
	[[nodiscard]] auto tileCount() const -> uint32_t { return columns * rows; }

	/**
	 * @brief Resize the atlas grid and reset per-tile metadata to defaults.
	 *
	 * Any previously stored metadata is discarded — caller is expected to
	 * re-author after a resize.
	 * @param[in] iColumns New column count (>= 1).
	 * @param[in] iRows New row count (>= 1).
	 */
	void resize(uint32_t iColumns, uint32_t iRows);

	/**
	 * @brief Compute the four UV corners for a given tile index.
	 *
	 * UVs map the tile to the atlas using `tileWidth × tileHeight` slots. The
	 * returned order matches `Quad2DData::textureCoords`: bottom-left, bottom-right,
	 * top-right, top-left (all in [0, 1]).
	 * @param[in] iIndex 0-based tile index. Out-of-range indices return the full atlas (0..1).
	 * @return Four UV corners, ready to feed into `Quad2DData::textureCoords`.
	 */
	[[nodiscard]] auto getTileUv(uint32_t iIndex) const -> std::array<math::vec2, 4>;

	/**
	 * @brief Look up the metadata for a tile, returning a default for out-of-range indices.
	 * @param[in] iIndex 0-based tile index.
	 * @return The tile metadata.
	 */
	[[nodiscard]] auto getTileMeta(uint32_t iIndex) const -> const TileMeta&;

	/**
	 * @brief Whether the tile at the given index is flagged collidable.
	 * @param[in] iIndex 0-based tile index.
	 * @return True if the slot exists and is collidable.
	 */
	[[nodiscard]] auto isCollidable(uint32_t iIndex) const -> bool;

	/**
	 * @brief Serialize the tileset to a YAML string.
	 * @param[in] iName Optional display name written under the `Tileset:` key.
	 * @return The YAML document as a string.
	 */
	[[nodiscard]] auto serializeToString(std::string_view iName = "") const -> std::string;

	/**
	 * @brief Populate the tileset from a YAML string (tileset is reset on success).
	 * @param[in] iYaml The YAML document.
	 * @return True on success, false on malformed input (tileset left unchanged).
	 */
	[[nodiscard]] auto deserializeFromString(std::string_view iYaml) -> bool;

	/**
	 * @brief Save the tileset to disk.
	 * @param[in] iPath Destination file (`.owltileset` is conventional).
	 * @param[in] iName Optional display name; defaults to the file stem.
	 * @return True on success.
	 */
	[[nodiscard]] auto saveToFile(const std::filesystem::path& iPath, std::string_view iName = "") const -> bool;

	/**
	 * @brief Load the tileset from disk.
	 * @param[in] iPath Source file.
	 * @return True on success.
	 */
	[[nodiscard]] auto loadFromFile(const std::filesystem::path& iPath) -> bool;
};

}// namespace owl::scene
