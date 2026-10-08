/**
 * @file GamePackager.h
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Project.h"
#include "panel/AsyncProgressModal.h"

#include <data/assets/pack/AssetScanner.h>
#include <owl.h>

#include <filesystem>
#include <string>
#include <vector>

namespace owl::nest {

/**
 * @brief
 *  Packages a scene or the whole project: the packaging wizard, the validation report and the background export.
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
	 */
	GamePackager(const Project& iProject, panel::AsyncProgressModal& ioProgress)
		: mp_project{&iProject}, mp_progress{&ioProgress} {}

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
	 *  Draw the wizard and the validation report when they are open.
	 */
	void onRender();

	/**
	 * @brief
	 *  Check whether the wizard or the validation report is open.
	 * @return True while the user is choosing the packaging options or reviewing the warnings.
	 */
	[[nodiscard]] auto isOpen() const -> bool { return m_showWizard || m_showValidation; }

private:
	/**
	 * @brief
	 *  Draw the packaging wizard (destination folder and pack options).
	 */
	void renderWizardModal();

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
	/// Destination directory chosen in the wizard.
	std::filesystem::path m_destDir;
	/// Warnings of the last validation, shown before packaging.
	std::vector<std::string> m_warnings;
	/// Assets found by the validation, reused by the export.
	shared<std::vector<data::assets::pack::AssetReference>> m_scannedAssets;
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
