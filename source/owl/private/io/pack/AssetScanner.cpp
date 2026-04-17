/**
 * @file AssetScanner.cpp
 * @author Silmaen
 * @date 09/03/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "io/pack/AssetScanner.h"

#include "core/Application.h"

#include <fstream>
#include <regex>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
OWL_DIAG_DISABLE_CLANG("-Wshadow")
#include <yaml-cpp/yaml.h>
OWL_DIAG_POP

namespace owl::io::pack {

namespace {

auto makeRelativePath(const std::filesystem::path& iAbsolute) -> std::string {
	if (!core::Application::instanced())
		return iAbsolute.filename().string();
	for (const auto& [title, assetsPath]: core::Application::get().getAssetDirectories()) {
		if (iAbsolute.string().starts_with(assetsPath.string())) {
			return relative(iAbsolute, assetsPath).string();
		}
	}
	return iAbsolute.filename().string();
}

auto hasAsset(const std::vector<AssetReference>& iAssets, const std::string& iPackPath) -> bool {
	return std::ranges::any_of(iAssets, [&iPackPath](const auto& ref) -> auto { return ref.packPath == iPackPath; });
}

/// Add a warning for an unresolvable reference. Skips if warnings are not being collected.
void pushWarning(std::vector<std::string>* ioWarnings, const std::string& iKind, const std::string& iName,
				 const std::string& iSource) {
	if (ioWarnings != nullptr && !iName.empty())
		ioWarnings->push_back(std::format("{} '{}' referenced by {} not found", iKind, iName, iSource));
}

}// namespace

auto AssetScanner::resolveTexture(const std::string& iSerialized) -> std::optional<AssetReference> {
	if (iSerialized.size() < 4)
		return std::nullopt;
	const auto key = iSerialized.substr(0, 4);
	const auto val = iSerialized.substr(4);

	if (key == "emp:" || key == "siz:" || key == "spec")
		return std::nullopt;

	std::filesystem::path resolvedPath;
	std::string packPath;

	if (key == "nam:") {
		if (!core::Application::instanced())
			return std::nullopt;
		const std::filesystem::path name(val);
		for (const auto& [title, assetsPath]: core::Application::get().getAssetDirectories()) {
			if (const auto filePath = assetsPath / name; std::filesystem::exists(filePath)) {
				resolvedPath = filePath;
				packPath = val;
				break;
			}
		}
		if (resolvedPath.empty())
			return std::nullopt;
	} else if (key == "pat:") {
		resolvedPath = val;
		if (!std::filesystem::exists(resolvedPath))
			return std::nullopt;
		packPath = makeRelativePath(resolvedPath);
	} else {
		return std::nullopt;
	}

	return AssetReference{.packPath = packPath, .diskPath = resolvedPath, .assetType = AssetType::Texture};
}

auto AssetScanner::resolveFont(const std::string& iFontName) -> std::optional<AssetReference> {
	if (iFontName.empty())
		return std::nullopt;
	if (!core::Application::instanced())
		return std::nullopt;

	for (const auto& [title, assetsPath]: core::Application::get().getAssetDirectories()) {
		const auto fontsDir = assetsPath / "fonts";
		if (!std::filesystem::exists(fontsDir))
			continue;
		for (const auto& item: std::filesystem::recursive_directory_iterator(fontsDir)) {
			if (!item.is_regular_file() || item.path().extension() != ".ttf")
				continue;
			if (item.path().stem() == iFontName) {
				const auto rel = relative(item.path(), assetsPath).string();
				return AssetReference{.packPath = rel, .diskPath = item.path(), .assetType = AssetType::Font};
			}
		}
	}
	return std::nullopt;
}

auto AssetScanner::resolveSound(const std::string& iSoundAsset) -> std::optional<AssetReference> {
	if (iSoundAsset.empty())
		return std::nullopt;
	// Absolute path: resolve directly.
	if (const std::filesystem::path absPath(iSoundAsset); absPath.is_absolute() && exists(absPath))
		return AssetReference{.packPath = makeRelativePath(absPath), .diskPath = absPath, .assetType = AssetType::Sound};
	if (!core::Application::instanced())
		return std::nullopt;
	for (const auto& [title, assetsPath]: core::Application::get().getAssetDirectories()) {
		if (auto p = assetsPath / iSoundAsset; exists(p))
			return AssetReference{.packPath = iSoundAsset, .diskPath = p, .assetType = AssetType::Sound};
		if (auto p = assetsPath / "sounds" / iSoundAsset; exists(p)) {
			const auto rel = std::filesystem::path("sounds") / iSoundAsset;
			return AssetReference{.packPath = rel.string(), .diskPath = p, .assetType = AssetType::Sound};
		}
	}
	return std::nullopt;
}

auto AssetScanner::resolveScript(const std::string& iScriptPath) -> std::optional<AssetReference> {
	if (iScriptPath.empty())
		return std::nullopt;
	// If the path is absolute and exists, use it directly.
	if (const std::filesystem::path absPath(iScriptPath); absPath.is_absolute() && exists(absPath))
		return AssetReference{.packPath = makeRelativePath(absPath), .diskPath = absPath, .assetType = AssetType::Script};
	if (!core::Application::instanced())
		return std::nullopt;
	for (const auto& [title, assetsPath]: core::Application::get().getAssetDirectories()) {
		if (auto p = assetsPath / iScriptPath; exists(p))
			return AssetReference{.packPath = iScriptPath, .diskPath = p, .assetType = AssetType::Script};
		if (auto p = assetsPath / "scripts" / iScriptPath; exists(p)) {
			const auto rel = std::filesystem::path("scripts") / iScriptPath;
			return AssetReference{.packPath = rel.string(), .diskPath = p, .assetType = AssetType::Script};
		}
	}
	return std::nullopt;
}

auto AssetScanner::resolveScene(const std::string& iLevelName) -> std::optional<std::filesystem::path> {
	if (iLevelName.empty())
		return std::nullopt;

	std::string resolvedName = iLevelName;
	if (std::filesystem::path(resolvedName).extension() != ".owl")
		resolvedName += ".owl";

	// Absolute path: resolve directly.
	if (std::filesystem::path absPath(resolvedName); absPath.is_absolute() && exists(absPath))
		return absPath;

	if (!core::Application::instanced())
		return std::nullopt;

	for (const auto& [title, assetsPath]: core::Application::get().getAssetDirectories()) {
		if (auto p = assetsPath / resolvedName; exists(p))
			return p;
		if (auto p = assetsPath / "scenes" / resolvedName; exists(p))
			return p;
	}
	return std::nullopt;
}

/// Scan a Lua script file for scene references and recursively add referenced scenes.
/// Matches both direct calls (scene.load_scene("x.owl")) and string literals that look like
/// scene paths (e.g., assigned to a variable for deferred loading).
void AssetScanner::scanLuaScriptForScenes(const std::filesystem::path& iScriptPath,// NOLINT(misc-no-recursion)
										  std::set<std::string>& ioVisitedScenes,
										  std::vector<AssetReference>& ioAssets,
										  std::vector<std::string>* ioWarnings) {
	if (!exists(iScriptPath))
		return;
	std::ifstream scriptFile(iScriptPath);
	const std::string scriptContent((std::istreambuf_iterator<char>(scriptFile)), std::istreambuf_iterator<char>());
	// Match any quoted string that looks like a scene path (contains "scenes/" or ends with ".owl").
	static const std::regex sceneStringPattern(R"(["']((?:scenes/)?[^"']+\.owl)["'])");
	auto begin = std::sregex_iterator(scriptContent.begin(), scriptContent.end(), sceneStringPattern);
	const auto end = std::sregex_iterator();
	for (auto it = begin; it != end; ++it) {
		const auto sceneName = (*it)[1].str();
		if (auto scenePath = resolveScene(sceneName); scenePath)
			scanSceneRecursive(*scenePath, ioVisitedScenes, ioAssets, ioWarnings);
		else if (ioWarnings != nullptr)
			ioWarnings->push_back(std::format("Scene '{}' referenced by {} not found", sceneName,
											  iScriptPath.filename().string()));
	}
}

void AssetScanner::scanLuaScriptForSounds(const std::filesystem::path& iScriptPath,
										  std::vector<AssetReference>& ioAssets,
										  std::vector<std::string>* ioWarnings) {
	if (!exists(iScriptPath))
		return;
	std::ifstream scriptFile(iScriptPath);
	const std::string scriptContent((std::istreambuf_iterator<char>(scriptFile)), std::istreambuf_iterator<char>());
	static const std::regex soundPlayPattern(R"(sound\.play\s*\(\s*["']([^"']+)["']\s*\))");
	auto begin = std::sregex_iterator(scriptContent.begin(), scriptContent.end(), soundPlayPattern);
	const auto end = std::sregex_iterator();
	for (auto it = begin; it != end; ++it) {
		const auto soundName = (*it)[1].str();
		if (auto ref = resolveSound(soundName); ref) {
			if (!hasAsset(ioAssets, ref->packPath))
				ioAssets.push_back(*ref);
		} else if (ioWarnings != nullptr) {
			ioWarnings->push_back(std::format("Sound '{}' referenced by {} not found", soundName,
											  iScriptPath.filename().string()));
		}
	}
}

void AssetScanner::scanSceneRecursive(const std::filesystem::path& iSceneFile,// NOLINT(misc-no-recursion)
									  std::set<std::string>& ioVisitedScenes,
									  std::vector<AssetReference>& ioAssets,
									  std::vector<std::string>* ioWarnings) {
	const auto sceneStr = iSceneFile.string();
	if (ioVisitedScenes.contains(sceneStr))
		return;
	ioVisitedScenes.insert(sceneStr);

	if (!exists(iSceneFile))
		return;

	// Add the scene file itself.
	if (const auto scenePack = makeRelativePath(iSceneFile); !hasAsset(ioAssets, scenePack)) {
		ioAssets.push_back({.packPath = scenePack, .diskPath = iSceneFile, .assetType = AssetType::Scene});
	}

	// Parse the YAML.
	YAML::Node data;
	try {
		data = YAML::LoadFile(sceneStr);
	} catch (...) {
		return;
	}

	if (!data["Scene"])
		return;

	auto entities = data["Entities"];
	if (!entities)
		return;

	const auto sceneName = iSceneFile.filename().string();
	for (auto entity: entities)
		scanEntity(entity, sceneName, ioVisitedScenes, ioAssets, ioWarnings);
}

// NOLINTNEXTLINE(misc-no-recursion)
void AssetScanner::scanEntity(const YAML::Node& iEntity, const std::string& iSceneName,
							  std::set<std::string>& ioVisitedScenes, std::vector<AssetReference>& ioAssets,
							  std::vector<std::string>* ioWarnings) {
	const auto addTextureField = [&](const YAML::Node& iComponent) {
		if (auto tex = iComponent["texture"]; tex) {
			const auto val = tex.as<std::string>();
			if (auto ref = resolveTexture(val); ref) {
				if (!hasAsset(ioAssets, ref->packPath))
					ioAssets.push_back(*ref);
			} else
				pushWarning(ioWarnings, "Texture", val, iSceneName);
		}
	};
	if (auto sprite = iEntity["SpriteRenderer"]; sprite)
		addTextureField(sprite);
	if (auto anim = iEntity["AnimatedSpriteRenderer"]; anim)
		addTextureField(anim);
	if (auto bg = iEntity["BackgroundTexture"]; bg)
		addTextureField(bg);
	if (auto img = iEntity["UIImage"]; img)
		addTextureField(img);
	if (auto text = iEntity["TextRenderer"]; text)
		if (auto font = text["font"]; font) {
			const auto name = font.as<std::string>();
			if (auto ref = resolveFont(name); ref && !hasAsset(ioAssets, ref->packPath))
				ioAssets.push_back(*ref);
			else if (!ref)
				pushWarning(ioWarnings, "Font", name, iSceneName);
		}
	if (auto soundSrc = iEntity["SoundSource"]; soundSrc)
		if (auto asset = soundSrc["soundAsset"]; asset) {
			const auto name = asset.as<std::string>();
			if (auto ref = resolveSound(name); ref && !hasAsset(ioAssets, ref->packPath))
				ioAssets.push_back(*ref);
			else if (!ref)
				pushWarning(ioWarnings, "Sound", name, iSceneName);
		}
	if (auto luaScript = iEntity["LuaScript"]; luaScript)
		if (auto scriptPath = luaScript["scriptPath"]; scriptPath) {
			const auto name = scriptPath.as<std::string>();
			if (auto ref = resolveScript(name); ref) {
				if (!hasAsset(ioAssets, ref->packPath))
					ioAssets.push_back(*ref);
				scanLuaScriptForScenes(ref->diskPath, ioVisitedScenes, ioAssets, ioWarnings);
				scanLuaScriptForSounds(ref->diskPath, ioAssets, ioWarnings);
			} else
				pushWarning(ioWarnings, "Script", name, iSceneName);
		}
	if (auto trigger = iEntity["Trigger"]; trigger)
		if (auto level = trigger["LevelName"]; level) {
			const auto name = level.as<std::string>();
			if (!name.empty()) {
				if (auto scenePath = resolveScene(name); scenePath)
					scanSceneRecursive(*scenePath, ioVisitedScenes, ioAssets, ioWarnings);
				else
					pushWarning(ioWarnings, "Trigger scene", name, iSceneName);
			}
		}
}

void AssetScanner::collectEngineAssets(std::vector<AssetReference>& ioAssets) {
	if (!core::Application::instanced())
		return;
	for (const auto& [title, assetsPath]: core::Application::get().getAssetDirectories()) {
		// Collect all shader files.
		if (const auto shadersDir = assetsPath / "shaders"; exists(shadersDir)) {
			for (const auto& item: std::filesystem::recursive_directory_iterator(shadersDir)) {
				if (!item.is_regular_file())
					continue;
				if (const auto rel = relative(item.path(), assetsPath).string(); !hasAsset(ioAssets, rel)) {
					ioAssets.push_back({.packPath = rel, .diskPath = item.path(), .assetType = AssetType::Other});
				}
			}
		}
		// Collect the default font (OpenSans-Regular).
		if (const auto fontsDir = assetsPath / "fonts"; exists(fontsDir)) {
			for (const auto& item: std::filesystem::recursive_directory_iterator(fontsDir)) {
				if (!item.is_regular_file() || item.path().extension() != ".ttf")
					continue;
				if (item.path().stem() == "OpenSans-Regular") {
					if (const auto rel = relative(item.path(), assetsPath).string(); !hasAsset(ioAssets, rel)) {
						ioAssets.push_back(
								{.packPath = rel, .diskPath = item.path(), .assetType = AssetType::Font});
					}
				}
			}
		}
	}
}

auto AssetScanner::scanScene(const std::filesystem::path& iSceneFile,
							 std::vector<std::string>* oWarnings) -> std::vector<AssetReference> {
	std::set<std::string> visited;
	std::vector<AssetReference> assets;
	scanSceneRecursive(iSceneFile, visited, assets, oWarnings);
	collectEngineAssets(assets);
	return assets;
}

auto AssetScanner::scanProject(const std::filesystem::path& iProjectDir, const std::string& iFirstScene,
							   std::vector<std::string>* oWarnings) -> std::vector<AssetReference> {
	// Resolve the first scene path.
	std::filesystem::path firstScenePath;
	std::string resolvedName = iFirstScene;
	if (std::filesystem::path(resolvedName).extension() != ".owl")
		resolvedName += ".owl";

	if (core::Application::instanced()) {
		for (const auto& [title, assetsPath]: core::Application::get().getAssetDirectories()) {
			if (const auto p = assetsPath / resolvedName; exists(p)) {
				firstScenePath = p;
				break;
			}
			if (const auto p = assetsPath / "scenes" / resolvedName; exists(p)) {
				firstScenePath = p;
				break;
			}
		}
	}
	if (firstScenePath.empty()) {
		// Fallback: try project directory directly.
		if (const auto p = iProjectDir / resolvedName; exists(p))
			firstScenePath = p;
		else if (const auto p2 = iProjectDir / "scenes" / resolvedName; exists(p2))
			firstScenePath = p2;
	}

	if (firstScenePath.empty()) {
		if (oWarnings != nullptr)
			oWarnings->push_back(std::format("First scene '{}' not found in project", iFirstScene));
		return {};
	}

	return scanScene(firstScenePath, oWarnings);
}

}// namespace owl::io::pack
