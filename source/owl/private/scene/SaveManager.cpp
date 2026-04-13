/**
 * @file SaveManager.cpp
 * @author Silmaen
 * @date 13/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "scene/SaveManager.h"

#include "core/SerializerImpl.h"
#include "physic/PhysicCommand.h"
#include "scene/Entity.h"
#include "scene/SceneSerializer.h"
#include "scene/component/PhysicBody.h"

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
OWL_DIAG_DISABLE_CLANG("-Wshadow")
#include <yaml-cpp/yaml.h>
OWL_DIAG_POP

#include <chrono>
#include <fstream>

namespace owl::scene {

std::string SaveManager::s_gameName;

void SaveManager::setGameName(const std::string& iGameName) { s_gameName = iGameName; }

auto SaveManager::getSaveDirectory() -> std::filesystem::path {
	const std::string name = s_gameName.empty() ? "OwlGame" : s_gameName;
	std::filesystem::path baseDir;
#if defined(OWL_PLATFORM_WINDOWS)
	if (const char* appdata = std::getenv("APPDATA"); appdata != nullptr)// NOLINT(concurrency-mt-unsafe)
		baseDir = appdata;
	else
		baseDir = std::filesystem::temp_directory_path();
#else
	if (const char* home = std::getenv("HOME"); home != nullptr)// NOLINT(concurrency-mt-unsafe)
		baseDir = std::filesystem::path(home) / ".local" / "share";
	else
		baseDir = std::filesystem::temp_directory_path();
#endif
	const auto saveDir = baseDir / name / "saves";
	if (!exists(saveDir))
		create_directories(saveDir);
	return saveDir;
}

auto SaveManager::getSlotPath(const uint32_t iSlot) -> std::filesystem::path {
	return getSaveDirectory() / std::format("save_{}.owl_save", iSlot);
}

auto SaveManager::save(const uint32_t iSlot, const shared<Scene>& iScene, const std::string& iScenePath) -> bool {
	OWL_PROFILE_FUNCTION()
	try {
		const SceneSerializer serializer(iScene);
		const std::string sceneYaml = serializer.serializeToString();

		// Timestamp.
		const auto now = std::chrono::system_clock::now();
		const auto timeT = std::chrono::system_clock::to_time_t(now);
		std::string timestamp(30, '\0');
		// NOLINTNEXTLINE(concurrency-mt-unsafe)
		if (const auto len = std::strftime(timestamp.data(), timestamp.size(), "%Y-%m-%dT%H:%M:%S", std::localtime(&timeT)))
			timestamp.resize(len);

		// Build save file using a single Serializer.
		const core::Serializer sOut;
		sOut.getImpl()->emitter << YAML::BeginMap;
		sOut.getImpl()->emitter << YAML::Key << "OwlSave" << YAML::Value << YAML::BeginMap;
		sOut.getImpl()->emitter << YAML::Key << "version" << YAML::Value << 1;
		sOut.getImpl()->emitter << YAML::Key << "timestamp" << YAML::Value << timestamp;
		sOut.getImpl()->emitter << YAML::Key << "scenePath" << YAML::Value << iScenePath;
		sOut.getImpl()->emitter << YAML::EndMap;

		// GameState (emitted directly).
		iScene->getGameState().serialize(sOut);

		// Scene data as embedded YAML node.
		sOut.getImpl()->emitter << YAML::Key << "SceneData" << YAML::Value << YAML::Load(sceneYaml);

		// Physics snapshots (velocities for dynamic bodies).
		if (physic::PhysicCommand::isInitialized()) {
			sOut.getImpl()->emitter << YAML::Key << "PhysicsSnapshots" << YAML::Value << YAML::BeginSeq;
			for (const auto& entity: iScene->getAllEntities()) {
				if (!entity.hasComponent<component::PhysicBody>())
					continue;
				const auto& [body] = entity.getComponent<component::PhysicBody>();
				if (body.type == SceneBody::BodyType::Static)
					continue;
				const auto snap = physic::PhysicCommand::getSnapshot(entity);
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

		std::ofstream file(getSlotPath(iSlot));
		if (!file.is_open()) {
			OWL_CORE_ERROR("SaveManager: Cannot write to slot {}", iSlot)
			return false;
		}
		file << sOut.getImpl()->emitter.c_str();
		file.close();
		OWL_CORE_INFO("SaveManager: Saved to slot {} at {}", iSlot, timestamp)
		return true;
	} catch (const std::exception& e) {
		OWL_CORE_ERROR("SaveManager: Save failed: {}", e.what())
		return false;
	}
}

auto SaveManager::load(const uint32_t iSlot, const shared<Scene>& iScene) -> LoadResult {
	OWL_PROFILE_FUNCTION()
	LoadResult result;
	try {
		const auto path = getSlotPath(iSlot);
		if (!exists(path)) {
			OWL_CORE_ERROR("SaveManager: No save file for slot {}", iSlot)
			return result;
		}
		const YAML::Node root = YAML::LoadFile(path.string());

		// Load GameState.
		if (root["GameState"]) {
			const core::Serializer gsNode;
			gsNode.getImpl()->node = root;
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

		// Load physics snapshots.
		if (const auto snapNode = root["PhysicsSnapshots"]; snapNode && snapNode.IsSequence()) {
			for (const auto& entry: snapNode) {
				if (!entry["uuid"])
					continue;
				const auto uuid = entry["uuid"].as<uint64_t>();
				physic::PhysicCommand::PhysicsSnapshot snap;
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

		OWL_CORE_INFO("SaveManager: Loaded slot {} ({} physics snapshots)", iSlot, result.physicsSnapshots.size())
		result.success = true;
		return result;
	} catch (const std::exception& e) {
		OWL_CORE_ERROR("SaveManager: Load failed: {}", e.what())
		return result;
	}
}

auto SaveManager::listSaves() -> std::vector<SaveInfo> {
	std::vector<SaveInfo> saves;
	const auto saveDir = getSaveDirectory();
	if (!exists(saveDir))
		return saves;
	for (const auto& entry: std::filesystem::directory_iterator(saveDir)) {
		if (!entry.is_regular_file() || entry.path().extension() != ".owl_save")
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
			OWL_CORE_WARN("SaveManager: Skipping corrupt save '{}': {}", entry.path().string(), e.what())
		}
	}
	std::ranges::sort(saves, [](const auto& iA, const auto& iB) -> auto { return iA.slot < iB.slot; });
	return saves;
}

auto SaveManager::hasSave(const uint32_t iSlot) -> bool { return exists(getSlotPath(iSlot)); }

void SaveManager::deleteSave(const uint32_t iSlot) {
	const auto path = getSlotPath(iSlot);
	if (exists(path))
		std::filesystem::remove(path);
}

auto SaveManager::getScenePath(const uint32_t iSlot) -> std::string {
	try {
		const auto path = getSlotPath(iSlot);
		if (!exists(path))
			return {};
		const YAML::Node root = YAML::LoadFile(path.string());
		if (const auto header = root["OwlSave"]; header && header["scenePath"])
			return header["scenePath"].as<std::string>();
	} catch (const std::exception& e) {
		OWL_CORE_WARN("SaveManager: Cannot read save slot {}: {}", iSlot, e.what())
	}
	return {};
}

}// namespace owl::scene
