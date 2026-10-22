/**
 * @file MeshPreview.h
 * @author Silmaen
 * @date 21/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <renderer/Camera.h>
#include <renderer/Material.h>
#include <renderer/MeshCache.h>
#include <renderer/gpu/Framebuffer.h>

#include <cstdint>

namespace owl::nest::viewer {

/// How a mesh preview colours the surface.
enum struct MeshShading : uint8_t {
	Textured,///< The material: base-colour texture and factor, lit.
	Shaded,///< A plain light grey, lit: the shape alone.
};

/// Orbit of the preview camera around the centre of the mesh bounds.
struct MeshOrbit {
	/// Turn around the vertical axis, in radians (0 looks down -Z).
	float yaw = 0.7853982f;
	/// Tilt, in radians (negative looks down on the mesh).
	float pitch = -0.45f;
	/// Distance factor: 1 fits the bounding sphere in the view, larger moves away.
	float zoom = 1.f;
};

/**
 * @brief
 *  Camera of a mesh preview: perspective, orbiting the centre of the bounds and far enough to show them whole.
 * @param[in] iGeometry The geometry framed (its bounds).
 * @param[in] iOrbit The orbit of the camera.
 * @param[in] iAspect Width over height of the view.
 * @param[in] iVulkanClip Use the Vulkan clip space (depth from 0 to 1, Y down).
 * @return The camera.
 */
[[nodiscard]] auto makeMeshPreviewCamera(const renderer::MeshGeometry& iGeometry, const MeshOrbit& iOrbit,
										 float iAspect, bool iVulkanClip) -> renderer::Camera;

/**
 * @brief
 *  Render a mesh alone into a framebuffer (cleared first), lit from the side of the viewer.
 * @param[in,out] ioTarget The framebuffer: colour, entity id and depth attachments.
 * @param[in,out] ioMesh The mesh (its GPU mesh is built on first use).
 * @param[in] iOrbit The orbit of the camera.
 * @param[in] iShading How the surface is coloured.
 */
void renderMeshPreview(renderer::gpu::Framebuffer& ioTarget, renderer::MeshResource& ioMesh, const MeshOrbit& iOrbit,
					   MeshShading iShading);

/// Shape a material preview is drawn on.
enum struct PreviewShape : uint8_t {
	Sphere,///< A unit sphere.
	Cube,///< A cube.
};

/**
 * @brief
 *  Build the mesh of a preview shape (one submesh, with normals and texture coordinates).
 * @param[in] iShape The shape.
 * @return The mesh, null when it cannot be built.
 */
[[nodiscard]] auto makePreviewShape(PreviewShape iShape) -> shared<renderer::MeshResource>;

/**
 * @brief
 *  Render a mesh alone into a framebuffer with a given material, lit like `renderMeshPreview`.
 * @param[in,out] ioTarget The framebuffer: colour, entity id and depth attachments.
 * @param[in,out] ioMesh The mesh (its first submesh is drawn).
 * @param[in] iOrbit The orbit of the camera.
 * @param[in] iMaterial The material, kept alive by the caller until the call returns.
 */
void renderMaterialPreview(renderer::gpu::Framebuffer& ioTarget, renderer::MeshResource& ioMesh,
						   const MeshOrbit& iOrbit, const renderer::Material& iMaterial);

}// namespace owl::nest::viewer
