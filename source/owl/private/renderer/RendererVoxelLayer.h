/**
 * @file RendererVoxelLayer.h
 * @author Silmaen
 * @date 04/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "renderer/RenderLayer.h"
#include "renderer/RendererVoxel.h"

namespace owl::renderer {

/**
 * @brief
 *  Render-stack layer drawing voxel worlds through `RendererVoxel`.
 *
 * Entities tagged for this layer (a `RendererTag` matching the layer name) with
 * a `scene::component::VoxelWorld` are drawn in 3D. Unlike the raycast layer it
 * keeps the scene camera's perspective view-projection (the default), so it
 * needs a perspective camera to look right.
 */
class OWL_API RendererVoxelLayer final : public RenderLayer {
public:
	/**
	 * @brief
	 *  The factory type key for this layer.
	 * @return The type key string.
	 */
	static constexpr auto typeKey() -> const char* { return "RendererVoxel"; }

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
	explicit RendererVoxelLayer(std::string iName);

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
	 *  Render the layer (no-op; `Scene::render` dispatches voxel worlds).
	 * @param[in,out] ioScene The scene being rendered.
	 */
	void onRender(scene::Scene& ioScene) override;

	/**
	 * @brief
	 *  Close the frame.
	 */
	void onEndFrame() override;

	/**
	 * @brief
	 *  Apply the merged YAML configuration (lighting).
	 * @param[in] iConfig The merged config node.
	 */
	void applyConfig(const YAML::Node& iConfig) override;

	/**
	 * @brief
	 *  Get the layer configuration.
	 * @return The voxel configuration.
	 */
	[[nodiscard]] auto getConfig() const -> const VoxelConfig& { return m_config; }

private:
	/// Instance name from the project stack.
	std::string m_name;
	/// Voxel rendering configuration (lighting).
	VoxelConfig m_config;
};

}// namespace owl::renderer
