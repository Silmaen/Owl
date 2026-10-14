/**
 * @file TilemapAsset.h
 * @author Silmaen
 * @date 08/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/FormatVersion.h"
#include "scene/component/Tilemap.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace owl::scene {

class Tileset;

/**
 * @brief
 *  Reusable tilemap level asset (`.owltilemap`).
 *
 * Extracts the grid data of a tilemap (size, layers, per-cell tile indices,
 * tileset reference) into a standalone authoring document so the same level
 * can be referenced by multiple scenes and edited in a dedicated tilemap
 * editor instead of being inlined into a single `.owl` scene.
 *
 * The on-disk format (version 2) is a YAML header, the YAML end-of-document line `...`, then a binary payload
 * holding the non-empty chunks of every layer (little-endian, checksummed, see `doc/pages/scene.md`):
 * ```yaml
 * Tilemap: <name>
 * FormatVersion: 2
 * tilesetPath: assets/tilesets/dungeon.owltileset
 * width: 32
 * height: 16
 * cellSize: 1.0
 * layers:
 *   - name: ground
 *     visible: false
 *     parallax: [0.5, 0.5]
 *   - name: props
 * payloadBytes: 10268
 * ...
 * <binary payload>
 * ```.
 *
 * Version 1 files (pure YAML, one comma-separated `tiles` string per layer) load through the migration chain and
 * are written back as version 2.
 *
 * The `TilemapLayer` value type is reused from
 * `scene::component::Tilemap` — both the inline-on-component path
 * (transitional) and this asset format share the same in-memory
 * representation.
 */
class OWL_API TilemapAsset final {
public:
	TilemapAsset() = default;

	~TilemapAsset() = default;

	/**
	 * @brief
	 *  Copy constructor.
	 */
	TilemapAsset(const TilemapAsset&) = default;

	/**
	 * @brief
	 *  Move constructor.
	 */
	TilemapAsset(TilemapAsset&&) = default;

	/**
	 * @brief
	 *  Copy assignment operator.
	 * @return A reference to this object.
	 */
	auto operator=(const TilemapAsset&) -> TilemapAsset& = default;

	/**
	 * @brief
	 *  Move assignment operator.
	 * @return A reference to this object.
	 */
	auto operator=(TilemapAsset&&) -> TilemapAsset& = default;

	/**
	 * @brief
	 *  File extension used by `.owltilemap` assets (with leading dot).
	 * @return The extension literal (`".owltilemap"`).
	 */
	static auto fileExtension() noexcept -> const char* { return ".owltilemap"; }

	/// Largest grid width or height: a file announcing more is refused, `resize` clamps to it.
	static constexpr uint32_t maxGridSide = 16384;

	/**
	 * @brief
	 *  Format descriptor of the `.owltilemap` files, with its migration chain.
	 * @return The TilemapAsset format.
	 */
	[[nodiscard]] static auto format() -> const core::DocumentFormat&;

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
	std::vector<scene::component::TilemapLayer> layers;

	/**
	 * @brief
	 *  Resize all existing layers to a new grid shape, padding with empty.
	 *
	 * Existing tile data is preserved for cells that fit in the new bounds; new
	 * cells are filled with `g_EmptyTileIndex`. Width / height are clamped to
	 * `[1, maxGridSide]`.
	 * @param[in] iWidth New grid width (>= 1).
	 * @param[in] iHeight New grid height (>= 1).
	 */
	void resize(uint32_t iWidth, uint32_t iHeight);

	/**
	 * @brief
	 *  Append a new empty layer with the given name.
	 *
	 * The new layer is created `width × height` cells, all empty, visible, and
	 * with `(1, 1)` parallax. Returns a reference to the new layer.
	 * @param[in] iName The layer name.
	 * @return The newly created layer.
	 */
	auto addLayer(const std::string& iName) -> scene::component::TilemapLayer&;

	/**
	 * @brief
	 *  Get a tile index from a layer.
	 * @param[in] iLayer The 0-based layer index.
	 * @param[in] iX Cell x (0-based, left to right).
	 * @param[in] iY Cell y (0-based, top to bottom).
	 * @return The tile index, or `g_EmptyTileIndex` for out-of-grid cells.
	 */
	[[nodiscard]] auto getTile(uint32_t iLayer, uint32_t iX, uint32_t iY) const -> int32_t;

	/**
	 * @brief
	 *  Set a tile in a layer.
	 *
	 * Out-of-grid cells are silently ignored. Out-of-range layer indices are a no-op.
	 * @param[in] iLayer The 0-based layer index.
	 * @param[in] iX Cell x.
	 * @param[in] iY Cell y.
	 * @param[in] iValue Tile index, or `g_EmptyTileIndex` to clear.
	 */
	void setTile(uint32_t iLayer, uint32_t iX, uint32_t iY, int32_t iValue);

	/**
	 * @brief
	 *  Memory used by the cells of every layer.
	 * @return The size in bytes (`TilemapLayer::getStorageBytes` summed).
	 */
	[[nodiscard]] auto getStorageBytes() const -> size_t;

	/**
	 * @brief
	 *  Whether two assets hold the same tileset path, grid, cell size and layers (the resolved tileset ignored).
	 * @param[in] iOther The other asset.
	 * @return True when the assets have the same content.
	 */
	[[nodiscard]] auto hasSameContent(const TilemapAsset& iOther) const -> bool;

	/**
	 * @brief
	 *  Serialize the tilemap asset in the `.owltilemap` format (YAML header and binary payload).
	 * @param[in] iName Optional display name written under the `Tilemap:` key.
	 * @return The file content (binary after the header).
	 */
	[[nodiscard]] auto serializeToString(std::string_view iName = "") const -> std::string;

	/**
	 * @brief
	 *  Populate the tilemap asset from a `.owltilemap` content, version 1 (YAML) or 2 (YAML header and binary).
	 * @param[in] iText The file content.
	 * @return True on success, false on malformed, truncated or corrupted input (asset left unchanged).
	 */
	[[nodiscard]] auto deserializeFromString(std::string_view iText) -> bool;

	/**
	 * @brief
	 *  Save the tilemap asset to disk. Atomic: a failed write keeps the previous file.
	 * @param[in] iPath Destination file (`.owltilemap` is conventional).
	 * @param[in] iName Optional display name; defaults to the file stem.
	 * @return True on success.
	 */
	[[nodiscard]] auto saveToFile(const std::filesystem::path& iPath, std::string_view iName = "") const -> bool;

	/**
	 * @brief
	 *  Load the tilemap asset from disk.
	 * @param[in] iPath Source file.
	 * @return True on success.
	 */
	[[nodiscard]] auto loadFromFile(const std::filesystem::path& iPath) -> bool;
};

}// namespace owl::scene
