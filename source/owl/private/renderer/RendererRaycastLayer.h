/**
 * @file RendererRaycastLayer.h
 * @author Silmaen
 * @date 04/05/2026
 * Copyright © 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "renderer/CameraOrtho.h"
#include "renderer/RenderLayer.h"
#include "renderer/RendererRaycast.h"

namespace owl::renderer {

/**
 * @brief `RenderLayer` adapter for the `RendererRaycast` static facade.
 *
 * Registered with the `RenderLayerFactory` under the type key `"RendererRaycast"`
 * during `Renderer::initShaders`. Stores the per-instance configuration (FOV,
 * draw distance, sky / floor colours) and the viewport size needed to set up
 * the pixel-space ortho camera under which `Renderer2D` emits the wall stripes.
 *
 * The layer drives the begin/end of both `Renderer2D` (with an ortho camera so
 * wall stripes land at exact pixel coordinates) and the static `RendererRaycast`
 * facade. The actual `drawTilemapWalls` calls are emitted by `Scene::render`
 * when it recognises that the active layer is of type `"RendererRaycast"`.
 */
class OWL_API RendererRaycastLayer final : public RenderLayer {
public:
	RendererRaycastLayer(const RendererRaycastLayer&) = delete;
	RendererRaycastLayer(RendererRaycastLayer&&) = delete;
	auto operator=(const RendererRaycastLayer&) -> RendererRaycastLayer& = delete;
	auto operator=(RendererRaycastLayer&&) -> RendererRaycastLayer& = delete;

	/// Factory key used by `RenderLayerFactory::registerType`.
	static constexpr auto typeKey() -> const char* { return "RendererRaycast"; }

	/**
	 * @brief Register this layer type with the factory (idempotent).
	 */
	static void registerWithFactory();

	/**
	 * @brief Constructor.
	 * @param[in] iName The runtime instance name (e.g. `"world"`).
	 */
	explicit RendererRaycastLayer(std::string iName);

	~RendererRaycastLayer() override = default;

	[[nodiscard]] auto getName() const -> const std::string& override { return m_name; }
	[[nodiscard]] auto getTypeKey() const -> const char* override { return typeKey(); }

	void onBeginFrame(const Camera& iCamera) override;
	void onRender(scene::Scene& ioScene) override;
	void onEndFrame() override;
	void applyConfig(const YAML::Node& iConfig) override;
	void setViewport(const math::vec2ui& iViewport) override;
	[[nodiscard]] auto getEffectiveViewProjection(const Camera& iCamera) const -> math::mat4 override;

	/**
	 * @brief Read-only access to the layer's current configuration.
	 *
	 * Used by `Scene::render` to drive the active `RendererRaycast::beginScene`
	 * that the dispatch falls into when the layer is active.
	 * @return The active configuration (FOV, draw distance, colours).
	 */
	[[nodiscard]] auto getConfig() const -> const RaycastConfig& { return m_config; }

	/**
	 * @brief Read-only access to the viewport size last set on this layer.
	 * @return The viewport size in pixels.
	 */
	[[nodiscard]] auto getViewport() const -> const math::vec2ui& { return m_viewport; }

private:
	/// Runtime instance name (e.g. "world").
	std::string m_name;
	/// Active raycast configuration (rebuilt by `applyConfig`).
	RaycastConfig m_config;
	/// Viewport in pixels (latched by `setViewport`).
	math::vec2ui m_viewport{1280, 720};
};

}// namespace owl::renderer
