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
#include "core/external/yaml.h"
#include "data/assets/pack/PackWriter.h"

#include <chrono>
#include <fstream>
#include <sstream>

namespace owl::data::assets::pack {

namespace {

#ifdef OWL_PLATFORM_WINDOWS
constexpr std::array<const char*, 1> g_runnerNames = {"OwlRunner.exe"};
constexpr auto g_platformName = "windows-x64";
#else
constexpr std::array<const char*, 1> g_runnerNames = {"OwlRunner"};
constexpr auto g_platformName = "linux-x64";
#endif

auto findRunner(const std::filesystem::path& iRunnerDir) -> std::optional<std::filesystem::path> {
	for (const auto* name: g_runnerNames) {
		if (auto candidate = iRunnerDir / name; exists(candidate))
			return candidate;
	}
	return std::nullopt;
}

auto readText(const std::filesystem::path& iFile) -> std::optional<std::string> {
	std::ifstream in(iFile, std::ios::binary);
	if (!in.is_open())
		return std::nullopt;
	std::stringstream buffer;
	buffer << in.rdbuf();
	return buffer.str();
}

auto isRelocatable(const AssetReference& iRef) -> bool {
	if (iRef.assetType == AssetType::Scene)
		return true;
	const auto ext = iRef.diskPath.extension().string();
	return ext == ".owltileset" || ext == ".owltilemap" || ext == ".owlprefab";
}

auto resolveIcon(const ExportSettings& iSettings) -> std::optional<std::filesystem::path> {
	if (iSettings.icon.empty())
		return std::nullopt;
	if (auto local = iSettings.projectDirectory / iSettings.icon; exists(local))
		return local;
	if (!app::Application::instanced())
		return std::nullopt;
	for (const auto& [title, assetsPath]: app::Application::get().getAssetDirectories()) {
		if (auto candidate = assetsPath / iSettings.icon; exists(candidate))
			return candidate;
	}
	return std::nullopt;
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

#ifdef OWL_PLATFORM_LINUX
void addExecPermission(const std::filesystem::path& iFile) {
	std::error_code ec;
	std::filesystem::permissions(iFile,
								 std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec |
										 std::filesystem::perms::others_exec,
								 std::filesystem::perm_options::add, ec);
	if (ec)
		OWL_CORE_WARN("GameExporter: Cannot set exec permissions on {}: {}.", iFile.string(), ec.message())
}
#endif

void copySharedLibs(const std::filesystem::path& iSrcDir, const std::filesystem::path& iDestDir) {
	std::error_code ec;
	for (const auto& entry: std::filesystem::directory_iterator(iSrcDir, ec)) {
		if (!entry.is_regular_file() && !entry.is_symlink())
			continue;
		const auto ext = entry.path().extension().string();
		if (const auto filename = entry.path().filename().string();
			ext != ".so" && filename.find(".so.") == std::string::npos && ext != ".dll")
			continue;
		const auto dest = iDestDir / entry.path().filename();
		std::error_code copyEc;
		std::filesystem::remove(dest, copyEc);
		std::filesystem::copy(entry.path(), dest, std::filesystem::copy_options::copy_symlinks, copyEc);
		if (copyEc)
			OWL_CORE_WARN("GameExporter: Cannot copy {}: {}.", entry.path().string(), copyEc.message())
	}
}

void writeRunnerConfig(const ExportSettings& iSettings, const std::filesystem::path& iGameDir,
					   const std::string& iFirstScene, const std::string& iPackFile, const std::string& iIcon) {
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
	out << YAML::Key << "WindowWidth" << YAML::Value << iSettings.windowSize.x();
	out << YAML::Key << "WindowHeight" << YAML::Value << iSettings.windowSize.y();
	out << YAML::Key << "Fullscreen" << YAML::Value << iSettings.fullscreen;
	out << YAML::Key << "Resizable" << YAML::Value << iSettings.resizable;
	if (!iSettings.rendererStack.isEmpty())
		out << YAML::Key << "RendererStack" << YAML::Value << iSettings.rendererStack.toYaml();
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
	out << YAML::Key << "Platform" << YAML::Value << g_platformName;
	out << YAML::EndMap;
	out << YAML::EndMap;
	std::ofstream file(iGameDir / "game_info.yml");
	file << out.c_str() << "\n";
}

#ifdef OWL_PLATFORM_LINUX
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
#endif

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

}// namespace

auto GameExporter::getErrorMessage(const ExportError iError) -> std::string_view {
	switch (iError) {
		case ExportError::NoAssets:
			return "no asset to pack, check the first scene and its references";
		case ExportError::OutputDirectory:
			return "cannot create the output directory";
		case ExportError::PackWrite:
			return "cannot write the game pack";
		case ExportError::RunnerNotFound:
			return "OwlRunner executable not found";
		case ExportError::RunnerCopy:
			return "cannot copy the runner executable";
		case ExportError::Cancelled:
			return "cancelled";
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

auto GameExporter::validate(const ExportSettings& iSettings, std::vector<AssetReference>& oAssets)
		-> std::vector<std::string> {
	std::vector<std::string> warnings;
	oAssets = AssetScanner::scanProject(iSettings.projectDirectory, iSettings.firstScene, &warnings);
	if (oAssets.empty())
		warnings.emplace_back("No assets to pack — check firstScene and its references");
	if (!findRunner(iSettings.runnerDirectory))
		warnings.emplace_back(std::format("OwlRunner executable not found in '{}' — packed game will not be playable",
										  iSettings.runnerDirectory.string()));
	if (!iSettings.icon.empty() && !resolveIcon(iSettings))
		warnings.emplace_back(std::format("Icon '{}' not found", iSettings.icon));
	return warnings;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity) Linear sequence of export steps with early returns.
auto GameExporter::exportGame(const ExportSettings& iSettings, const std::vector<AssetReference>& iAssets,
							  const ProgressCallback& iProgress, const CancelCheck& iCancel)
		-> expected<ExportReport, ExportError> {
	const auto startTime = std::chrono::steady_clock::now();
	std::vector<AssetReference> assets = iAssets;
	if (assets.empty()) {
		report(iProgress, 0.f, "Scanning assets...");
		assets = AssetScanner::scanProject(iSettings.projectDirectory, iSettings.firstScene);
	}
	if (assets.empty()) {
		OWL_CORE_ERROR("GameExporter: No asset found to pack for first scene '{}'.", iSettings.firstScene)
		return unexpected{ExportError::NoAssets};
	}
	const auto runnerExe = findRunner(iSettings.runnerDirectory);
	if (!runnerExe) {
		OWL_CORE_ERROR("GameExporter: OwlRunner executable not found in '{}'.", iSettings.runnerDirectory.string())
		return unexpected{ExportError::RunnerNotFound};
	}
	const auto baseName = sanitizeFilename(iSettings.gameName);
	ExportReport result;
	result.gameDirectory = iSettings.outputDirectory / baseName;
	result.assetCount = assets.size();
	if (std::error_code ec; !std::filesystem::create_directories(result.gameDirectory, ec) && ec) {
		OWL_CORE_ERROR("GameExporter: Cannot create output directory '{}': {}.", result.gameDirectory.string(),
					   ec.message())
		return unexpected{ExportError::OutputDirectory};
	}

	report(iProgress, 0.15f, std::format("Building pack ({} assets)...", assets.size()));
	PackWriter writer;
	for (const auto& ref: assets) {
		if (isRelocatable(ref)) {
			if (const auto text = readText(ref.diskPath); text) {
				const auto relocated = relocateAbsolutePaths(*text, assets);
				writer.addData(std::vector<uint8_t>(relocated.begin(), relocated.end()), ref.packPath, ref.assetType);
				continue;
			}
		}
		writer.addFile(ref.diskPath, ref.packPath, ref.assetType);
	}
	if (const auto settingsPath = iSettings.projectDirectory / "game_settings.yml"; exists(settingsPath))
		writer.addFile(settingsPath, "game_settings.yml", AssetType::Other);
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
			OWL_CORE_WARN("GameExporter: Export cancelled.")
			return unexpected{ExportError::Cancelled};
		}
		OWL_CORE_ERROR("GameExporter: Cannot write pack '{}'.", result.packFile.string())
		return unexpected{ExportError::PackWrite};
	}

	report(iProgress, 0.82f, "Writing configuration...");
	std::string iconEntry;
	if (const auto iconSrc = resolveIcon(iSettings); iconSrc) {
		const auto iconDst = result.gameDirectory / iSettings.icon;
		std::error_code ec;
		std::filesystem::create_directories(iconDst.parent_path(), ec);
		std::filesystem::copy_file(*iconSrc, iconDst, std::filesystem::copy_options::overwrite_existing, ec);
		if (ec)
			OWL_CORE_WARN("GameExporter: Cannot copy icon {}: {}.", iconSrc->string(), ec.message())
		else
			iconEntry = iSettings.icon;
	} else if (!iSettings.icon.empty()) {
		OWL_CORE_WARN("GameExporter: Icon '{}' not found, the game keeps the default icon.", iSettings.icon)
	}
	writeRunnerConfig(iSettings, result.gameDirectory, resolveFirstScene(iSettings.firstScene, assets), packFilename,
					  iconEntry);
	writeMetadata(iSettings, result.gameDirectory);
	if (isCancelled(iCancel)) {
		OWL_CORE_WARN("GameExporter: Export cancelled.")
		return unexpected{ExportError::Cancelled};
	}

	report(iProgress, 0.88f, "Copying runner...");
#ifdef OWL_PLATFORM_WINDOWS
	result.executable = result.gameDirectory / (baseName + ".exe");
#else
	result.executable = result.gameDirectory / baseName;
#endif
	if (std::error_code ec; !std::filesystem::copy_file(*runnerExe, result.executable,
														std::filesystem::copy_options::overwrite_existing, ec) ||
							ec) {
		OWL_CORE_ERROR("GameExporter: Cannot copy runner to '{}': {}.", result.executable.string(), ec.message())
		return unexpected{ExportError::RunnerCopy};
	}
	copySharedLibs(iSettings.runnerDirectory, result.gameDirectory);
#ifdef OWL_PLATFORM_LINUX
	addExecPermission(result.executable);
	writeLinuxLauncher(result.gameDirectory, result.executable.filename().string());
#endif
#ifdef OWL_PLATFORM_WINDOWS
	if (!createZipArchive(result.gameDirectory, result.gameDirectory.parent_path() / (baseName + ".zip")))
		OWL_CORE_WARN("GameExporter: Cannot create the zip archive of {}.", result.gameDirectory.string())
#endif
	std::error_code sizeEc;
	result.packBytes = std::filesystem::file_size(result.packFile, sizeEc);
	report(iProgress, 1.f, "Done.");
	const auto seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime).count();
	OWL_CORE_INFO("GameExporter: Exported {} ({} assets, {:.2f} MiB) to {} in {:.1f}s.", iSettings.gameName,
				  result.assetCount, static_cast<double>(result.packBytes) / (1024.0 * 1024.0),
				  result.gameDirectory.string(), seconds)
	return result;
}

}// namespace owl::data::assets::pack
