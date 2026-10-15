/**
 * @file IsometricObjects.h
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/expected.h"
#include "math/vectors.h"
#include "renderer/gpu/Texture.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace owl::renderer {

/**
 * @brief
 *  Why an object sheet could not be read.
 */
enum struct IsometricObjectError : uint8_t {
	/// The text is not a YAML / JSON document.
	InvalidDocument,
	/// The document has no `buildings` map, or it is empty.
	NoObjects,
	/// An object or one of its slices is malformed (missing key, empty footprint, empty rectangle).
	InvalidObject,
};

/**
 * @brief
 *  One footprint tile of a sliced object: a sprite rectangle of the sheet and the 3D box it occupies.
 *
 * The sprite is an upright billboard anchored at the front corner of its tile, so each slice is depth-correct on its
 * own and a vehicle can pass between the slices of a wide building.
 */
struct OWL_API IsometricSlice {
	/// Footprint tile of the slice, in tiles from the back corner of the object.
	math::vec2ui cell{0u, 0u};
	/// Rectangle in the sheet: x, y, width, height, in sheet pixels (Y down).
	math::vec4 rect{0.f, 0.f, 64.f, 128.f};
	/// Anchor inside the rectangle, in sheet pixels from its top left; the point put on the foot.
	math::vec2 anchor{32.f, 128.f};
	/// Minimum corner of the 3D box, in tiles from the back corner of the object.
	math::vec3 boxMin{0.f, 0.f, 0.f};
	/// Maximum corner of the 3D box, in tiles from the back corner of the object.
	math::vec3 boxMax{1.f, 1.f, 1.f};
};

/**
 * @brief
 *  A building or prop of the sheet: its footprint and its slices.
 */
struct OWL_API IsometricObjectDef {
	/// Name of the object in the sheet.
	std::string name;
	/// Footprint in tiles (x, y).
	math::vec2ui footprint{1u, 1u};
	/// Slices, one per footprint tile.
	std::vector<IsometricSlice> slices;

	/**
	 * @brief
	 *  Get the height of the object: the highest point of its slice boxes.
	 * @return The height, in tiles of height.
	 */
	[[nodiscard]] auto getHeight() const -> float;
};

/**
 * @brief
 *  The objects of one sheet (`buildings.json` of the atlas generator) and the texture they are cut from.
 */
class OWL_API IsometricObjectSet {
public:
	/**
	 * @brief
	 *  Default constructor: an empty set.
	 */
	IsometricObjectSet() = default;

	/**
	 * @brief
	 *  Read a sheet description.
	 *
	 * The text is the JSON written by `tools/iso_atlas`: `zoom` (`1x`, `2x`, `0.5x` or `half`) and `buildings`, a
	 * map of name to `footprint` and `slices` (`cell`, `rect`, `anchor`, `box`).
	 * @param[in] iText The JSON (or YAML) text.
	 * @return The set without its texture, or the reason it was refused.
	 */
	[[nodiscard]] static auto parse(std::string_view iText) -> owl::expected<IsometricObjectSet, IsometricObjectError>;

	/**
	 * @brief
	 *  Find an object by name.
	 * @param[in] iName The object name.
	 * @return The object, or null when the sheet has none of that name.
	 */
	[[nodiscard]] auto find(const std::string& iName) const -> const IsometricObjectDef*;

	/**
	 * @brief
	 *  Get every object of the sheet.
	 * @return The objects, sorted by name.
	 */
	[[nodiscard]] auto getObjects() const -> const std::vector<IsometricObjectDef>& { return m_objects; }

	/**
	 * @brief
	 *  Get the tile width of the sheet art, in pixels (64 at `1x`, 128 at `2x`, 32 at `0.5x`).
	 * @return The tile width.
	 */
	[[nodiscard]] auto getTileWidth() const -> float { return m_tileWidth; }

	/**
	 * @brief
	 *  Set the texture the slices are cut from.
	 * @param[in] iTexture The sheet texture.
	 */
	void setTexture(const shared<gpu::Texture2D>& iTexture) { m_texture = iTexture; }

	/**
	 * @brief
	 *  Get the texture the slices are cut from.
	 * @return The sheet texture, null until set.
	 */
	[[nodiscard]] auto getTexture() const -> const shared<gpu::Texture2D>& { return m_texture; }

private:
	/// Objects, sorted by name.
	std::vector<IsometricObjectDef> m_objects;
	/// Tile width of the sheet art, in pixels.
	float m_tileWidth = 64.f;
	/// Sheet texture.
	shared<gpu::Texture2D> m_texture = nullptr;
};

}// namespace owl::renderer
