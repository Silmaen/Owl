/**
 * @file LayerTilemaps.h
 * @author Silmaen
 * @date 13/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "math/Transform.h"
#include "scene/Entity.h"
#include "scene/Scene.h"
#include "scene/TilemapAsset.h"
#include "scene/Tileset.h"
#include "scene/component/Tilemap.h"
#include "scene/component/Transform.h"

namespace owl::renderer::utils {

/**
 * @brief
 *  Call a function for every drawable tilemap entity the scene routes to the active render layer.
 *
 * An entity qualifies when it is effectively visible (editor or game visibility after the scene status), accepted by
 * `Scene::layerAccepts` and holds a tilemap asset whose tileset atlas is loaded.
 * @tparam Fn Callable as `void(const scene::TilemapAsset&, const math::Transform&, int)`.
 * @param[in] iScene The scene being rendered.
 * @param[in] iFunction Receives the asset, the entity world transform and the entity id.
 */
template<typename Fn>
void forEachLayerTilemap(scene::Scene& iScene, const Fn& iFunction) {
	const bool editorMode = iScene.status == scene::Scene::Status::Editing;
	for (const auto view = iScene.registry.view<scene::component::Transform, scene::component::Tilemap>();
		 const auto entity: view) {
		const scene::Entity ent{entity, &iScene};
		if (!iScene.isEffectivelyVisible(ent, editorMode) || !iScene.layerAccepts(ent))
			continue;
		const auto& asset = view.template get<scene::component::Tilemap>(entity).asset;
		if (!asset || !asset->tileset || !asset->tileset->texture)
			continue;
		iFunction(*asset, iScene.getWorldTransform(ent), static_cast<int>(entity));
	}
}

}// namespace owl::renderer::utils
