/**
 * @file GameExporter.h
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "AssetScanner.h"
#include "core/expected.h"
#include "math/vectors.h"

#include <filesystem>
#include <functional>

namespace owl::data::assets::pack {
/**
 * @brief
 *  Everything needed to export a game: project metadata, window defaults and locations.
 */
struct OWL_API ExportSettings {
	/// Game display name (also the base name of the output folder, pack and executable).
	std::string gameName;
	/// First scene, relative to an asset directory (extension optional).
	std::string firstScene;
	/// Game version string.
	std::string version;
	/// Author or studio name.
	std::string author;
	/// Short game description.
	std::string description;
	/// Game icon, relative to the project directory or to an asset directory.
	std::string icon;
	/// Project root directory (holds `game_settings.yml`).
	std::filesystem::path projectDirectory;
	/// Parent folder of the exported game; the game lands in `outputDirectory / sanitized(gameName)`.
	std::filesystem::path outputDirectory;
	/// Folder holding the `OwlRunner` executable and the shared libraries to bundle.
	std::filesystem::path runnerDirectory;
	/// Default window size in pixels.
	math::vec2ui windowSize{1280, 720};
	/// Whether the game starts in fullscreen mode.
	bool fullscreen{false};
	/// Whether the game window is resizable.
	bool resizable{true};
	/// Project renderer stack as YAML (`RendererStackConfig::toYaml`), forwarded to `runner.yml`; empty for none.
	std::string rendererStackYaml;
	/// Pack flags (compression, obfuscation).
	PackFlags packFlags{PackFlags::Default};
};

/**
 * @brief
 *  Reasons an export can fail.
 */
enum struct ExportError : uint8_t {
	/// The scan found no asset to pack.
	NoAssets,
	/// The output folder could not be created.
	OutputDirectory,
	/// The `.owlpack` could not be written.
	PackWrite,
	/// No runner executable in the runner directory.
	RunnerNotFound,
	/// The runner executable could not be copied.
	RunnerCopy,
	/// The export was cancelled by the caller.
	Cancelled,
};

/**
 * @brief
 *  Summary of a successful export.
 */
struct OWL_API ExportReport {
	/// Folder of the exported game.
	std::filesystem::path gameDirectory;
	/// Path of the written `.owlpack`.
	std::filesystem::path packFile;
	/// Path of the copied runner executable.
	std::filesystem::path executable;
	/// Number of packed assets.
	size_t assetCount{0};
	/// Size of the pack in bytes.
	uintmax_t packBytes{0};
};

/**
 * @brief
 *  Exports a project as a standalone game folder: `.owlpack`, `runner.yml`, `game_info.yml`, runner executable,
 *  shared libraries, icon and launcher.
 *
 *  Shared by Owl Nest *Pack Game*, the `OwlNest --export` command line and the end-to-end export test. The asset
 *  scan relies on the application's asset directories, so the project directory must be registered first.
 */
class OWL_API GameExporter final {
public:
	/// Progress callback (fraction in [0, 1], step message).
	using ProgressCallback = std::function<void(float, const std::string&)>;
	/// Cancel check (returns true to abort).
	using CancelCheck = std::function<bool()>;

	/**
	 * @brief
	 *  Scan the project and list what would make the exported game incomplete.
	 * @param[in] iSettings The export settings.
	 * @param[out] oAssets The scanned assets, reusable by exportGame().
	 * @return The warnings (missing references, missing runner); empty when the export is clean.
	 */
	[[nodiscard]] static auto validate(const ExportSettings& iSettings, std::vector<AssetReference>& oAssets)
			-> std::vector<std::string>;

	/**
	 * @brief
	 *  Export the game.
	 * @param[in] iSettings The export settings.
	 * @param[in] iAssets Pre-scanned assets (from validate()); scanned again when empty.
	 * @param[in] iProgress Optional progress callback.
	 * @param[in] iCancel Optional cancel check.
	 * @return The export report, or the reason of the failure.
	 */
	[[nodiscard]] static auto exportGame(const ExportSettings& iSettings, const std::vector<AssetReference>& iAssets,
										 const ProgressCallback& iProgress = {}, const CancelCheck& iCancel = {})
			-> expected<ExportReport, ExportError>;

	/**
	 * @brief
	 *  Describe an export error for the user.
	 * @param[in] iError The error.
	 * @return A short human-readable message.
	 */
	[[nodiscard]] static auto getErrorMessage(ExportError iError) -> std::string_view;

	/**
	 * @brief
	 *  Turn a game name into a file name (path separators, reserved characters and spaces become `_`).
	 * @param[in] iName The game name.
	 * @return The sanitized name.
	 */
	[[nodiscard]] static auto sanitizeFilename(const std::string& iName) -> std::string;

	/**
	 * @brief
	 *  Rewrite the absolute `pat:` texture references of a scene or tileset into pack-relative `nam:` ones.
	 * @param[in] iText The YAML text.
	 * @param[in] iAssets The packed assets (disk path to pack path mapping).
	 * @return The rewritten text.
	 */
	[[nodiscard]] static auto relocateAbsolutePaths(const std::string& iText,
													const std::vector<AssetReference>& iAssets) -> std::string;
};

}// namespace owl::data::assets::pack
