/**
 * @file GamePackager.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "GamePackager.h"

#include "signing/AuthenticodeSigner.h"

#include <data/assets/pack/GameExporter.h>
#include <data/assets/pack/PackWriter.h>
#include <gui/IconBank.h>
#include <gui/IconFont.h>
#include <gui/UiLayer.h>
#include <gui/utils.h>
#include <gui/widgets/Combo.h>
#include <gui/widgets/IconButton.h>
#include <gui/widgets/PropertyGrid.h>
#include <gui/widgets/SectionHeader.h>
#include <gui/widgets/Toggle.h>

#include <imgui.h>
#include <imgui_stdlib.h>
#include <platform/FileDialog.h>
#include <platform/UserDirectory.h>
#include <platform/openExternalUrl.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <utility>
#include <vector>

namespace owl::nest {

namespace {

auto joinNames(const std::vector<std::string>& iNames, const std::string_view iSeparator) -> std::string {
	std::string joined;
	for (const auto& name: iNames) {
		if (!joined.empty())
			joined += iSeparator;
		joined += name;
	}
	return joined;
}

}// namespace

auto getExportReportRows(const data::assets::pack::ExportReport& iReport) -> std::vector<ExportReportRow> {
	using data::assets::pack::formatByteSize;
	std::vector<ExportReportRow> rows;
	rows.push_back({.label = "Folder", .value = iReport.gameDirectory.string(), .tooltip = {}});
	rows.push_back({.label = "Platform", .value = iReport.platform, .tooltip = {}});
	rows.push_back({.label = "Total Size",
					.value = formatByteSize(iReport.sizes.getTotal()),
					.tooltip = "Every file of the game folder."});
	rows.push_back({.label = "Pack",
					.value = formatByteSize(iReport.sizes.pack),
					.tooltip = "The .owlpack of the game and the .owlmod of its mods."});
	rows.push_back({.label = "Runner",
					.value = formatByteSize(iReport.sizes.runner),
					.tooltip = "The runner executable, or the page, script and module of the Web runner."});
	rows.push_back({.label = "Libraries",
					.value = formatByteSize(iReport.sizes.libraries),
					.tooltip = "The shared libraries of the runner."});
	rows.push_back({.label = "Other Files",
					.value = formatByteSize(iReport.sizes.other),
					.tooltip = "Configuration, icon, launcher and report."});
	rows.push_back({.label = "Assets",
					.value = std::format("{} packed ({} cooked, {} from the cache)", iReport.assetCount,
										 iReport.cookedAssets, iReport.cacheHits),
					.tooltip = {}});
	std::vector<std::string> renderers{"Renderer2D"};
	renderers.insert(renderers.end(), iReport.renderers.begin(), iReport.renderers.end());
	rows.push_back({.label = "Renderers",
					.value = joinNames(renderers, ", "),
					.tooltip = "The renderer families the game initialises."});
	rows.push_back({.label = "Shaders Baked",
					.value = iReport.shaderTargets.empty() ? std::to_string(iReport.shadersBaked)
														   : std::format("{} for {}", iReport.shadersBaked,
																		 joinNames(iReport.shaderTargets, ", ")),
					.tooltip = "Project shaders compiled by this export; the engine shaders ship baked."});
	rows.push_back({.label = "Engine Files Left Out",
					.value = std::to_string(iReport.engineFilesSkipped),
					.tooltip = "Shaders of the renderers the game does not use."});
	if (!iReport.mods.empty()) {
		std::vector<std::string> mods;
		mods.reserve(iReport.mods.size());
		for (const auto& mod: iReport.mods) mods.push_back(mod.filename().string());
		rows.push_back({.label = "Mods", .value = joinNames(mods, ", "), .tooltip = {}});
	}
	return rows;
}

void GamePackager::onRender() {
	renderWizardModal();
	renderValidationModal();
	renderReportModal();
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
		renderSigningState();

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
	auto validationWarnings = std::move(m_warnings);
	m_warnings.clear();
	auto preScanned = m_scannedAssets ? m_scannedAssets : mkShared<std::vector<data::assets::pack::AssetReference>>();
	m_scannedAssets.reset();

	const bool playInBrowser = m_target == data::assets::pack::ExportTarget::Web && m_runInBrowser;
	const bool makeApk = m_target == data::assets::pack::ExportTarget::Android;
	const Project project = *mp_project;
	const ExportSigning signing{.windowsPfx = mp_settings->signWindowsPfx,
								.windowsPassword = std::move(m_windowsPassword),
								.windowsCommand = mp_settings->signWindowsCommand,
								.androidKeystore = mp_settings->signAndroidKeystore,
								.androidPassword = std::move(m_androidPassword)};
	m_windowsPassword.clear();
	m_androidPassword.clear();
	const bool signWindows = m_target == data::assets::pack::ExportTarget::WindowsX64;
	auto exported = mkShared<std::optional<data::assets::pack::ExportReport>>();
	auto apkNote = mkShared<std::string>();
	auto state = mkShared<AsyncProgressState>();
	mp_progress->open("Packing Game...", state, true);
	app::Application::get().getTaskScheduler().pushTask(core::task::Task(
			[state, exportSettings = std::move(settings), preScanned, exported, apkNote, makeApk, signWindows, project,
			 signing, warnings = std::move(validationWarnings)]() -> void {
				auto result = data::assets::pack::GameExporter::exportGame(
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
				if (!warnings.empty()) {
					result->addWarnings(warnings);
					std::ignore = result->write();
				}
				if (signWindows) {
					state->setMessage("Signing the executable...");
					const auto signedGame = signWindowsGame(signing, result->executable);
					if (!signedGame) {
						state->setError(std::format("Signing failed: {}.", signedGame.error()));
						return;
					}
					*apkNote = *signedGame;
				}
				if (makeApk) {
					state->setMessage("Writing the APK...");
					const auto apk =
							writeAndroidApk(project, result->gameDirectory, exportSettings.runnerDirectory, signing);
					if (!apk) {
						state->setError(std::format("Packaging failed: {}.", android::getErrorMessage(apk.error())));
						return;
					}
					*apkNote = "APK: " + apk->string();
				}
				*exported = std::move(*result);
			},
			[this, state, exported, apkNote, playInBrowser]() -> void {
				state->completed.store(true);
				if (!exported->has_value())
					return;
				mp_progress->close();
				m_reportNote = playInBrowser ? playWebExport((*exported)->gameDirectory) : std::move(*apkNote);
				m_report = std::move(*exported);
				m_showReport = true;
			}));
}

void GamePackager::renderReportModal() {
	if (!m_showReport || !m_report.has_value())
		return;
	if (!ImGui::IsPopupOpen("Export Report"))
		ImGui::OpenPopup("Export Report");
	const auto* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(ImGui::GetFontSize() * 36.f, 0), ImGuiCond_Always);
	if (!ImGui::BeginPopupModal("Export Report", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
		return;
	const auto& theme = gui::UiLayer::getTheme();
	const auto& report = *m_report;
	if (gui::widgets::sectionHeader("Summary", gui::icon::packageCheck, false).open) {
		if (gui::widgets::beginProperties("##exportSummary")) {
			for (const auto& row: getExportReportRows(report)) {
				gui::widgets::propertyLabel(row.label.c_str(), row.tooltip.empty() ? nullptr : row.tooltip.c_str());
				ImGui::TextUnformatted(row.value.c_str());
				std::ignore = gui::widgets::propertyReset(false);
			}
			gui::widgets::endProperties();
		}
		gui::widgets::endSection();
	}
	const auto listSection = [](const std::string& iTitle, const char* iIcon, const char* iId,
								const std::vector<std::string>& iLines, const char* iEmpty,
								const std::optional<math::vec4>& iColor) -> void {
		if (!gui::widgets::sectionHeader(iTitle.c_str(), iIcon, false, !iLines.empty()).open)
			return;
		if (iLines.empty()) {
			ImGui::TextDisabled("%s", iEmpty);
		} else {
			const float height =
					ImGui::GetTextLineHeightWithSpacing() * static_cast<float>(std::min<size_t>(iLines.size(), 8)) +
					ImGui::GetStyle().WindowPadding.y * 2.f;
			ImGui::BeginChild(iId, ImVec2(0, height), ImGuiChildFlags_Borders);
			for (const auto& line: iLines) {
				if (iColor.has_value())
					ImGui::TextColored(gui::vec(*iColor), "%s", line.c_str());
				else
					ImGui::TextUnformatted(line.c_str());
			}
			ImGui::EndChild();
		}
		gui::widgets::endSection();
	};
	listSection(std::format("Unused Assets ({})", report.unusedAssets.size()), gui::icon::folderSearch,
				"##exportUnused", report.unusedAssets, "Every asset of the project is packed.", std::nullopt);
	listSection(std::format("Warnings ({})", report.warnings.size()), gui::icon::triangleAlert, "##exportWarnings",
				report.warnings, "No warning.", theme.warning);
	if (!m_reportNote.empty())
		ImGui::TextWrapped("%s", m_reportNote.c_str());
	const std::string reportFile{data::assets::pack::g_exportReportFile};
	ImGui::TextDisabled("Written to %s.", reportFile.c_str());
	ImGui::Spacing();
	if (gui::widgets::button("Close##exportReport",
							 {.glyph = gui::icon::x, .label = "Close", .width = ImGui::GetFontSize() * 7.f})) {
		m_showReport = false;
		m_report.reset();
		m_reportNote.clear();
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndPopup();
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
		ImGui::SetTooltip("Writes the APK of the game, signed with the keystore of the settings or the debug key.");
	if (m_target == ExportTarget::Web) {
		gui::widgets::checkbox("Run in browser when done", &m_runInBrowser);
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay))
			ImGui::SetTooltip("Serve the game on 127.0.0.1 and open it in the default browser.");
	}
	renderRunnerState();
}

void GamePackager::renderSigningState() {
	using data::assets::pack::ExportTarget;
	const auto& theme = gui::UiLayer::getTheme();
	const bool windows = m_target == ExportTarget::WindowsX64;
	const auto& file = windows ? mp_settings->signWindowsPfx : mp_settings->signAndroidKeystore;
	if (!windows && m_target != ExportTarget::Android)
		return;
	if (file.empty()) {
		const bool unsignedWindows = windows && mp_settings->signWindowsCommand.empty();
		const char* text = "Signed with the debug key: set a keystore in the Settings for a release.";
		if (unsignedWindows)
			text = "Unsigned: set a certificate or a command in the Settings.";
		else if (windows)
			text = "Signed by the command of the Settings.";
		ImGui::TextColored(gui::vec(unsignedWindows ? theme.warning : theme.textMuted), "%s", text);
		return;
	}
	ImGui::Text("%s password:", windows ? "Certificate" : "Keystore");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(-1);
	ImGui::InputText(windows ? "##windowsPassword" : "##androidPassword",
					 windows ? &m_windowsPassword : &m_androidPassword, ImGuiInputTextFlags_Password);
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
		ImGui::SetTooltip("%s\nThe password is kept for this export only.", file.c_str());
}

void GamePackager::renderRunnerState() {
	using data::assets::pack::ExportTarget;
	auto& runners = getRunners();
	const auto& theme = gui::UiLayer::getTheme();
	if (m_target == ExportTarget::Web && !hasRunner())
		ImGui::TextColored(gui::vec(theme.warning), "No Web runner in '%s'.",
						   runners.getRunnerDirectory(m_target).string().c_str());
	if (RunnerStore::isBundled(m_target)) {
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
	return getRunners().isInstalled(m_target);
}

auto GamePackager::makeSettings() -> data::assets::pack::ExportSettings {
	auto settings = mp_project->makeExportSettings(m_destDir, getRunners().getRunnerDirectory(m_target));
	settings.target = m_target;
	return settings;
}

auto GamePackager::signWindowsGame(const ExportSigning& iSigning, const std::filesystem::path& iExecutable)
		-> expected<std::string, std::string> {
	if (!iSigning.windowsPfx.empty()) {
		const auto key = android::SigningKey::load(iSigning.windowsPfx, iSigning.windowsPassword);
		if (!key)
			return unexpected{
					std::format("the certificate '{}' cannot be opened (file or password)", iSigning.windowsPfx)};
		if (const auto signedFile = signing::signExecutable(iExecutable, *key); !signedFile)
			return unexpected{std::string{signing::getErrorMessage(signedFile.error())}};
		return "Signed with the certificate " + std::filesystem::path{iSigning.windowsPfx}.filename().string();
	}
	if (iSigning.windowsCommand.empty())
		return std::string{};
	if (const auto signedFile = signing::signExecutableWithCommand(iExecutable, iSigning.windowsCommand); !signedFile)
		return unexpected{std::string{signing::getErrorMessage(signedFile.error())}};
	return std::string{"Signed by the signing command"};
}

auto GamePackager::writeAndroidApk(const Project& iProject, const std::filesystem::path& iGameDirectory,
								   const std::filesystem::path& iRunnerDirectory, const ExportSigning& iSigning)
		-> expected<std::filesystem::path, android::ApkError> {
	const bool release = !iSigning.androidKeystore.empty();
	const auto key =
			release ? android::SigningKey::load(iSigning.androidKeystore, iSigning.androidPassword)
					: android::SigningKey::loadOrCreateDebug(platform::getUserDirectory() / "android" / "debug.p12");
	if (!key)
		return unexpected{android::ApkError::Signing};
	const auto version = iProject.getGameVersion();
	const android::ApkManifest manifest{.package = iProject.getAndroidPackage(),
										.label = iProject.name,
										.versionName = version,
										.versionCode = android::getVersionCode(version),
										.minSdk = 33,
										.targetSdk = 36,
										.debuggable = !release,
										.hasIcon = false};
	// The export copies the icon into the game folder, at the path `runner.yml` names.
	const auto icon = iProject.icon.empty() || std::filesystem::path{iProject.icon}.extension() != ".png"
							  ? std::filesystem::path{}
							  : iGameDirectory / iProject.icon;
	auto output = iGameDirectory.parent_path() / (iGameDirectory.filename().string() + ".apk");
	if (const auto written = android::writeApk(iGameDirectory, iRunnerDirectory, icon, manifest, *key, output);
		!written)
		return unexpected{written.error()};
	return output;
}

auto GamePackager::playWebExport(const std::filesystem::path& iGameDirectory) -> std::string {
	if (!m_webPreview.start(iGameDirectory))
		return "The browser preview could not start.";
	platform::openExternalUrl(m_webPreview.getUrl());
	return "Playing at " + m_webPreview.getUrl();
}

}// namespace owl::nest
