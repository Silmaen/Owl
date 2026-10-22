/**
 * @file AssetScanner.cpp
 * @author Silmaen
 * @date 09/03/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "data/assets/pack/AssetScanner.h"

#include "app/Application.h"
#include "data/fonts/FontSettings.h"
#include "data/geometry/MeshLoader.h"

#include <filesystem>
#include <fstream>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
OWL_DIAG_DISABLE_CLANG("-Wshadow")
#include <yaml-cpp/yaml.h>
OWL_DIAG_POP

namespace owl::data::assets::pack {

namespace {
auto makeRelativePath(const std::filesystem::path& iAbsolute) -> std::string {
	if (!app::Application::instanced())
		return iAbsolute.filename().string();
	for (const auto& [title, assetsPath]: app::Application::get().getAssetDirectories()) {
		if (iAbsolute.string().starts_with(assetsPath.string())) {
			return relative(iAbsolute, assetsPath).generic_string();
		}
	}
	return iAbsolute.filename().string();
}

auto hasAsset(const std::vector<AssetReference>& iAssets, const std::string& iPackPath) -> bool {
	return std::ranges::any_of(iAssets, [&iPackPath](const auto& ref) -> auto { return ref.packPath == iPackPath; });
}

void pushWarning(std::vector<std::string>* ioWarnings, const std::string& iKind, const std::string& iName,
				 const std::string& iSource) {
	if (ioWarnings != nullptr && !iName.empty())
		ioWarnings->push_back(std::format("{} '{}' referenced by {} not found", iKind, iName, iSource));
}

auto resolveDataAsset(const std::string& iRelative) -> std::optional<AssetReference> {
	if (iRelative.empty())
		return std::nullopt;
	if (const std::filesystem::path absPath(iRelative); absPath.is_absolute() && std::filesystem::exists(absPath))
		return AssetReference{.packPath = makeRelativePath(absPath),
							  .diskPath = absPath,
							  .assetType = AssetType::Other};
	if (!app::Application::instanced())
		return std::nullopt;
	for (const auto& [title, assetsPath]: app::Application::get().getAssetDirectories()) {
		if (auto p = assetsPath / iRelative; std::filesystem::exists(p))
			return AssetReference{.packPath = iRelative, .diskPath = p, .assetType = AssetType::Other};
	}
	return std::nullopt;
}

auto readYamlScalar(const std::filesystem::path& iFile, const char* iKey) -> std::string {
	std::ifstream in(iFile, std::ios::binary);
	if (!in.is_open())
		return {};
	std::stringstream buffer;
	buffer << in.rdbuf();
	auto text = buffer.str();
	// A document-end line may be followed by binary data (`.owltilemap` chunks): only the first document is read.
	if (const auto end = text.find("\n...\n"); end != std::string::npos)
		text.resize(end + 1);
	try {
		if (const YAML::Node root = YAML::Load(text); root && root[iKey] && root[iKey].IsScalar())
			return root[iKey].as<std::string>();
	} catch (const YAML::Exception&) { return {}; }
	return {};
}

void scanSceneRecursive(const std::filesystem::path& iSceneFile, std::set<std::string>& ioVisitedScenes,
						std::vector<AssetReference>& ioAssets, std::vector<std::string>* ioWarnings);

void scanEntity(const YAML::Node& iEntity, const std::string& iSceneName, std::set<std::string>& ioVisitedScenes,
				std::vector<AssetReference>& ioAssets, std::vector<std::string>* ioWarnings);

void scanTileset(const std::string& iTilesetPath, const std::string& iSceneName, std::vector<AssetReference>& ioAssets,
				 std::vector<std::string>* ioWarnings);

void scanHud(const std::string& iHudPath, const std::string& iSource, std::set<std::string>& ioVisitedScenes,
			 std::vector<AssetReference>& ioAssets, std::vector<std::string>* ioWarnings);

auto resolveTexture(const std::string& iSerialized) -> std::optional<AssetReference> {
	if (iSerialized.size() < 4)
		return std::nullopt;
	const auto key = iSerialized.substr(0, 4);
	const auto val = iSerialized.substr(4);

	if (key == "emp:" || key == "siz:" || key == "spec")
		return std::nullopt;

	std::filesystem::path resolvedPath;
	std::string packPath;

	if (key == "nam:") {
		if (!app::Application::instanced())
			return std::nullopt;
		const std::filesystem::path name(val);
		for (const auto& [title, assetsPath]: app::Application::get().getAssetDirectories()) {
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

void addFontSidecar(std::vector<AssetReference>& ioAssets, const AssetReference& iFont) {
	const auto diskPath = fonts::FontSettings::sidecarPath(iFont.diskPath);
	if (!exists(diskPath))
		return;
	if (const auto packPath = fonts::FontSettings::sidecarPath(iFont.packPath).generic_string();
		!hasAsset(ioAssets, packPath))
		ioAssets.push_back({.packPath = packPath, .diskPath = diskPath, .assetType = AssetType::Other});
}

auto resolveFont(const std::string& iFontName) -> std::optional<AssetReference> {
	if (iFontName.empty())
		return std::nullopt;
	if (!app::Application::instanced())
		return std::nullopt;

	for (const auto& [title, assetsPath]: app::Application::get().getAssetDirectories()) {
		const auto fontsDir = assetsPath / "fonts";
		if (!std::filesystem::exists(fontsDir))
			continue;
		for (const auto& item: std::filesystem::recursive_directory_iterator(fontsDir)) {
			if (!item.is_regular_file() || item.path().extension() != ".ttf")
				continue;
			if (item.path().stem() == iFontName) {
				const auto rel = relative(item.path(), assetsPath).generic_string();
				return AssetReference{.packPath = rel, .diskPath = item.path(), .assetType = AssetType::Font};
			}
		}
	}
	return std::nullopt;
}

auto resolveSound(const std::string& iSoundAsset) -> std::optional<AssetReference> {
	if (iSoundAsset.empty())
		return std::nullopt;
	// Absolute path: resolve directly.
	if (const std::filesystem::path absPath(iSoundAsset); absPath.is_absolute() && exists(absPath))
		return AssetReference{.packPath = makeRelativePath(absPath),
							  .diskPath = absPath,
							  .assetType = AssetType::Sound};
	if (!app::Application::instanced())
		return std::nullopt;
	for (const auto& [title, assetsPath]: app::Application::get().getAssetDirectories()) {
		if (auto p = assetsPath / iSoundAsset; exists(p))
			return AssetReference{.packPath = iSoundAsset, .diskPath = p, .assetType = AssetType::Sound};
		if (auto p = assetsPath / "sounds" / iSoundAsset; exists(p)) {
			const auto rel = std::filesystem::path("sounds") / iSoundAsset;
			return AssetReference{.packPath = rel.string(), .diskPath = p, .assetType = AssetType::Sound};
		}
	}
	return std::nullopt;
}

auto resolveScript(const std::string& iScriptPath) -> std::optional<AssetReference> {
	if (iScriptPath.empty())
		return std::nullopt;
	// If the path is absolute and exists, use it directly.
	if (const std::filesystem::path absPath(iScriptPath); absPath.is_absolute() && exists(absPath))
		return AssetReference{.packPath = makeRelativePath(absPath),
							  .diskPath = absPath,
							  .assetType = AssetType::Script};
	if (!app::Application::instanced())
		return std::nullopt;
	for (const auto& [title, assetsPath]: app::Application::get().getAssetDirectories()) {
		if (auto p = assetsPath / iScriptPath; exists(p))
			return AssetReference{.packPath = iScriptPath, .diskPath = p, .assetType = AssetType::Script};
		if (auto p = assetsPath / "scripts" / iScriptPath; exists(p)) {
			const auto rel = std::filesystem::path("scripts") / iScriptPath;
			return AssetReference{.packPath = rel.string(), .diskPath = p, .assetType = AssetType::Script};
		}
	}
	return std::nullopt;
}

auto resolveScene(const std::string& iLevelName) -> std::optional<std::filesystem::path> {
	if (iLevelName.empty())
		return std::nullopt;

	std::string resolvedName = iLevelName;
	if (std::filesystem::path(resolvedName).extension() != ".owl")
		resolvedName += ".owl";

	// Absolute path: resolve directly.
	if (std::filesystem::path absPath(resolvedName); absPath.is_absolute() && exists(absPath))
		return absPath;

	if (!app::Application::instanced())
		return std::nullopt;

	for (const auto& [title, assetsPath]: app::Application::get().getAssetDirectories()) {
		if (auto p = assetsPath / resolvedName; exists(p))
			return p;
		if (auto p = assetsPath / "scenes" / resolvedName; exists(p))
			return p;
	}
	return std::nullopt;
}

void followScene(const std::string& iName, const std::string& iSource,// NOLINT(misc-no-recursion)
				 std::set<std::string>& ioVisitedScenes, std::vector<AssetReference>& ioAssets,
				 std::vector<std::string>* ioWarnings) {
	if (auto scenePath = resolveScene(iName); scenePath)
		scanSceneRecursive(*scenePath, ioVisitedScenes, ioAssets, ioWarnings);
	else
		pushWarning(ioWarnings, "Scene", iName, iSource);
}

void followSound(const std::string& iName, const std::string& iSource, std::vector<AssetReference>& ioAssets,
				 std::vector<std::string>* ioWarnings) {
	if (auto ref = resolveSound(iName); ref) {
		if (!hasAsset(ioAssets, ref->packPath))
			ioAssets.push_back(*ref);
	} else
		pushWarning(ioWarnings, "Sound", iName, iSource);
}

void followTexture(const std::string& iSerialized, const std::string& iSource, std::vector<AssetReference>& ioAssets,
				   std::vector<std::string>* ioWarnings) {
	if (auto ref = resolveTexture(iSerialized); ref) {
		if (!hasAsset(ioAssets, ref->packPath))
			ioAssets.push_back(*ref);
	} else
		pushWarning(ioWarnings, "Texture", iSerialized, iSource);
}

auto isSoundFile(const std::string& iName) -> bool {
	const auto ext = std::filesystem::path(iName).extension();
	return ext == ".wav" || ext == ".ogg" || ext == ".mp3" || ext == ".flac";
}

auto isSerializedTexture(const std::string& iValue) -> bool {
	return iValue.starts_with("nam:") || iValue.starts_with("pat:");
}

void scanLuaScript(const std::filesystem::path& iScriptPath,// NOLINT(misc-no-recursion)
				   std::set<std::string>& ioVisitedScenes, std::vector<AssetReference>& ioAssets,
				   std::vector<std::string>* ioWarnings) {
	if (!exists(iScriptPath))
		return;
	std::ifstream scriptFile(iScriptPath);
	const std::string scriptContent((std::istreambuf_iterator<char>(scriptFile)), std::istreambuf_iterator<char>());
	const auto source = iScriptPath.filename().string();
	// Any quoted scene path, the argument of `sound.play` or any quoted sound file, any quoted serialized texture.
	static const std::regex scenePattern(R"(["']((?:scenes/)?[^"']+\.owl)["'])");
	static const std::regex soundPattern(
			R"((?:sound\.play\s*\(\s*["']([^"']+)["']\s*\))|["']([^"'\s]+\.(?:wav|ogg|mp3|flac))["'])");
	static const std::regex texturePattern(R"(["']((?:nam|pat):[^"']+)["'])");
	const auto end = std::sregex_iterator();
	for (auto it = std::sregex_iterator(scriptContent.begin(), scriptContent.end(), scenePattern); it != end; ++it)
		followScene((*it)[1].str(), source, ioVisitedScenes, ioAssets, ioWarnings);
	for (auto it = std::sregex_iterator(scriptContent.begin(), scriptContent.end(), soundPattern); it != end; ++it)
		followSound((*it)[1].matched ? (*it)[1].str() : (*it)[2].str(), source, ioAssets, ioWarnings);
	for (auto it = std::sregex_iterator(scriptContent.begin(), scriptContent.end(), texturePattern); it != end; ++it)
		followTexture((*it)[1].str(), source, ioAssets, ioWarnings);
}

// The string properties of a `LuaScript` follow the rules of the strings quoted in its script.
void scanScriptProperties(const YAML::Node& iProperties,// NOLINT(misc-no-recursion)
						  const std::string& iSceneName, std::set<std::string>& ioVisitedScenes,
						  std::vector<AssetReference>& ioAssets, std::vector<std::string>* ioWarnings) {
	if (!iProperties || !iProperties.IsSequence())
		return;
	for (const auto property: iProperties) {
		const auto type = property["type"];
		const auto value = property["value"];
		if (!type || !type.IsScalar() || type.as<std::string>() != "string" || !value || !value.IsScalar())
			continue;
		const auto text = value.as<std::string>();
		const auto name = property["name"] ? property["name"].as<std::string>() : std::string{};
		const auto source = std::format("the '{}' script property in {}", name, iSceneName);
		if (isSerializedTexture(text))
			followTexture(text, source, ioAssets, ioWarnings);
		else if (std::filesystem::path(text).extension() == ".owl")
			followScene(text, source, ioVisitedScenes, ioAssets, ioWarnings);
		else if (isSoundFile(text))
			followSound(text, source, ioAssets, ioWarnings);
	}
}

void scanSceneRecursive(const std::filesystem::path& iSceneFile,// NOLINT(misc-no-recursion)
						std::set<std::string>& ioVisitedScenes, std::vector<AssetReference>& ioAssets,
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
	} catch (...) { return; }

	if (!data["Scene"])
		return;

	const auto sceneName = iSceneFile.filename().string();
	if (const auto hud = data["Hud"]; hud && hud.IsScalar())
		scanHud(hud.as<std::string>(), sceneName, ioVisitedScenes, ioAssets, ioWarnings);

	if (const auto sceneScript = data["SceneScript"]; sceneScript && sceneScript.IsMap()) {
		YAML::Node holder;
		holder["LuaScript"] = sceneScript;
		scanEntity(holder, sceneName, ioVisitedScenes, ioAssets, ioWarnings);
	}

	auto entities = data["Entities"];
	if (!entities)
		return;

	for (auto entity: entities) scanEntity(entity, sceneName, ioVisitedScenes, ioAssets, ioWarnings);
}

// NOLINTNEXTLINE(misc-no-recursion)
void scanTilemap(const std::string& iTilemapPath, const std::string& iSceneName, std::vector<AssetReference>& ioAssets,
				 std::vector<std::string>* ioWarnings) {
	const auto tilemapRef = resolveDataAsset(iTilemapPath);
	if (!tilemapRef) {
		pushWarning(ioWarnings, "Tilemap", iTilemapPath, iSceneName);
		return;
	}
	if (!hasAsset(ioAssets, tilemapRef->packPath))
		ioAssets.push_back(*tilemapRef);

	if (const auto tilesetPath = readYamlScalar(tilemapRef->diskPath, "tilesetPath"); !tilesetPath.empty())
		scanTileset(tilesetPath, iSceneName, ioAssets, ioWarnings);
}

void scanObjectSheet(const std::string& iSheetPath, const std::string& iSceneName,
					 std::vector<AssetReference>& ioAssets, std::vector<std::string>* ioWarnings) {
	const auto sheetRef = resolveDataAsset(iSheetPath);
	if (!sheetRef) {
		pushWarning(ioWarnings, "Object sheet", iSheetPath, iSceneName);
		return;
	}
	if (!hasAsset(ioAssets, sheetRef->packPath))
		ioAssets.push_back(*sheetRef);
	const std::filesystem::path texturePath = std::filesystem::path{iSheetPath}.replace_extension(".png");
	const auto textureSerialized = (texturePath.is_absolute() ? "pat:" : "nam:") + texturePath.generic_string();
	if (auto textureRef = resolveTexture(textureSerialized)) {
		if (!hasAsset(ioAssets, textureRef->packPath))
			ioAssets.push_back(*textureRef);
	} else
		pushWarning(ioWarnings, "Texture", textureSerialized, iSceneName);
}

void scanTileset(const std::string& iTilesetPath, const std::string& iSceneName, std::vector<AssetReference>& ioAssets,
				 std::vector<std::string>* ioWarnings) {
	const auto tilesetRef = resolveDataAsset(iTilesetPath);
	if (!tilesetRef) {
		pushWarning(ioWarnings, "Tileset", iTilesetPath, iSceneName);
		return;
	}
	if (!hasAsset(ioAssets, tilesetRef->packPath))
		ioAssets.push_back(*tilesetRef);

	for (const char* key: {"texture", "normalTexture", "roughnessTexture"}) {
		const auto textureSerialized = readYamlScalar(tilesetRef->diskPath, key);
		if (textureSerialized.empty())
			continue;
		if (auto textureRef = resolveTexture(textureSerialized)) {
			if (!hasAsset(ioAssets, textureRef->packPath))
				ioAssets.push_back(*textureRef);
		} else
			pushWarning(ioWarnings, "Texture", textureSerialized, iSceneName);
	}
}

// A plain data file (environment image, colour LUT) referenced by its asset path.
void scanDataFile(const std::string& iPath, const char* iKind, const std::string& iSource,
				  std::vector<AssetReference>& ioAssets, std::vector<std::string>* ioWarnings) {
	if (iPath.empty())
		return;
	auto ref = resolveDataAsset(iPath);
	if (!ref) {
		pushWarning(ioWarnings, iKind, iPath, iSource);
		return;
	}
	if (!hasAsset(ioAssets, ref->packPath))
		ioAssets.push_back(*ref);
}

// The texture maps written in a material map (a `.owlmat` or the overrides of a mesh slot).
void scanMaterialMaps(const YAML::Node& iMaterial, const std::string& iSource, std::vector<AssetReference>& ioAssets,
					  std::vector<std::string>* ioWarnings) {
	for (const char* key: {"baseColorMap", "metallicMap", "roughnessMap", "normalMap", "occlusionMap", "emissiveMap"}) {
		const auto map = iMaterial[key];
		if (!map || !map.IsScalar() || map.as<std::string>().empty())
			continue;
		const auto serialized = map.as<std::string>();
		if (auto ref = resolveTexture(serialized); ref) {
			if (!hasAsset(ioAssets, ref->packPath))
				ioAssets.push_back(*ref);
		} else
			pushWarning(ioWarnings, "Texture", serialized, iSource);
	}
}

// NOLINTNEXTLINE(misc-no-recursion): a `.owlmat` parent chain stops at a material already collected.
void scanMaterial(const std::string& iPath, const std::string& iSource, std::vector<AssetReference>& ioAssets,
				  std::vector<std::string>* ioWarnings) {
	auto ref = resolveDataAsset(iPath);
	if (!ref) {
		pushWarning(ioWarnings, "Material", iPath, iSource);
		return;
	}
	if (hasAsset(ioAssets, ref->packPath))
		return;
	ioAssets.push_back(*ref);
	YAML::Node root;
	try {
		root = YAML::LoadFile(ref->diskPath.string());
	} catch (const YAML::Exception&) {
		pushWarning(ioWarnings, "Material", iPath, iSource);
		return;
	}
	const auto material = root["Material"];
	if (!material || !material.IsMap())
		return;
	scanMaterialMaps(material, iPath, ioAssets, ioWarnings);
	if (const auto parent = material["parent"]; parent && parent.IsScalar() && !parent.as<std::string>().empty())
		scanMaterial(parent.as<std::string>(), iPath, ioAssets, ioWarnings);
}

// The mesh file of a `MeshRenderer` (with the buffers and images a `.gltf` names) and its texture override.
void scanMeshRenderer(const YAML::Node& iMeshRenderer, const std::string& iSceneName,
					  std::vector<AssetReference>& ioAssets, std::vector<std::string>* ioWarnings) {
	if (auto texture = iMeshRenderer["texture"]; texture) {
		const auto serialized = texture.as<std::string>();
		if (auto ref = resolveTexture(serialized); ref) {
			if (!hasAsset(ioAssets, ref->packPath))
				ioAssets.push_back(*ref);
		} else
			pushWarning(ioWarnings, "Texture", serialized, iSceneName);
	}
	if (const auto slots = iMeshRenderer["materials"]; slots && slots.IsSequence()) {
		for (const auto& slot: slots) {
			if (const auto material = slot["material"];
				material && material.IsScalar() && !material.as<std::string>().empty())
				scanMaterial(material.as<std::string>(), iSceneName, ioAssets, ioWarnings);
			if (const auto overrides = slot["overrides"]; overrides && overrides.IsMap())
				scanMaterialMaps(overrides, iSceneName, ioAssets, ioWarnings);
		}
	}
	const auto path = iMeshRenderer["mesh"] ? iMeshRenderer["mesh"].as<std::string>() : std::string{};
	if (path.empty())
		return;
	auto meshRef = resolveDataAsset(path);
	if (!meshRef) {
		pushWarning(ioWarnings, "Mesh", path, iSceneName);
		return;
	}
	meshRef->assetType = AssetType::Mesh;
	if (!hasAsset(ioAssets, meshRef->packPath))
		ioAssets.push_back(*meshRef);
	const auto packFolder = std::filesystem::path{meshRef->packPath}.parent_path();
	const auto diskFolder = meshRef->diskPath.parent_path();
	for (const auto& uri: data::geometry::MeshLoader::getExternalFiles(meshRef->diskPath)) {
		const auto diskPath = (diskFolder / uri).lexically_normal();
		if (!exists(diskPath)) {
			pushWarning(ioWarnings, "Mesh file", uri, path);
			continue;
		}
		const auto packPath = (packFolder / uri).lexically_normal().generic_string();
		if (!hasAsset(ioAssets, packPath))
			ioAssets.push_back({.packPath = packPath, .diskPath = diskPath, .assetType = AssetType::Other});
	}
}

// The fonts of the texts of an entity and the sound of its `SoundSource`.
void scanFontsAndSounds(const YAML::Node& iEntity, const std::string& iSceneName, std::vector<AssetReference>& ioAssets,
						std::vector<std::string>* ioWarnings) {
	for (const char* component: {"TextRenderer", "UiText"})
		if (auto text = iEntity[component]; text)
			if (auto font = text["font"]; font) {
				const auto name = font.as<std::string>();
				if (auto ref = resolveFont(name); ref) {
					if (!hasAsset(ioAssets, ref->packPath))
						ioAssets.push_back(*ref);
					addFontSidecar(ioAssets, *ref);
				} else {
					pushWarning(ioWarnings, "Font", name, iSceneName);
				}
			}
	if (auto soundSrc = iEntity["SoundSource"]; soundSrc)
		if (auto asset = soundSrc["soundAsset"]; asset) {
			const auto name = asset.as<std::string>();
			if (auto ref = resolveSound(name); ref && !hasAsset(ioAssets, ref->packPath))
				ioAssets.push_back(*ref);
			else if (!ref)
				pushWarning(ioWarnings, "Sound", name, iSceneName);
		}
}

void scanEntity(const YAML::Node& iEntity,// NOLINT(misc-no-recursion)
				const std::string& iSceneName, std::set<std::string>& ioVisitedScenes,
				std::vector<AssetReference>& ioAssets, std::vector<std::string>* ioWarnings) {
	const auto addTextureField = [&](const YAML::Node& iComponent) -> void {
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
	for (const char* component: {"UiImage", "UiToolCursor"})
		if (auto widget = iEntity[component]; widget)
			addTextureField(widget);
	scanFontsAndSounds(iEntity, iSceneName, ioAssets, ioWarnings);
	if (auto meshRenderer = iEntity["MeshRenderer"]; meshRenderer)
		scanMeshRenderer(meshRenderer, iSceneName, ioAssets, ioWarnings);
	if (auto environment = iEntity["Environment"]; environment && environment["hdri"])
		scanDataFile(environment["hdri"].as<std::string>(), "Environment image", iSceneName, ioAssets, ioWarnings);
	if (auto camera = iEntity["Camera"]; camera && camera["camera"] && camera["camera"]["postProcess"]) {
		if (const auto lut = camera["camera"]["postProcess"]["lut"]; lut && lut.IsScalar())
			scanDataFile(lut.as<std::string>(), "Colour LUT", iSceneName, ioAssets, ioWarnings);
	}
	if (auto luaScript = iEntity["LuaScript"]; luaScript) {
		if (auto scriptPath = luaScript["scriptPath"]; scriptPath) {
			const auto name = scriptPath.as<std::string>();
			if (auto ref = resolveScript(name); ref) {
				if (!hasAsset(ioAssets, ref->packPath))
					ioAssets.push_back(*ref);
				scanLuaScript(ref->diskPath, ioVisitedScenes, ioAssets, ioWarnings);
			} else
				pushWarning(ioWarnings, "Script", name, iSceneName);
		}
		scanScriptProperties(luaScript["properties"], iSceneName, ioVisitedScenes, ioAssets, ioWarnings);
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
	if (auto tilemap = iEntity["Tilemap"]; tilemap)
		if (auto path = tilemap["tilemapPath"]; path)
			scanTilemap(path.as<std::string>(), iSceneName, ioAssets, ioWarnings);
	if (auto object = iEntity["IsometricObject"]; object)
		if (auto path = object["atlasPath"]; path)
			scanObjectSheet(path.as<std::string>(), iSceneName, ioAssets, ioWarnings);
	for (const char* component: {"RaycastDoor", "RaycastPushWall"})
		if (auto node = iEntity[component]; node)
			if (auto path = node["tilesetPath"]; path)
				scanTileset(path.as<std::string>(), iSceneName, ioAssets, ioWarnings);
	if (auto voxel = iEntity["VoxelWorld"]; voxel)
		if (auto path = voxel["Tileset"]; path)
			scanTileset(path.as<std::string>(), iSceneName, ioAssets, ioWarnings);
}

// NOLINTNEXTLINE(misc-no-recursion) a HUD is scanned like a scene, and its scripts can name scenes
void scanHud(const std::string& iHudPath, const std::string& iSource, std::set<std::string>& ioVisitedScenes,
			 std::vector<AssetReference>& ioAssets, std::vector<std::string>* ioWarnings) {
	if (iHudPath.empty())
		return;
	const auto hudRef = resolveDataAsset(iHudPath);
	if (!hudRef) {
		pushWarning(ioWarnings, "HUD", iHudPath, iSource);
		return;
	}
	if (hasAsset(ioAssets, hudRef->packPath))
		return;
	ioAssets.push_back(*hudRef);
	YAML::Node data;
	try {
		data = YAML::LoadFile(hudRef->diskPath.string());
	} catch (...) {
		pushWarning(ioWarnings, "HUD", iHudPath, iSource);
		return;
	}
	const auto entities = data["Entities"];
	if (!data["Hud"] || !entities)
		return;
	const auto hudName = hudRef->diskPath.filename().string();
	for (auto entity: entities) scanEntity(entity, hudName, ioVisitedScenes, ioAssets, ioWarnings);
}

void collectEngineAssets(std::vector<AssetReference>& ioAssets) {
	if (!app::Application::instanced())
		return;
	for (const auto& [title, assetsPath]: app::Application::get().getAssetDirectories()) {
		// Collect all shader files (slang sources + any pre-compiled .spv).
		if (const auto shadersDir = assetsPath / "shaders"; exists(shadersDir)) {
			for (const auto& item: std::filesystem::recursive_directory_iterator(shadersDir)) {
				if (!item.is_regular_file())
					continue;
				if (const auto rel = relative(item.path(), assetsPath).generic_string(); !hasAsset(ioAssets, rel)) {
					ioAssets.push_back({.packPath = rel, .diskPath = item.path(), .assetType = AssetType::Other});
				}
			}
		}
		if (const auto fontsDir = assetsPath / "fonts"; exists(fontsDir)) {
			for (const auto& item: std::filesystem::recursive_directory_iterator(fontsDir)) {
				if (!item.is_regular_file())
					continue;
				const auto ext = item.path().extension().string();
				if (ext != ".ttf" && ext != ".otf")
					continue;
				const AssetReference font{.packPath = relative(item.path(), assetsPath).generic_string(),
										  .diskPath = item.path(),
										  .assetType = AssetType::Font};
				if (!hasAsset(ioAssets, font.packPath))
					ioAssets.push_back(font);
				addFontSidecar(ioAssets, font);
			}
		}
		for (const auto* dirName: {"logo", "icons"}) {
			const auto dir = assetsPath / dirName;
			if (!exists(dir))
				continue;
			for (const auto& item: std::filesystem::recursive_directory_iterator(dir)) {
				if (!item.is_regular_file())
					continue;
				const auto ext = item.path().extension().string();
				if (ext != ".png" && ext != ".jpg" && ext != ".jpeg")
					continue;
				if (const auto rel = relative(item.path(), assetsPath).generic_string(); !hasAsset(ioAssets, rel)) {
					ioAssets.push_back({.packPath = rel, .diskPath = item.path(), .assetType = AssetType::Texture});
				}
			}
		}
	}
}

}// namespace

auto AssetScanner::scanScene(const std::filesystem::path& iSceneFile, std::vector<std::string>* oWarnings)
		-> std::vector<AssetReference> {
	std::set<std::string> visited;
	std::vector<AssetReference> assets;
	scanSceneRecursive(iSceneFile, visited, assets, oWarnings);
	collectEngineAssets(assets);
	return assets;
}

auto AssetScanner::scanProject(const std::filesystem::path& iProjectDir, const std::string& iFirstScene,
							   std::vector<std::string>* oWarnings, const std::string& iDefaultHud)
		-> std::vector<AssetReference> {
	// Resolve the first scene path.
	std::filesystem::path firstScenePath;
	std::string resolvedName = iFirstScene;
	if (std::filesystem::path(resolvedName).extension() != ".owl")
		resolvedName += ".owl";

	if (app::Application::instanced()) {
		for (const auto& [title, assetsPath]: app::Application::get().getAssetDirectories()) {
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

	auto assets = scanScene(firstScenePath, oWarnings);
	std::set<std::string> visited;
	scanHud(iDefaultHud, "the project's DefaultHud", visited, assets, oWarnings);
	return assets;
}

}// namespace owl::data::assets::pack
