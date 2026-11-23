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
#include <gui/IconFont.h>
#include <gui/UiLayer.h>
#include <gui/utils.h>
#include <gui/widgets/Combo.h>
#include <gui/widgets/IconButton.h>
#include <gui/widgets/Toggle.h>

#include <imgui.h>
#include <imgui_stdlib.h>
#include <platform/FileDialog.h>
#include <platform/UserDirectory.h>
#include <platform/openExternalUrl.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
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
	if (iScenePath.empty())
		return;
	if (mp_progress->isActive()) {
		OWL_CORE_WARN("GamePackager: Cannot pack the scene while another operation runs.")
		return;
	}

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
				OWL_CORE_INFO("GamePackager: Scene '{}' packed ({} assets, {:.2f} MiB) to '{}'.", sceneFilename,
							  assets.size(), mib, outputPath.string())
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
		gui::UiLayer::pushTextStyle(gui::TextStyle::Title);
		ImGui::TextColored(gui::vec(gui::UiLayer::getTheme().accent), "Pack Game");
		gui::UiLayer::popTextStyle();
		ImGui::TextWrapped("Project: %s  (version: %s)", mp_project->name.c_str(),
						   mp_project->getGameVersion().c_str());
		ImGui::Spacing();
		ImGui::Separator();

		renderTargetChoice();

		// Destination folder with Browse button.
		ImGui::Spacing();
		ImGui::Text("Output folder:");
		auto destStr = m_destDir.string();
		ImGui::SetNextItemWidth(-120);
		if (ImGui::InputText("##dest", &destStr))
			m_destDir = std::filesystem::path(destStr);
		ImGui::SameLine();
		if (gui::widgets::button("Browse...##packDest", {.glyph = gui::icon::folderOpen, .label = "Browse..."})) {
			const auto picked = platform::FileDialog::pickFolder();
			if (!picked.empty())
				m_destDir = picked;
		}

		// Options.
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Text("Options");
		gui::widgets::checkbox("Compress pack (zstd)", &m_compress);
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay))
			ImGui::SetTooltip("Compress asset blobs with zstd. Smaller output at the cost of a slower pack.");
		gui::widgets::checkbox("Obfuscate TOC (XOR)", &m_obfuscate);
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay))
			ImGui::SetTooltip("XOR-obfuscate the table of contents to deter casual pack inspection.");

		ImGui::Spacing();
		ImGui::Separator();
		const bool canStart = !m_destDir.empty() && hasRunner();
		ImGui::BeginDisabled(!canStart);
		if (gui::widgets::button("Start Packaging", {.glyph = gui::icon::packageCheck,
													 .label = "Start Packaging",
													 .primary = true,
													 .width = ImGui::GetFontSize() * 10.f})) {
			m_showWizard = false;
			ImGui::CloseCurrentPopup();
			launchValidation();
		}
		ImGui::EndDisabled();
		if (!canStart && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay))
			ImGui::SetTooltip(hasRunner() ? "Choose an output folder to enable packaging."
										  : "No runner of this platform next to Owl Nest.");
		ImGui::SameLine();
		if (gui::widgets::button("Cancel##packWiz",
								 {.glyph = gui::icon::x, .label = "Cancel", .width = ImGui::GetFontSize() * 7.f})) {
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

	auto settings = makeSettings();
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
		ImGui::TextColored(gui::vec(gui::UiLayer::getTheme().warning),
						   "%zu issue(s) found before packaging:", m_warnings.size());
		ImGui::Separator();
		ImGui::BeginChild("##packWarnings", ImVec2(0, 200), ImGuiChildFlags_Borders);
		for (const auto& warning: m_warnings) ImGui::BulletText("%s", warning.c_str());
		ImGui::EndChild();
		ImGui::TextDisabled("The game may be missing assets or not playable. Proceed anyway?");
		ImGui::Spacing();
		if (gui::widgets::button("Proceed anyway", {.glyph = gui::icon::packageCheck,
													.label = "Proceed anyway",
													.width = ImGui::GetFontSize() * 10.f})) {
			m_showValidation = false;
			ImGui::CloseCurrentPopup();
			startPacking();
		}
		ImGui::SameLine();
		if (gui::widgets::button("Cancel",
								 {.glyph = gui::icon::x, .label = "Cancel", .width = ImGui::GetFontSize() * 7.f})) {
			m_showValidation = false;
			m_warnings.clear();
			m_destDir.clear();
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

void GamePackager::startPacking() {
	auto settings = makeSettings();
	settings.packFlags =
			(m_compress ? data::assets::pack::PackFlags::Compressed : data::assets::pack::PackFlags::None) |
			(m_obfuscate ? data::assets::pack::PackFlags::Obfuscated : data::assets::pack::PackFlags::None);
	m_destDir.clear();
	m_warnings.clear();
	auto preScanned = m_scannedAssets ? m_scannedAssets : mkShared<std::vector<data::assets::pack::AssetReference>>();
	m_scannedAssets.reset();

	const bool playInBrowser = m_target == data::assets::pack::ExportTarget::Web && m_runInBrowser;
	auto gameDirectory = mkShared<std::filesystem::path>();
	auto state = mkShared<AsyncProgressState>();
	mp_progress->open("Packing Game...", state, true);
	app::Application::get().getTaskScheduler().pushTask(core::task::Task(
			[state, exportSettings = std::move(settings), preScanned, gameDirectory]() -> void {
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
				*gameDirectory = result->gameDirectory;
				const auto seconds =
						std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime).count();
				std::string renderers{"Renderer2D"};
				for (const auto& name: result->renderers) renderers += ", " + name;
				state->setMessage(std::format("Packed {} assets ({:.2f} MiB)\nRenderers: {} ({} unused engine shader "
											  "files left out)\nOutput: {}\nDuration: {:.1f}s",
											  result->assetCount,
											  static_cast<double>(result->packBytes) / (1024.0 * 1024.0), renderers,
											  result->engineFilesSkipped, result->gameDirectory.string(), seconds));
			},
			[this, state, gameDirectory, playInBrowser]() -> void {
				if (playInBrowser && !gameDirectory->empty())
					playWebExport(*state, *gameDirectory);
				state->completed.store(true);
			}));
}

auto GamePackager::getRunners() -> RunnerStore& {
	if (!mp_runners)
		mp_runners = mkUniq<RunnerStore>(app::Application::get().getWorkingDirectory(),
										 platform::getUserDirectory() / "runners");
	return *mp_runners;
}

void GamePackager::renderTargetChoice() {
	using data::assets::pack::ExportTarget;
	struct Choice {
		ExportTarget target;
		const char* label;
	};
	static constexpr std::array<Choice, 6> g_choices{{
			{.target = ExportTarget::Host, .label = "This computer"},
			{.target = ExportTarget::LinuxX64, .label = "Linux (x64)"},
			{.target = ExportTarget::LinuxArm64, .label = "Linux (arm64)"},
			{.target = ExportTarget::WindowsX64, .label = "Windows (x64)"},
			{.target = ExportTarget::Android, .label = "Android (arm64)"},
			{.target = ExportTarget::Web, .label = "Web (browser)"},
	}};
	const auto host = data::assets::pack::getHostExportTarget();
	std::vector<std::string> names;
	std::vector<ExportTarget> targets;
	names.reserve(g_choices.size());
	targets.reserve(g_choices.size());
	int selected = 0;
	for (const auto& choice: g_choices) {
		if (choice.target == host)
			continue;
		if (choice.target == m_target)
			selected = static_cast<int>(targets.size());
		targets.push_back(choice.target);
		names.emplace_back(choice.target == ExportTarget::Host
								   ? std::format("{} ({})", choice.label, RunnerStore::getTargetName(host))
								   : choice.label);
	}
	std::vector<const char*> labels;
	labels.reserve(names.size());
	for (const auto& name: names) labels.push_back(name.c_str());
	ImGui::Text("Target platform:");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(-1);
	if (gui::widgets::combo("##packTarget", &selected, labels)) {
		m_target = targets[static_cast<size_t>(selected)];
		m_runnerMessage.clear();
	}
	if (m_target == ExportTarget::Android && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
		ImGui::SetTooltip("Writes the assets of the APK; the Android build of the runner makes the APK.");
	if (m_target == ExportTarget::Web) {
		gui::widgets::checkbox("Run in browser when done", &m_runInBrowser);
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay))
			ImGui::SetTooltip("Serve the game on 127.0.0.1 and open it in the default browser.");
	}
	renderRunnerState();
}

void GamePackager::renderRunnerState() {
	using data::assets::pack::ExportTarget;
	auto& runners = getRunners();
	const auto& theme = gui::UiLayer::getTheme();
	if (m_target == ExportTarget::Web && !hasRunner())
		ImGui::TextColored(gui::vec(theme.warning), "No Web runner in '%s'.",
						   runners.getRunnerDirectory(m_target).string().c_str());
	if (m_target == ExportTarget::Android || RunnerStore::isBundled(m_target)) {
		if (!m_runnerMessage.empty())
			ImGui::TextWrapped("%s", m_runnerMessage.c_str());
		return;
	}
	const std::string name{RunnerStore::getTargetName(m_target)};
	if (runners.isInstalled(m_target)) {
		ImGui::TextColored(gui::vec(theme.success), "%s %s runner ready.", gui::icon::check, name.c_str());
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
			ImGui::SetTooltip("%s", runners.getRunnerDirectory(m_target).string().c_str());
	} else if (const auto archive = runners.getArchive(m_target); archive) {
		ImGui::TextColored(gui::vec(theme.warning), "The %s runner is not on this computer (%.1f MB).", name.c_str(),
						   static_cast<double>(archive->size) / 1e6);
		if (gui::widgets::button("downloadRunner", {.glyph = gui::icon::download, .label = "Download"}))
			startRunnerDownload();
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
			ImGui::SetTooltip("%s", runners.getDownloadUrl(m_target, mp_settings->runnerServer).c_str());
		ImGui::SameLine();
		if (gui::widgets::button("importRunner", {.glyph = gui::icon::folderOpen, .label = "Import Runner..."}))
			importRunner();
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
			ImGui::SetTooltip("Install %s, downloaded by hand from the revision page of the site.",
							  archive->file.c_str());
	} else {
		ImGui::TextColored(gui::vec(theme.warning), "This Owl Nest lists no %s runner (no runners.json).",
						   name.c_str());
	}
	if (!m_runnerMessage.empty())
		ImGui::TextWrapped("%s", m_runnerMessage.c_str());
}

void GamePackager::startRunnerDownload() {
	if (mp_progress->isActive()) {
		OWL_CORE_WARN("GamePackager: Cannot download a runner while another operation runs.")
		return;
	}
	auto state = mkShared<AsyncProgressState>();
	auto result = mkShared<std::string>();
	mp_progress->open("Downloading Runner...", state, true);
	app::Application::get().getTaskScheduler().pushTask(core::task::Task(
			[this, state, result, target = m_target, server = mp_settings->runnerServer]() -> void {
				state->setMessage(std::format("Downloading the {} runner...", RunnerStore::getTargetName(target)));
				const auto downloaded = getRunners().download(
						target, server, [&state](const uint64_t iReceived, const uint64_t iTotal) -> bool {
							if (iTotal > 0)
								state->progress.store(static_cast<float>(iReceived) / static_cast<float>(iTotal));
							return !state->cancelRequested.load();
						});
				*result = downloaded ? std::format("{} runner downloaded.", RunnerStore::getTargetName(target))
									 : std::format("Download failed: {}.",
												   RunnerStore::getErrorMessage(downloaded.error()));
				if (downloaded)
					state->setMessage(*result);
				else
					state->setError(*result);
			},
			[this, state, result]() -> void {
				m_runnerMessage = *result;
				state->completed.store(true);
			}));
}

void GamePackager::importRunner() {
	const auto archive = platform::FileDialog::openFile("Owl Game Runner (*.tar.gz)|gz\n");
	if (archive.empty())
		return;
	const auto imported = getRunners().importArchive(archive);
	m_runnerMessage = imported ? std::format("{} runner imported.", RunnerStore::getTargetName(*imported))
							   : std::format("Import failed: {}.", RunnerStore::getErrorMessage(imported.error()));
	if (imported)
		m_target = *imported;
}

auto GamePackager::hasRunner() -> bool {
	using data::assets::pack::ExportTarget;
	if (m_target == ExportTarget::Web)
		return is_regular_file(getRunners().getRunnerDirectory(m_target) / "OwlGameRunner.html");
	return m_target == ExportTarget::Android || getRunners().isInstalled(m_target);
}

auto GamePackager::makeSettings() -> data::assets::pack::ExportSettings {
	auto settings = mp_project->makeExportSettings(m_destDir, getRunners().getRunnerDirectory(m_target));
	settings.target = m_target;
	return settings;
}

void GamePackager::playWebExport(AsyncProgressState& ioState, const std::filesystem::path& iGameDirectory) {
	if (!m_webPreview.start(iGameDirectory)) {
		ioState.setMessage(ioState.getMessage() + "\nThe browser preview could not start.");
		return;
	}
	ioState.setMessage(ioState.getMessage() + "\nPlaying at " + m_webPreview.getUrl());
	platform::openExternalUrl(m_webPreview.getUrl());
}

}// namespace owl::nest
