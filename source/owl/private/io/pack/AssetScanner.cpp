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

/// Scan a Lua script file for scene.load_scene() calls and recursively add referenced scenes.
void AssetScanner::scanLuaScriptForScenes(const std::filesystem::path& iScriptPath,// NOLINT(misc-no-recursion)
										  std::set<std::string>& ioVisitedScenes,
										  std::vector<AssetReference>& ioAssets) {
	if (!exists(iScriptPath))
		return;
	std::ifstream scriptFile(iScriptPath);
	const std::string scriptContent((std::istreambuf_iterator<char>(scriptFile)), std::istreambuf_iterator<char>());
	static const std::regex sceneLoadPattern(R"(scene\.load_scene\s*\(\s*["']([^"']+)["']\s*\))");
	auto begin = std::sregex_iterator(scriptContent.begin(), scriptContent.end(), sceneLoadPattern);
	const auto end = std::sregex_iterator();
	for (auto it = begin; it != end; ++it) {
		if (auto scenePath = resolveScene((*it)[1].str()); scenePath)
			scanSceneRecursive(*scenePath, ioVisitedScenes, ioAssets);
	}
}

void AssetScanner::scanSceneRecursive(const std::filesystem::path& iSceneFile,// NOLINT(misc-no-recursion)
									  std::set<std::string>& ioVisitedScenes,
									  std::vector<AssetReference>& ioAssets) {
	const auto sceneStr = iSceneFile.string();
	if (ioVisitedScenes.contains(sceneStr))
		return;
	ioVisitedScenes.insert(sceneStr);

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

	for (auto entity: entities) {
		// Scan SpriteRenderer texture.
		if (auto sprite = entity["SpriteRenderer"]; sprite) {
			if (auto tex = sprite["texture"]; tex) {
				if (auto ref = resolveTexture(tex.as<std::string>()); ref && !hasAsset(ioAssets, ref->packPath)) {
					ioAssets.push_back(*ref);
				}
			}
		}
		// Scan BackgroundTexture texture.
		if (auto bg = entity["BackgroundTexture"]; bg) {
			if (auto tex = bg["texture"]; tex) {
				if (auto ref = resolveTexture(tex.as<std::string>()); ref && !hasAsset(ioAssets, ref->packPath)) {
					ioAssets.push_back(*ref);
				}
			}
		}
		// Scan TextRenderer font.
		if (auto text = entity["TextRenderer"]; text) {
			if (auto font = text["font"]; font) {
				if (auto ref = resolveFont(font.as<std::string>()); ref && !hasAsset(ioAssets, ref->packPath)) {
					ioAssets.push_back(*ref);
				}
			}
		}
		// Scan SoundSource sound asset.
		if (auto soundSrc = entity["SoundSource"]; soundSrc) {
			if (auto asset = soundSrc["soundAsset"]; asset) {
				if (auto ref = resolveSound(asset.as<std::string>()); ref && !hasAsset(ioAssets, ref->packPath)) {
					ioAssets.push_back(*ref);
				}
			}
		}
		// Scan LuaScript script path and content for scene references.
		if (auto luaScript = entity["LuaScript"]; luaScript) {
			if (auto scriptPath = luaScript["scriptPath"]; scriptPath) {
				if (auto ref = resolveScript(scriptPath.as<std::string>()); ref && !hasAsset(ioAssets, ref->packPath)) {
					ioAssets.push_back(*ref);
					scanLuaScriptForScenes(ref->diskPath, ioVisitedScenes, ioAssets);
				}
			}
		}
		// Scan Trigger for teleport scene references.
		if (auto trigger = entity["Trigger"]; trigger) {
			if (auto type = trigger["Type"]; type && type.as<std::string>() == "Teleport") {
				if (auto level = trigger["LevelName"]; level) {
					if (auto scenePath = resolveScene(level.as<std::string>()); scenePath) {
						scanSceneRecursive(*scenePath, ioVisitedScenes, ioAssets);
					}
				}
			}
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

auto AssetScanner::scanScene(const std::filesystem::path& iSceneFile) -> std::vector<AssetReference> {
	std::set<std::string> visited;
	std::vector<AssetReference> assets;
	scanSceneRecursive(iSceneFile, visited, assets);
	collectEngineAssets(assets);
	return assets;
}

auto AssetScanner::scanProject(const std::filesystem::path& iProjectDir,
							   const std::string& iFirstScene) -> std::vector<AssetReference> {
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

	if (firstScenePath.empty())
		return {};

	return scanScene(firstScenePath);
}

}// namespace owl::io::pack
