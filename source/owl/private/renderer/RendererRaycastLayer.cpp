/**
 * @file RendererRaycastLayer.cpp
 * @author Silmaen
 * @date 04/05/2026
 * Copyright © 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/RendererRaycastLayer.h"

#include "math/YamlSerializers.h"
#include "renderer/RenderLayerFactory.h"
#include "renderer/Renderer2D.h"
#include "renderer/gpu/RenderCommand.h"

namespace owl::renderer {

void RendererRaycastLayer::registerWithFactory() {
	if (RenderLayerFactory::hasType(typeKey()))
		return;
	RenderLayerFactory::registerType(typeKey(), [](const std::string& iName) -> shared<RenderLayer> {
		return mkShared<RendererRaycastLayer>(iName);
	});
}

RendererRaycastLayer::RendererRaycastLayer(std::string iName) : m_name{std::move(iName)} {}

void RendererRaycastLayer::onBeginFrame(const Camera& iCamera) {
	OWL_PROFILE_FUNCTION()
	// Pixel-space ortho camera so the per-stripe quads emitted by `RendererRaycast`
	// land at exact pixel coordinates. (Origin at bottom-left, X right, Y up.)
	const auto vw = static_cast<float>(m_viewport.x());
	const auto vh = static_cast<float>(m_viewport.y());
	const CameraOrtho ortho(0.f, vw, 0.f, vh);
	// Disable depth test for the layer: backdrop and stripe quads all sit at z=0,
	// so with the default `GL_LESS` the backdrop wins at every pixel and rejects
	// every wall stripe drawn on top of it. Painter's order is enough for a
	// screen-space pass — depth test is restored in `onEndFrame` so subsequent
	// layers (e.g. world-space `Renderer2D` or HUD overlays) keep their normal
	// depth behaviour.
	gpu::RenderCommand::setDepthTest(false);
	Renderer2D::resetStats();
	Renderer2D::beginScene(ortho);
	RendererRaycast::resetStats();
	RendererRaycast::beginScene(iCamera, m_viewport, m_config);
}

void RendererRaycastLayer::onRender([[maybe_unused]] scene::Scene& ioScene) {
	// Per-tilemap dispatch is driven by `Scene::render` based on the active layer's
	// type key — kept centralised there so the existing 2D path and the new raycast
	// path share the same entity-iteration loop and visibility / picking logic.
}

void RendererRaycastLayer::onEndFrame() {
	OWL_PROFILE_FUNCTION()
	RendererRaycast::endScene();
	Renderer2D::endScene();
	gpu::RenderCommand::setDepthTest(true);
}

void RendererRaycastLayer::applyConfig(const YAML::Node& iConfig) {
	if (!iConfig || !iConfig.IsMap())
		return;
	if (const auto v = iConfig["Fov"]; v && v.IsScalar())
		m_config.fovDegrees = v.as<float>(m_config.fovDegrees);
	if (const auto v = iConfig["MaxDistance"]; v && v.IsScalar())
		m_config.maxDistance = v.as<float>(m_config.maxDistance);
	if (const auto v = iConfig["CeilingColor"]; v && v.IsSequence() && v.size() == 4)
		m_config.ceilingColor = v.as<math::vec4>(m_config.ceilingColor);
	if (const auto v = iConfig["FloorColor"]; v && v.IsSequence() && v.size() == 4)
		m_config.floorColor = v.as<math::vec4>(m_config.floorColor);
	if (const auto v = iConfig["NumRays"]; v && v.IsScalar())
		m_config.numRays = v.as<uint32_t>(m_config.numRays);
}

void RendererRaycastLayer::setViewport(const math::vec2ui& iViewport) {
	if (iViewport.x() == 0 || iViewport.y() == 0)
		return;
	m_viewport = iViewport;
}

auto RendererRaycastLayer::getEffectiveViewProjection([[maybe_unused]] const Camera& iCamera) const -> math::mat4 {
	const auto vw = static_cast<float>(m_viewport.x());
	const auto vh = static_cast<float>(m_viewport.y());
	const CameraOrtho ortho(0.f, vw, 0.f, vh);
	return ortho.getViewProjection();
}

}// namespace owl::renderer
