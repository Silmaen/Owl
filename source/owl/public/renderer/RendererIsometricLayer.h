/**
 * @file RendererIsometricLayer.h
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "renderer/RenderLayer.h"
#include "renderer/RendererIsometric.h"

#include <cstdint>
#include <string>

namespace owl::renderer {

/**
 * @brief
 *  `RenderLayer` drawing the entities routed to it as isometric billboards, from its own `onRender`.
 *
 * Factory key `"RendererIsometric"`. Tilemaps become flat terrain billboards, `SpriteRenderer` and
 * `AnimatedSpriteRenderer` entities upright billboards anchored at their world translation, sized
 * `scale.xy × tileWidth` pixels. The camera is the layer's own `IsometricCamera`: an incoming `IsometricCamera` (the
 * editor's) is used as is, any other camera gives the target (its world position, Z dropped) and the layer config the
 * presentation and zoom. Config keys: `TileWidth`, `TileHeight` (pixels), `ZStep` (pixels per height unit) and `Zoom`
 * (power-of-two level).
 */
class OWL_API RendererIsometricLayer final : public RenderLayer {
public:
	/**
	 * @brief
	 *  Copy constructor (deleted).
	 */
	RendererIsometricLayer(const RendererIsometricLayer&) = delete;

	/**
	 * @brief
	 *  Move constructor (deleted).
	 */
	RendererIsometricLayer(RendererIsometricLayer&&) = delete;

	/**
	 * @brief
	 *  Copy assignment (deleted).
	 * @return A reference to this object.
	 */
	auto operator=(const RendererIsometricLayer&) -> RendererIsometricLayer& = delete;

	/**
	 * @brief
	 *  Move assignment (deleted).
	 * @return A reference to this object.
	 */
	auto operator=(RendererIsometricLayer&&) -> RendererIsometricLayer& = delete;

	/**
	 * @brief
	 *  Factory key used by `RenderLayerFactory::registerType`.
	 * @return The type key.
	 */
	static constexpr auto typeKey() -> const char* { return "RendererIsometric"; }

	/**
	 * @brief
	 *  Register this layer type with the factory (idempotent).
	 */
	static void registerWithFactory();

	/**
	 * @brief
	 *  Constructor.
	 * @param[in] iName The runtime instance name.
	 */
	explicit RendererIsometricLayer(std::string iName);

	/**
	 * @brief
	 *  Destructor.
	 */
	~RendererIsometricLayer() override;

	/**
	 * @brief
	 *  Get the name.
	 * @return The name.
	 */
	[[nodiscard]] auto getName() const -> const std::string& override { return m_name; }

	/**
	 * @brief
	 *  Get the type key.
	 * @return The type key.
	 */
	[[nodiscard]] auto getTypeKey() const -> const char* override { return typeKey(); }

	/**
	 * @brief
	 *  Build the frame's isometric camera and start the isometric renderer.
	 * @param[in] iCamera The scene or editor camera.
	 */
	void onBeginFrame(const Camera& iCamera) override;

	/**
	 * @brief
	 *  Queue the tilemaps and sprites routed to this layer.
	 * @param[in,out] ioScene The scene being rendered.
	 */
	void onRender(scene::Scene& ioScene) override;

	/**
	 * @brief
	 *  Draw the queued billboards.
	 */
	void onEndFrame() override;

	/**
	 * @brief
	 *  Apply the YAML config (`TileWidth`, `TileHeight`, `ZStep`, `Zoom`).
	 * @param[in] iConfig YAML text describing the layer configuration.
	 */
	void applyConfig(const std::string& iConfig) override;

	/**
	 * @brief
	 *  Set the viewport.
	 * @param[in] iViewport Viewport size in pixels.
	 */
	void setViewport(const math::vec2ui& iViewport) override;

	/**
	 * @brief
	 *  The isometric view-projection the layer uses with a camera.
	 * @param[in] iCamera The scene or editor camera.
	 * @return The view-projection matrix.
	 */
	[[nodiscard]] auto getEffectiveViewProjection(const Camera& iCamera) const -> math::mat4 override;

	/**
	 * @brief
	 *  The layer draws every entity routed to it in `onRender`.
	 * @return True.
	 */
	[[nodiscard]] auto drawsOwnEntities() const -> bool override { return true; }

	/**
	 * @brief
	 *  Get the presentation read from the config.
	 * @return The presentation.
	 */
	[[nodiscard]] auto getConfig() const -> const IsometricConfig& { return m_config; }

	/**
	 * @brief
	 *  Get the zoom level read from the config.
	 * @return The zoom level.
	 */
	[[nodiscard]] auto getZoomLevel() const -> int32_t { return m_zoomLevel; }

	/**
	 * @brief
	 *  Build the isometric camera of a frame.
	 * @param[in] iCamera The scene or editor camera.
	 * @return The camera the layer draws with.
	 */
	[[nodiscard]] auto makeCamera(const Camera& iCamera) const -> IsometricCamera;

private:
	/// Runtime instance name.
	std::string m_name;
	/// Tile size and height step.
	IsometricConfig m_config;
	/// Zoom level used with a non-isometric camera.
	int32_t m_zoomLevel = 0;
	/// Viewport size in pixels.
	math::vec2ui m_viewport{1280u, 720u};
	/// Camera of the current frame.
	IsometricCamera m_camera;
};

}// namespace owl::renderer
