/**
 * @file Block.h
 * @author Silmaen
 * @date 03/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace owl::data::voxel {

/// Numeric identifier of a block type inside a `BlockRegistry`. `0` is always air.
using BlockId = uint16_t;

/// The block id reserved for empty space (no geometry, no collision).
constexpr BlockId g_AirBlock = 0;

/**
 * @brief
 *  The six axis-aligned faces of a cubic block.
 *
 * Ordering is fixed and relied upon by per-face texture arrays and by the
 * chunk mesher: negative then positive along X, then Y (Y is up), then Z.
 */
enum struct BlockFace : uint8_t {
	XNeg = 0,///< Face whose outward normal points toward -X (west).
	XPos = 1,///< Face whose outward normal points toward +X (east).
	YNeg = 2,///< Face whose outward normal points toward -Y (down).
	YPos = 3,///< Face whose outward normal points toward +Y (up).
	ZNeg = 4,///< Face whose outward normal points toward -Z (north).
	ZPos = 5,///< Face whose outward normal points toward +Z (south).
};

/// Number of faces on a cubic block (always six).
constexpr uint32_t g_FaceCount = 6;

/**
 * @brief
 *  In-world orientation of a block, applied by the mesher as a face permutation.
 *
 * Orientation never changes a block's geometry (a voxel is always a unit cube)
 * nor its collision / opacity — it only remaps which per-face texture appears on
 * each world face, so a single block type can read as a pillar laid along any
 * axis (logs) or a horizontally-facing block (furnaces, stairs). `Identity` is
 * the default and leaves every face textured as authored.
 */
enum struct BlockOrientation : uint8_t {
	Identity = 0,///< Local axes match world axes (no rotation).
	YawCw90 = 1,///< Rotated a quarter turn about the up (+Y) axis.
	Yaw180 = 2,///< Rotated a half turn about the up (+Y) axis.
	YawCcw90 = 3,///< Rotated a quarter turn the other way about the up (+Y) axis.
	AxisX = 4,///< Pillar laid along world X: the top/bottom caps face ±X.
	AxisZ = 5,///< Pillar laid along world Z: the top/bottom caps face ±Z.
};

/// Number of distinct block orientations.
constexpr uint8_t g_OrientationCount = 6;

/**
 * @brief
 *  Resolve which block-local face is shown at a given world face under an orientation.
 *
 * The mesher emits geometry with world-aligned normals and uses this to pick the
 * texture: the tile drawn on `iWorldFace` is `BlockType::faceTexture` of the
 * returned local face. `Identity` is the identity permutation.
 * @param[in] iWorldFace The world-aligned face being textured.
 * @param[in] iOrientation The block's orientation.
 * @return The block-local face whose texture appears at `iWorldFace`.
 */
[[nodiscard]] OWL_API auto orientedFace(BlockFace iWorldFace, BlockOrientation iOrientation) noexcept -> BlockFace;

/**
 * @brief
 *  Per-voxel metadata stored alongside the block id: orientation plus a free state byte.
 *
 * The `state` byte is opaque to the engine — gameplay and the editor may use it
 * for growth stages, on/off, water level, etc. Only `orientation` is interpreted
 * by the mesher.
 */
struct BlockMeta {
	/// In-world orientation applied by the mesher.
	BlockOrientation orientation = BlockOrientation::Identity;
	/// Free gameplay / editor state byte (not interpreted by the mesher).
	uint8_t state = 0;

	auto operator==(const BlockMeta&) const noexcept -> bool = default;
};

/// Block metadata packed into 16 bits for storage: low byte orientation, high byte state.
using PackedMeta = uint16_t;

/// The default metadata word (`Identity` orientation, zero state); the implicit value of every block.
constexpr PackedMeta g_DefaultMeta = 0;

/**
 * @brief
 *  Pack block metadata into its 16-bit storage word.
 * @param[in] iMeta The metadata to pack.
 * @return The packed word (low byte orientation, high byte state).
 */
[[nodiscard]] constexpr auto packMeta(const BlockMeta iMeta) noexcept -> PackedMeta {
	return static_cast<PackedMeta>(static_cast<uint8_t>(iMeta.orientation)) |
		   static_cast<PackedMeta>(static_cast<PackedMeta>(iMeta.state) << 8);
}

/**
 * @brief
 *  Unpack a 16-bit storage word back into block metadata.
 * @param[in] iPacked The packed word produced by `packMeta`.
 * @return The decoded metadata.
 */
[[nodiscard]] constexpr auto unpackMeta(const PackedMeta iPacked) noexcept -> BlockMeta {
	return BlockMeta{.orientation = static_cast<BlockOrientation>(iPacked & 0xFFu),
					 .state = static_cast<uint8_t>(iPacked >> 8)};
}

/**
 * @brief
 *  How a block participates in meshing, lighting and sorting.
 *
 * Drives face culling (an opaque neighbour hides the shared face) and which
 * mesh pass a block lands in (opaque blocks are batched in the depth-tested
 * solid pass; transparent / water blocks are emitted into a separate
 * back-to-front pass in a later release).
 */
enum struct BlockRenderKind : uint8_t {
	Air = 0,///< Empty space: never meshed, never culls a neighbour face.
	Opaque = 1,///< Fully opaque: culls the shared face of an adjacent block.
	Transparent = 2,///< Alpha-blended (leaves, glass): does not cull opaque neighbours.
	Water = 3,///< Translucent volume rendered in the transparent pass with its own rules.
};

/**
 * @brief
 *  Authoring-time description of one block type.
 *
 * A `BlockType` is pure data: it pairs a designer-facing name with the flags
 * that drive runtime behaviour (render kind, collision) and the per-face
 * texture indices used by the renderer. Texture indices reference slots in the
 * voxel atlas; resolving them to GPU textures is a rendering concern handled by
 * `RendererVoxel`, not by this data model.
 */
struct OWL_API BlockType {
	/// Designer-facing unique name (used by the editor palette and by lookups).
	std::string name;
	/// How the block is meshed, lit and sorted.
	BlockRenderKind renderKind = BlockRenderKind::Opaque;
	/// Whether the block produces a collider (independent of its visual kind).
	bool solid = true;
	/// Per-face atlas tile indices, indexed by `BlockFace` (XNeg, XPos, YNeg, YPos, ZNeg, ZPos).
	std::array<uint16_t, g_FaceCount> faceTextures{};

	/**
	 * @brief
	 *  Whether the block fully occludes the faces of its neighbours.
	 * @return True for the `Opaque` render kind only.
	 */
	[[nodiscard]] auto isOpaque() const noexcept -> bool { return renderKind == BlockRenderKind::Opaque; }

	/**
	 * @brief
	 *  Get the atlas tile index used to texture one face.
	 * @param[in] iFace The face to query.
	 * @return The atlas tile index assigned to that face.
	 */
	[[nodiscard]] auto faceTexture(BlockFace iFace) const noexcept -> uint16_t {
		return faceTextures[static_cast<size_t>(iFace)];
	}

	/**
	 * @brief
	 *  Assign the same atlas tile index to all six faces.
	 * @param[in] iTexture The atlas tile index to apply to every face.
	 */
	void setAllFaces(uint16_t iTexture) noexcept { faceTextures.fill(iTexture); }
};

/**
 * @brief
 *  Ordered table of block types, addressed by `BlockId`.
 *
 * Slot `0` is always the built-in air block and cannot be removed. New block
 * types are appended and receive the next free id; ids are therefore stable for
 * the lifetime of the registry, which is what lets `Chunk` store bare `BlockId`
 * values and resolve metadata here on demand.
 *
 * The on-disk format is YAML; air is implicit and never written:
 * ```yaml
 * BlockRegistry: <name>
 * Version: 1
 * Blocks:
 *   - id: 1
 *     name: stone
 *     render: Opaque
 *     solid: true
 *     faces: [3, 3, 3, 3, 3, 3]
 *   - id: 2
 *     name: water
 *     render: Water
 *     solid: false
 *     faces: [9, 9, 9, 9, 9, 9]
 * ```.
 */
class OWL_API BlockRegistry final {
public:
	BlockRegistry();

	~BlockRegistry() = default;

	BlockRegistry(const BlockRegistry&) = default;

	BlockRegistry(BlockRegistry&&) = default;

	auto operator=(const BlockRegistry&) -> BlockRegistry& = default;

	auto operator=(BlockRegistry&&) -> BlockRegistry& = default;

	/**
	 * @brief
	 *  Append a new block type and return its freshly allocated id.
	 * @param[in] iType The block type to register.
	 * @return The id assigned to the new block type.
	 */
	auto registerBlock(const BlockType& iType) -> BlockId;

	/**
	 * @brief
	 *  Look up a block type by id.
	 * @param[in] iId The block id to resolve.
	 * @return The matching block type, or the air block for out-of-range ids.
	 */
	[[nodiscard]] auto get(BlockId iId) const -> const BlockType&;

	/**
	 * @brief
	 *  Find the id of a block type by its name.
	 * @param[in] iName The block name to search for.
	 * @return The matching id, or `std::nullopt` if no block has that name.
	 */
	[[nodiscard]] auto findByName(std::string_view iName) const -> std::optional<BlockId>;

	/**
	 * @brief
	 *  Total number of registered block types, including air.
	 * @return The block type count (always >= 1).
	 */
	[[nodiscard]] auto count() const noexcept -> size_t { return m_types.size(); }

	/**
	 * @brief
	 *  Whether an id refers to the air block (empty space).
	 * @param[in] iId The block id to test.
	 * @return True for `g_AirBlock` and for any out-of-range id.
	 */
	[[nodiscard]] auto isAir(BlockId iId) const noexcept -> bool { return iId == g_AirBlock || iId >= m_types.size(); }

	/**
	 * @brief
	 *  Whether the block at an id occludes neighbour faces.
	 * @param[in] iId The block id to test.
	 * @return True if the block exists and is of the opaque render kind.
	 */
	[[nodiscard]] auto isOpaque(BlockId iId) const -> bool;

	/**
	 * @brief
	 *  Whether the block at an id produces a collider.
	 * @param[in] iId The block id to test.
	 * @return True if the block exists and is flagged solid.
	 */
	[[nodiscard]] auto isSolid(BlockId iId) const -> bool;

	/**
	 * @brief
	 *  Serialize the registry to a YAML string (air is implicit, never written).
	 * @param[in] iName Optional display name written under the `BlockRegistry:` key.
	 * @return The YAML document as a string.
	 */
	[[nodiscard]] auto serializeToString(std::string_view iName = "") const -> std::string;

	/**
	 * @brief
	 *  Populate the registry from a YAML string (registry is reset on success).
	 * @param[in] iYaml The YAML document.
	 * @return True on success, false on malformed input (registry left unchanged).
	 */
	[[nodiscard]] auto deserializeFromString(std::string_view iYaml) -> bool;

private:
	/// Block types indexed by id; slot 0 is always the built-in air block.
	std::vector<BlockType> m_types;
};

}// namespace owl::data::voxel
