/**
 * @file Renderer2DLayer.cpp
 * @author Silmaen
 * @date 30/04/2026
 * Copyright © 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/Renderer2DLayer.h"

#include "renderer/RenderLayerFactory.h"
#include "renderer/Renderer2D.h"

namespace owl::renderer {

void Renderer2DLayer::registerWithFactory() {
	if (RenderLayerFactory::hasType(typeKey()))
		return;
	RenderLayerFactory::registerType(typeKey(), [](const std::string& iName) -> shared<RenderLayer> {
		return mkShared<Renderer2DLayer>(iName);
	});
}

Renderer2DLayer::Renderer2DLayer(std::string iName) : m_name{std::move(iName)} {}

void Renderer2DLayer::onBeginFrame(const Camera& iCamera) {
	Renderer2D::resetStats();
	Renderer2D::beginScene(iCamera);
}

void Renderer2DLayer::onRender([[maybe_unused]] scene::Scene& ioScene) {
	// Per-layer entity dispatch is not yet wired — Scene::render still drives Renderer2D
	// directly. This will be hooked in a later release once a second layer type exists
	// and `RendererTag`-based routing becomes meaningful.
}

void Renderer2DLayer::onEndFrame() { Renderer2D::endScene(); }

void Renderer2DLayer::applyConfig([[maybe_unused]] const YAML::Node& iConfig) {
	// Renderer2D currently has no per-instance config knobs.
}

}// namespace owl::renderer
