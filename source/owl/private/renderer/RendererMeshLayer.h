/**
 * @file RendererMeshLayer.h
 * @author Silmaen
 * @date 21/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "renderer/RenderLayer.h"
#include "renderer/RendererMesh.h"

#include <string>

namespace owl::renderer {

/**
 * @brief
 *  Render-stack layer drawing static meshes through `RendererMesh`.
 *
 * Entities of this layer (a `RendererTag` matching the layer name, or untagged when it is the first layer) with a
 * `scene::component::MeshRenderer` are drawn in 3D: opaque ones first, then the blended ones back-to-front. It keeps
 * the scene camera's view-projection, so it needs a perspective camera to look 3D.
 */
class OWL_API RendererMeshLayer final : public RenderLayer {
public:
	/**
	 * @brief
	 *  The factory type key for this layer.
	 * @return The type key string.
	 */
	static constexpr auto typeKey() -> const char* { return "RendererMesh"; }

	/**
	 * @brief
	 *  Register this layer type with the `RenderLayerFactory`.
	 */
	static void registerWithFactory();

	/**
	 * @brief
	 *  Construct the layer with its scene-unique instance name.
	 * @param[in] iName The instance name from the project stack.
	 */
	explicit RendererMeshLayer(std::string iName);

	/**
	 * @brief
	 *  Get the instance name.
	 * @return The instance name.
	 */
	[[nodiscard]] auto getName() const -> const std::string& override { return m_name; }

	/**
	 * @brief
	 *  Get the factory type key.
	 * @return The type key.
	 */
	[[nodiscard]] auto getTypeKey() const -> const char* override { return typeKey(); }

	/**
	 * @brief
	 *  Open a frame: bind the camera and lighting.
	 * @param[in] iCamera The active camera.
	 */
	void onBeginFrame(const Camera& iCamera) override;

	/**
	 * @brief
	 *  Render the layer (no-op; `Scene::render` dispatches the meshes).
	 * @param[in,out] ioScene The scene being rendered.
	 */
	void onRender(scene::Scene& ioScene) override;

	/**
	 * @brief
	 *  Close the frame: draw the blended meshes.
	 */
	void onEndFrame() override;

	/**
	 * @brief
	 *  Apply the merged YAML configuration (lighting).
	 * @param[in] iConfig The merged config node.
	 */
	void applyConfig(const std::string& iConfig) override;

	/**
	 * @brief
	 *  Get the layer configuration.
	 * @return The mesh layer configuration.
	 */
	[[nodiscard]] auto getConfig() const -> const MeshLayerConfig& { return m_config; }

private:
	/// Instance name from the project stack.
	std::string m_name;
	/// Mesh rendering configuration (lighting).
	MeshLayerConfig m_config;
};

}// namespace owl::renderer
