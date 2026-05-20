/**
 * @file RendererRaycastLayer.cpp
 * @author Silmaen
 * @date 04/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/RendererRaycastLayer.h"

#include "core/Application.h"
#include "math/YamlSerializers.h"
#include "renderer/RenderLayerFactory.h"
#include "renderer/Renderer2D.h"
#include "renderer/gpu/RenderCommand.h"

namespace owl::renderer {

namespace {
auto loadTilesetByPath(const std::string& iPath) -> shared<scene::Tileset> {
	if (iPath.empty() || !core::Application::instanced())
		return nullptr;
	const auto& assetDirs = core::Application::get().getAssetDirectories();
	auto tileset = mkShared<scene::Tileset>();
	for (const auto& [title, assetsPath]: assetDirs) {
		if (const auto full = assetsPath / iPath; exists(full) && tileset->loadFromFile(full))
			return tileset;
	}
	const std::filesystem::path bare{iPath};
	if (exists(bare) && tileset->loadFromFile(bare))
		return tileset;
	return nullptr;
}

// Build the `(BL.x, BL.y, TR.x, TR.y)` atlas UV rect for a tile index.
auto tileUvRect(const scene::Tileset& iTileset, const uint32_t iTileIndex) -> math::vec4 {
	const auto corners = iTileset.getTileUv(iTileIndex);
	return {corners[0].x(), corners[0].y(), corners[2].x(), corners[2].y()};
}
}// namespace

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

	const auto vw = static_cast<float>(m_viewport.x());
	const auto vh = static_cast<float>(m_viewport.y());
	const CameraOrtho ortho(0.f, vw, 0.f, vh);
	resolveBackdropTilesets();
	gpu::RenderCommand::setDepthTest(false);
	Renderer2D::resetStats();
	Renderer2D::beginScene(ortho);
	RendererRaycast::resetStats();
	RendererRaycast::beginScene(iCamera, m_viewport, m_config);
}

void RendererRaycastLayer::resolveBackdropTilesets() {
	if (!m_floorTileset && !m_config.floorTilesetPath.empty()) {
		m_floorTileset = loadTilesetByPath(m_config.floorTilesetPath);
	}
	if (!m_ceilingTileset && !m_config.ceilingTilesetPath.empty()) {
		m_ceilingTileset = loadTilesetByPath(m_config.ceilingTilesetPath);
	}
	if (m_floorTileset && m_floorTileset->texture) {
		m_config.floorTexture = m_floorTileset->texture;
		m_config.floorUvRect = tileUvRect(*m_floorTileset, m_config.floorTileIndex);
	} else {
		m_config.floorTexture.reset();
	}
	if (m_ceilingTileset && m_ceilingTileset->texture) {
		m_config.ceilingTexture = m_ceilingTileset->texture;
		m_config.ceilingUvRect = tileUvRect(*m_ceilingTileset, m_config.ceilingTileIndex);
	} else {
		m_config.ceilingTexture.reset();
	}
}

void RendererRaycastLayer::onRender([[maybe_unused]] scene::Scene& ioScene) {}

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
	if (const auto v = iConfig["FogColor"]; v && v.IsSequence() && v.size() == 4)
		m_config.fogColor = v.as<math::vec4>(m_config.fogColor);
	if (const auto v = iConfig["FogStart"]; v && v.IsScalar())
		m_config.fogStart = v.as<float>(m_config.fogStart);
	if (const auto v = iConfig["FogEnd"]; v && v.IsScalar())
		m_config.fogEnd = v.as<float>(m_config.fogEnd);
	if (const auto v = iConfig["FloorTileset"]; v && v.IsScalar())
		m_config.floorTilesetPath = v.as<std::string>(m_config.floorTilesetPath);
	if (const auto v = iConfig["FloorTileIndex"]; v && v.IsScalar())
		m_config.floorTileIndex = v.as<uint32_t>(m_config.floorTileIndex);
	if (const auto v = iConfig["CeilingTileset"]; v && v.IsScalar())
		m_config.ceilingTilesetPath = v.as<std::string>(m_config.ceilingTilesetPath);
	if (const auto v = iConfig["CeilingTileIndex"]; v && v.IsScalar())
		m_config.ceilingTileIndex = v.as<uint32_t>(m_config.ceilingTileIndex);
	m_floorTileset.reset();
	m_ceilingTileset.reset();
	m_config.floorTexture.reset();
	m_config.ceilingTexture.reset();
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
