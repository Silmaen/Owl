/**
 * @file Renderer2DLayer.cpp
 * @author Silmaen
 * @date 30/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/Renderer2DLayer.h"

#include "renderer/RenderLayerFactory.h"
#include "renderer/Renderer2D.h"

#include <algorithm>
#include <cctype>

namespace owl::renderer {

namespace {
auto parseSpace(const std::string& iValue) -> Renderer2DLayer::Space {
	std::string lower = iValue;
	std::ranges::transform(lower, lower.begin(),
						   [](const unsigned char iC) -> char { return static_cast<char>(std::tolower(iC)); });
	if (lower == "screen" || lower == "screenoverlay" || lower == "pixel")
		return Renderer2DLayer::Space::Screen;
	return Renderer2DLayer::Space::World;
}

}// namespace

void Renderer2DLayer::registerWithFactory() {
	if (RenderLayerFactory::hasType(typeKey()))
		return;
	RenderLayerFactory::registerType(typeKey(), [](const std::string& iName) -> shared<RenderLayer> {
		return mkShared<Renderer2DLayer>(iName);
	});
}

Renderer2DLayer::Renderer2DLayer(std::string iName) : m_name{std::move(iName)} {}

auto Renderer2DLayer::buildPixelOrtho() const -> CameraOrtho {
	const auto vw = static_cast<float>(m_viewport.x());
	const auto vh = static_cast<float>(m_viewport.y());
	return CameraOrtho{0.f, vw, 0.f, vh};
}

void Renderer2DLayer::onBeginFrame(const Camera& iCamera) {
	Renderer2D::resetStats();
	if (m_space == Space::Screen) {
		const auto ortho = buildPixelOrtho();
		Renderer2D::beginScene(ortho);
	} else {
		Renderer2D::beginScene(iCamera);
	}
}

void Renderer2DLayer::onRender([[maybe_unused]] scene::Scene& ioScene) {
	// Per-entity draw calls are still emitted by `Scene::render` against the active layer
	// name. This hook stays empty until a future refactor migrates the dispatch here.
}

void Renderer2DLayer::onEndFrame() { Renderer2D::endScene(); }

void Renderer2DLayer::applyConfig(const YAML::Node& iConfig) {
	if (!iConfig || !iConfig.IsMap())
		return;
	if (const auto v = iConfig["Space"]; v && v.IsScalar())
		m_space = parseSpace(v.as<std::string>());
}

void Renderer2DLayer::setViewport(const math::vec2ui& iViewport) {
	if (iViewport.x() == 0 || iViewport.y() == 0)
		return;
	m_viewport = iViewport;
}

auto Renderer2DLayer::getEffectiveViewProjection(const Camera& iCamera) const -> math::mat4 {
	if (m_space == Space::Screen)
		return buildPixelOrtho().getViewProjection();
	return iCamera.getViewProjection();
}

}// namespace owl::renderer
