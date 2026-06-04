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
#include "data/voxel/VoxelWorld.h"
#include "math/vectors.h"

#include <filesystem>
#include <vector>

namespace owl::scene::component {

/**
 * @brief
 *  A block-based voxel world attached to an entity, drawn by `RendererVoxel`.
 *
 * Holds the authored voxel data — a `BlockRegistry` (the block palette) and a
 * `data::voxel::VoxelWorld` (the chunks) — plus the per-block texture paths and
 * the directional-light settings the voxel renderer needs. The entity must
 * carry a `RendererTag` routing it to a `RendererVoxel` layer for the world to
 * appear. The data is serialized inline in the scene (registry as a nested YAML
 * string, chunks run-length encoded); a standalone `.owlvoxel` asset format is a
 * later refinement.
 */
struct OWL_API VoxelWorld {
	/// Block palette resolving ids to render kind, collision and per-face texture slot.
	data::voxel::BlockRegistry registry;
	/// The voxel data (sparse chunk map).
	data::voxel::VoxelWorld world;
	/// Texture file paths indexed by block texture slot (slot `i` is sampled by faces whose `faceTexture == i`).
	std::vector<std::filesystem::path> blockTextures;
	/// World-space direction the sun light travels.
	math::vec3 sunDirection{-0.4f, -1.f, -0.6f};
	/// Ambient light colour added before the directional term.
	math::vec3 ambient{0.35f, 0.35f, 0.4f};

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
