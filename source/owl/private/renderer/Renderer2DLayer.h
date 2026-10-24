/**
 * @file Renderer2DLayer.h
 * @author Silmaen
 * @date 30/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "math/matrixCreation.h"
#include "renderer/CameraOrtho.h"
#include "renderer/RenderLayer.h"

#include <cstdint>
#include <string>

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
 *   (e.g. the raycast scene). With a `ReferenceHeight` above 0 the units are
 *   reference pixels instead: the screen is `ReferenceHeight` units high (its
 *   width follows the aspect ratio), so an overlay keeps its share of the
 *   screen at any resolution, like a `ScaleWithHeight` HUD canvas.
 *
 * The active mode is read from `applyConfig` via the YAML key `Space`
 * (`World` / `Screen`, case-insensitive), the reference height from `ReferenceHeight` (0, the default, keeps one unit
 * per screen pixel). Default is `World`, for
 * projects that don't declare it.
 *
 * Two keys put a world layer into a 3D scene: `Billboard` (`None`, `Upright`, `Full`) turns its sprites to the camera
 * and lights them on the ground plane (x, z) of a Y-up world, `DepthTest` tests its primitives against the depth the
 * 3D layers wrote before it (without writing it) and sorts them back to front.
 *
 * `onRender` iterates entities tagged with this layer's name (or untagged
 * entities when this is the first layer in the stack) — `Scene::layerAccepts`
 * does the routing. The tilemaps are drawn here (queued to `RendererTilemap`);
 * the other 2D entity types are drawn by `Scene::render`.
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

	/**
	 * @brief
	 *  How the sprites of a world layer turn to the camera.
	 */
	enum struct Billboard : uint8_t {
		/// Sprites keep their own rotation (a 2D world in the XY plane).
		None,
		/// Sprites stand up along the world Y axis and turn around it to face the camera.
		Upright,
		/// Sprites lie in the plane of the screen (pre-rendered art seen from above).
		Full,
	};

	Renderer2DLayer(const Renderer2DLayer&) = delete;

	Renderer2DLayer(Renderer2DLayer&&) = delete;

	auto operator=(const Renderer2DLayer&) -> Renderer2DLayer& = delete;

	auto operator=(Renderer2DLayer&&) -> Renderer2DLayer& = delete;

	/**
	 * @brief
	 *  Factory key used by `RenderLayerFactory::registerType`.
	 * @return Pointer to the type key, or `nullptr` when not available.
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
	 *  Handle the render event: queue the tilemaps routed to this layer.
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
	void applyConfig(const std::string& iConfig) override;

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

	/**
	 * @brief
	 *  Height of the screen in `Space::Screen` units.
	 * @return The configured `ReferenceHeight`, 0 when the units are screen pixels.
	 */
	[[nodiscard]] auto getReferenceHeight() const -> float { return m_referenceHeight; }

	/**
	 * @brief
	 *  How the sprites of the layer turn to the camera.
	 * @return The configured `Billboard`, `None` by default.
	 */
	[[nodiscard]] auto getBillboard() const -> Billboard { return m_billboard; }

	/**
	 * @brief
	 *  Whether the primitives of the layer are tested against the depth of the layers drawn before it.
	 * @return The configured `DepthTest`, false by default.
	 */
	[[nodiscard]] auto isDepthTested() const -> bool { return m_depthTest; }

	/**
	 * @brief
	 *  World matrix of the camera of the frame (`onBeginFrame`), the frame billboards turn to.
	 * @return The inverse of the camera view.
	 */
	[[nodiscard]] auto getCameraWorld() const -> const math::mat4& { return m_cameraWorld; }

private:
	/**
	 * @brief
	 *  Build the pixel-space ortho matching the current viewport.
	 * @return The CameraOrtho.
	 */
	[[nodiscard]] auto buildPixelOrtho() const -> CameraOrtho;

	/// Runtime instance name (e.g. "default", "hud", "ui").
	std::string m_name;
	/// Coordinate space the layer binds for `Renderer2D` (set by `applyConfig`).
	Space m_space = Space::World;
	/// Screen height in `Space::Screen` units (0: one unit per pixel), set by `applyConfig`.
	float m_referenceHeight = 0.f;
	/// Viewport size in pixels (latched by `setViewport`, used for screen-space ortho).
	math::vec2ui m_viewport{1280, 720};
	/// How the sprites turn to the camera (set by `applyConfig`).
	Billboard m_billboard = Billboard::None;
	/// Depth test against the layers drawn before (set by `applyConfig`).
	bool m_depthTest = false;
	/// Inverse view of the camera of the frame, latched by `onBeginFrame`.
	math::mat4 m_cameraWorld = math::identity<float, 4>();
};

}// namespace owl::renderer
