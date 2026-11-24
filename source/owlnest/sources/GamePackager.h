/**
 * @file GamePackager.h
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "EditorSettings.h"
#include "Project.h"
#include "RunnerStore.h"
#include "WebPreviewServer.h"
#include "panel/AsyncProgressModal.h"

#include <data/assets/pack/AssetScanner.h>
#include <data/assets/pack/GameExporter.h>
#include <owl.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace owl::nest {

/**
 * @brief
 *  One line of the summary of an export report: a label and its value.
 */
struct ExportReportRow {
	/// Name of the figure.
	std::string label;
	/// The figure, written for a person.
	std::string value;
	/// Help shown when the label is hovered, empty for none.
	std::string tooltip;
};

/**
 * @brief
 *  The summary of an export report as the Export Report window shows it: folder, sizes by category, assets,
 *  renderers, shaders and mods.
 * @param[in] iReport The report.
 * @return The rows, in display order.
 */
[[nodiscard]] auto getExportReportRows(const data::assets::pack::ExportReport& iReport) -> std::vector<ExportReportRow>;

/**
 * @brief
 *  Packages a scene or the whole project: the packaging wizard, the validation report, the background export and the
 *  export report.
 *
 * Progress and errors are shown in the editor's shared progress modal.
 */
class GamePackager final {
public:
	GamePackager(const GamePackager&) = delete;

	GamePackager(GamePackager&&) = delete;

	auto operator=(const GamePackager&) -> GamePackager& = delete;

	auto operator=(GamePackager&&) -> GamePackager& = delete;

	/**
	 * @brief
	 *  Constructor.
	 * @param[in] iProject The editor project (kept by address: it outlives the packager).
	 * @param[in,out] ioProgress The progress modal that shows the background work.
	 * @param[in] iSettings The editor settings, for the runner server (kept by address: they outlive the packager).
	 */
	GamePackager(const Project& iProject, panel::AsyncProgressModal& ioProgress, const EditorSettings& iSettings)
		: mp_project{&iProject}, mp_progress{&ioProgress}, mp_settings{&iSettings} {}

	~GamePackager() = default;

	/**
	 * @brief
	 *  Ask for a destination, then pack one scene and the assets it references into an `.owlpack`.
	 * @param[in] iScenePath The scene file to pack; nothing happens when it is empty.
	 */
	void packScene(const std::filesystem::path& iScenePath);

	/**
	 * @brief
	 *  Open the packaging wizard for the loaded project.
	 */
	void open();

	/**
	 * @brief
	 *  Draw the wizard, the validation report and the export report when they are open.
	 */
	void onRender();

	/**
	 * @brief
	 *  Check whether the wizard, the validation report or the export report is open.
	 * @return True while the user is choosing the packaging options, reviewing the warnings or the report.
	 */
	[[nodiscard]] auto isOpen() const -> bool { return m_showWizard || m_showValidation || m_showReport; }

	/**
	 * @brief
	 *  Get the runners of the exports: shipped with Owl Nest, or downloaded into the user folder.
	 * @return The store, made at the first call.
	 */
	[[nodiscard]] auto getRunners() -> RunnerStore&;

	/**
	 * @brief
	 *  Get the local server of the last Web export played in the browser.
	 * @return The server, not running until such an export.
	 */
	[[nodiscard]] auto getWebPreview() const -> const WebPreviewServer& { return m_webPreview; }

private:
	/**
	 * @brief
	 *  Draw the packaging wizard (destination folder and pack options).
	 */
	void renderWizardModal();

	/**
	 * @brief
	 *  Draw the choice of the target platform and its options.
	 */
	void renderTargetChoice();

	/**
	 * @brief
	 *  Draw the state of the runner of the chosen platform, with its download and import buttons when it is missing.
	 */
	void renderRunnerState();

	/**
	 * @brief
	 *  Download the runner of the chosen platform in the background, from the server of the settings.
	 */
	void startRunnerDownload();

	/**
	 * @brief
	 *  Ask for a runner archive downloaded by hand and install it.
	 */
	void importRunner();

	/**
	 * @brief
	 *  Check whether the runner of the chosen platform is installed.
	 * @return True when the export can bundle it (always for a target without runner file).
	 */
	[[nodiscard]] auto hasRunner() -> bool;

	/**
	 * @brief
	 *  Build the export settings of the project for the chosen platform and folder.
	 * @return The settings.
	 */
	[[nodiscard]] auto makeSettings() -> data::assets::pack::ExportSettings;

	/**
	 * @brief
	 *  Serve a Web export and open it in the default browser.
	 * @param[in] iGameDirectory The folder of the exported game.
	 * @return The address the game plays at, or why the preview could not start.
	 */
	[[nodiscard]] auto playWebExport(const std::filesystem::path& iGameDirectory) -> std::string;

	/**
	 * @brief
	 *  Draw the report of the last export: summary, unused assets and warnings.
	 */
	void renderReportModal();

	/**
	 * @brief
	 *  Draw the validation report and let the user proceed or cancel.
	 */
	void renderValidationModal();

	/**
	 * @brief
	 *  Validate the project on a worker; packaging starts directly when there is no warning.
	 */
	void launchValidation();

	/**
	 * @brief
	 *  Export the game on a worker with the chosen options.
	 */
	void startPacking();

	/// The editor project.
	const Project* mp_project;
	/// The progress modal of the editor.
	panel::AsyncProgressModal* mp_progress;
	/// The editor settings (runner server).
	const EditorSettings* mp_settings;
	/// The runners of the exports, made at the first use.
	uniq<RunnerStore> mp_runners;
	/// Result of the last runner download or import, shown under the target.
	std::string m_runnerMessage;
	/// Destination directory chosen in the wizard.
	std::filesystem::path m_destDir;
	/// Platform chosen in the wizard.
	data::assets::pack::ExportTarget m_target = data::assets::pack::ExportTarget::Host;
	/// Whether a Web export opens in the browser once written.
	bool m_runInBrowser = true;
	/// Serves the last Web export to the browser.
	WebPreviewServer m_webPreview;
	/// Warnings of the last validation, shown before packaging.
	std::vector<std::string> m_warnings;
	/// Assets found by the validation, reused by the export.
	shared<std::vector<data::assets::pack::AssetReference>> m_scannedAssets;
	/// Report of the last export, shown once it is written.
	std::optional<data::assets::pack::ExportReport> m_report;
	/// Line under the report (the address of a Web export played in the browser).
	std::string m_reportNote;
	/// Whether the export report is shown.
	bool m_showReport = false;
	/// Whether the validation report is shown.
	bool m_showValidation = false;
	/// Whether the wizard is shown.
	bool m_showWizard = false;
	/// Compress the pack blobs with zstd.
	bool m_compress = true;
	/// Obfuscate the pack table of contents.
	bool m_obfuscate = true;
};

}// namespace owl::nest
