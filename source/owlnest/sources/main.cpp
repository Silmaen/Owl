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
#include <gui/UiLayer.h>

#include <cstdio>
#include <filesystem>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace owl {

namespace {

// Command line of a headless export: `OwlNest --export <project> <output>`.
struct ExportCommand {
	// Project folder or `owl_project.yml` file.
	std::filesystem::path project;
	// Parent folder of the exported game.
	std::filesystem::path output;
};

auto parseExportCommand(const int iArgc, char** iArgv) -> std::optional<ExportCommand> {
	const std::span args(iArgv, static_cast<size_t>(iArgc));
	for (size_t i = 1; i < args.size(); ++i) {
		if (std::string_view(args[i]) != "--export")
			continue;
		if (i + 2 >= args.size()) {
			std::fputs("Usage: OwlNest --export <project dir or owl_project.yml> <output dir>\n", stderr);
			return ExportCommand{};
		}
		return ExportCommand{.project = std::filesystem::absolute(args[i + 1]),
							 .output = std::filesystem::absolute(args[i + 2])};
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
		if (getState() == State::Running)
			pushLayer(mkShared<nest::EditorLayer>());
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
		const auto settings = project.makeExportSettings(iCommand.output, iRunnerDir);
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
OWL_DIAG_POP

auto app::createApplication(int iArgc, char** iArgv) -> shared<Application> {
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
			.name = "Owl Nest - Owl Engine Editor",
#ifdef OWL_ASSETS_LOCATION
			.assetsPattern = OWL_ASSETS_LOCATION,
#endif
			.icon = "icons/logo_owl_icon.png",
			.argCount = iArgc,
			.hotReload = true,
	});
}

}// namespace owl
