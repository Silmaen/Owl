/**
 * @file SettingsManager.cpp
 * @author Silmaen
 * @date 14/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "scene/SettingsManager.h"

#include "core/Application.h"
#include "sound/SoundCommand.h"

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
OWL_DIAG_DISABLE_CLANG("-Wshadow")
#include <yaml-cpp/yaml.h>
OWL_DIAG_POP

#include <fstream>

namespace owl::scene {

std::string SettingsManager::s_gameName;
std::unordered_map<std::string, SettingsManager::Value> SettingsManager::s_defaults;
std::unordered_map<std::string, SettingsManager::Value> SettingsManager::s_overrides;

void SettingsManager::setGameName(const std::string& iGameName) { s_gameName = iGameName; }

auto SettingsManager::getUserDirectory() -> std::filesystem::path {
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
	const auto userDir = baseDir / name;
	if (!exists(userDir))
		create_directories(userDir);
	return userDir;
}

auto SettingsManager::getSettingsPath() -> std::filesystem::path { return getUserDirectory() / "settings.yml"; }

namespace {

void serializeValue(YAML::Emitter& iOut, const std::string& iKey, const SettingsManager::Value& iValue) {
	iOut << YAML::BeginMap;
	iOut << YAML::Key << "key" << YAML::Value << iKey;
	std::visit(
			[&](const auto& iVal) {
				using T = std::decay_t<decltype(iVal)>;
				if constexpr (std::is_same_v<T, int64_t>) {
					iOut << YAML::Key << "type" << YAML::Value << "int";
					iOut << YAML::Key << "value" << YAML::Value << iVal;
				} else if constexpr (std::is_same_v<T, float>) {
					iOut << YAML::Key << "type" << YAML::Value << "float";
					iOut << YAML::Key << "value" << YAML::Value << iVal;
				} else if constexpr (std::is_same_v<T, std::string>) {
					iOut << YAML::Key << "type" << YAML::Value << "string";
					iOut << YAML::Key << "value" << YAML::Value << iVal;
				} else if constexpr (std::is_same_v<T, bool>) {
					iOut << YAML::Key << "type" << YAML::Value << "bool";
					iOut << YAML::Key << "value" << YAML::Value << iVal;
				}
			},
			iValue);
	iOut << YAML::EndMap;
}

auto deserializeEntries(const YAML::Node& iNode) -> std::unordered_map<std::string, SettingsManager::Value> {
	std::unordered_map<std::string, SettingsManager::Value> result;
	if (!iNode || !iNode.IsSequence())
		return result;
	for (const auto& entry: iNode) {
		if (!entry["key"] || !entry["type"] || !entry["value"])
			continue;
		const auto entryKey = entry["key"].as<std::string>();
		const auto type = entry["type"].as<std::string>();
		if (type == "int")
			result[entryKey] = entry["value"].as<int64_t>();
		else if (type == "float")
			result[entryKey] = entry["value"].as<float>();
		else if (type == "string")
			result[entryKey] = entry["value"].as<std::string>();
		else if (type == "bool")
			result[entryKey] = entry["value"].as<bool>();
	}
	return result;
}

}// namespace

void SettingsManager::loadDefaults(const std::filesystem::path& iPath) {
	if (!exists(iPath))
		return;
	try {
		const auto data = YAML::LoadFile(iPath.string());
		if (const auto entries = data["GameSettings"]; entries)
			s_defaults = deserializeEntries(entries);
	} catch (...) {
		OWL_CORE_WARN("Failed to load game settings from {}", iPath.string())
	}
}

void SettingsManager::loadUserSettings() {
	const auto path = getSettingsPath();
	if (!exists(path))
		return;
	try {
		const auto data = YAML::LoadFile(path.string());
		if (const auto entries = data["UserSettings"]; entries)
			s_overrides = deserializeEntries(entries);
	} catch (...) {
		OWL_CORE_WARN("Failed to load user settings from {}", path.string())
	}
}

void SettingsManager::saveUserSettings() {
	const auto path = getSettingsPath();
	create_directories(path.parent_path());
	YAML::Emitter out;
	out << YAML::BeginMap;
	out << YAML::Key << "UserSettings" << YAML::Value << YAML::BeginSeq;
	for (const auto& [key, value]: s_overrides)
		serializeValue(out, key, value);
	out << YAML::EndSeq;
	out << YAML::EndMap;
	std::ofstream fileOut(path);
	fileOut << out.c_str();
}

void SettingsManager::setDefault(const std::string& iKey, Value iValue) { s_defaults[iKey] = std::move(iValue); }

void SettingsManager::set(const std::string& iKey, Value iValue) { s_overrides[iKey] = std::move(iValue); }

auto SettingsManager::get(const std::string& iKey) -> std::optional<Value> {
	if (const auto it = s_overrides.find(iKey); it != s_overrides.end())
		return it->second;
	if (const auto it = s_defaults.find(iKey); it != s_defaults.end())
		return it->second;
	return std::nullopt;
}

auto SettingsManager::get(const std::string& iKey, const Value& iDefault) -> Value {
	if (const auto val = get(iKey); val.has_value())
		return val.value();
	return iDefault;
}

void SettingsManager::resetToDefault(const std::string& iKey) { s_overrides.erase(iKey); }

void SettingsManager::resetAllToDefaults() { s_overrides.clear(); }

auto SettingsManager::hasOverride(const std::string& iKey) -> bool { return s_overrides.contains(iKey); }

auto SettingsManager::has(const std::string& iKey) -> bool {
	return s_overrides.contains(iKey) || s_defaults.contains(iKey);
}

auto SettingsManager::keys() -> std::vector<std::string> {
	std::unordered_set<std::string> allKeys;
	for (const auto& [key, val]: s_defaults)
		allKeys.insert(key);
	for (const auto& [key, val]: s_overrides)
		allKeys.insert(key);
	return {allKeys.begin(), allKeys.end()};
}

void SettingsManager::applyBuiltins() {
	if (!core::Application::instanced())
		return;
	auto& window = core::Application::get().getWindow();

	// Window settings.
	const auto width = getAs<int64_t>(KeyResolutionWidth);
	const auto height = getAs<int64_t>(KeyResolutionHeight);
	if (width.has_value() && height.has_value())
		window.setSize(static_cast<uint32_t>(width.value()), static_cast<uint32_t>(height.value()));

	if (const auto fs = getAs<bool>(KeyFullscreen); fs.has_value())
		window.setFullscreen(fs.value());

	if (const auto rs = getAs<bool>(KeyResizable); rs.has_value())
		window.setResizable(rs.value());

	// Sound settings — master volume via listener gain.
	if (const auto vol = getAs<float>(KeyVolumeMaster); vol.has_value())
		sound::SoundCommand::setListenerGain(vol.value());
}

void SettingsManager::clear() {
	s_defaults.clear();
	s_overrides.clear();
	s_gameName.clear();
}

}// namespace owl::scene
