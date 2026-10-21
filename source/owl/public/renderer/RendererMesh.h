/**
 * @file RendererMesh.h
 * @author Silmaen
 * @date 21/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "math/matrices.h"
#include "math/vectors.h"
#include "renderer/Camera.h"
#include "renderer/MeshCache.h"
#include "renderer/Renderer3D.h"

#include <cstdint>

namespace owl::renderer {

/**
 * @brief
 *  Per-layer configuration of the static mesh renderer.
 */
struct MeshLayerConfig {
	/// World-space direction the sun light travels.
	math::vec3 sunDirection{-0.4f, -1.f, -0.6f};
	/// Ambient light colour added before the directional term.
	math::vec3 ambient{0.35f, 0.35f, 0.4f};
};

/**
 * @brief
 *  Draws static meshes (`scene::component::MeshRenderer`) on top of `Renderer3D`.
 *
 * Opaque meshes are drawn as they are submitted; blended ones are queued and drawn at `endScene`, sorted
 * back-to-front by the distance of their bounding-box centre to the camera. A static facade like the other
 * renderers; it keeps no GPU resource of its own.
 */
class OWL_API RendererMesh final {
public:
	/**
	 * @brief
	 *  Draw counters of the last `beginScene` / `endScene` pass, for diagnostics and headless tests.
	 */
	struct Statistics {
		/// Opaque meshes drawn.
		uint32_t opaqueDrawCount = 0;
		/// Blended meshes drawn.
		uint32_t transparentDrawCount = 0;
	};

	/**
	 * @brief
	 *  Begin a frame: bind the camera and the lighting.
	 * @param[in] iCamera The camera of the frame.
	 * @param[in] iConfig The lighting of the layer.
	 */
	static void beginScene(const Camera& iCamera, const MeshLayerConfig& iConfig);

	/**
	 * @brief
	 *  Draw the blended meshes queued since `beginScene`, back-to-front, then end the frame.
	 */
	static void endScene();

	/**
	 * @brief
	 *  Draw a mesh, or queue it for `endScene` when it is blended.
	 * @param[in,out] ioMesh The mesh resource (its GPU mesh is uploaded by `MeshResource::getMesh` before the layers
	 *  draw).
	 * @param[in] iModel The model (object-to-world) matrix.
	 * @param[in] iTexture Texture of slot `1`; null samples the default white texture.
	 * @param[in] iParams Tint and entity id of the draw.
	 * @param[in] iTransparent True to draw with alpha blending, after the opaque meshes.
	 */
	static void drawMesh(MeshResource& ioMesh, const math::mat4& iModel, const shared<gpu::Texture2D>& iTexture,
						 const MeshDrawParams& iParams, bool iTransparent);

	/**
	 * @brief
	 *  Get the counters of the last pass.
	 * @return The counters.
	 */
	[[nodiscard]] static auto getStatistics() -> Statistics;
};

}// namespace owl::renderer
