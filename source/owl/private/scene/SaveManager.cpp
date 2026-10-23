/**
 * @file SaveManager.cpp
 * @author Silmaen
 * @date 13/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "scene/SaveManager.h"

#include "TilemapCodec.h"
#include "core/FormatVersionYaml.h"
#include "core/SerializerImpl.h"
#include "data/assets/AssetSearchPaths.h"
#include "physics/PhysicCommand.h"
#include "platform/AtomicFile.h"
#include "scene/Entity.h"
#include "scene/SceneSerializer.h"
#include "scene/TilemapAsset.h"
#include "scene/component/PhysicBody.h"
#include "scene/component/Tilemap.h"

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
OWL_DIAG_DISABLE_CLANG("-Wshadow")
#include <yaml-cpp/yaml.h>
OWL_DIAG_POP

#include <chrono>
#include <cstdint>
#include <ctime>
#include <exception>
#include <filesystem>
#include <format>
#include <fstream>
#include <string_view>
#include <system_error>
#include <tuple>

namespace owl::scene {

namespace {
constexpr std::array<core::MigrationStep, 0> g_saveMigrations{};
constexpr core::DocumentFormat g_saveFormat{.name = "Save", .migrations = g_saveMigrations};

// Put the saved widget values back into the HUD the scene draws (shared with the other scenes asking for it).
void restoreHud(Scene& ioScene, const YAML::Node& iHudData) {
	if (const auto path = iHudData["path"];
		path && path.IsScalar() && path.as<std::string>() != ioScene.getEffectiveHudPath())
		ioScene.setHudPath(path.as<std::string>());
	const auto hud = ioScene.getHud();
	const auto entities = iHudData["state"]["Entities"];
	if (!hud || !entities.IsSequence())
		return;
	for (const auto& node: entities) {
		if (!node["Entity"])
			continue;
		const auto widget = hud->findEntityByUUID(core::UUID{node["Entity"].as<uint64_t>()});
		if (!widget)
			continue;
		YAML::Emitter emitter;
		emitter << node;
		std::ignore = SceneSerializer::applyEntityFromString(widget, emitter.c_str());
	}
}
// Whether a runtime tilemap holds what its asset file holds (a file that cannot be read counts as changed).
auto matchesItsFile(const component::Tilemap& iTilemap) -> bool {
	const auto text = data::assets::readAssetText(iTilemap.tilemapPath);
	if (!text)
		return false;
	TilemapAsset source;
	return source.deserializeFromString(*text) && source.hasSameContent(*iTilemap.asset);
}

// The cells of the tilemaps changed since they were read from their file.
void emitTilemapCells(YAML::Emitter& ioEmitter, const Scene& iScene) {
	bool started = false;
	for (const auto view = iScene.registry.view<component::ID, component::Tilemap>(); const auto entity: view) {
		const auto& tilemap = view.get<component::Tilemap>(entity);
		if (!tilemap.asset || tilemap.tilemapPath.empty() || matchesItsFile(tilemap))
			continue;
		if (!started) {
			ioEmitter << YAML::Key << "TilemapCells" << YAML::Value << YAML::BeginSeq;
			started = true;
		}
		const auto& asset = *tilemap.asset;
		const std::string payload = codec::encodePayload(asset.layers, asset.getCornerHeights());
		ioEmitter << YAML::BeginMap;
		ioEmitter << YAML::Key << "uuid" << YAML::Value << static_cast<uint64_t>(view.get<component::ID>(entity).id);
		ioEmitter << YAML::Key << "width" << YAML::Value << asset.width;
		ioEmitter << YAML::Key << "height" << YAML::Value << asset.height;
		ioEmitter << YAML::Key << "layers" << YAML::Value << asset.layers.size();
		ioEmitter << YAML::Key << "payload" << YAML::Value
				  << YAML::Binary(reinterpret_cast<const unsigned char*>(payload.data()), payload.size());
		ioEmitter << YAML::EndMap;
	}
	if (started)
		ioEmitter << YAML::EndSeq;
}

auto readTilemapCells(const YAML::Node& iNode) -> std::unordered_map<uint64_t, SaveManager::TilemapCells> {
	std::unordered_map<uint64_t, SaveManager::TilemapCells> cells;
	if (!iNode || !iNode.IsSequence())
		return cells;
	for (const auto& entry: iNode) {
		if (!entry["uuid"] || !entry["payload"])
			continue;
		const auto binary = entry["payload"].as<YAML::Binary>();
		cells[entry["uuid"].as<uint64_t>()] = SaveManager::TilemapCells{
				.width = entry["width"] ? entry["width"].as<uint32_t>() : 0,
				.height = entry["height"] ? entry["height"].as<uint32_t>() : 0,
				.layerCount = entry["layers"] ? entry["layers"].as<uint32_t>() : 0,
				.payload = std::string{reinterpret_cast<const char*>(binary.data()), binary.size()}};
	}
	return cells;
}

}// namespace

auto SaveManager::restoreTilemapCells(Scene& ioScene, const std::unordered_map<uint64_t, TilemapCells>& iCells)
		-> size_t {
	size_t restored = 0;
	for (const auto& [uuid, cells]: iCells) {
		const auto entity = ioScene.findEntityByUUID(core::UUID{uuid});
		auto* tilemap =
				entity ? ioScene.registry.try_get<component::Tilemap>(static_cast<entt::entity>(entity)) : nullptr;
		if (tilemap == nullptr || !tilemap->asset) {
			OWL_CORE_WARN("SaveManager: No tilemap {} to restore the saved cells into.", uuid)
			continue;
		}
		auto& asset = *tilemap->asset;
		if (asset.width != cells.width || asset.height != cells.height || asset.layers.size() != cells.layerCount) {
			OWL_CORE_WARN("SaveManager: Tilemap {} has another grid than the saved one, its cells are not restored.",
						  uuid)
			continue;
		}
		auto layers = asset.layers;
		for (auto& layer: layers) layer.fill(component::g_EmptyTileIndex);
		std::vector<uint8_t> heights;
		const size_t cornerCount = (static_cast<size_t>(asset.width) + 1) * (static_cast<size_t>(asset.height) + 1);
		if (!codec::decodePayload(cells.payload, layers, heights, cornerCount, true)) {
			OWL_CORE_WARN("SaveManager: Saved cells of tilemap {} are corrupted, the file ones are kept.", uuid)
			continue;
		}
		asset.layers = std::move(layers);
		std::ignore = asset.setCornerHeights(std::move(heights));
		++restored;
	}
	return restored;
}

auto SaveManager::format() -> const core::DocumentFormat& { return g_saveFormat; }

std::string SaveManager::s_gameName;

void SaveManager::setGameName(const std::string& iGameName) { s_gameName = iGameName; }

auto SaveManager::getSaveDirectory() -> std::filesystem::path {
	const std::string name = s_gameName.empty() ? "OwlGame" : s_gameName;
	std::filesystem::path baseDir;
#if defined(OWL_PLATFORM_WINDOWS)
	if (const char* appdata = std::getenv("APPDATA"); appdata != nullptr)
		baseDir = appdata;
	else
		baseDir = std::filesystem::temp_directory_path();
#else
	if (const char* home = std::getenv("HOME"); home != nullptr)
		baseDir = std::filesystem::path(home) / ".local" / "share";
	else
		baseDir = std::filesystem::temp_directory_path();
#endif
	const auto saveDir = baseDir / name / "saves";
	std::error_code error;
	if (!exists(saveDir, error) && !create_directories(saveDir, error) && error)
		OWL_CORE_WARN("SaveManager: Cannot create the save directory '{}': {}.", saveDir.string(), error.message())
	return saveDir;
}

auto SaveManager::getSlotPath(const uint32_t iSlot) -> std::filesystem::path {
	return getSaveDirectory() / std::format("save_{}.owl_save", iSlot);
}

auto SaveManager::save(const uint32_t iSlot, const shared<Scene>& iScene, const std::string& iScenePath) -> bool {
	OWL_PROFILE_FUNCTION()

	try {
		// Interpolated transforms lag the simulation: save the poses that match the saved velocities.
		physics::PhysicCommand::syncSimulatedTransforms(*iScene);
		const SceneSerializer serializer(iScene);
		const std::string sceneYaml = serializer.serializeToString();

		// Timestamp.
		const auto now = std::chrono::system_clock::now();
		const auto timeT = std::chrono::system_clock::to_time_t(now);
		std::string timestamp(30, '\0');
		if (const auto len =
					std::strftime(timestamp.data(), timestamp.size(), "%Y-%m-%dT%H:%M:%S", std::localtime(&timeT)))
			timestamp.resize(len);

		// Build save file using a single Serializer.
		const core::Serializer sOut;
		sOut.getImpl()->emitter << YAML::BeginMap;
		emitFormatVersion(sOut.getImpl()->emitter, g_saveFormat);
		sOut.getImpl()->emitter << YAML::Key << "OwlSave" << YAML::Value << YAML::BeginMap;
		sOut.getImpl()->emitter << YAML::Key << "timestamp" << YAML::Value << timestamp;
		sOut.getImpl()->emitter << YAML::Key << "scenePath" << YAML::Value << iScenePath;
		sOut.getImpl()->emitter << YAML::EndMap;

		// GameState (emitted directly).
		iScene->getGameState().serialize(sOut);

		// Scene data as embedded YAML node.
		sOut.getImpl()->emitter << YAML::Key << "SceneData" << YAML::Value << YAML::Load(sceneYaml);

		// Widget values of the HUD asset.
		if (const auto hud = iScene->getHud(); hud) {
			sOut.getImpl()->emitter << YAML::Key << "HudData" << YAML::Value << YAML::BeginMap;
			sOut.getImpl()->emitter << YAML::Key << "path" << YAML::Value << iScene->getEffectiveHudPath();
			sOut.getImpl()->emitter << YAML::Key << "state" << YAML::Value
									<< YAML::Load(SceneSerializer(hud, DocumentKind::Hud).serializeToString());
			sOut.getImpl()->emitter << YAML::EndMap;
		}

		emitTilemapCells(sOut.getImpl()->emitter, *iScene);

		// Physics snapshots (velocities for dynamic bodies).
		if (physics::PhysicCommand::isInitialized(*iScene)) {
			sOut.getImpl()->emitter << YAML::Key << "PhysicsSnapshots" << YAML::Value << YAML::BeginSeq;
			for (const auto& entity: iScene->getAllEntities()) {
				if (!entity.hasComponent<component::PhysicBody>())
					continue;
				const auto& [body] = entity.getComponent<component::PhysicBody>();
				if (body.type == SceneBody::BodyType::Static)
					continue;
				const auto snap = physics::PhysicCommand::getSnapshot(entity);
				sOut.getImpl()->emitter << YAML::BeginMap;
				sOut.getImpl()->emitter << YAML::Key << "uuid" << YAML::Value
										<< static_cast<uint64_t>(entity.getUUID());
				sOut.getImpl()->emitter << YAML::Key << "vx" << YAML::Value << snap.linearVelocity.x();
				sOut.getImpl()->emitter << YAML::Key << "vy" << YAML::Value << snap.linearVelocity.y();
				sOut.getImpl()->emitter << YAML::Key << "av" << YAML::Value << snap.angularVelocity;
				sOut.getImpl()->emitter << YAML::Key << "awake" << YAML::Value << snap.awake;
				sOut.getImpl()->emitter << YAML::EndMap;
			}
			sOut.getImpl()->emitter << YAML::EndSeq;
		}

		sOut.getImpl()->emitter << YAML::EndMap;

		if (const auto written = platform::writeFileAtomic(getSlotPath(iSlot), sOut.getImpl()->emitter.c_str());
			!written) {
			OWL_CORE_ERROR("SaveManager: Cannot write to slot {}: {}.", iSlot, describe(written.error()))
			return false;
		}
		OWL_CORE_INFO("SaveManager: Saved to slot {} at {}.", iSlot, timestamp)
		return true;
	} catch (const std::exception& e) {
		OWL_CORE_ERROR("SaveManager: Save failed: {}.", e.what())
		return false;
	}
}

auto SaveManager::load(const uint32_t iSlot, const shared<Scene>& iScene) -> LoadResult {
	OWL_PROFILE_FUNCTION()

	LoadResult result;
	try {
		const auto path = getSlotPath(iSlot);
		if (!exists(path)) {
			OWL_CORE_ERROR("SaveManager: No save file for slot {}.", iSlot)
			return result;
		}
		YAML::Node root = YAML::LoadFile(path.string());
		if (const auto version = upgradeYamlDocument(g_saveFormat, root, path.string()); !version) {
			OWL_CORE_ERROR("SaveManager: Cannot load slot {}: {}.", iSlot, describe(version.error()))
			result.formatError = version.error();
			return result;
		}

		// Load GameState.
		if (root["GameState"]) {
			const core::Serializer gsNode;
			gsNode.getImpl()->document = root;
			iScene->getGameState().deserialize(gsNode);
		}

		// Load Scene.
		if (root["SceneData"]) {
			YAML::Emitter sceneEmitter;
			sceneEmitter << root["SceneData"];
			const std::string sceneYaml = sceneEmitter.c_str();
			const std::vector<uint8_t> sceneData(sceneYaml.begin(), sceneYaml.end());
			const SceneSerializer serializer(iScene);
			if (!serializer.deserializeFromBuffer(sceneData, "save_slot_" + std::to_string(iSlot)))
				return result;
		}

		if (const auto hudData = root["HudData"]; hudData.IsMap())
			restoreHud(*iScene, hudData);

		result.tilemapCells = readTilemapCells(root["TilemapCells"]);
		iScene->resolveAllTilemapAssets();
		std::ignore = restoreTilemapCells(*iScene, result.tilemapCells);

		// Load physics snapshots.
		if (const auto snapNode = root["PhysicsSnapshots"]; snapNode && snapNode.IsSequence()) {
			for (const auto& entry: snapNode) {
				if (!entry["uuid"])
					continue;
				const auto uuid = entry["uuid"].as<uint64_t>();
				physics::PhysicCommand::PhysicsSnapshot snap;
				if (entry["vx"])
					snap.linearVelocity.x() = entry["vx"].as<float>();
				if (entry["vy"])
					snap.linearVelocity.y() = entry["vy"].as<float>();
				if (entry["av"])
					snap.angularVelocity = entry["av"].as<float>();
				if (entry["awake"])
					snap.awake = entry["awake"].as<bool>();
				result.physicsSnapshots[uuid] = snap;
			}
		}

		// Mark as loaded from save in GameState.
		iScene->getGameState().set("loaded_from_save", true);

		OWL_CORE_INFO("SaveManager: Loaded slot {} ({} physics snapshots).", iSlot, result.physicsSnapshots.size())
		result.success = true;
		return result;
	} catch (const std::exception& e) {
		OWL_CORE_ERROR("SaveManager: Load failed: {}.", e.what())
		return result;
	}
}

auto SaveManager::listSaves() -> std::vector<SaveInfo> {
	std::vector<SaveInfo> saves;
	const auto saveDir = getSaveDirectory();
	// Another process may add or remove saves while they are listed: a vanished entry is skipped, never thrown.
	std::error_code error;
	if (!exists(saveDir, error))
		return saves;
	for (auto it = std::filesystem::directory_iterator(saveDir, error);
		 !error && it != std::filesystem::directory_iterator{}; it.increment(error)) {
		const auto& entry = *it;
		std::error_code entryError;
		if (!entry.is_regular_file(entryError) || entry.path().extension() != ".owl_save")
			continue;
		try {
			const YAML::Node root = YAML::LoadFile(entry.path().string());
			if (const auto header = root["OwlSave"]) {
				SaveInfo info;
				// Parse slot number from filename: save_N.owl_save
				const std::string stem = entry.path().stem().string();
				if (stem.starts_with("save_"))
					info.slot = static_cast<uint32_t>(std::stoul(stem.substr(5)));
				if (header["timestamp"])
					info.timestamp = header["timestamp"].as<std::string>();
				if (header["scenePath"])
					info.scenePath = header["scenePath"].as<std::string>();
				saves.push_back(info);
			}
		} catch (const std::exception& e) {
			OWL_CORE_WARN("SaveManager: Skipping corrupt save '{}': {}.", entry.path().string(), e.what())
		}
	}
	if (error)
		OWL_CORE_WARN("SaveManager: Listing of '{}' stopped: {}.", saveDir.string(), error.message())
	std::ranges::sort(saves, [](const auto& iA, const auto& iB) -> auto { return iA.slot < iB.slot; });
	return saves;
}

auto SaveManager::hasSave(const uint32_t iSlot) -> bool {
	std::error_code error;
	return exists(getSlotPath(iSlot), error);
}

void SaveManager::deleteSave(const uint32_t iSlot) {
	const auto path = getSlotPath(iSlot);
	std::error_code error;
	if (std::filesystem::remove(path, error); error)
		OWL_CORE_WARN("SaveManager: Cannot delete '{}': {}.", path.string(), error.message())
}

auto SaveManager::getScenePath(const uint32_t iSlot) -> std::string {
	try {
		const auto path = getSlotPath(iSlot);
		if (!exists(path))
			return {};
		const YAML::Node root = YAML::LoadFile(path.string());
		if (const auto header = root["OwlSave"]; header && header["scenePath"])
			return header["scenePath"].as<std::string>();
	} catch (const std::exception& e) { OWL_CORE_WARN("SaveManager: Cannot read save slot {}: {}.", iSlot, e.what()) }
	return {};
}

}// namespace owl::scene
