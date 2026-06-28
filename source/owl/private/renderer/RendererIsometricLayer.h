/**
 * @file RendererIsometricLayer.h
 * @author Silmaen
 * @date 27/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "renderer/RenderLayer.h"
#include "renderer/RendererIsometric.h"

namespace owl::renderer {

/**
 * @brief
 *  Render-stack layer presenting the scene in a fixed 2:1 dimetric projection.
 *
 * Entities tagged for this layer (a `RendererTag` matching the layer name) are
 * drawn back-to-front through `Renderer2D` using the dimetric view-projection
 * built from the layer `IsometricConfig`. The projection is a pure screen-space
 * mapping (no free-look camera) shared with the editor gizmos and picking, so
 * the active scene camera is ignored.
 */
class OWL_API RendererIsometricLayer final : public RenderLayer {
public:
	/**
	 * @brief
	 *  The factory type key for this layer.
	 * @return The type key string.
	 */
	static constexpr auto typeKey() -> const char* { return "RendererIsometric"; }

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
	explicit RendererIsometricLayer(std::string iName);

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
	 *  Open a frame: bind the dimetric projection to `Renderer2D`.
	 * @param[in] iCamera The active scene camera (ignored; the projection is fixed).
	 */
	void onBeginFrame(const Camera& iCamera) override;

	/**
	 * @brief
	 *  Render the layer (no-op; scene dispatch lands in a later phase).
	 * @param[in,out] ioScene The scene being rendered.
	 */
	void onRender(scene::Scene& ioScene) override;

	/**
	 * @brief
	 *  Close the frame (flush the `Renderer2D` batch).
	 */
	void onEndFrame() override;

	/**
	 * @brief
	 *  Apply the merged YAML configuration (tile size, Z step, origin).
	 * @param[in] iConfig The merged config node.
	 */
	void applyConfig(const YAML::Node& iConfig) override;

	/**
	 * @brief
	 *  Capture the active viewport size driving the dimetric projection.
	 * @param[in] iViewport The viewport size in pixels.
	 */
	void setViewport(const math::vec2ui& iViewport) override;

	/**
	 * @brief
	 *  The dimetric view-projection bound by this layer.
	 * @param[in] iCamera The active scene camera (ignored).
	 * @return The dimetric view-projection matrix.
	 */
	[[nodiscard]] auto getEffectiveViewProjection(const Camera& iCamera) const -> math::mat4 override;

	/**
	 * @brief
	 *  Get the layer configuration.
	 * @return The isometric configuration.
	 */
	[[nodiscard]] auto getConfig() const -> const IsometricConfig& { return m_config; }

private:
	/// Instance name from the project stack.
	std::string m_name;
	/// Isometric projection configuration.
	IsometricConfig m_config;
	/// Active viewport size in pixels (drives the dimetric projection).
	math::vec2ui m_viewport{1280u, 720u};
};

}// namespace owl::renderer
