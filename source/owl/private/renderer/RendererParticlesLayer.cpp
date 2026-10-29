/**
 * @file RendererParticlesLayer.cpp
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/RendererParticlesLayer.h"

#include "renderer/RenderLayerFactory.h"
#include "renderer/RendererParticles.h"

#include <string>
#include <utility>

namespace owl::renderer {

void RendererParticlesLayer::registerWithFactory() {
	if (RenderLayerFactory::hasType(typeKey()))
		return;
	RenderLayerFactory::registerType(typeKey(), [](const std::string& iName) -> shared<RenderLayer> {
		return mkShared<RendererParticlesLayer>(iName);
	});
}

RendererParticlesLayer::RendererParticlesLayer(std::string iName) : m_name{std::move(iName)} {}

void RendererParticlesLayer::onBeginFrame(const Camera& iCamera) { RendererParticles::beginScene(iCamera); }

void RendererParticlesLayer::onRender([[maybe_unused]] scene::Scene& ioScene) {}

void RendererParticlesLayer::onEndFrame() { RendererParticles::endScene(); }

void RendererParticlesLayer::applyConfig([[maybe_unused]] const std::string& iConfig) {}

}// namespace owl::renderer
