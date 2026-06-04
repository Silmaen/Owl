/**
 * @file RendererVoxelLayer.cpp
 * @author Silmaen
 * @date 04/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/RendererVoxelLayer.h"

#include "core/external/yaml.h"
#include "renderer/RenderLayerFactory.h"

namespace owl::renderer {

namespace {
auto readVec3(const YAML::Node& iNode, const math::vec3& iFallback) -> math::vec3 {
	if (iNode && iNode.IsSequence() && iNode.size() >= 3)
		return math::vec3{iNode[0].as<float>(), iNode[1].as<float>(), iNode[2].as<float>()};
	return iFallback;
}
}// namespace

void RendererVoxelLayer::registerWithFactory() {
	if (RenderLayerFactory::hasType(typeKey()))
		return;
	RenderLayerFactory::registerType(typeKey(), [](const std::string& iName) -> shared<RenderLayer> {
		return mkShared<RendererVoxelLayer>(iName);
	});
}

RendererVoxelLayer::RendererVoxelLayer(std::string iName) : m_name{std::move(iName)} {}

void RendererVoxelLayer::onBeginFrame(const Camera& iCamera) {
	OWL_PROFILE_FUNCTION()

	RendererVoxel::beginScene(iCamera, m_config);
}

void RendererVoxelLayer::onRender([[maybe_unused]] scene::Scene& ioScene) {}

void RendererVoxelLayer::onEndFrame() {
	OWL_PROFILE_FUNCTION()

	RendererVoxel::endScene();
}

void RendererVoxelLayer::applyConfig(const YAML::Node& iConfig) {
	if (!iConfig || !iConfig.IsMap())
		return;
	m_config.sunDirection = readVec3(iConfig["SunDirection"], m_config.sunDirection);
	m_config.ambient = readVec3(iConfig["Ambient"], m_config.ambient);
}

}// namespace owl::renderer
