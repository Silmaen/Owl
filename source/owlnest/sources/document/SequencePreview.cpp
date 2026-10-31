/**
 * @file SequencePreview.cpp
 * @author Silmaen
 * @date 31/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "SequencePreview.h"

#include <app/Application.h>
#include <math/linAlgebra.h>
#include <scene/Scene.h>
#include <scene/component/Hierarchy.h>
#include <scene/component/SequencePlayer.h>

#include <filesystem>
#include <optional>
#include <string>

namespace owl::nest {

auto getSequenceDiskPath(const std::string& iAsset) -> std::optional<std::filesystem::path> {
	if (iAsset.empty())
		return std::nullopt;
	if (app::Application::instanced())
		for (const auto& [title, folder]: app::Application::get().getAssetDirectories())
			if (const auto candidate = folder / iAsset; exists(candidate))
				return candidate.lexically_normal();
	if (exists(std::filesystem::path{iAsset}))
		return std::filesystem::path{iAsset}.lexically_normal();
	return std::nullopt;
}

auto isPlayerOf(const scene::Entity& iEntity, const std::filesystem::path& iPath) -> bool {
	if (!iEntity || !iEntity.hasComponent<scene::component::SequencePlayer>())
		return false;
	const auto path = getSequenceDiskPath(iEntity.getComponent<scene::component::SequencePlayer>().sequence);
	return path && *path == iPath.lexically_normal();
}

auto findSequencePlayer(scene::Scene& iScene, const std::filesystem::path& iPath) -> scene::Entity {
	auto view = iScene.registry.view<scene::component::SequencePlayer>();
	for (const auto handle: view)
		if (const scene::Entity entity{handle, &iScene}; isPlayerOf(entity, iPath))
			return entity;
	return {};
}

auto getSequenceAnchor(const scene::Scene& iScene, const scene::Entity& iPlayer) -> math::mat4 {
	if (iPlayer.getComponent<scene::component::SequencePlayer>().camera != core::UUID{0})
		return iScene.getWorldTransform(iPlayer)();
	if (const auto parentId = iPlayer.getComponent<scene::component::Hierarchy>().parentId; parentId != core::UUID{0})
		if (const auto parent = iScene.findEntityByUUID(parentId); parent)
			return iScene.getWorldTransform(parent)();
	return math::identity<float, 4>();
}

}// namespace owl::nest
