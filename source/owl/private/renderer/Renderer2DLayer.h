/**
 * @file Renderer2DLayer.h
 * @author Silmaen
 * @date 30/04/2026
 * Copyright © 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "renderer/RenderLayer.h"

namespace owl::renderer {

/**
 * @brief `RenderLayer` adapter wrapping the existing `Renderer2D` static API.
 *
 * Registered with the `RenderLayerFactory` under the type key `"Renderer2D"`
 * during `Renderer::initShaders`. Holds no state of its own beyond the
 * instance name — `Renderer2D` itself is a stateless static facade.
 *
 * `onRender` is currently a no-op: the engine still drives draw calls from
 * `Scene::render` using the legacy `Renderer2D::drawXxx` API. Per-layer entity
 * dispatch will be wired in a follow-up release once a second renderer type
 * (raycasting) lands and the routing is meaningful.
 */
class Renderer2DLayer final : public RenderLayer {
public:
	Renderer2DLayer(const Renderer2DLayer&) = delete;
	Renderer2DLayer(Renderer2DLayer&&) = delete;
	auto operator=(const Renderer2DLayer&) -> Renderer2DLayer& = delete;
	auto operator=(Renderer2DLayer&&) -> Renderer2DLayer& = delete;

	/// Factory key used by `RenderLayerFactory::registerType`.
	static constexpr auto typeKey() -> const char* { return "Renderer2D"; }

	/**
	 * @brief Register this layer type with the factory (idempotent).
	 */
	static void registerWithFactory();

	/**
	 * @brief Constructor.
	 * @param[in] iName The runtime instance name.
	 */
	explicit Renderer2DLayer(std::string iName);

	~Renderer2DLayer() override = default;

	[[nodiscard]] auto getName() const -> const std::string& override { return m_name; }
	[[nodiscard]] auto getTypeKey() const -> const char* override { return typeKey(); }

	void onBeginFrame(const Camera& iCamera) override;
	void onRender(scene::Scene& ioScene) override;
	void onEndFrame() override;
	void applyConfig(const YAML::Node& iConfig) override;

private:
	/// Runtime instance name (e.g. "default", "hud").
	std::string m_name;
};

}// namespace owl::renderer
