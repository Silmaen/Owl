/**
 * @file MeshRenderer.h
 * @author Silmaen
 * @date 21/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Serializer.h"
#include "math/vectors.h"
#include "renderer/gpu/Texture.h"

#include <filesystem>

namespace owl::renderer {
class MeshResource;
}// namespace owl::renderer

namespace owl::scene::component {

/**
 * @brief
 *  Component drawing a static mesh file (`.gltf`, `.glb` or `.obj`) at the entity transform, in 3D.
 *
 * Drawn by a `RendererMesh` layer of the render stack. The mesh is loaded once per file by the engine context's
 * `renderer::MeshCache` and shared with every entity drawing the same file. The colour multiplies the base colour of
 * the file's material; the texture, when set, replaces the material's base-colour texture.
 *
 * On-disk form:
 * ```yaml
 * MeshRenderer:
 *   mesh: meshes/crate.glb
 *   color: [1, 1, 1, 1]
 *   texture: nam:crate_alt
 *   transparent: false
 * ```.
 */
struct OWL_API MeshRenderer {
	/// Asset path of the mesh file.
	std::filesystem::path meshPath;
	/// Tint multiplied with the material base colour (its alpha matters with `transparent`).
	math::vec4 color{1.f, 1.f, 1.f, 1.f};
	/// Texture replacing the material's base-colour texture; null keeps the material's.
	shared<renderer::gpu::Texture2D> texture;
	/// True to draw with alpha blending, sorted back-to-front after the opaque meshes.
	bool transparent = false;
	/// The mesh resolved from `meshPath` before the next draw (runtime only, not serialized; reset it to reload).
	shared<renderer::MeshResource> mesh;

	/**
	 * @brief
	 *  Get the class title.
	 * @return The class title.
	 */
	static auto name() -> const char* { return "Mesh Renderer"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "MeshRenderer"; }

	/**
	 * @brief
	 *  Write this component to a YAML context.
	 * @param[in] iOut The YAML context.
	 */
	void serialize(const core::Serializer& iOut) const;

	/**
	 * @brief
	 *  Read this component from YAML node.
	 * @param[in] iNode The YAML node to read.
	 */
	void deserialize(const core::Serializer& iNode);

	/**
	 * @brief
	 *  Change the mesh file and drop the resolved mesh, so the next draw loads the new one.
	 * @param[in] iPath The asset path.
	 */
	void setMeshPath(const std::filesystem::path& iPath);
};

}// namespace owl::scene::component
