/**
 * @file VoxelWorld.h
 * @author Silmaen
 * @date 04/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/Serializer.h"
#include "data/voxel/TerrainGenerator.h"
#include "data/voxel/VoxelWorld.h"
#include "math/vectors.h"
#include "scene/Tileset.h"

#include <filesystem>
#include <unordered_set>

namespace owl::scene::component {

/**
 * @brief
 *  A block-based voxel world attached to an entity, drawn by `RendererVoxel`.
 *
 * Holds the authored voxel data — a `BlockRegistry` (the block palette) and a
 * `data::voxel::VoxelWorld` (the chunks) — plus the tileset atlas the block faces
 * index into and the directional-light settings the voxel renderer needs. A
 * block's per-face `faceTexture` is a tile index into `tilesetPath`'s atlas grid;
 * the renderer maps it to the tile's UV sub-rect. The entity must carry a
 * `RendererTag` routing it to a `RendererVoxel` layer for the world to appear. The
 * data is serialized inline in the scene (registry as a nested YAML string, chunks
 * run-length encoded); a standalone `.owlvoxel` asset format is a later refinement.
 */
struct OWL_API VoxelWorld {
	/// Block palette resolving ids to render kind, collision and per-face atlas tile index.
	data::voxel::BlockRegistry registry;
	/// The voxel data (sparse chunk map).
	data::voxel::VoxelWorld world;
	/// Path to the `.owltileset` atlas whose tiles the block faces index into.
	std::filesystem::path tilesetPath;
	/// Runtime-resolved tileset atlas (loaded from `tilesetPath` during scene resolve; not serialized).
	shared<Tileset> tileset;
	/// World-space direction the sun light travels.
	math::vec3 sunDirection{-0.4f, -1.f, -0.6f};
	/// Ambient light colour added before the directional term.
	math::vec3 ambient{0.35f, 0.35f, 0.4f};
	/// When true, chunks are streamed in/out around the camera from `terrain` instead of being authored by hand.
	bool proceduralTerrain = false;
	/// Procedural terrain parameters (seed, height field, caves, block ids) used when `proceduralTerrain` is true.
	data::voxel::TerrainParams terrain;
	/// Horizontal streaming radius in chunks around the camera (X/Z).
	int32_t streamRadius = 4;
	/// Vertical streaming half-extent in chunks around the camera (Y).
	int32_t streamHeight = 2;
	/// When true, the mesher bakes per-vertex ambient occlusion into block edges (darkens concave corners).
	bool ambientOcclusion = true;
	/// Runtime set of chunk keys currently being generated asynchronously (not serialized; cleared on regenerate).
	std::unordered_set<uint64_t> pendingChunks;

	/**
	 * @brief
	 *  Get the display name for this component.
	 * @return The display name.
	 */
	static auto name() -> const char* { return "Voxel World"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "VoxelWorld"; }

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
