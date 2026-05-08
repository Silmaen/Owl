/**
 * @file Renderer2DLayer.h
 * @author Silmaen
 * @date 30/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "renderer/CameraOrtho.h"
#include "renderer/RenderLayer.h"

namespace owl::renderer {
/**
 * @brief
 *  `RenderLayer` adapter wrapping the existing `Renderer2D` static API.
 *
 * Registered with the `RenderLayerFactory` under the type key `"Renderer2D"`
 * during `Renderer::initShaders`. Holds the instance name plus a `Space`
 * setting that selects between two camera-binding modes:
 *
 * - `Space::World` (default) — `onBeginFrame` calls `Renderer2D::beginScene`
 *   with the active scene camera. Suitable for world-space rendering: tilemaps,
 *   sprites, particles. The active camera's projection / view applies as usual,
 *   so the layer follows the player camera.
 * - `Space::Screen` — `onBeginFrame` builds a pixel-space ortho camera and
 *   binds it to `Renderer2D` instead. Quads emitted at pixel coordinates
 *   (origin at bottom-left, X right, Y up) land at the matching screen pixels
 *   regardless of the scene camera's rotation / zoom. Used by HUD / menu
 *   layers so the overlay stays upright when the player camera is rotated
 *   (e.g. the raycast scene).
 *
 * The active mode is read from `applyConfig` via the YAML key `Space`
 * (`World` / `Screen`, case-insensitive). Default is `World` for backwards
 * compatibility with projects that don't declare it.
 *
 * `onRender` iterates entities tagged with this layer's name (or untagged
 * entities when this is the first layer in the stack) — `Scene::layerAccepts`
 * does the routing. The legacy `Scene::render` path still drives the actual
 * `Renderer2D::drawXxx` calls; this layer's `onRender` only emits draws for
 * entity types whose dispatch has been migrated (currently empty — Tilemap
 * dispatch is handled in `Scene::render` based on the active layer name).
 */
class OWL_API Renderer2DLayer final : public RenderLayer {
public:
	/**
	 * @brief
	 *  Coordinate space the layer binds for `Renderer2D`.
	 */
	enum struct Space : uint8_t {
		/// Layer binds the active scene camera (sprites/tilemaps follow the world).
		World,
		/// Layer binds a pixel-space ortho (HUD / screen overlays, immune to camera rotation).
		Screen,
	};

	Renderer2DLayer(const Renderer2DLayer&) = delete;

	Renderer2DLayer(Renderer2DLayer&&) = delete;

	auto operator=(const Renderer2DLayer&) -> Renderer2DLayer& = delete;

	auto operator=(Renderer2DLayer&&) -> Renderer2DLayer& = delete;

	/**
	 * @brief
	 *  Factory key used by `RenderLayerFactory::registerType`.
	 */
	static constexpr auto typeKey() -> const char* { return "Renderer2D"; }

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
	explicit Renderer2DLayer(std::string iName);

	~Renderer2DLayer() override = default;

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
	 *  Handle the begin frame event.
	 * @param[in] iCamera The camera providing the view/projection used for rendering.
	 */
	void onBeginFrame(const Camera& iCamera) override;

	/**
	 * @brief
	 *  Handle the render event.
	 * @param[in,out] ioScene The scene being rendered (entities are queried in-place).
	 */
	void onRender(scene::Scene& ioScene) override;

	/**
	 * @brief
	 *  Handle the end frame event.
	 */
	void onEndFrame() override;

	/**
	 * @brief
	 *  Apply config.
	 * @param[in] iConfig YAML node describing the layer configuration.
	 */
	void applyConfig(const YAML::Node& iConfig) override;

	/**
	 * @brief
	 *  Set the viewport.
	 * @param[in] iViewport Viewport size in pixels.
	 */
	void setViewport(const math::vec2ui& iViewport) override;

	/**
	 * @brief
	 *  Get the effective view projection.
	 * @param[in] iCamera The camera providing the view/projection used for rendering.
	 * @return The effective view-projection matrix used by this layer.
	 */
	[[nodiscard]] auto getEffectiveViewProjection(const Camera& iCamera) const -> math::mat4 override;

	/**
	 * @brief
	 *  The coordinate space the layer is currently bound to.
	 * @return The configured `Space` (`World` or `Screen`).
	 */
	[[nodiscard]] auto getSpace() const -> Space { return m_space; }

private:
	/**
	 * @brief
	 *  Build the pixel-space ortho matching the current viewport.
	 */
	[[nodiscard]] auto buildPixelOrtho() const -> CameraOrtho;

	/// Runtime instance name (e.g. "default", "hud", "ui").
	std::string m_name;
	/// Coordinate space the layer binds for `Renderer2D` (set by `applyConfig`).
	Space m_space = Space::World;
	/// Viewport size in pixels (latched by `setViewport`, used for screen-space ortho).
	math::vec2ui m_viewport{1280, 720};
};

}// namespace owl::renderer
