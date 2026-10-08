/**
 * @file GamePackager.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "GamePackager.h"

#include <data/assets/pack/GameExporter.h>
#include <data/assets/pack/PackWriter.h>
#include <gui/IconBank.h>

#include <imgui.h>
#include <imgui_stdlib.h>

#include <chrono>
#include <cstdint>
#include <format>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace owl::nest {

void GamePackager::onRender() {
	renderWizardModal();
	renderValidationModal();
}

void GamePackager::packScene(const std::filesystem::path& iScenePath) {
	if (iScenePath.empty() || mp_progress->isActive())
		return;

	const std::string defaultPackName = iScenePath.stem().string() + ".owlpack";
	const auto destPath = platform::FileDialog::saveFile("Owl Scene Pack (*.owlpack)|owlpack\n", defaultPackName);
	if (destPath.empty())
		return;

	auto outputPath = destPath;
	if (outputPath.extension() != ".owlpack")
		outputPath.replace_extension(".owlpack");

	const auto& scenePath = iScenePath;
	const auto sceneFilename = scenePath.filename().string();

	auto state = mkShared<AsyncProgressState>();
	mp_progress->open("Packing Scene...", state, true);

	app::Application::get().getTaskScheduler().pushTask(core::task::Task(
			[state, scenePath, sceneFilename, outputPath]() -> void {
				state->setMessage("Scanning assets...");
				const auto assets = data::assets::pack::AssetScanner::scanScene(scenePath);
				if (assets.empty()) {
					state->setError("No assets found to pack for scene " + sceneFilename);
					return;
				}
				if (state->cancelRequested.load())
					return;
				state->progress.store(0.2f);
				state->setMessage("Writing pack (" + std::to_string(assets.size()) + " assets)...");

				data::assets::pack::PackWriter writer;
				for (const auto& ref: assets) writer.addFile(ref.diskPath, ref.packPath, ref.assetType);

				const bool writeOk = writer.write(
						outputPath, data::assets::pack::PackFlags::Default,
						[&state](const uint32_t iCurrent, const uint32_t iTotal) -> void {
							state->progress.store(0.2f +
												  0.75f * static_cast<float>(iCurrent) / static_cast<float>(iTotal));
						},
						[&state]() -> bool { return state->cancelRequested.load(); });
				if (!writeOk) {
					if (state->cancelRequested.load())
						state->setError("Packing cancelled.");
					else
						state->setError("Failed to write scene pack.");
					return;
				}
				state->progress.store(1.0f);
				std::error_code sizeEc;
				const auto packBytes = exists(outputPath) ? std::filesystem::file_size(outputPath, sizeEc) : 0;
				const auto mib = static_cast<double>(packBytes) / (1024.0 * 1024.0);
				state->setMessage(std::format("Packed {} assets ({:.2f} MiB)\nOutput: {}", assets.size(), mib,
											  outputPath.string()));
				OWL_CORE_INFO("Scene packed: {} ({} assets, {:.2f} MiB) -> {}.", sceneFilename, assets.size(), mib,
							  outputPath.string())
			},
			[state]() -> void { state->completed.store(true); }));
}

void GamePackager::open() {
	if (!mp_project->isLoaded() || mp_progress->isActive() || m_showValidation || m_showWizard)
		return;
	// Open the packaging wizard to let the user choose destination + options.
	m_destDir.clear();
	m_showWizard = true;
}

void GamePackager::renderWizardModal() {
	if (!m_showWizard)
		return;
	if (!ImGui::IsPopupOpen("Packaging Wizard"))
		ImGui::OpenPopup("Packaging Wizard");

	const auto* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(560, 0), ImGuiCond_Always);

	if (ImGui::BeginPopupModal("Packaging Wizard", nullptr,
							   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
		ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "Pack Game");
		ImGui::TextWrapped("Project: %s  (version: %s)", mp_project->name.c_str(),
						   mp_project->version.empty() ? "-" : mp_project->version.c_str());
		ImGui::Spacing();
		ImGui::Separator();

		// Target platform (read-only — current build platform).
		const char* platform =
#ifdef OWL_PLATFORM_WINDOWS
				"Windows (x64)";
#else
				"Linux (x64)";
#endif
		ImGui::Text("Target platform: %s", platform);
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay))
			ImGui::SetTooltip("The pack is built for the current platform. Cross-compilation is planned for v0.3.");

		// Destination folder with Browse button.
		ImGui::Spacing();
		ImGui::Text("Output folder:");
		auto destStr = m_destDir.string();
		ImGui::SetNextItemWidth(-120);
		if (ImGui::InputText("##dest", &destStr))
			m_destDir = std::filesystem::path(destStr);
		ImGui::SameLine();
		if (gui::IconBank::instance().iconButton("open", "Browse...##packDest")) {
			const auto picked = platform::FileDialog::pickFolder();
			if (!picked.empty())
				m_destDir = picked;
		}

		// Options.
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Text("Options");
		ImGui::Checkbox("Compress pack (zstd)", &m_compress);
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay))
			ImGui::SetTooltip("Compress asset blobs with zstd. Smaller output at the cost of a slower pack.");
		ImGui::Checkbox("Obfuscate TOC (XOR)", &m_obfuscate);
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay))
			ImGui::SetTooltip("XOR-obfuscate the table of contents to deter casual pack inspection.");

		ImGui::Spacing();
		ImGui::Separator();
		const bool canStart = !m_destDir.empty();
		const auto& iconBank = gui::IconBank::instance();
		ImGui::BeginDisabled(!canStart);
		if (iconBank.iconButton("pack", "Start Packaging", {ImGui::GetFontSize() * 10.f, 0})) {
			m_showWizard = false;
			ImGui::CloseCurrentPopup();
			launchValidation();
		}
		ImGui::EndDisabled();
		if (!canStart && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay))
			ImGui::SetTooltip("Choose an output folder to enable packaging.");
		ImGui::SameLine();
		if (iconBank.iconButton("close", "Cancel##packWiz", {ImGui::GetFontSize() * 7.f, 0})) {
			m_showWizard = false;
			m_destDir.clear();
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

void GamePackager::launchValidation() {
	m_warnings.clear();

	auto state = mkShared<AsyncProgressState>();
	state->setMessage("Validating project...");
	state->progress.store(0.3f);
	mp_progress->open("Validating...", state, false);

	auto settings = mp_project->makeExportSettings(m_destDir, app::Application::get().getWorkingDirectory());
	auto warningsOut = mkShared<std::vector<std::string>>();
	auto assetsOut = mkShared<std::vector<data::assets::pack::AssetReference>>();

	app::Application::get().getTaskScheduler().pushTask(core::task::Task(
			[state, exportSettings = std::move(settings), warningsOut, assetsOut]() -> void {
				*warningsOut = data::assets::pack::GameExporter::validate(exportSettings, *assetsOut);
				state->progress.store(1.0f);
			},
			[this, state, warningsOut, assetsOut]() -> void {
				state->completed.store(true);
				mp_progress->close();
				m_warnings = std::move(*warningsOut);
				m_scannedAssets = assetsOut;
				if (m_warnings.empty())
					startPacking();
				else
					m_showValidation = true;
			}));
}

void GamePackager::renderValidationModal() {
	if (!m_showValidation)
		return;
	if (!ImGui::IsPopupOpen("Packaging Validation"))
		ImGui::OpenPopup("Packaging Validation");

	const auto* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(520, 0), ImGuiCond_Always);

	if (ImGui::BeginPopupModal("Packaging Validation", nullptr,
							   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
		ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f), "%zu issue(s) found before packaging:", m_warnings.size());
		ImGui::Separator();
		ImGui::BeginChild("##packWarnings", ImVec2(0, 200), ImGuiChildFlags_Borders);
		for (const auto& warning: m_warnings) ImGui::BulletText("%s", warning.c_str());
		ImGui::EndChild();
		ImGui::TextDisabled("The game may be missing assets or not playable. Proceed anyway?");
		ImGui::Spacing();
		const auto& iconBank = gui::IconBank::instance();
		if (iconBank.iconButton("pack", "Proceed anyway", {ImGui::GetFontSize() * 10.f, 0})) {
			m_showValidation = false;
			ImGui::CloseCurrentPopup();
			startPacking();
		}
		ImGui::SameLine();
		if (iconBank.iconButton("close", "Cancel", {ImGui::GetFontSize() * 7.f, 0})) {
			m_showValidation = false;
			m_warnings.clear();
			m_destDir.clear();
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

void GamePackager::startPacking() {
	auto settings = mp_project->makeExportSettings(m_destDir, app::Application::get().getWorkingDirectory());
	settings.packFlags =
			(m_compress ? data::assets::pack::PackFlags::Compressed : data::assets::pack::PackFlags::None) |
			(m_obfuscate ? data::assets::pack::PackFlags::Obfuscated : data::assets::pack::PackFlags::None);
	m_destDir.clear();
	m_warnings.clear();
	auto preScanned = m_scannedAssets ? m_scannedAssets : mkShared<std::vector<data::assets::pack::AssetReference>>();
	m_scannedAssets.reset();

	auto state = mkShared<AsyncProgressState>();
	mp_progress->open("Packing Game...", state, true);
	app::Application::get().getTaskScheduler().pushTask(core::task::Task(
			[state, exportSettings = std::move(settings), preScanned]() -> void {
				const auto startTime = std::chrono::steady_clock::now();
				const auto result = data::assets::pack::GameExporter::exportGame(
						exportSettings, *preScanned,
						[&state](const float iFraction, const std::string& iMessage) -> void {
							state->progress.store(iFraction);
							state->setMessage(iMessage);
						},
						[&state]() -> bool { return state->cancelRequested.load(); });
				if (!result) {
					state->setError(std::format("Packaging failed: {}.",
												data::assets::pack::GameExporter::getErrorMessage(result.error())));
					return;
				}
				const auto seconds =
						std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime).count();
				state->setMessage(std::format("Packed {} assets ({:.2f} MiB)\nOutput: {}\nDuration: {:.1f}s",
											  result->assetCount,
											  static_cast<double>(result->packBytes) / (1024.0 * 1024.0),
											  result->gameDirectory.string(), seconds));
			},
			[state]() -> void { state->completed.store(true); }));
}

}// namespace owl::nest
