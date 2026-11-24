/**
 * @file GameExporter.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "data/assets/pack/GameExporter.h"

#include "app/Application.h"
#include "app/EngineContext.h"
#include "core/external/yaml.h"
#include "data/assets/AssetRegistry.h"
#include "data/assets/cook/CookCache.h"
#include "data/assets/pack/ExecutableIcon.h"
#include "data/assets/pack/ExtensionManifest.h"
#include "data/assets/pack/ExtensionPack.h"
#include "data/assets/pack/ExtensionRegistry.h"
#include "data/assets/pack/PackWriter.h"
#include "data/fonts/Font.h"
#include "data/fonts/FontLibrary.h"
#include "data/fonts/FontSettings.h"
#include "data/localisation/Localisation.h"
#include "input/ActionMap.h"
#include "renderer/Material.h"
#include "renderer/MaterialGraph.h"
#include "renderer/RenderStack.h"
#include "renderer/RendererSet.h"
#include "renderer/gpu/Shader.h"
#if OWL_WITH_RENDER
#include "renderer/utils/shaderFileUtils.h"
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <span>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace owl::data::assets::pack {

namespace {

// The runner executable of a desktop target.
auto runnerNameOf(const ExportTarget iTarget) -> std::string_view {
	return iTarget == ExportTarget::WindowsX64 ? "OwlGameRunner.exe" : "OwlGameRunner";
}

auto findRunner(const std::filesystem::path& iRunnerDir, const ExportTarget iTarget)
		-> std::optional<std::filesystem::path> {
	if (auto candidate = iRunnerDir / runnerNameOf(iTarget); exists(candidate))
		return candidate;
	return std::nullopt;
}

// The files of the Web runner: its page, the script the page loads and the module the script loads.
constexpr std::array<std::string_view, 3> g_webRunnerFiles = {"OwlGameRunner.html", "OwlGameRunner.js",
															  "OwlGameRunner.wasm"};
// Name of the page in an exported Web game, the one a server gives for the folder.
constexpr std::string_view g_webPage = "index.html";
// List of the game files the page fetches before main (`runner/web/shell.html`).
constexpr std::string_view g_webFileList = "files.json";

auto hasWebRunner(const std::filesystem::path& iRunnerDir) -> bool {
	return std::ranges::all_of(g_webRunnerFiles, [&iRunnerDir](const std::string_view iName) -> bool {
		return exists(iRunnerDir / iName);
	});
}

// Whether the runner of a target is in the runner directory; Android brings its own.
auto hasRunnerFor(const ExportSettings& iSettings) -> bool {
	switch (const auto target = resolveExportTarget(iSettings.target)) {
		case ExportTarget::Android:
			return true;
		case ExportTarget::Web:
			return hasWebRunner(iSettings.runnerDirectory);
		case ExportTarget::Host:
		case ExportTarget::LinuxX64:
		case ExportTarget::LinuxArm64:
		case ExportTarget::WindowsX64:
			return findRunner(iSettings.runnerDirectory, target).has_value();
	}
	return false;
}

auto platformNameOf(const ExportTarget iTarget) -> std::string_view {
	switch (resolveExportTarget(iTarget)) {
		case ExportTarget::Android:
			return "android-arm64";
		case ExportTarget::Web:
			return "web-wasm32";
		case ExportTarget::LinuxArm64:
			return "linux-arm64";
		case ExportTarget::WindowsX64:
			return "windows-x64";
		case ExportTarget::Host:
		case ExportTarget::LinuxX64:
			return "linux-x64";
	}
	return "linux-x64";
}

auto writeText(const std::filesystem::path& iFile, const std::string& iText) -> bool {
	std::ofstream out(iFile, std::ios::binary);
	out << iText;
	return out.good();
}

auto readText(const std::filesystem::path& iFile) -> std::optional<std::string> {
	std::ifstream in(iFile, std::ios::binary);
	if (!in.is_open())
		return std::nullopt;
	std::stringstream buffer;
	buffer << in.rdbuf();
	return buffer.str();
}

// Import settings of a packed asset: its sidecar when the registry knows it, the defaults of its importer otherwise.
auto metaOf(const AssetReference& iRef) -> AssetMeta {
	if (app::Application::instanced()) {
		const auto& registry = app::Application::get().getAssetRegistry();
		if (const auto guid = registry.findGuid(iRef.diskPath.generic_string()); guid)
			if (const auto entry = registry.find(*guid); entry)
				return entry->meta;
	}
	return AssetMeta{.guid = AssetGuid{0},
					 .importer = importerOf(iRef.diskPath).value_or(Importer::Copy),
					 .texture = {},
					 .mesh = {}};
}

auto isRelocatable(const AssetReference& iRef) -> bool {
	if (iRef.assetType == AssetType::Scene)
		return true;
	const auto ext = iRef.diskPath.extension().string();
	return ext == ".owltileset" || ext == ".owltilemap" || ext == ".owlprefab" || ext == ".owlhud";
}

constexpr std::string_view g_defaultIcon{"logo/logo_owl_low.png"};

auto iconOf(const ExportSettings& iSettings) -> std::string {
	return iSettings.icon.empty() ? std::string{g_defaultIcon} : iSettings.icon;
}

auto findInAssetDirectories(const std::filesystem::path& iRelative) -> std::optional<std::filesystem::path> {
	if (!app::Application::instanced())
		return std::nullopt;
	for (const auto& [title, assetsPath]: app::Application::get().getAssetDirectories()) {
		if (auto candidate = assetsPath / iRelative; exists(candidate))
			return candidate;
	}
	return std::nullopt;
}

auto resolveIcon(const ExportSettings& iSettings) -> std::optional<std::filesystem::path> {
	const auto icon = iconOf(iSettings);
	if (auto local = iSettings.projectDirectory / icon; exists(local))
		return local;
	return findInAssetDirectories(icon);
}

// Explorer and the taskbar show the icon of the executable, the window shows the one of `runner.yml`.
void setWindowsIcon(const ExportSettings& iSettings, const std::filesystem::path& iExecutable) {
	const auto icon = resolveIcon(iSettings);
	if (!icon)
		return;
	if (const auto patched = setExecutableIcon(iExecutable, *icon); !patched)
		OWL_CORE_WARN("GameExporter: The executable keeps the Owl icon, '{}' cannot be set: {}.", icon->string(),
					  getErrorMessage(patched.error()))
}

// A scene with an `Environment` draws its sky and lights through `Renderer3D`, whatever its stack.
auto hasEnvironment(const AssetReference& iRef) -> bool {
	if (iRef.assetType != AssetType::Scene)
		return false;
	try {
		const auto entities = YAML::LoadFile(iRef.diskPath.string())["Entities"];
		return entities && std::ranges::any_of(entities, [](const YAML::Node& iEntity) -> bool {
				   return static_cast<bool>(iEntity["Environment"]);
			   });
	} catch (...) {
		OWL_CORE_WARN("GameExporter: Cannot read scene '{}' for its renderers.", iRef.diskPath.string())
		return false;
	}
}

auto rendererSetOf(const ExportSettings& iSettings, const std::vector<AssetReference>& iAssets)
		-> renderer::RendererSet {
	const auto stack = iSettings.rendererStackYaml.empty()
							   ? renderer::RendererStackConfig::makeDefault()
							   : renderer::RendererStackConfig::fromYaml(iSettings.rendererStackYaml);
	auto set = renderer::RendererSet::base();
	for (const auto& entry: stack.entries) set |= renderer::RendererSet::forLayerType(entry.typeKey);
	if (!set.renderer3D && std::ranges::any_of(iAssets, hasEnvironment))
		set.renderer3D = true;
	return set;
}

// Keeps the shaders of the renderer families of the game and adds the fonts of its ImGui overlay.
auto selectEngineFiles(const ExportSettings& iSettings, const renderer::RendererSet& iRenderers,
					   std::vector<AssetReference>& ioAssets) -> size_t {
	const auto folders = iRenderers.getShaderFolders();
	// The browser draws with WGSL only: the SPIR-V of the other targets stays out of a Web game.
	const bool web = iSettings.target == ExportTarget::Web;
	const auto skipped = std::erase_if(ioAssets, [&folders, web](const AssetReference& iRef) -> bool {
		const std::filesystem::path path{iRef.packPath};
		if (path.empty() || *path.begin() != "shaders")
			return false;
		const auto folder = std::next(path.begin());
		if (folder == path.end() || std::ranges::find(folders, folder->string()) == folders.end())
			return true;
		return web && std::next(folder) != path.end() && *std::next(folder) == "spirv";
	});
	if (!iSettings.imGui)
		return skipped;
	for (const auto file: fonts::g_UiFontFiles) {
		const std::string packPath{file};
		if (std::ranges::any_of(ioAssets,
								[&packPath](const AssetReference& iRef) -> bool { return iRef.packPath == packPath; }))
			continue;
		if (const auto disk = findInAssetDirectories(packPath); disk)
			ioAssets.push_back({.packPath = packPath, .diskPath = *disk, .assetType = AssetType::Font});
		else
			OWL_CORE_WARN("GameExporter: ImGui font '{}' not found, the overlay falls back to the ImGui font.",
						  packPath)
	}
	return skipped;
}

#if OWL_WITH_RENDER
constexpr std::array g_shaderStages{renderer::gpu::ShaderType::Vertex, renderer::gpu::ShaderType::Fragment,
									renderer::gpu::ShaderType::Compute};
// The shader targets of a game per platform.
constexpr std::array g_desktopShaderTargets{renderer::utils::ShaderTarget::Vulkan,
											renderer::utils::ShaderTarget::OpenGl};
constexpr std::array g_androidShaderTargets{renderer::utils::ShaderTarget::VulkanAndroid};
constexpr std::array g_webShaderTargets{renderer::utils::ShaderTarget::WebGpu};

auto shaderTargetsOf(const ExportTarget iTarget) -> std::span<const renderer::utils::ShaderTarget> {
	switch (iTarget) {
		case ExportTarget::Android:
			return g_androidShaderTargets;
		case ExportTarget::Web:
			return g_webShaderTargets;
		case ExportTarget::Host:
		case ExportTarget::LinuxX64:
		case ExportTarget::LinuxArm64:
		case ExportTarget::WindowsX64:
			return g_desktopShaderTargets;
	}
	return g_desktopShaderTargets;
}

// Where a baked stage goes: the WGSL of the WebGPU target, the SPIR-V of the others.
auto bakedShaderPath(const std::string& iName, const std::string& iRenderer,
					 const renderer::utils::ShaderTarget iTarget, const renderer::gpu::ShaderType iStage)
		-> std::filesystem::path {
	if (iTarget == renderer::utils::ShaderTarget::WebGpu)
		return renderer::utils::getPrecompiledWgslPath(iName, iRenderer, iStage);
	return renderer::utils::getPrecompiledShaderPath(
			iName, iRenderer, std::string{renderer::utils::getShaderTargetFolder(iTarget)}, iStage);
}

// True when the stored baked stages of a shader match its source, for one target.
auto hasCurrentBake(const std::vector<AssetReference>& iAssets, const std::string& iName, const std::string& iRenderer,
					const renderer::utils::ShaderTarget iTarget, const std::string& iCacheKey) -> bool {
	size_t found = 0;
	for (const auto stage: g_shaderStages) {
		const auto packPath = bakedShaderPath(iName, iRenderer, iTarget, stage).generic_string();
		const auto ref = std::ranges::find(iAssets, packPath, &AssetReference::packPath);
		if (ref == iAssets.end())
			continue;
		if (!renderer::utils::isShaderCacheValid(ref->diskPath, iCacheKey))
			return false;
		++found;
	}
	return found > 0;
}

// The stages of a shader baked by an earlier export, when all of them match the source.
auto findBakedStages(const std::filesystem::path& iCacheDir, const std::string& iName, const std::string& iRenderer,
					 const renderer::utils::ShaderTarget iTarget, const std::string& iCacheKey)
		-> std::vector<std::filesystem::path> {
	std::vector<std::filesystem::path> entries;
	for (const auto stage: g_shaderStages) {
		const auto entry = bakedShaderPath(iName, iRenderer, iTarget, stage);
		if (std::error_code ec; !std::filesystem::is_regular_file(iCacheDir / entry, ec))
			continue;
		if (!renderer::utils::isShaderCacheValid(iCacheDir / entry, iCacheKey))
			return {};
		entries.push_back(entry);
	}
	return entries;
}

// Points the pack entry at a baked file, adding the entry when the scan had none.
void useBakedFile(std::vector<AssetReference>& ioAssets, const std::string& iPackPath,
				  const std::filesystem::path& iDiskPath) {
	if (const auto ref = std::ranges::find(ioAssets, iPackPath, &AssetReference::packPath); ref != ioAssets.end())
		ref->diskPath = iDiskPath;
	else
		ioAssets.push_back({.packPath = iPackPath, .diskPath = iDiskPath, .assetType = AssetType::Other});
}

// Compiles a shader source for every target when its baked stages are missing or outdated, into the cook cache.
auto bakeShaderSource(std::vector<AssetReference>& ioAssets, const std::filesystem::path& iCacheDir,
					  const std::string& iSource, const std::string& iName, const std::string& iRenderer,
					  const std::span<const renderer::utils::ShaderTarget> iTargets) -> expected<bool, ExportError> {
	const auto moduleName = std::format("{}/{}", iRenderer, iName);
	bool compiled = false;
	for (const auto target: iTargets) {
		const std::string api = renderer::utils::getShaderTargetFolder(target);
		const auto cacheKey = renderer::utils::getShaderCacheKey(iSource, moduleName, target);
		if (hasCurrentBake(ioAssets, iName, iRenderer, target, cacheKey))
			continue;
		compiled = true;
		if (const auto stored = findBakedStages(iCacheDir, iName, iRenderer, target, cacheKey); !stored.empty()) {
			for (const auto& entry: stored) {
				useBakedFile(ioAssets, entry.generic_string(), iCacheDir / entry);
				useBakedFile(ioAssets, entry.generic_string() + ".hash", (iCacheDir / entry).string() + ".hash");
			}
			continue;
		}
		const auto result = renderer::utils::compileShader(iSource, iName, target);
		if (!result.success) {
			OWL_CORE_ERROR("GameExporter: Cannot compile shader '{}' for {}.", moduleName, api)
			return unexpected{ExportError::CookFailed};
		}
		for (const auto& [stage, spirv]: result.spirvData) {
			const auto entry = renderer::utils::getPrecompiledShaderPath(iName, iRenderer, api, stage);
			const auto file = iCacheDir / entry;
			std::error_code ec;
			std::filesystem::create_directories(file.parent_path(), ec);
			if (!renderer::utils::writeCachedShader(file, spirv)) {
				OWL_CORE_ERROR("GameExporter: Cannot write the SPIR-V '{}'.", file.string())
				return unexpected{ExportError::CookFailed};
			}
			renderer::utils::writeShaderHash(file, cacheKey);
			useBakedFile(ioAssets, entry.generic_string(), file);
			useBakedFile(ioAssets, entry.generic_string() + ".hash", file.string() + ".hash");
		}
		for (const auto& [stage, wgsl]: result.wgslData) {
			const auto entry = renderer::utils::getPrecompiledWgslPath(iName, iRenderer, stage);
			const auto file = iCacheDir / entry;
			std::error_code ec;
			std::filesystem::create_directories(file.parent_path(), ec);
			if (!writeText(file, wgsl)) {
				OWL_CORE_ERROR("GameExporter: Cannot write the WGSL '{}'.", file.string())
				return unexpected{ExportError::CookFailed};
			}
			renderer::utils::writeShaderHash(file, cacheKey);
			useBakedFile(ioAssets, entry.generic_string(), file);
			useBakedFile(ioAssets, entry.generic_string() + ".hash", file.string() + ".hash");
		}
	}
	return compiled;
}

// The variants of the mesh shaders the material graphs of the pack draw with, one per root graph and mesh shader.
auto bakeMaterialGraphs(std::vector<AssetReference>& ioAssets, const std::filesystem::path& iCacheDir,
						const std::span<const renderer::utils::ShaderTarget> iTargets)
		-> expected<size_t, ExportError> {
	std::vector<std::filesystem::path> materials;
	for (const auto& ref: ioAssets) {
		if (std::filesystem::path{ref.packPath}.extension() == ".owlmat")
			materials.push_back(ref.diskPath);
	}
	std::set<std::string> names;
	size_t baked = 0;
	for (const auto& file: materials) {
		const auto asset = renderer::MaterialAsset::load(file);
		if (!asset.has_value() || !asset->graph.has_value())
			continue;
		const auto program = renderer::compileMaterialGraph(*asset->graph);
		if (!program.isValid()) {
			OWL_CORE_WARN("GameExporter: The graph of '{}' does not compile, the game draws its parameters.",
						  file.string())
			continue;
		}
		for (const auto shader: renderer::g_materialGraphShaders) {
			const auto base = std::format("shaders/{}/{}.slang", renderer::g_materialGraphRenderer, shader);
			const auto name = renderer::getMaterialShaderName(shader, program);
			const bool packed = std::ranges::any_of(
					ioAssets, [&base](const AssetReference& iRef) -> bool { return iRef.packPath == base; });
			if (!packed || !names.insert(name).second)
				continue;
			const auto source = renderer::insertMaterialProgram(renderer::utils::readShaderSource(base), program);
			if (!source.has_value())
				continue;
			const auto compiled = bakeShaderSource(ioAssets, iCacheDir, *source, name,
												   std::string{renderer::g_materialGraphRenderer}, iTargets);
			if (!compiled.has_value())
				return unexpected{compiled.error()};
			if (*compiled)
				++baked;
		}
	}
	return baked;
}

// Compiles the packed shaders and the material graph variants whose baked stages are outdated; the game compiles none.
auto bakeShaders(std::vector<AssetReference>& ioAssets, const std::filesystem::path& iCacheDir,
				 const ExportTarget iTarget) -> expected<size_t, ExportError> {
	const auto targets = shaderTargetsOf(iTarget);
	std::vector<AssetReference> sources;
	std::ranges::copy_if(ioAssets, std::back_inserter(sources), [](const AssetReference& iRef) -> bool {
		const std::filesystem::path path{iRef.packPath};
		return path.extension() == ".slang" && *path.begin() == "shaders";
	});
	size_t baked = 0;
	for (const auto& ref: sources) {
		const std::filesystem::path packPath{ref.packPath};
		// Expanded as the game expands it from its pack, so that the keys match.
		const auto compiled =
				bakeShaderSource(ioAssets, iCacheDir, renderer::utils::readShaderSource(packPath),
								 packPath.stem().string(), packPath.parent_path().filename().string(), targets);
		if (!compiled.has_value())
			return unexpected{compiled.error()};
		if (*compiled)
			++baked;
	}
	const auto graphs = bakeMaterialGraphs(ioAssets, iCacheDir, targets);
	if (!graphs.has_value())
		return unexpected{graphs.error()};
	return baked + *graphs;
}

auto shaderTargetNamesOf(const ExportTarget iTarget) -> std::vector<std::string> {
	std::vector<std::string> names;
	for (const auto target: shaderTargetsOf(iTarget)) names.push_back(renderer::utils::getShaderTargetFolder(target));
	return names;
}
#else
// Without the render module no shader compiler is built: the shaders are packed as they are.
auto bakeShaders([[maybe_unused]] std::vector<AssetReference>& ioAssets,
				 [[maybe_unused]] const std::filesystem::path& iCacheDir, [[maybe_unused]] const ExportTarget iTarget)
		-> expected<size_t, ExportError> {
	return size_t{0};
}

auto shaderTargetNamesOf([[maybe_unused]] const ExportTarget iTarget) -> std::vector<std::string> { return {}; }
#endif

// Bakes the MSDF atlas of the scene and HUD fonts into the pack, so that the game does not build it at start.
auto bakeFontAtlases(const std::vector<AssetReference>& iTextFonts, PackWriter& ioWriter) -> size_t {
	size_t baked = 0;
	for (const auto& ref: iTextFonts) {
		const fonts::Font font{ref.diskPath, false, fonts::FontSettings::loadForFont(ref.diskPath)};
		const auto& atlas = font.getAtlasCacheFile();
		if (std::error_code ec; atlas.empty() || !std::filesystem::is_regular_file(atlas, ec)) {
			OWL_CORE_WARN("GameExporter: No atlas for font '{}', the game generates it at start.", ref.packPath)
			continue;
		}
		ioWriter.addFile(atlas, fonts::Font::getPackedAtlasPath(atlas).generic_string(), AssetType::Other);
		++baked;
	}
	return baked;
}

auto resolveFirstScene(const std::string& iFirstScene, const std::vector<AssetReference>& iAssets) -> std::string {
	std::string resolved = iFirstScene;
	if (std::filesystem::path(resolved).extension() != ".owl")
		resolved += ".owl";
	for (const auto& ref: iAssets) {
		if (ref.assetType == AssetType::Scene && ref.packPath.ends_with(resolved))
			return ref.packPath;
	}
	return resolved;
}

void addExecPermission(const std::filesystem::path& iFile) {
	std::error_code ec;
	std::filesystem::permissions(iFile,
								 std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec |
										 std::filesystem::perms::others_exec,
								 std::filesystem::perm_options::add, ec);
	if (ec)
		OWL_CORE_WARN("GameExporter: Cannot set the exec permissions of '{}': {}.", iFile.string(), ec.message())
}

void copySharedLibs(const std::filesystem::path& iSrcDir, const std::filesystem::path& iDestDir) {
	std::error_code ec;
	for (const auto& entry: std::filesystem::directory_iterator(iSrcDir, ec)) {
		if (!entry.is_regular_file() && !entry.is_symlink())
			continue;
		const auto ext = entry.path().extension().string();
		const auto filename = entry.path().filename().string();
		if (ext != ".so" && filename.find(".so.") == std::string::npos && ext != ".dll")
			continue;
		// The shader compiler of Owl Nest: the game reads baked shaders only.
		if (filename.starts_with("slang") || filename.starts_with("libslang"))
			continue;
		const auto dest = iDestDir / entry.path().filename();
		std::error_code copyEc;
		std::filesystem::remove(dest, copyEc);
		std::filesystem::copy(entry.path(), dest, std::filesystem::copy_options::copy_symlinks, copyEc);
		if (copyEc)
			OWL_CORE_WARN("GameExporter: Cannot copy '{}': {}.", entry.path().string(), copyEc.message())
	}
}

// Copies the page of the Web runner as the index of the game, with its script and module.
auto copyWebRunner(const std::filesystem::path& iRunnerDir, const std::filesystem::path& iGameDir) -> bool {
	for (const auto name: g_webRunnerFiles) {
		const auto dest = iGameDir / (name.ends_with(".html") ? g_webPage : name);
		if (std::error_code ec; !std::filesystem::copy_file(iRunnerDir / name, dest,
															std::filesystem::copy_options::overwrite_existing, ec) ||
								ec) {
			OWL_CORE_ERROR("GameExporter: Cannot copy '{}' to '{}': {}.", name, dest.string(), ec.message())
			return false;
		}
	}
	return true;
}

// Lists the game files the page fetches before main: everything in the folder but the page and the runner.
void writeWebFileList(const std::filesystem::path& iGameDir) {
	std::vector<std::string> files;
	std::error_code ec;
	for (const auto& entry: std::filesystem::recursive_directory_iterator(iGameDir, ec)) {
		if (!entry.is_regular_file())
			continue;
		const auto relative = entry.path().lexically_relative(iGameDir).generic_string();
		if (relative == g_webPage || relative == g_webFileList ||
			std::ranges::find(g_webRunnerFiles, relative) != g_webRunnerFiles.end())
			continue;
		files.push_back(relative);
	}
	std::ranges::sort(files);
	std::string json{"["};
	for (const auto& file: files) {
		if (json.size() > 1)
			json += ", ";
		// Exported names are sanitized: no quote nor backslash to escape.
		json += std::format("\"{}\"", file);
	}
	json += "]\n";
	if (!writeText(iGameDir / g_webFileList, json))
		OWL_CORE_WARN("GameExporter: Cannot write '{}'.", (iGameDir / g_webFileList).string())
}

void writeRunnerConfig(const ExportSettings& iSettings, const std::filesystem::path& iGameDir,
					   const std::string& iFirstScene, const std::string& iPackFile, const std::string& iIcon,
					   const std::string& iGamepadMappings, const std::vector<std::string>& iRenderers) {
	YAML::Emitter out;
	out << YAML::BeginMap;
	out << YAML::Key << "RunnerConfig" << YAML::Value << YAML::BeginMap;
	out << YAML::Key << "FirstScene" << YAML::Value << iFirstScene;
	out << YAML::Key << "PackFile" << YAML::Value << iPackFile;
	out << YAML::Key << "GameName" << YAML::Value << iSettings.gameName;
	if (!iSettings.version.empty())
		out << YAML::Key << "Version" << YAML::Value << iSettings.version;
	if (!iSettings.author.empty())
		out << YAML::Key << "Author" << YAML::Value << iSettings.author;
	if (!iIcon.empty())
		out << YAML::Key << "Icon" << YAML::Value << iIcon;
	if (!iGamepadMappings.empty())
		out << YAML::Key << "GamepadMappings" << YAML::Value << iGamepadMappings;
	out << YAML::Key << "WindowWidth" << YAML::Value << iSettings.windowSize.x();
	out << YAML::Key << "WindowHeight" << YAML::Value << iSettings.windowSize.y();
	out << YAML::Key << "Fullscreen" << YAML::Value << iSettings.fullscreen;
	out << YAML::Key << "Resizable" << YAML::Value << iSettings.resizable;
	if (!iSettings.defaultHud.empty())
		out << YAML::Key << "DefaultHud" << YAML::Value << iSettings.defaultHud;
	if (!iSettings.defaultHudTheme.empty())
		out << YAML::Key << "DefaultHudTheme" << YAML::Value << iSettings.defaultHudTheme;
	if (!iSettings.modScreen.empty())
		out << YAML::Key << "ModScreen" << YAML::Value << iSettings.modScreen;
	out << YAML::Key << "TickRate" << YAML::Value << iSettings.tickRate;
	out << YAML::Key << "MaxTicksPerFrame" << YAML::Value << iSettings.maxTicksPerFrame;
	if (!iSettings.rendererStackYaml.empty())
		out << YAML::Key << "RendererStack" << YAML::Value << YAML::Load(iSettings.rendererStackYaml);
	if (!iSettings.networkRulesYaml.empty())
		out << YAML::Key << "Network" << YAML::Value << YAML::Load(iSettings.networkRulesYaml);
	out << YAML::Key << "Renderers" << YAML::Value << YAML::Flow << iRenderers;
	out << YAML::Key << "ImGui" << YAML::Value << iSettings.imGui;
	out << YAML::EndMap;
	out << YAML::EndMap;
	std::ofstream file(iGameDir / "runner.yml");
	file << out.c_str() << "\n";
}

void writeMetadata(const ExportSettings& iSettings, const std::filesystem::path& iGameDir) {
	const auto days = std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now());
	const std::chrono::year_month_day ymd{days};
	YAML::Emitter out;
	out << YAML::BeginMap;
	out << YAML::Key << "GameInfo" << YAML::Value << YAML::BeginMap;
	out << YAML::Key << "Name" << YAML::Value << iSettings.gameName;
	if (!iSettings.version.empty())
		out << YAML::Key << "Version" << YAML::Value << iSettings.version;
	if (!iSettings.author.empty())
		out << YAML::Key << "Author" << YAML::Value << iSettings.author;
	if (!iSettings.description.empty())
		out << YAML::Key << "Description" << YAML::Value << iSettings.description;
	out << YAML::Key << "EngineVersion" << YAML::Value << owl::getVersionString();
	out << YAML::Key << "PackDate" << YAML::Value << std::format("{:%Y-%m-%d}", ymd);
	out << YAML::Key << "Platform" << YAML::Value << std::string{platformNameOf(iSettings.target)};
	out << YAML::EndMap;
	out << YAML::EndMap;
	std::ofstream file(iGameDir / "game_info.yml");
	file << out.c_str() << "\n";
}

void writeLinuxLauncher(const std::filesystem::path& iGameDir, const std::string& iExeName) {
	{
		std::ofstream script(iGameDir / "launch.sh");
		script << "#!/bin/sh\n";
		script << "# Launcher for " << iExeName << "\n";
		script << "SCRIPT_DIR=\"$(cd \"$(dirname \"$0\")\" && pwd)\"\n";
		script << "export LD_LIBRARY_PATH=\"${SCRIPT_DIR}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}\"\n";
		script << "exec \"${SCRIPT_DIR}/" << iExeName << "\" \"$@\"\n";
	}
	addExecPermission(iGameDir / "launch.sh");
}

#ifdef OWL_PLATFORM_WINDOWS
auto createZipArchive(const std::filesystem::path& iSourceDir, const std::filesystem::path& iOutputZip) -> bool {
	const auto cmd = std::format(
			"powershell -NoProfile -Command \"Compress-Archive -Path '{}\\*' -DestinationPath '{}' -Force\"",
			iSourceDir.string(), iOutputZip.string());
	// NOLINTNEXTLINE(bugprone-command-processor) No zip API: use PowerShell.
	return std::system(cmd.c_str()) == 0;
}
#endif

void report(const GameExporter::ProgressCallback& iProgress, const float iFraction, const std::string& iMessage) {
	if (iProgress)
		iProgress(iFraction, iMessage);
}

auto isCancelled(const GameExporter::CancelCheck& iCancel) -> bool { return iCancel && iCancel(); }

// The extension packs as `mods/<id>.owlmod`; the `.owlmod` files of an earlier export are replaced.
auto writeMods(const ExportSettings& iSettings, const std::filesystem::path& iGameDir)
		-> expected<std::vector<std::filesystem::path>, ExportError> {
	std::vector<std::filesystem::path> mods;
	const auto folder = iGameDir / "mods";
	std::error_code error;
	for (const auto& old: ExtensionRegistry::listSources(folder)) {
		if (old.extension() != g_extensionPackExtension)
			continue;
		if (std::filesystem::remove(old, error); error) {
			OWL_CORE_ERROR("GameExporter: Earlier extension pack '{}' not removed: {}.", old.string(), error.message())
			return unexpected{ExportError::ExtensionWrite};
		}
	}
	for (const auto& source: iSettings.extensionSources) {
		const auto manifest = ExtensionPack::readManifest(source);
		if (!manifest) {
			OWL_CORE_ERROR("GameExporter: Extension pack '{}' cannot be read: {}.", source.string(),
						   describe(manifest.error()))
			return unexpected{ExportError::ExtensionWrite};
		}
		const auto file = folder / (manifest->id + std::string{g_extensionPackExtension});
		if (std::filesystem::is_directory(source, error)) {
			if (const auto written = writeExtensionPack(source, file, iSettings.packFlags); !written) {
				OWL_CORE_ERROR("GameExporter: Extension pack '{}' not written: {}.", manifest->id,
							   describe(written.error()))
				return unexpected{ExportError::ExtensionWrite};
			}
		} else if (std::filesystem::create_directories(folder, error); !std::filesystem::copy_file(
						   source, file, std::filesystem::copy_options::overwrite_existing, error)) {
			OWL_CORE_ERROR("GameExporter: Extension pack '{}' not copied: {}.", manifest->id, error.message())
			return unexpected{ExportError::ExtensionWrite};
		}
		mods.push_back(file);
	}
	return mods;
}

// Completes the report with what the written folder holds and the unused project assets, then writes it beside the game.
void finishReport(ExportReport& ioReport, const ExportSettings& iSettings, const std::vector<AssetReference>& iAssets) {
	std::error_code sizeEc;
	ioReport.packBytes = std::filesystem::file_size(ioReport.packFile, sizeEc);
	ioReport.platform = platformNameOf(iSettings.target);
	ioReport.shaderTargets = shaderTargetNamesOf(iSettings.target);
	std::vector<std::filesystem::path> excluded{iSettings.outputDirectory, iSettings.cookCacheDirectory};
	if (const auto icon = resolveIcon(iSettings); icon)
		excluded.push_back(*icon);
	excluded.insert(excluded.end(), iSettings.extensionSources.begin(), iSettings.extensionSources.end());
	ioReport.unusedAssets = findUnusedAssets(iSettings.projectDirectory, iAssets, excluded);
	ioReport.sizes = measureExportSizes(ioReport.gameDirectory, ioReport.executable);
	std::ignore = ioReport.write();
}

}// namespace

auto getHostExportTarget() -> ExportTarget {
#if defined(OWL_PLATFORM_WINDOWS)
	return ExportTarget::WindowsX64;
#elif defined(OWL_PLATFORM_ARM64)
	return ExportTarget::LinuxArm64;
#else
	return ExportTarget::LinuxX64;
#endif
}

auto resolveExportTarget(const ExportTarget iTarget) -> ExportTarget {
	return iTarget == ExportTarget::Host ? getHostExportTarget() : iTarget;
}

auto GameExporter::getErrorMessage(const ExportError iError) -> std::string_view {
	switch (iError) {
		case ExportError::NoAssets:
			return "no asset to pack, check the first scene and its references";
		case ExportError::OutputDirectory:
			return "cannot create the output directory";
		case ExportError::PackWrite:
			return "cannot write the game pack";
		case ExportError::RunnerNotFound:
			return "OwlGameRunner executable not found";
		case ExportError::RunnerCopy:
			return "cannot copy the runner executable";
		case ExportError::Cancelled:
			return "cancelled";
		case ExportError::CookFailed:
			return "an asset could not be cooked, see the log";
		case ExportError::ExtensionWrite:
			return "an extension pack could not be written as a mod, see the log";
	}
	return "unknown error";
}

auto GameExporter::sanitizeFilename(const std::string& iName) -> std::string {
	std::string result;
	result.reserve(iName.size());
	for (const auto ch: iName) {
		if (ch == '/' || ch == '\\' || ch == ':' || ch == '*' || ch == '?' || ch == '"' || ch == '<' || ch == '>' ||
			ch == '|' || ch == ' ')
			result += '_';
		else
			result += ch;
	}
	return result;
}

auto GameExporter::relocateAbsolutePaths(const std::string& iText, const std::vector<AssetReference>& iAssets)
		-> std::string {
	static constexpr std::string_view g_key = "pat:";
	std::string result;
	result.reserve(iText.size());
	size_t cursor = 0;
	while (cursor < iText.size()) {
		const auto found = iText.find(g_key, cursor);
		if (found == std::string::npos) {
			result.append(iText, cursor);
			break;
		}
		const auto valueStart = found + g_key.size();
		auto valueEnd = iText.find_first_of("\r\n\"'", valueStart);
		if (valueEnd == std::string::npos)
			valueEnd = iText.size();
		const std::filesystem::path diskPath{iText.substr(valueStart, valueEnd - valueStart)};
		result.append(iText, cursor, found - cursor);
		const auto ref = std::ranges::find_if(iAssets, [&diskPath](const AssetReference& iRef) -> bool {
			std::error_code ec;
			return std::filesystem::equivalent(iRef.diskPath, diskPath, ec);
		});
		if (diskPath.is_absolute() && ref != iAssets.end())
			result += "nam:" + ref->packPath;
		else
			result.append(iText, found, valueEnd - found);
		cursor = valueEnd;
	}
	return result;
}

namespace {

// The mod screen a game packs: the one of the project, else the one of the engine.
auto modScreenOf(const ExportSettings& iSettings) -> std::string {
	return iSettings.modScreen.empty() ? app::EngineContext::defaultModScreen() : iSettings.modScreen;
}

}// namespace

auto GameExporter::validate(const ExportSettings& iSettings, std::vector<AssetReference>& oAssets)
		-> std::vector<std::string> {
	std::vector<std::string> warnings;
	oAssets = AssetScanner::scanProject(iSettings.projectDirectory, iSettings.firstScene, &warnings,
										iSettings.defaultHud, iSettings.defaultHudTheme, modScreenOf(iSettings));
	if (oAssets.empty())
		warnings.emplace_back("No assets to pack — check firstScene and its references");
	if (!hasRunnerFor(iSettings))
		warnings.emplace_back(std::format("OwlGameRunner {} not found in '{}' — packed game will not be playable",
										  iSettings.target == ExportTarget::Web ? "page" : "executable",
										  iSettings.runnerDirectory.string()));
	if (!iSettings.icon.empty() && !resolveIcon(iSettings))
		warnings.emplace_back(std::format("Icon '{}' not found", iSettings.icon));
	if (!iSettings.gamepadMappings.empty() && !exists(iSettings.projectDirectory / iSettings.gamepadMappings))
		warnings.emplace_back(std::format("Gamepad mapping file '{}' not found", iSettings.gamepadMappings));
	return warnings;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity) Linear sequence of export steps with early returns.
auto GameExporter::exportGame(const ExportSettings& iSettings, const std::vector<AssetReference>& iAssets,
							  const ProgressCallback& iProgress, const CancelCheck& iCancel)
		-> expected<ExportReport, ExportError> {
	const auto startTime = std::chrono::steady_clock::now();
	std::vector<AssetReference> assets = iAssets;
	std::vector<std::string> warnings;
	if (assets.empty()) {
		report(iProgress, 0.f, "Scanning assets...");
		assets = AssetScanner::scanProject(iSettings.projectDirectory, iSettings.firstScene, &warnings,
										   iSettings.defaultHud, iSettings.defaultHudTheme, modScreenOf(iSettings));
	}
	if (assets.empty()) {
		OWL_CORE_ERROR("GameExporter: No asset found to pack for first scene '{}'.", iSettings.firstScene)
		return unexpected{ExportError::NoAssets};
	}
	if (!hasRunnerFor(iSettings)) {
		OWL_CORE_ERROR("GameExporter: OwlGameRunner not found in '{}'.", iSettings.runnerDirectory.string())
		return unexpected{ExportError::RunnerNotFound};
	}
	const auto renderers = rendererSetOf(iSettings, assets);
	std::vector<AssetReference> textFonts;
	std::ranges::copy_if(assets, std::back_inserter(textFonts), [](const AssetReference& iRef) -> bool {
		return iRef.assetType == AssetType::Font && std::filesystem::path{iRef.packPath}.extension() == ".ttf";
	});
	const auto engineFilesSkipped = selectEngineFiles(iSettings, renderers, assets);
	const auto cookDirectory = iSettings.cookCacheDirectory.empty()
									   ? iSettings.projectDirectory / ".owl" / "cache" / "cooked"
									   : iSettings.cookCacheDirectory;
	const auto shadersBaked = bakeShaders(assets, cookDirectory / "shaders", iSettings.target);
	if (!shadersBaked)
		return unexpected{shadersBaked.error()};
	std::string rendererList{"Renderer2D"};
	for (const auto& name: renderers.getNames()) rendererList += " + " + name;
	OWL_CORE_DEBUG("GameExporter: Renderers {}, {} unused engine shader files left out, {} shader(s) baked.",
				   rendererList, engineFilesSkipped, *shadersBaked)
	const auto baseName = sanitizeFilename(iSettings.gameName);
	ExportReport result;
	result.warnings = std::move(warnings);
	result.renderers = renderers.getNames();
	result.engineFilesSkipped = engineFilesSkipped;
	result.shadersBaked = *shadersBaked;
	result.gameDirectory = iSettings.outputDirectory / baseName;
	result.assetCount = assets.size();
	if (std::error_code ec; !std::filesystem::create_directories(result.gameDirectory, ec) && ec) {
		OWL_CORE_ERROR("GameExporter: Cannot create output directory '{}': {}.", result.gameDirectory.string(),
					   ec.message())
		return unexpected{ExportError::OutputDirectory};
	}

	report(iProgress, 0.05f, std::format("Cooking {} assets...", assets.size()));
	PackWriter writer;
	cook::CookCache cache(cookDirectory);
	std::vector<std::string> packPaths;
	packPaths.reserve(assets.size());
	for (size_t index = 0; index < assets.size(); ++index) {
		const auto& ref = assets[index];
		if (isCancelled(iCancel)) {
			OWL_CORE_INFO("GameExporter: Export cancelled.")
			return unexpected{ExportError::Cancelled};
		}
		report(iProgress, 0.05f + 0.15f * static_cast<float>(index) / static_cast<float>(assets.size()),
			   std::format("Cooking {}...", ref.packPath));
		packPaths.push_back(ref.packPath);
		if (isRelocatable(ref)) {
			if (const auto text = readText(ref.diskPath); text) {
				// Only the YAML header is text: what follows a document-end line (tilemap chunks) is copied as is.
				const auto headerEnd = std::min(text->size(), text->find("\n...\n"));
				auto relocated = relocateAbsolutePaths(text->substr(0, headerEnd), assets);
				relocated.append(*text, headerEnd);
				// Scenes, HUDs and prefabs are cooked after the relocation, which depends on the other assets.
				auto cooked = cook::cookAsset(metaOf(ref), std::vector<uint8_t>(relocated.begin(), relocated.end()),
											  ref.packPath);
				if (!cooked) {
					OWL_CORE_ERROR("GameExporter: Cannot cook '{}': {}.", ref.packPath, cook::describe(cooked.error()))
					return unexpected{ExportError::CookFailed};
				}
				writer.addData(*cooked, ref.packPath, ref.assetType);
				continue;
			}
		}
		auto cooked = cache.cook(ref.diskPath, metaOf(ref), ref.packPath);
		if (!cooked) {
			OWL_CORE_ERROR("GameExporter: Cannot cook '{}': {}.", ref.packPath, cook::describe(cooked.error()))
			return unexpected{ExportError::CookFailed};
		}
		writer.addData(*cooked, ref.packPath, ref.assetType);
	}
	std::ignore = cache.saveManifest();
	const auto bakedFonts = bakeFontAtlases(textFonts, writer);
	OWL_CORE_DEBUG("GameExporter: {} font atlas(es) baked.", bakedFonts)
	result.cookedAssets = cache.getStats().cooked;
	result.cacheHits = cache.getStats().hits;
	if (app::Application::instanced()) {
		const auto index = app::Application::get().getAssetRegistry().serializeIndex(packPaths);
		writer.addData(std::vector<uint8_t>(index.begin(), index.end()), std::string{g_assetIndexEntry},
					   AssetType::Other);
	}
	if (const auto settingsPath = iSettings.projectDirectory / "game_settings.yml"; exists(settingsPath))
		writer.addFile(settingsPath, "game_settings.yml", AssetType::Other);
	if (const auto actionsPath = iSettings.projectDirectory / input::ActionMap::fileName; exists(actionsPath))
		writer.addFile(actionsPath, std::string{input::ActionMap::fileName}, AssetType::Other);
	if (const auto localisationPath = iSettings.projectDirectory / localisation::Localisation::fileName;
		exists(localisationPath)) {
		if (const auto localised = localisation::Localisation::loadFile(localisationPath); localised.has_value()) {
			const auto exported = localised->withExportedLanguages().serializeToString();
			writer.addData(std::vector<uint8_t>(exported.begin(), exported.end()),
						   std::string{localisation::Localisation::fileName}, AssetType::Other);
		} else {
			OWL_CORE_WARN("GameExporter: '{}' cannot be read, not packed.", localisationPath.string())
			result.warnings.push_back(std::format("Localisation file '{}' cannot be read, not packed",
												  localisationPath.filename().string()));
		}
	}
	std::string mappingsEntry;
	if (!iSettings.gamepadMappings.empty()) {
		if (const auto mappingsPath = iSettings.projectDirectory / iSettings.gamepadMappings; exists(mappingsPath)) {
			mappingsEntry = std::filesystem::path{iSettings.gamepadMappings}.generic_string();
			writer.addFile(mappingsPath, mappingsEntry, AssetType::Other);
		} else {
			OWL_CORE_WARN("GameExporter: Gamepad mapping file '{}' not found, not packed.", iSettings.gamepadMappings)
			result.warnings.push_back(std::format("Gamepad mapping file '{}' not found", iSettings.gamepadMappings));
		}
	}
	const auto packFilename = baseName + ".owlpack";
	result.packFile = result.gameDirectory / packFilename;
	const bool written = writer.write(
			result.packFile, iSettings.packFlags,
			[&iProgress](const uint32_t iCurrent, const uint32_t iTotal) -> void {
				report(iProgress, 0.2f + 0.6f * static_cast<float>(iCurrent) / static_cast<float>(iTotal),
					   "Writing pack...");
			},
			iCancel);
	if (!written) {
		if (isCancelled(iCancel)) {
			OWL_CORE_INFO("GameExporter: Export cancelled.")
			return unexpected{ExportError::Cancelled};
		}
		OWL_CORE_ERROR("GameExporter: Cannot write pack '{}'.", result.packFile.string())
		return unexpected{ExportError::PackWrite};
	}

	auto mods = writeMods(iSettings, result.gameDirectory);
	if (!mods)
		return unexpected{mods.error()};
	result.mods = std::move(*mods);

	report(iProgress, 0.82f, "Writing configuration...");
	std::string iconEntry;
	if (const auto iconSrc = resolveIcon(iSettings); iconSrc) {
		const auto iconDst = result.gameDirectory / iconOf(iSettings);
		std::error_code ec;
		std::filesystem::create_directories(iconDst.parent_path(), ec);
		std::filesystem::copy_file(*iconSrc, iconDst, std::filesystem::copy_options::overwrite_existing, ec);
		if (ec) {
			OWL_CORE_WARN("GameExporter: Cannot copy icon '{}': {}.", iconSrc->string(), ec.message())
			result.warnings.push_back(std::format("Icon '{}' cannot be copied", iconSrc->string()));
		} else {
			iconEntry = iconOf(iSettings);
		}
	} else {
		OWL_CORE_WARN("GameExporter: Icon '{}' not found, the game has no window icon.", iconOf(iSettings))
		result.warnings.push_back(std::format("Icon '{}' not found", iconOf(iSettings)));
	}
	writeRunnerConfig(iSettings, result.gameDirectory, resolveFirstScene(iSettings.firstScene, assets), packFilename,
					  iconEntry, mappingsEntry, result.renderers);
	writeMetadata(iSettings, result.gameDirectory);
	if (isCancelled(iCancel)) {
		OWL_CORE_INFO("GameExporter: Export cancelled.")
		return unexpected{ExportError::Cancelled};
	}

	if (iSettings.target == ExportTarget::Web) {
		report(iProgress, 0.88f, "Copying the page...");
		if (!copyWebRunner(iSettings.runnerDirectory, result.gameDirectory))
			return unexpected{ExportError::RunnerCopy};
		result.executable = result.gameDirectory / g_webPage;
		writeWebFileList(result.gameDirectory);
		finishReport(result, iSettings, assets);
		report(iProgress, 1.f, "Done.");
		OWL_CORE_INFO("GameExporter: Exported '{}' for the Web ({} assets) to '{}'.", iSettings.gameName,
					  result.assetCount, result.gameDirectory.string())
		return result;
	}
	if (iSettings.target == ExportTarget::Android) {
		// The APK packaging adds the runner libraries next to this folder, its assets.
		finishReport(result, iSettings, assets);
		report(iProgress, 1.f, "Done.");
		OWL_CORE_INFO("GameExporter: Exported '{}' for Android ({} assets) to '{}'.", iSettings.gameName,
					  result.assetCount, result.gameDirectory.string())
		return result;
	}
	report(iProgress, 0.88f, "Copying runner...");
	const auto target = resolveExportTarget(iSettings.target);
	const bool windows = target == ExportTarget::WindowsX64;
	const auto runnerExe = findRunner(iSettings.runnerDirectory, target);
	result.executable = result.gameDirectory / (windows ? baseName + ".exe" : baseName);
	if (std::error_code ec; !std::filesystem::copy_file(*runnerExe, result.executable,
														std::filesystem::copy_options::overwrite_existing, ec) ||
							ec) {
		OWL_CORE_ERROR("GameExporter: Cannot copy runner to '{}': {}.", result.executable.string(), ec.message())
		return unexpected{ExportError::RunnerCopy};
	}
	copySharedLibs(iSettings.runnerDirectory, result.gameDirectory);
	if (windows) {
		setWindowsIcon(iSettings, result.executable);
	} else {
		addExecPermission(result.executable);
		writeLinuxLauncher(result.gameDirectory, result.executable.filename().string());
	}
#ifdef OWL_PLATFORM_WINDOWS
	if (windows && !createZipArchive(result.gameDirectory, result.gameDirectory.parent_path() / (baseName + ".zip"))) {
		OWL_CORE_WARN("GameExporter: Cannot create the zip archive of '{}'.", result.gameDirectory.string())
		result.warnings.emplace_back("The zip archive of the game cannot be created");
	}
#endif
	finishReport(result, iSettings, assets);
	report(iProgress, 1.f, "Done.");
	const auto seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime).count();
	OWL_CORE_INFO("GameExporter: Exported '{}' ({} assets, {:.2f} MiB) to '{}' in {:.1f} s.", iSettings.gameName,
				  result.assetCount, static_cast<double>(result.packBytes) / (1024.0 * 1024.0),
				  result.gameDirectory.string(), seconds)
	return result;
}

}// namespace owl::data::assets::pack
