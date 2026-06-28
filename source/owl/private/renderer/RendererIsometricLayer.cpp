/**
 * @file RendererIsometricLayer.cpp
 * @author Silmaen
 * @date 27/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/RendererIsometricLayer.h"

#include "core/external/yaml.h"
#include "renderer/Camera.h"
#include "renderer/RenderLayerFactory.h"
#include "renderer/Renderer2D.h"

namespace owl::renderer {

void RendererIsometricLayer::registerWithFactory() {
	if (RenderLayerFactory::hasType(typeKey()))
		return;
	RenderLayerFactory::registerType(typeKey(), [](const std::string& iName) -> shared<RenderLayer> {
		return mkShared<RendererIsometricLayer>(iName);
	});
}

RendererIsometricLayer::RendererIsometricLayer(std::string iName) : m_name{std::move(iName)} {}

void RendererIsometricLayer::onBeginFrame([[maybe_unused]] const Camera& iCamera) {
	OWL_PROFILE_FUNCTION()

	Renderer2D::resetStats();
	Camera isoCamera{dimetricViewProjection(m_viewport, m_config)};
	isoCamera.setTransform(math::identity<float, 4>());
	Renderer2D::beginScene(isoCamera);
}

void RendererIsometricLayer::onRender([[maybe_unused]] scene::Scene& ioScene) {}

void RendererIsometricLayer::onEndFrame() {
	OWL_PROFILE_FUNCTION()

	Renderer2D::endScene();
}

void RendererIsometricLayer::applyConfig(const YAML::Node& iConfig) {
	if (!iConfig || !iConfig.IsMap())
		return;
	if (const auto v = iConfig["TileWidth"]; v && v.IsScalar())
		m_config.tileSize.x() = v.as<uint32_t>(m_config.tileSize.x());
	if (const auto v = iConfig["TileHeight"]; v && v.IsScalar())
		m_config.tileSize.y() = v.as<uint32_t>(m_config.tileSize.y());
	if (const auto v = iConfig["ZStep"]; v && v.IsScalar())
		m_config.zStep = v.as<float>(m_config.zStep);
	if (const auto v = iConfig["Origin"]; v && v.IsSequence() && v.size() >= 2)
		m_config.origin = math::vec2{v[0].as<float>(), v[1].as<float>()};
}

void RendererIsometricLayer::setViewport(const math::vec2ui& iViewport) { m_viewport = iViewport; }

auto RendererIsometricLayer::getEffectiveViewProjection([[maybe_unused]] const Camera& iCamera) const -> math::mat4 {
	return dimetricViewProjection(m_viewport, m_config);
}

}// namespace owl::renderer
