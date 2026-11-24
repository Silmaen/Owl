/**
 * @file ExportReport.h
 * @author Silmaen
 * @date 23/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "AssetScanner.h"
#include "core/Core.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace owl::data::assets::pack {

/// Name of the report an export writes in the folder of the game.
inline constexpr std::string_view g_exportReportFile{"export_report.yml"};

/**
 * @brief
 *  What a file of an exported game is, for the size of each category.
 */
enum struct ExportFileKind : uint8_t {
	/// The `.owlpack` of the game and the `.owlmod` of its embedded mods.
	Pack,
	/// The runner executable, or the page, script and module of the Web runner.
	Runner,
	/// A shared library of the runner (`.so`, `.dll`, `.dylib`).
	Library,
	/// Anything else: configuration, icon, launcher, file list.
	Other,
};

/**
 * @brief
 *  Size in bytes of an exported game folder, by category.
 */
struct OWL_API ExportSizes {
	/// The packs of the game and its mods.
	uintmax_t pack{0};
	/// The runner executable (or the Web runner files).
	uintmax_t runner{0};
	/// The shared libraries.
	uintmax_t libraries{0};
	/// The other files.
	uintmax_t other{0};

	/**
	 * @brief
	 *  Size of the whole folder.
	 * @return The sum of the categories.
	 */
	[[nodiscard]] auto getTotal() const -> uintmax_t { return pack + runner + libraries + other; }
};

/**
 * @brief
 *  Report of a successful export: where the game is, what it holds, its size, and what the user should look at.
 */
struct OWL_API ExportReport {
	/// Folder of the exported game.
	std::filesystem::path gameDirectory;
	/// Path of the written `.owlpack`.
	std::filesystem::path packFile;
	/// Path of the copied runner executable, its page (`index.html`) for the Web, empty for Android.
	std::filesystem::path executable;
	/// Platform of the game, as `game_info.yml` names it (`linux-x64`, `web-wasm32`...).
	std::string platform;
	/// Number of packed assets.
	size_t assetCount{0};
	/// Size of the pack in bytes.
	uintmax_t packBytes{0};
	/// Size of the game folder by category.
	ExportSizes sizes;
	/// Assets cooked during this export (the others came from the cook cache).
	size_t cookedAssets{0};
	/// Assets served by the cook cache.
	size_t cacheHits{0};
	/// Optional renderer families the game initialises (`RendererSet::getNames`), besides the 2D family.
	std::vector<std::string> renderers;
	/// Engine files of the scan left out of the pack: shaders of the renderers the game does not use.
	size_t engineFilesSkipped{0};
	/// Shaders compiled by the export because the pack held no up-to-date SPIR-V for them (project shaders).
	size_t shadersBaked{0};
	/// Shader targets the packed shaders are baked for (`vulkan`, `opengl`, `webgpu`...).
	std::vector<std::string> shaderTargets;
	/// The `.owlmod` files written in the `mods` folder of the game, one per extension pack embedded.
	std::vector<std::filesystem::path> mods;
	/// Asset files of the project the export did not pack, relative to the project directory, sorted.
	std::vector<std::string> unusedAssets;
	/// What may make the game incomplete: missing references, missing icon, files that could not be written.
	std::vector<std::string> warnings;

	/**
	 * @brief
	 *  Add warnings not already in the report (the ones of `GameExporter::validate`, for instance).
	 * @param[in] iWarnings The warnings.
	 */
	void addWarnings(const std::vector<std::string>& iWarnings);

	/**
	 * @brief
	 *  Write the report as YAML.
	 * @return The YAML text, rooted at `ExportReport`.
	 */
	[[nodiscard]] auto toYaml() const -> std::string;

	/**
	 * @brief
	 *  Short text of the report for a terminal: folder, sizes, shaders, unused assets and warning counts.
	 * @return A few lines, without trailing newline.
	 */
	[[nodiscard]] auto getSummary() const -> std::string;

	/**
	 * @brief
	 *  Write the report in the folder of the game, as `g_exportReportFile`.
	 * @return True when the file is written.
	 */
	[[nodiscard]] auto write() const -> bool;
};

/**
 * @brief
 *  Tell what a file of an exported game is.
 * @param[in] iFile The file, inside the game folder.
 * @param[in] iExecutable The runner executable (or page) of the game, empty for none.
 * @return The category of the file.
 */
[[nodiscard]] OWL_API auto classifyExportFile(const std::filesystem::path& iFile,
											  const std::filesystem::path& iExecutable) -> ExportFileKind;

/**
 * @brief
 *  Measure an exported game folder, recursively, by category.
 * @param[in] iGameDirectory The folder of the game.
 * @param[in] iExecutable The runner executable (or page) of the game, empty for none.
 * @return The sizes; all zero when the folder cannot be read.
 */
[[nodiscard]] OWL_API auto measureExportSizes(const std::filesystem::path& iGameDirectory,
											  const std::filesystem::path& iExecutable) -> ExportSizes;

/**
 * @brief
 *  List the asset files of a project an export does not pack.
 *
 *  Walks the project directory, skipping hidden folders (`.owl`, `.git`) and the excluded paths; only files with an
 *  importer (`importerOf`) count, so sidecars, settings and text files never show.
 * @param[in] iProjectDirectory The project directory.
 * @param[in] iUsed The assets the export packs.
 * @param[in] iExcluded Files or folders to ignore (output folder, icon, extension sources).
 * @return The unused files, relative to the project directory with `/` separators, sorted.
 */
[[nodiscard]] OWL_API auto findUnusedAssets(const std::filesystem::path& iProjectDirectory,
											const std::vector<AssetReference>& iUsed,
											const std::vector<std::filesystem::path>& iExcluded)
		-> std::vector<std::string>;

/**
 * @brief
 *  Write a size in bytes for a person: `512 B`, `12.3 KiB`, `4.56 MiB`, `1.20 GiB`.
 * @param[in] iBytes The size.
 * @return The text.
 */
[[nodiscard]] OWL_API auto formatByteSize(uintmax_t iBytes) -> std::string;

}// namespace owl::data::assets::pack
