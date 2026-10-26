/**
 * @file main.cpp
 * @author Silmaen
 * @date 24/11/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include <owlgui.h>

#include "EditorLayer.h"
#include "EditorSettings.h"
#include "Project.h"
#include <app/EntryPoint.h>
#include <data/assets/AssetRegistry.h>
#include <core/Core.h>
#include <gui/UiLayer.h>
#include <platform/AtomicFile.h>
#include <platform/FileUtils.h>

#include <cstdio>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace owl {

namespace {

// Command line of a headless export: `OwlNest --export <project> <output> [--cook-cache <dir>]`.
struct ExportCommand {
	// Project folder or `owl_project.yml` file.
	std::filesystem::path project;
	// Parent folder of the exported game.
	std::filesystem::path output;
	// Cook cache folder, empty for the one of the project.
	std::filesystem::path cookCache;
};

auto parseExportCommand(const int iArgc, char** iArgv) -> std::optional<ExportCommand> {
	const std::span args(iArgv, static_cast<size_t>(iArgc));
	for (size_t i = 1; i < args.size(); ++i) {
		if (std::string_view(args[i]) != "--export")
			continue;
		if (i + 2 >= args.size()) {
			std::fputs("Usage: OwlNest --export <project dir or owl_project.yml> <output dir> [--cook-cache <dir>]\n",
					   stderr);
			return ExportCommand{};
		}
		ExportCommand command{.project = std::filesystem::absolute(args[i + 1]),
							  .output = std::filesystem::absolute(args[i + 2]),
							  .cookCache = {}};
		for (size_t j = 1; j + 1 < args.size(); ++j)
			if (std::string_view(args[j]) == "--cook-cache")
				command.cookCache = std::filesystem::absolute(args[j + 1]);
		return command;
	}
	return std::nullopt;
}

// Project whose assets get sidecars and GUID references: `OwlNest --upgrade-assets <project dir>`.
auto parseUpgradeCommand(const int iArgc, char** iArgv) -> std::optional<std::filesystem::path> {
	const std::span args(iArgv, static_cast<size_t>(iArgc));
	for (size_t i = 1; i + 1 < args.size(); ++i) {
		if (std::string_view(args[i]) == "--upgrade-assets")
			return std::filesystem::absolute(args[i + 1]);
	}
	return std::nullopt;
}

// Project to open at start-up: `OwlNest --project <project dir or owl_project.yml>`.
auto parseProjectArgument(const int iArgc, char** iArgv) -> std::optional<std::filesystem::path> {
	const std::span args(iArgv, static_cast<size_t>(iArgc));
	for (size_t i = 1; i + 1 < args.size(); ++i) {
		if (std::string_view(args[i]) == "--project")
			return std::filesystem::absolute(args[i + 1]);
	}
	return std::nullopt;
}

auto executableDirectory(char** iArgv) -> std::filesystem::path {
	std::error_code ec;
	if (const auto self = std::filesystem::canonical("/proc/self/exe", ec); !ec)
		return self.parent_path();
	return std::filesystem::absolute(iArgv[0]).parent_path();
}

}// namespace

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wweak-vtables")
class OwlNest final : public app::Application {
public:
	OwlNest() = delete;
	explicit OwlNest(const app::AppParams& iParam) : Application(iParam) {
		if (getState() != State::Running)
			return;
		auto editor = mkShared<nest::EditorLayer>();
		if (const auto project = parseProjectArgument(iParam.argCount, iParam.args); project.has_value())
			editor->setStartupProject(*project);
		pushLayer(editor);
	}
};

// Headless Owl Nest: exports a project as a standalone game, then stops.
class OwlNestExport final : public app::Application {
public:
	OwlNestExport() = delete;
	OwlNestExport(const app::AppParams& iParam, const ExportCommand& iCommand, const std::filesystem::path& iRunnerDir)
		: Application(iParam) {
		setExitCode(exportProject(iCommand, iRunnerDir));
		close();
	}

private:
	auto exportProject(const ExportCommand& iCommand, const std::filesystem::path& iRunnerDir) -> int {
		if (getState() != State::Running)
			return 1;
		if (iCommand.project.empty() || iCommand.output.empty())
			return 2;
		auto projectFile = iCommand.project;
		if (is_directory(projectFile))
			projectFile /= "owl_project.yml";
		if (!exists(projectFile)) {
			OWL_ERROR("Export: Project file {} not found.", projectFile.string())
			return 1;
		}
		nest::Project project;
		if (!project.loadFromFile(projectFile)) {
			OWL_ERROR("Export: Unable to load project {}.", projectFile.string())
			return 1;
		}
		addAssetDirectory({"Project: " + project.name, project.projectDirectory});
		auto settings = project.makeExportSettings(iCommand.output, iRunnerDir);
		settings.cookCacheDirectory = iCommand.cookCache;
		std::vector<data::assets::pack::AssetReference> assets;
		for (const auto& warning: data::assets::pack::GameExporter::validate(settings, assets))
			OWL_WARN("Export: {}.", warning)
		const auto result = data::assets::pack::GameExporter::exportGame(settings, assets);
		if (!result) {
			OWL_ERROR("Export: Failed, {}.", data::assets::pack::GameExporter::getErrorMessage(result.error()))
			return 1;
		}
		OWL_INFO("Export: Game written to {}.", result->gameDirectory.string())
		return 0;
	}
};

// Headless Owl Nest: gives every asset of a project a sidecar, rewrites its documents by GUID, then stops.
class OwlNestUpgrade final : public app::Application {
public:
	OwlNestUpgrade() = delete;
	OwlNestUpgrade(const app::AppParams& iParam, const std::filesystem::path& iProject) : Application(iParam) {
		setExitCode(upgrade(iProject));
		close();
	}

private:
	auto upgrade(const std::filesystem::path& iProject) -> int {
		if (getState() != State::Running)
			return 1;
		if (!is_directory(iProject)) {
			OWL_ERROR("Upgrade: Project folder {} not found.", iProject.string())
			return 1;
		}
		addAssetDirectory({"Project", iProject}, true);
		const auto& registry = getAssetRegistry();
		int failed = 0;
		size_t files = 0;
		for (const auto& entry: registry.getEntries()) {
			if (entry.meta.importer != data::assets::Importer::Document)
				continue;
			const auto file = entry.root / entry.path;
			const auto text = platform::fileToString(file);
			const auto rewritten = registry.rewriteReferences(text);
			if (rewritten == text)
				continue;
			if (const auto written = platform::writeFileAtomic(file, rewritten); !written) {
				OWL_ERROR("Upgrade: Cannot write {}: {}.", file.string(), platform::describe(written.error()))
				++failed;
				continue;
			}
			++files;
		}
		OWL_INFO("Upgrade: {} asset(s) with a GUID, {} document(s) rewritten.", registry.size(), files)
		return failed == 0 ? 0 : 1;
	}
};
OWL_DIAG_POP

auto app::createApplication(int iArgc, char** iArgv) -> shared<Application> {
	if (const auto project = parseUpgradeCommand(iArgc, iArgv); project) {
		std::filesystem::current_path(executableDirectory(iArgv));
		return mkShared<OwlNestUpgrade>(
				AppParams{
						.args = iArgv,
						.name = "Owl Nest - Upgrade",
#ifdef OWL_ASSETS_LOCATION
						.assetsPattern = OWL_ASSETS_LOCATION,
#endif
						.argCount = iArgc,
						.renderer = renderer::gpu::RenderAPI::Type::Null,
						.sound = sound::SoundAPI::Type::Null,
						.hasGui = false,
						.isDummy = true,
				},
				*project);
	}
	if (const auto command = parseExportCommand(iArgc, iArgv); command) {
		// Engine and editor assets are searched from the working directory: start from the executable's.
		const auto exeDir = executableDirectory(iArgv);
		std::filesystem::current_path(exeDir);
		return mkShared<OwlNestExport>(
				AppParams{
						.args = iArgv,
						.name = "Owl Nest - Export",
#ifdef OWL_ASSETS_LOCATION
						.assetsPattern = OWL_ASSETS_LOCATION,
#endif
						.argCount = iArgc,
						.renderer = renderer::gpu::RenderAPI::Type::Null,
						.sound = sound::SoundAPI::Type::Null,
						.hasGui = false,
						.isDummy = true,
				},
				*command, exeDir);
	}
	nest::EditorSettings preSettings;
	if (const auto settingsFile = std::filesystem::current_path() / "OwlNest_settings.yml"; exists(settingsFile))
		preSettings.loadFromFile(settingsFile);
	gui::UiLayer::setUiFontSize(static_cast<float>(preSettings.uiFontSize));
	gui::UiLayer::setCodeFontSize(static_cast<float>(preSettings.codeEditorFontSize));
	return mkShared<OwlNest>(AppParams{
			.args = iArgv,
			.name = "Owl Nest " + getVersionString() + " - Owl Engine Editor",
#ifdef OWL_ASSETS_LOCATION
			.assetsPattern = OWL_ASSETS_LOCATION,
#endif
			.icon = "icons/logo_owl_icon.png",
			.argCount = iArgc,
			// Fixed: the title carries the version, and a desktop names its icon after the identifier.
			.appId = "owl-nest-owl-engine-editor",
			.hotReload = true,
	});
}

}// namespace owl
