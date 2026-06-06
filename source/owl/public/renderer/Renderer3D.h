/**
 * @file Renderer3D.h
 * @author Silmaen
 * @date 04/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "math/vectors.h"
#include "renderer/Camera.h"
#include "renderer/gpu/DrawData.h"
#include "renderer/gpu/Texture.h"

#include <span>

namespace owl::renderer {

/**
 * @brief
 *  One vertex of a generic 3D mesh uploaded to `Renderer3D`.
 *
 * The layout mirrors the `mesh3d.slang` vertex input: object-space position,
 * normal (for the directional-light term), texture coordinate, the index of the
 * texture slot to sample from the bound array, and an atlas sub-rect used by
 * frac-tiling shaders. The struct is tightly packed (52 bytes) so it can be
 * uploaded straight into a vertex buffer.
 */
struct Mesh3DVertex {
	/// Object-space position.
	math::vec3 position;
	/// Object-space normal (need not be normalized; the shader renormalizes).
	math::vec3 normal;
	/// Texture coordinate sampled from the selected texture slot.
	math::vec2 uv;
	/// Index into the bound texture array (`0` is the default white texture).
	uint32_t textureIndex = 0;
	/// Atlas sub-rect `(uMin, vMin, uSize, vSize)` for frac-tiling shaders (`voxel`); `{0,0,1,1}` = full texture, ignored by `mesh3d`.
	math::vec4 tileRect{0.f, 0.f, 1.f, 1.f};
};

/**
 * @brief
 *  Minimal forward 3D renderer: depth-tested, textured, single directional light.
 *
 * The first true 3D draw path in the engine. A mesh is uploaded once via
 * `createMesh` (returning an opaque GPU handle) and drawn each frame with a
 * model matrix through `drawMesh`; the renderer feeds a per-frame camera and a
 * per-draw model matrix to the `mesh3d` shader. It is immediate-mode (no
 * batching) and intentionally generic — `RendererVoxel` and future static-mesh
 * renderers build on it. Like `Renderer2D` it is a static facade with no
 * instances; it owns its descriptor block and scene uniform buffer.
 */
class OWL_API Renderer3D final {
public:
	/// Opaque handle to a GPU-resident mesh created by `createMesh`.
	using MeshHandle = shared<gpu::DrawData>;

	/**
	 * @brief
	 *  Initialize the renderer (descriptor block, scene UBO, default texture, shader).
	 */
	static void init();

	/**
	 * @brief
	 *  Release all GPU resources held by the renderer.
	 */
	static void shutdown();

	/**
	 * @brief
	 *  Begin a frame: bind the camera and enable depth testing.
	 * @param[in] iCamera The camera whose view-projection drives the frame.
	 */
	static void beginScene(const Camera& iCamera);

	/**
	 * @brief
	 *  End a frame: restore depth state for subsequent 2D layers.
	 */
	static void endScene();

	/**
	 * @brief
	 *  Set the world-space direction the sun light travels and the ambient term.
	 * @param[in] iSunDirection Direction the light travels (will be normalized).
	 * @param[in] iAmbient Ambient colour added before the Lambert term.
	 */
	static void setLighting(const math::vec3& iSunDirection, const math::vec3& iAmbient);

	/**
	 * @brief
	 *  Upload a mesh to the GPU and return a handle for later drawing.
	 *
	 * The mesh is static once created; rebuild it (create a new handle) when the
	 * geometry changes. An empty index span yields a handle that draws nothing.
	 * @param[in] iVertices The mesh vertices.
	 * @param[in] iIndices Triangle indices into `iVertices`.
	 * @param[in] iShaderName Shader used to draw the mesh (defaults to `mesh3d`; e.g. `voxel` for frac-tiled faces).
	 * @return A handle usable with `drawMesh`.
	 */
	[[nodiscard]] static auto createMesh(std::span<const Mesh3DVertex> iVertices, std::span<const uint32_t> iIndices,
										 const std::string& iShaderName = "mesh3d") -> MeshHandle;

	/**
	 * @brief
	 *  Draw a previously created mesh with a model transform.
	 *
	 * Textures are bound to slots `1..N` (slot `0` is the default white texture);
	 * a vertex's `textureIndex` selects which slot it samples. A null handle or an
	 * empty texture span is a no-op / white-only draw.
	 * @param[in] iMesh The mesh handle from `createMesh`.
	 * @param[in] iModel The model (object-to-world) matrix.
	 * @param[in] iTextures Textures bound to slots `1..N` in order.
	 */
	static void drawMesh(const MeshHandle& iMesh, const math::mat4& iModel,
						 std::span<const shared<gpu::Texture2D>> iTextures = {});
};

}// namespace owl::renderer
