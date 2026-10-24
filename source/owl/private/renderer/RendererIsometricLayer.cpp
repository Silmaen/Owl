/**
 * @file RendererIsometricLayer.cpp
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/RendererIsometricLayer.h"

#include "renderer/RenderLayerFactory.h"
#include "renderer/RenderStackYaml.h"
#include "renderer/utils/LayerTilemaps.h"
#include "scene/Entity.h"
#include "scene/Scene.h"
#include "scene/component/AnimatedSpriteRenderer.h"
#include "scene/component/IsometricObject.h"
#include "scene/component/SpriteRenderer.h"
#include "scene/component/Transform.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <utility>

namespace owl::renderer {

namespace {

auto readUnsigned(const YAML::Node& iNode, const uint32_t iFallback) -> uint32_t {
	if (!iNode || !iNode.IsScalar())
		return iFallback;
	return std::clamp(iNode.as<uint32_t>(iFallback), 2u, 4096u);
}

auto readFloat(const YAML::Node& iNode, const float iFallback) -> float {
	if (!iNode || !iNode.IsScalar())
		return iFallback;
	return std::clamp(iNode.as<float>(iFallback), 0.f, 4096.f);
}

template<typename Fn>
void forEachLayerEntity(scene::Scene& ioScene, const Fn& iFunction) {
	const bool editorMode = ioScene.status == scene::Scene::Status::Editing;
	for (const auto view = ioScene.registry.view<scene::component::Transform, scene::component::SpriteRenderer>();
		 const auto entity: view) {
		const scene::Entity ent{entity, &ioScene};
		if (!ioScene.isEffectivelyVisible(ent, editorMode) || !ioScene.layerAccepts(ent))
			continue;
		const auto& sprite = view.template get<scene::component::SpriteRenderer>(entity);
		iFunction(ent, sprite.texture, sprite.color,
				  std::array{math::vec2{0.f, 0.f}, math::vec2{1.f, 0.f}, math::vec2{1.f, 1.f}, math::vec2{0.f, 1.f}});
	}
	for (const auto view =
				 ioScene.registry.view<scene::component::Transform, scene::component::AnimatedSpriteRenderer>();
		 const auto entity: view) {
		const scene::Entity ent{entity, &ioScene};
		if (!ioScene.isEffectivelyVisible(ent, editorMode) || !ioScene.layerAccepts(ent))
			continue;
		const auto& anim = view.template get<scene::component::AnimatedSpriteRenderer>(entity);
		const uint32_t columns = std::max(anim.columns, 1u);
		const uint32_t rows = std::max(anim.rows, 1u);
		const uint32_t frame = std::clamp(anim.m_currentFrame, anim.firstFrame, anim.lastFrame);
		const float uMin = static_cast<float>(frame % columns) / static_cast<float>(columns);
		const float uMax = static_cast<float>(frame % columns + 1) / static_cast<float>(columns);
		const uint32_t row = frame / columns;
		const float vMax = 1.f - static_cast<float>(row) / static_cast<float>(rows);
		const float vMin = 1.f - static_cast<float>(row + 1) / static_cast<float>(rows);
		iFunction(ent, anim.texture, anim.color,
				  std::array{math::vec2{uMin, vMin}, math::vec2{uMax, vMin}, math::vec2{uMax, vMax},
							 math::vec2{uMin, vMax}});
	}
}

}// namespace

void RendererIsometricLayer::registerWithFactory() {
	if (RenderLayerFactory::hasType(typeKey()))
		return;
	RenderLayerFactory::registerType(typeKey(), [](const std::string& iName) -> shared<RenderLayer> {
		return mkShared<RendererIsometricLayer>(iName);
	});
}

RendererIsometricLayer::RendererIsometricLayer(std::string iName) : m_name{std::move(iName)} {}

RendererIsometricLayer::~RendererIsometricLayer() = default;

auto RendererIsometricLayer::makeCamera(const Camera& iCamera) const -> IsometricCamera {
	if (const auto* isometric = dynamic_cast<const IsometricCamera*>(&iCamera); isometric != nullptr) {
		IsometricCamera camera = *isometric;
		camera.setViewportSize(m_viewport);
		return camera;
	}
	IsometricCamera camera{m_config};
	camera.setViewportSize(m_viewport);
	camera.setZoomLevel(m_zoomLevel);
	const math::vec4 eye = math::inverse(iCamera.getView()) * math::vec4{0.f, 0.f, 0.f, 1.f};
	camera.setTarget({eye.x(), eye.y(), 0.f});
	return camera;
}

void RendererIsometricLayer::onBeginFrame(const Camera& iCamera) {
	OWL_PROFILE_FUNCTION()

	m_camera = makeCamera(iCamera);
	RendererIsometric::resetStats();
	RendererIsometric::beginScene(m_camera);
}

void RendererIsometricLayer::onRender(scene::Scene& ioScene) {
	OWL_PROFILE_FUNCTION()

	ioScene.applyLighting2D(ioScene.status == scene::Scene::Status::Editing);

	utils::forEachLayerTilemap(
			ioScene,
			[](const scene::TilemapAsset& iAsset, const math::Transform& iTransform, const int iEntityId) -> void {
				RendererIsometric::drawTilemap(iAsset, iTransform, iEntityId);
			});
	const auto tileWidth = static_cast<float>(m_config.tileSize.x());
	forEachLayerEntity(ioScene,
					   [&ioScene, tileWidth](const scene::Entity& iEntity, const shared<gpu::Texture2D>& iTexture,
											 const math::vec4& iColor, const std::array<math::vec2, 4>& iUv) -> void {
						   const math::Transform world = ioScene.getWorldTransform(iEntity);
						   const math::vec2 size{world.scale().x() * tileWidth, world.scale().y() * tileWidth};
						   const auto entityId = static_cast<int>(static_cast<uint32_t>(iEntity));
						   if (iColor.w() >= 1.f) {
							   RendererIsometric::drawSprite(
									   iTexture, {.foot = world.translation(),
												  .size = size,
												  .uvRect = {iUv[0].x(), iUv[0].y(), iUv[2].x(), iUv[2].y()},
												  .color = packColor(iColor),
												  .depthBias = RendererIsometric::spriteBias,
												  .entityId = entityId});
							   return;
						   }
						   RendererIsometric::drawBillboard({.foot = world.translation(),
															 .size = size,
															 .textureCoords = iUv,
															 .color = iColor,
															 .texture = iTexture,
															 .depthBias = RendererIsometric::spriteBias,
															 .flat = false,
															 .entityId = entityId});
					   });
	const bool editorMode = ioScene.status == scene::Scene::Status::Editing;
	for (const auto view = ioScene.registry.view<scene::component::Transform, scene::component::IsometricObject>();
		 const auto entity: view) {
		const scene::Entity ent{entity, &ioScene};
		if (!ioScene.isEffectivelyVisible(ent, editorMode) || !ioScene.layerAccepts(ent))
			continue;
		const auto& object = view.get<scene::component::IsometricObject>(entity);
		const auto* def = object.getObject();
		if (def == nullptr)
			continue;
		RendererIsometric::drawObject(*object.atlas, *def, ioScene.getWorldTransform(ent).translation(), object.color,
									  static_cast<int>(static_cast<uint32_t>(ent)));
	}
}

void RendererIsometricLayer::onEndFrame() {
	OWL_PROFILE_FUNCTION()

	RendererIsometric::endScene();
}

void RendererIsometricLayer::applyConfig(const std::string& iConfig) {
	const auto config = parseYamlText(iConfig);
	if (!config || !config.IsMap())
		return;
	m_config.tileSize = {readUnsigned(config["TileWidth"], m_config.tileSize.x()),
						 readUnsigned(config["TileHeight"], m_config.tileSize.y())};
	m_config.zStep = readFloat(config["ZStep"], m_config.zStep);
	if (const auto zoom = config["Zoom"]; zoom && zoom.IsScalar())
		m_zoomLevel =
				std::clamp(zoom.as<int32_t>(m_zoomLevel), IsometricCamera::minZoomLevel, IsometricCamera::maxZoomLevel);
}

void RendererIsometricLayer::setViewport(const math::vec2ui& iViewport) {
	if (iViewport.x() == 0 || iViewport.y() == 0)
		return;
	m_viewport = iViewport;
}

auto RendererIsometricLayer::getEffectiveViewProjection(const Camera& iCamera) const -> math::mat4 {
	return makeCamera(iCamera).getViewProjection();
}

}// namespace owl::renderer
