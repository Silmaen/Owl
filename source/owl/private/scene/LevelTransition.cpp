/**
 * @file LevelTransition.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "scene/LevelTransition.h"

#include "app/Application.h"
#include "physics/PhysicCommand.h"
#include "scene/Entity.h"
#include "scene/SaveManager.h"
#include "scene/component/Tag.h"
#include "scene/component/Transform.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace owl::scene {

namespace {
auto readFile(const std::filesystem::path& iPath, std::vector<uint8_t>& oBytes) -> bool {
	std::ifstream file(iPath, std::ios::binary | std::ios::ate);
	if (!file.is_open())
		return false;
	const auto size = static_cast<size_t>(file.tellg());
	file.seekg(0);
	oBytes.resize(size);
	file.read(reinterpret_cast<char*>(oBytes.data()), static_cast<std::streamsize>(size));
	return static_cast<bool>(file);
}
}// namespace

auto LevelTransition::resolveLevelName(const std::string& iLevelName) -> std::string {
	if (std::filesystem::path(iLevelName).extension() == ".owl")
		return iLevelName;
	return iLevelName + ".owl";
}

auto LevelTransition::readLevel(const std::string& iLevelName, const std::vector<std::filesystem::path>& iRoots)
		-> std::optional<LevelSource> {
	const auto fileName = resolveLevelName(iLevelName);
	const std::array<std::string, 2> names{fileName, "scenes/" + fileName};
	if (app::Application::instanced() && app::Application::get().hasOpenPack()) {
		for (const auto& name: names) {
			if (auto data = app::Application::get().loadFromPack(name); data)
				return LevelSource{.bytes = std::move(*data), .sourceName = name};
		}
	}
	for (const auto& root: iRoots) {
		for (const auto& name: names) {
			const auto path = root / name;
			if (!exists(path))
				continue;
			if (LevelSource source{.bytes = {}, .sourceName = path.string()}; readFile(path, source.bytes))
				return source;
		}
	}
	OWL_CORE_WARN("LevelTransition: Level '{}' not found in the pack nor in {} asset folder(s).", fileName,
				  iRoots.size())
	return std::nullopt;
}

auto LevelTransition::getSearchRoots() -> std::vector<std::filesystem::path> {
	std::vector<std::filesystem::path> roots;
	if (!app::Application::instanced())
		return roots;
	for (const auto& [title, path]: app::Application::get().getAssetDirectories()) roots.push_back(path);
	return roots;
}

auto LevelTransition::loadLevel(const ParsedScene& iParsed, const Scene& iCurrent) -> shared<Scene> {
	if (!iParsed.valid) {
		OWL_CORE_ERROR("Teleport: Level '{}' is invalid: {}. Fix: {}.", iParsed.sourceName, iParsed.error,
					   fixHint(iParsed.failure))
		return nullptr;
	}
	auto level = mkShared<Scene>();
	const SceneSerializer serializer(level);
	if (const auto loaded = serializer.applyParsed(iParsed); !loaded) {
		OWL_CORE_ERROR("Teleport: Failed to load level '{}': {}. Fix: {}.", iParsed.sourceName,
					   describe(loaded.error()), fixHint(loaded.error()))
		return nullptr;
	}
	level->getGameState() = iCurrent.getGameState();
	return level;
}

void LevelTransition::placeArrival(Scene& ioScene, const std::string& iTargetName, const math::vec2f& iVelocity) {
	const Entity player = ioScene.getPrimaryPlayer();
	if (!player)
		return;
	for (const auto view = ioScene.registry.view<component::Tag, component::Transform>(); const auto ent: view) {
		if (view.get<component::Tag>(ent).tag != iTargetName)
			continue;
		const auto& target = view.get<component::Transform>(ent).transform;
		const float rotation = target.rotation().z();
		const float cosR = std::cos(rotation);
		const float sinR = std::sin(rotation);
		const math::vec2f velocity = {iVelocity.x() * cosR - iVelocity.y() * sinR,
									  iVelocity.x() * sinR + iVelocity.y() * cosR};
		physics::PhysicCommand::setTransform(player, {target.translation().x(), target.translation().y()}, rotation);
		physics::PhysicCommand::setVelocity(player, velocity);
		auto& playerTransform = player.getComponent<component::Transform>().transform;
		playerTransform.translation().x() = target.translation().x();
		playerTransform.translation().y() = target.translation().y();
		playerTransform.rotation().z() = rotation;
		return;
	}
}

auto LevelTransition::loadSavedGame(const uint32_t iSlot, Scene& ioCurrent, const math::vec2ui& iViewportSize)
		-> shared<Scene> {
	auto loadedScene = mkShared<Scene>();
	const auto result = SaveManager::load(iSlot, loadedScene);
	if (!result.success) {
		OWL_CORE_WARN("LevelTransition: Save slot {} could not be loaded, the current level keeps running.", iSlot)
		return nullptr;
	}
	ioCurrent.onEndRuntime();
	loadedScene->onViewportResize(iViewportSize);
	loadedScene->onStartRuntime();
	for (const auto& [uuid, snapshot]: result.physicsSnapshots)
		if (auto entity = loadedScene->findEntityByUUID(core::UUID{uuid}); entity)
			physics::PhysicCommand::applySnapshot(entity, snapshot);
	return loadedScene;
}

}// namespace owl::scene
