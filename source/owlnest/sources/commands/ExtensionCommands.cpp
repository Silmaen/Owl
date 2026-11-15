/**
 * @file ExtensionCommands.cpp
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "ExtensionCommands.h"

#include "CommandRegistry.h"

#include <app/Application.h>
#include <data/assets/pack/ExtensionManifest.h>
#include <data/assets/pack/ExtensionPack.h>
#include <data/assets/pack/ExtensionRegistry.h>
#include <platform/AtomicFile.h>
#include <platform/FileUtils.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace owl::nest::commands {

namespace {

using data::assets::pack::ExtensionDependency;
using data::assets::pack::ExtensionKind;
using data::assets::pack::ExtensionManifest;
using data::assets::pack::ExtensionVersion;
using data::assets::pack::g_extensionManifestEntry;

auto fail(const CommandError iError, std::string iMessage) -> unexpected<CommandFailure> {
	return unexpected<CommandFailure>{CommandFailure{.error = iError, .message = std::move(iMessage)}};
}

auto isPackFolder(const std::filesystem::path& iFolder) -> bool {
	std::error_code error;
	return std::filesystem::is_regular_file(iFolder / g_extensionManifestEntry, error);
}

// A pack folder as the commands receive it: absolute, or relative to an asset folder or to its `extensions` folder.
auto resolvePackFolder(const std::string& iPath, const std::filesystem::path& iProjectDirectory)
		-> std::optional<std::filesystem::path> {
	std::filesystem::path path{iPath};
	if (path.filename() == g_extensionManifestEntry)
		path = path.parent_path();
	if (path.empty())
		return std::nullopt;
	if (path.is_absolute())
		return isPackFolder(path) ? std::optional{path.lexically_normal()} : std::nullopt;
	std::vector<std::filesystem::path> roots;
	if (!iProjectDirectory.empty())
		roots.push_back(iProjectDirectory);
	if (app::Application::instanced())
		for (const auto& [title, assetsPath]: app::Application::get().getAssetDirectories())
			roots.push_back(assetsPath);
	for (const auto& root: roots) {
		for (const auto& candidate: {root / path, getExtensionsFolder(root) / path})
			if (isPackFolder(candidate))
				return candidate.lexically_normal();
	}
	return std::nullopt;
}

auto trim(std::string_view iText) -> std::string_view {
	const auto first = iText.find_first_not_of(" \t");
	if (first == std::string_view::npos)
		return {};
	const auto last = iText.find_last_not_of(" \t");
	return iText.substr(first, last - first + 1);
}

// `id@1.2.0, other` into dependencies; nothing when one of them is invalid.
auto parseDependencies(const std::string_view iText) -> std::optional<std::vector<ExtensionDependency>> {
	std::vector<ExtensionDependency> dependencies;
	size_t start = 0;
	while (start <= iText.size()) {
		const auto comma = iText.find(',', start);
		const auto item =
				trim(iText.substr(start, comma == std::string_view::npos ? std::string_view::npos : comma - start));
		if (!item.empty()) {
			ExtensionDependency dependency;
			const auto at = item.find('@');
			dependency.id = std::string{trim(item.substr(0, at))};
			if (at != std::string_view::npos) {
				const auto version = ExtensionVersion::parse(trim(item.substr(at + 1)));
				if (!version)
					return std::nullopt;
				dependency.minVersion = *version;
			}
			if (!data::assets::pack::isValidExtensionId(dependency.id))
				return std::nullopt;
			dependencies.push_back(std::move(dependency));
		}
		if (comma == std::string_view::npos)
			break;
		start = comma + 1;
	}
	return dependencies;
}

auto formatDependencies(const std::vector<ExtensionDependency>& iDependencies) -> std::string {
	std::string text;
	for (const auto& dependency: iDependencies)
		text += std::format("{}{}@{}", text.empty() ? "" : ", ", dependency.id, dependency.minVersion.toString());
	return text;
}

auto readManifestFile(const std::filesystem::path& iFolder)
		-> std::optional<std::pair<std::string, ExtensionManifest>> {
	auto text = platform::fileToString(iFolder / g_extensionManifestEntry);
	auto manifest = ExtensionManifest::parse(text, (iFolder / g_extensionManifestEntry).string());
	if (!manifest)
		return std::nullopt;
	return std::pair{std::move(text), std::move(*manifest)};
}

auto applyArgs(const CommandArgs& iArgs, ExtensionManifest& ioManifest) -> std::optional<std::string> {
	if (iArgs.has("name"))
		ioManifest.name = iArgs.getText("name");
	if (iArgs.has("author"))
		ioManifest.author = iArgs.getText("author");
	if (iArgs.has("description"))
		ioManifest.description = iArgs.getText("description");
	if (const auto order = iArgs.get<int64_t>("load_order"); order)
		ioManifest.loadOrder = static_cast<int32_t>(*order);
	if (iArgs.has("version")) {
		const auto version = ExtensionVersion::parse(iArgs.getText("version"));
		if (!version)
			return std::format("`{}` is no version (`major.minor.patch`)", iArgs.getText("version"));
		ioManifest.version = *version;
	}
	if (iArgs.has("kind"))
		ioManifest.kind = data::assets::pack::parseKind(iArgs.getText("kind")).value_or(ExtensionKind::Data);
	if (iArgs.has("dependencies")) {
		auto dependencies = parseDependencies(iArgs.getText("dependencies"));
		if (!dependencies)
			return std::format("`{}` is no list of `id@version`", iArgs.getText("dependencies"));
		if (std::ranges::any_of(*dependencies, [&ioManifest](const ExtensionDependency& iDependency) -> bool {
				return iDependency.id == ioManifest.id;
			}))
			return "a pack cannot depend on itself";
		ioManifest.dependencies = std::move(*dependencies);
	}
	return std::nullopt;
}

auto listExtensions(const CommandContext& iContext, const CommandArgs& /*iArgs*/)
		-> expected<CommandResult, CommandFailure> {
	if (iContext.projectDirectory.empty())
		return fail(CommandError::NotApplicable, "no project is open");
	std::string ids;
	std::string enabled;
	int64_t count = 0;
	for (const auto& source:
		 data::assets::pack::ExtensionRegistry::listSources(getExtensionsFolder(iContext.projectDirectory))) {
		const auto manifest = data::assets::pack::ExtensionPack::readManifest(source);
		if (!manifest)
			continue;
		ids += std::format("{}{}", ids.empty() ? "" : ",", manifest->id);
		if (iContext.disabledExtensions == nullptr ||
			!std::ranges::contains(*iContext.disabledExtensions, manifest->id))
			enabled += std::format("{}{}", enabled.empty() ? "" : ",", manifest->id);
		++count;
	}
	return CommandResult{.entity = core::UUID{0},
						 .description = std::format("{} extension pack(s)", count),
						 .data = {{"count", count}, {"ids", ids}, {"enabled", enabled}}};
}

auto createExtension(const CommandContext& iContext, const CommandArgs& iArgs)
		-> expected<CommandResult, CommandFailure> {
	if (iContext.projectDirectory.empty())
		return fail(CommandError::NotApplicable, "no project is open");
	ExtensionManifest manifest;
	manifest.id = iArgs.getText("id");
	if (!data::assets::pack::isValidExtensionId(manifest.id))
		return fail(CommandError::InvalidArgument,
					std::format("`{}` is no extension id (lower-case letters, digits, `.`, `_`, `-`)", manifest.id));
	if (const auto error = applyArgs(iArgs, manifest); error)
		return fail(CommandError::InvalidArgument, *error);
	const auto folder = getExtensionsFolder(iContext.projectDirectory) / manifest.id;
	std::error_code error;
	if (std::filesystem::exists(folder, error))
		return fail(CommandError::NotApplicable, std::format("`{}` already exists", folder.string()));
	std::filesystem::create_directories(folder, error);
	if (const auto written = platform::writeFileAtomic(folder / g_extensionManifestEntry, manifest.serialize());
		error || !written)
		return fail(CommandError::NotApplicable, std::format("cannot write `{}`", folder.string()));
	OWL_INFO("Extensions: Pack '{}' created in '{}'.", manifest.id, folder.string())
	if (iContext.onExtensionsChanged)
		iContext.onExtensionsChanged();
	return CommandResult{.entity = core::UUID{0},
						 .description = std::format("Create the extension pack `{}`", manifest.id),
						 .data = {{"path", folder.string()}}};
}

auto setManifest(const shared<scene::Scene>& /*ioScene*/, const CommandArgs& iArgs)
		-> expected<PreparedCommand, CommandFailure> {
	const auto folder = resolvePackFolder(iArgs.getText("path"), {});
	if (!folder)
		return fail(CommandError::InvalidArgument, std::format("`{}` holds no extension.yml", iArgs.getText("path")));
	const auto read = readManifestFile(*folder);
	if (!read)
		return fail(CommandError::NotApplicable, std::format("the manifest of `{}` cannot be read", folder->string()));
	auto manifest = read->second;
	if (const auto error = applyArgs(iArgs, manifest); error)
		return fail(CommandError::InvalidArgument, *error);
	if (manifest == read->second)
		return fail(CommandError::NotApplicable, "the manifest does not change");
	return PreparedCommand{.command = mkUniq<SetExtensionManifestCommand>(*folder / g_extensionManifestEntry,
																		  read->first, manifest.serialize()),
						   .applied = false,
						   .entity = core::UUID{0},
						   .data = {{"id", manifest.id}, {"dependencies", formatDependencies(manifest.dependencies)}}};
}

auto enableExtension(const CommandContext& iContext, const CommandArgs& iArgs)
		-> expected<CommandResult, CommandFailure> {
	if (iContext.disabledExtensions == nullptr)
		return fail(CommandError::NotApplicable, "no project is open");
	const auto folder = resolvePackFolder(iArgs.getText("path"), iContext.projectDirectory);
	if (!folder)
		return fail(CommandError::InvalidArgument, std::format("`{}` holds no extension.yml", iArgs.getText("path")));
	const auto manifest = data::assets::pack::ExtensionPack::readManifest(*folder);
	if (!manifest)
		return fail(CommandError::NotApplicable, std::format("the manifest of `{}` cannot be read", folder->string()));
	auto& disabled = *iContext.disabledExtensions;
	const bool enable = iArgs.getBool("enabled", true);
	if (const bool wasEnabled = !std::ranges::contains(disabled, manifest->id); enable == wasEnabled)
		return fail(CommandError::NotApplicable,
					std::format("`{}` is already {}", manifest->id, enable ? "enabled" : "disabled"));
	if (enable)
		std::erase(disabled, manifest->id);
	else
		disabled.push_back(manifest->id);
	if (iContext.onExtensionsChanged)
		iContext.onExtensionsChanged();
	return CommandResult{.entity = core::UUID{0},
						 .description =
								 std::format("{} the extension pack `{}`", enable ? "Enable" : "Disable", manifest->id),
						 .data = {{"id", manifest->id}, {"enabled", enable}}};
}

auto exportExtension(const CommandContext& iContext, const CommandArgs& iArgs)
		-> expected<CommandResult, CommandFailure> {
	const auto folder = resolvePackFolder(iArgs.getText("path"), iContext.projectDirectory);
	if (!folder)
		return fail(CommandError::InvalidArgument, std::format("`{}` holds no extension.yml", iArgs.getText("path")));
	const auto manifest = data::assets::pack::ExtensionPack::readManifest(*folder);
	if (!manifest)
		return fail(CommandError::NotApplicable, std::format("the manifest of `{}` cannot be read", folder->string()));
	std::filesystem::path output{iArgs.getText("output")};
	if (output.empty()) {
		if (iContext.projectDirectory.empty())
			return fail(CommandError::MissingArgument, "no project is open: give the `output` file");
		output = getExtensionExportFile(iContext.projectDirectory, manifest->id);
	} else if (output.is_relative() && !iContext.projectDirectory.empty()) {
		output = iContext.projectDirectory / output;
	}
	const auto written = data::assets::pack::writeExtensionPack(*folder, output);
	if (!written)
		return fail(CommandError::NotApplicable, std::format("`{}` not written: {}", output.string(),
															 data::assets::pack::describe(written.error())));
	return CommandResult{.entity = core::UUID{0},
						 .description = std::format("Export `{}` to `{}`", manifest->id, output.filename().string()),
						 .data = {{"path", output.string()}, {"entries", static_cast<int64_t>(*written)}}};
}

}// namespace

auto getEnabledExtensionSources(const std::filesystem::path& iProjectDirectory,
								const std::vector<std::string>& iDisabled) -> std::vector<std::filesystem::path> {
	if (iProjectDirectory.empty())
		return {};
	return data::assets::pack::ExtensionRegistry::listSources(getExtensionsFolder(iProjectDirectory), iDisabled);
}

auto getExtensionsFolder(const std::filesystem::path& iProjectDirectory) -> std::filesystem::path {
	return iProjectDirectory / data::assets::pack::g_projectExtensionsFolder;
}

auto getExtensionExportFile(const std::filesystem::path& iProjectDirectory, const std::string& iId)
		-> std::filesystem::path {
	return iProjectDirectory / "exports" / (iId + std::string{data::assets::pack::g_extensionPackExtension});
}

SetExtensionManifestCommand::SetExtensionManifestCommand(std::filesystem::path iFile, std::string iBefore,
														 std::string iAfter)
	: m_file{std::move(iFile)}, m_before{std::move(iBefore)}, m_after{std::move(iAfter)} {}

SetExtensionManifestCommand::~SetExtensionManifestCommand() = default;

void SetExtensionManifestCommand::undo(scene::Scene& /*ioScene*/) {
	if (const auto written = platform::writeFileAtomic(m_file, m_before); !written)
		OWL_WARN("Extensions: Cannot write '{}': {}.", m_file.string(), platform::describe(written.error()))
}

void SetExtensionManifestCommand::redo(scene::Scene& /*ioScene*/) {
	if (const auto written = platform::writeFileAtomic(m_file, m_after); !written)
		OWL_WARN("Extensions: Cannot write '{}': {}.", m_file.string(), platform::describe(written.error()))
}

auto SetExtensionManifestCommand::description() const -> std::string {
	return std::format("Edit the manifest of `{}`", m_file.parent_path().filename().string());
}

void addExtensionCommands(CommandRegistry& ioRegistry) {
	const ArgSpec pathSpec{.name = "path",
						   .type = ArgType::Text,
						   .required = true,
						   .description = "Extension pack folder: absolute, or relative to the project or to its "
										  "`extensions` folder.",
						   .projectPath = true};
	const std::vector<ArgSpec> fields{
			{.name = "name", .type = ArgType::Text, .description = "Display name."},
			{.name = "version", .type = ArgType::Text, .description = "Version, `major.minor.patch`."},
			{.name = "kind",
			 .type = ArgType::Enum,
			 .description = "What the pack adds: assets only, or assets and scripts.",
			 .choices = {"Data", "Script"}},
			{.name = "load_order",
			 .type = ArgType::Integer,
			 .description = "Rank among the packs that do not depend on each other: lower first, the later wins."},
			{.name = "author", .type = ArgType::Text, .description = "Author or studio."},
			{.name = "description", .type = ArgType::Text, .description = "Short description."},
			{.name = "dependencies",
			 .type = ArgType::Text,
			 .description = "Packs needed, `id@version` separated by commas (`id` alone for any version)."}};
	ioRegistry.add({.name = "extension.list",
					.description = "List the extension packs of the project (`<project>/extensions`).",
					.args = {},
					.factory = {},
					.run = listExtensions,
					.allowedInPlay = true,
					.needsScene = false});
	std::vector<ArgSpec> createArgs{{.name = "id",
									 .type = ArgType::Text,
									 .required = true,
									 .description = "Id of the pack, also the name of its folder."}};
	createArgs.insert(createArgs.end(), fields.begin(), fields.end());
	ioRegistry.add({.name = "extension.create",
					.description = "Create an extension pack in the project: a folder and its manifest.",
					.args = std::move(createArgs),
					.factory = {},
					.run = createExtension,
					.allowedInPlay = true,
					.needsScene = false});
	std::vector<ArgSpec> setArgs{pathSpec};
	setArgs.insert(setArgs.end(), fields.begin(), fields.end());
	ioRegistry.add({.name = "extension.set",
					.description = "Change fields of the manifest of an extension pack (undoable).",
					.args = std::move(setArgs),
					.factory = setManifest,
					.allowedInPlay = true});
	ioRegistry.add({.name = "extension.enable",
					.description = "Enable or disable an extension pack of the project: mounted as a plugin in Owl "
								   "Nest and embedded as a mod by Pack Game when enabled (saves the project, not an "
								   "undo entry).",
					.args = {pathSpec,
							 {.name = "enabled",
							  .type = ArgType::Bool,
							  .description = "True to enable the pack, false to disable it.",
							  .defaultValue = true}},
					.factory = {},
					.run = enableExtension,
					.allowedInPlay = true,
					.needsScene = false});
	ioRegistry.add({.name = "extension.export",
					.description = "Write an extension pack of the project as a `.owlmod`, the file a game loads as a "
								   "mod.",
					.args = {pathSpec,
							 {.name = "output",
							  .type = ArgType::Text,
							  .description = "File written (`exports/<id>.owlmod` of the project when absent).",
							  .projectPath = true}},
					.factory = {},
					.run = exportExtension,
					.allowedInPlay = true,
					.needsScene = false});
}

}// namespace owl::nest::commands
