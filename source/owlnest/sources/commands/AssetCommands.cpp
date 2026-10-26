/**
 * @file AssetCommands.cpp
 * @author Silmaen
 * @date 26/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "AssetCommands.h"

#include "CommandRegistry.h"

#include <app/Application.h>
#include <data/assets/AssetRegistry.h>
#include <data/assets/cook/CookCache.h>
#include <platform/AtomicFile.h>
#include <platform/FileUtils.h>
#include <renderer/Renderer.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace owl::nest::commands {

namespace {

using data::assets::AssetMeta;
using data::assets::Importer;
using data::assets::TextureCompression;

auto fail(const CommandError iError, std::string iMessage) -> unexpected<CommandFailure> {
	return unexpected<CommandFailure>{CommandFailure{.error = iError, .message = std::move(iMessage)}};
}

auto getRegistry() -> data::assets::AssetRegistry& { return app::Application::get().getAssetRegistry(); }

// An asset path as the commands receive it: absolute, or relative to an asset directory (editable ones first).
auto resolvePath(const std::string& iPath) -> std::optional<std::filesystem::path> {
	const std::filesystem::path path{iPath};
	std::error_code error;
	if (path.is_absolute())
		return std::filesystem::exists(path, error) ? std::optional{path.lexically_normal()} : std::nullopt;
	auto roots = getRegistry().getEditableDirectories();
	for (const auto& [title, assetsPath]: app::Application::get().getAssetDirectories()) roots.push_back(assetsPath);
	for (const auto& root: roots)
		if (const auto full = (root / path).lexically_normal(); std::filesystem::exists(full, error))
			return full;
	return std::nullopt;
}

auto isUnder(const std::filesystem::path& iFile, const std::filesystem::path& iFolder) -> bool {
	const auto relative = iFile.lexically_normal().lexically_relative(iFolder.lexically_normal());
	return !relative.empty() && *relative.begin() != "..";
}

// Registered assets of a file, or of every file inside a folder.
auto entriesOf(const std::filesystem::path& iPath) -> std::vector<data::assets::AssetRegistry::Entry> {
	std::vector<data::assets::AssetRegistry::Entry> entries;
	std::error_code error;
	const bool folder = std::filesystem::is_directory(iPath, error);
	for (auto& entry: getRegistry().getEntries()) {
		if (entry.root.empty())
			continue;
		const auto file = (entry.root / entry.path).lexically_normal();
		if (folder ? isUnder(file, iPath) : file == iPath.lexically_normal())
			entries.push_back(std::move(entry));
	}
	std::ranges::sort(entries, {}, &data::assets::AssetRegistry::Entry::path);
	return entries;
}

auto compressionOf(const std::string& iName) -> TextureCompression {
	if (iName == "Uastc")
		return TextureCompression::Uastc;
	if (iName == "Etc1s")
		return TextureCompression::Etc1s;
	return TextureCompression::None;
}

auto cacheOf(const std::filesystem::path& iRoot) -> std::filesystem::path {
	return iRoot / ".owl" / "cache" / "cooked";
}

auto setImportSettings(const shared<scene::Scene>& /*ioScene*/, const CommandArgs& iArgs)
		-> expected<PreparedCommand, CommandFailure> {
	const auto path = resolvePath(iArgs.getText("path"));
	if (!path)
		return fail(CommandError::InvalidArgument,
					std::format("asset.set_import: `{}` not found", iArgs.getText("path")));
	std::vector<AssetMeta> before;
	std::vector<AssetMeta> after;
	for (const auto& entry: entriesOf(*path)) {
		if (entry.meta.importer != Importer::Texture)
			continue;
		auto changed = entry.meta;
		changed.texture.nearest = iArgs.getBool("nearest", changed.texture.nearest);
		changed.texture.mips = iArgs.getBool("mips", changed.texture.mips);
		if (iArgs.has("compression"))
			changed.texture.compression = compressionOf(iArgs.getText("compression"));
		if (changed == entry.meta)
			continue;
		before.push_back(entry.meta);
		after.push_back(changed);
	}
	if (after.empty())
		return fail(CommandError::NotApplicable,
					std::format("asset.set_import: no texture with a sidecar changes under `{}`", path->string()));
	const auto count = static_cast<int64_t>(after.size());
	return PreparedCommand{.command = mkUniq<SetImportSettingsCommand>(std::move(before), std::move(after)),
						   .applied = false,
						   .entity = core::UUID{0},
						   .data = {{"assets", count}}};
}

auto renameAsset(const CommandContext& /*iContext*/, const CommandArgs& iArgs)
		-> expected<CommandResult, CommandFailure> {
	const auto path = resolvePath(iArgs.getText("path"));
	const auto name = iArgs.getText("name");
	if (!path)
		return fail(CommandError::InvalidArgument, std::format("asset.rename: `{}` not found", iArgs.getText("path")));
	if (name.empty() || name.find_first_of("/\\") != std::string::npos)
		return fail(CommandError::InvalidArgument, std::format("asset.rename: `{}` is no file name", name));
	const auto target = path->parent_path() / name;
	if (std::error_code error; std::filesystem::exists(target, error))
		return fail(CommandError::NotApplicable, std::format("asset.rename: `{}` already exists", target.string()));
	if (!getRegistry().moveFile(*path, target))
		return fail(CommandError::NotApplicable, std::format("asset.rename: cannot rename `{}`", path->string()));
	return CommandResult{.entity = core::UUID{0},
						 .description = std::format("Rename {} to {}", path->filename().string(), name),
						 .data = {{"path", target.string()}}};
}

auto moveAsset(const CommandContext& /*iContext*/, const CommandArgs& iArgs)
		-> expected<CommandResult, CommandFailure> {
	const auto path = resolvePath(iArgs.getText("path"));
	const auto folder = resolvePath(iArgs.getText("folder"));
	if (!path || !folder)
		return fail(CommandError::InvalidArgument, "asset.move: the file or the folder is not found");
	const auto target = *folder / path->filename();
	if (target == *path || isUnder(*folder, *path))
		return fail(CommandError::NotApplicable, "asset.move: a folder cannot move into itself");
	if (std::error_code error; std::filesystem::exists(target, error))
		return fail(CommandError::NotApplicable, std::format("asset.move: `{}` already exists", target.string()));
	if (!getRegistry().moveFile(*path, target))
		return fail(CommandError::NotApplicable, std::format("asset.move: cannot move `{}`", path->string()));
	return CommandResult{.entity = core::UUID{0},
						 .description = std::format("Move {} to {}", path->filename().string(), folder->string()),
						 .data = {{"path", target.string()}}};
}

auto deleteAsset(const CommandContext& /*iContext*/, const CommandArgs& iArgs)
		-> expected<CommandResult, CommandFailure> {
	const auto path = resolvePath(iArgs.getText("path"));
	if (!path)
		return fail(CommandError::InvalidArgument, std::format("asset.delete: `{}` not found", iArgs.getText("path")));
	if (!getRegistry().deleteFile(*path))
		return fail(CommandError::NotApplicable, std::format("asset.delete: cannot delete `{}`", path->string()));
	return CommandResult{.entity = core::UUID{0},
						 .description = std::format("Delete {}", path->filename().string()),
						 .data = {}};
}

auto cookAssets(const CommandContext& /*iContext*/, const CommandArgs& iArgs)
		-> expected<CommandResult, CommandFailure> {
	const auto path = resolvePath(iArgs.getText("path"));
	if (!path)
		return fail(CommandError::InvalidArgument, std::format("asset.cook: `{}` not found", iArgs.getText("path")));
	const auto entries = entriesOf(*path);
	if (entries.empty())
		return fail(CommandError::NotApplicable,
					std::format("asset.cook: no asset with a sidecar in `{}`", path->string()));
	int64_t failed = 0;
	int64_t cooked = 0;
	int64_t hits = 0;
	for (const auto& entry: entries) {
		data::assets::cook::CookCache cache(cacheOf(entry.root));
		if (!cache.cook(entry.root / entry.path, entry.meta, entry.path))
			++failed;
		cooked += static_cast<int64_t>(cache.getStats().cooked);
		hits += static_cast<int64_t>(cache.getStats().hits);
	}
	if (failed > 0)
		return fail(CommandError::NotApplicable, std::format("asset.cook: {} asset(s) failed, see the log", failed));
	return CommandResult{.entity = core::UUID{0},
						 .description = std::format("Cook {} asset(s), {} from the cache", cooked + hits, hits),
						 .data = {{"cooked", cooked}, {"cached", hits}}};
}

auto clearCookCache(const CommandContext& /*iContext*/, const CommandArgs& /*iArgs*/)
		-> expected<CommandResult, CommandFailure> {
	const auto roots = getRegistry().getEditableDirectories();
	if (roots.empty())
		return fail(CommandError::NotApplicable, "asset.clear_cache: no project is open");
	for (const auto& root: roots) data::assets::cook::CookCache(cacheOf(root)).clear();
	return CommandResult{.entity = core::UUID{0}, .description = "Clear the cook cache", .data = {}};
}

auto upgradeReferences(const CommandContext& /*iContext*/, const CommandArgs& /*iArgs*/)
		-> expected<CommandResult, CommandFailure> {
	auto& registry = getRegistry();
	const auto roots = registry.getEditableDirectories();
	if (roots.empty())
		return fail(CommandError::NotApplicable, "asset.upgrade_references: no project is open");
	int64_t files = 0;
	int64_t failed = 0;
	for (const auto& entry: registry.getEntries()) {
		if (entry.meta.importer != Importer::Document || std::ranges::find(roots, entry.root) == roots.end())
			continue;
		const auto file = entry.root / entry.path;
		const auto text = platform::fileToString(file);
		const auto rewritten = registry.rewriteReferences(text);
		if (rewritten == text)
			continue;
		if (const auto written = platform::writeFileAtomic(file, rewritten); !written) {
			OWL_WARN("asset.upgrade_references: Cannot write '{}': {}.", file.string(),
					 platform::describe(written.error()))
			++failed;
			continue;
		}
		++files;
	}
	if (failed > 0)
		return fail(CommandError::NotApplicable,
					std::format("asset.upgrade_references: {} file(s) could not be written, see the log", failed));
	return CommandResult{.entity = core::UUID{0},
						 .description = std::format("Reference assets by GUID in {} file(s)", files),
						 .data = {{"files", files}}};
}

}// namespace

SetImportSettingsCommand::SetImportSettingsCommand(std::vector<AssetMeta> iBefore, std::vector<AssetMeta> iAfter)
	: m_before{std::move(iBefore)}, m_after{std::move(iAfter)} {}

SetImportSettingsCommand::~SetImportSettingsCommand() = default;

void SetImportSettingsCommand::undo(scene::Scene& /*ioScene*/) { apply(m_before); }

void SetImportSettingsCommand::redo(scene::Scene& /*ioScene*/) { apply(m_after); }

auto SetImportSettingsCommand::description() const -> std::string {
	return std::format("Change the import settings of {} asset(s)", m_after.size());
}

void SetImportSettingsCommand::apply(const std::vector<AssetMeta>& iMetas) {
	if (!app::Application::instanced())
		return;
	auto& registry = getRegistry();
	auto& library = renderer::Renderer::getTextureLibrary();
	for (const auto& meta: iMetas) {
		if (!registry.setMeta(meta))
			continue;
		const auto entry = registry.find(meta.guid);
		if (!entry)
			continue;
		const auto name = "nam:" + entry->path;
		const auto mode = meta.texture.nearest ? renderer::gpu::FilterMode::Nearest : renderer::gpu::FilterMode::Linear;
		library.forEach(
				[&name, mode](const std::string& iName, const shared<renderer::gpu::Texture2D>& iTexture) -> void {
					if (iTexture && iName == name)
						iTexture->setFilterMode(mode);
				});
	}
}

void addAssetCommands(CommandRegistry& ioRegistry) {
	const ArgSpec pathSpec{.name = "path",
						   .type = ArgType::Text,
						   .required = true,
						   .description = "Asset file or folder, absolute or relative to an asset folder.",
						   .projectPath = true};
	ioRegistry.add(
			{.name = "asset.set_import",
			 .description = "Change the import settings of a texture, or of every texture of a folder (undoable).",
			 .args = {pathSpec,
					  {.name = "nearest", .type = ArgType::Bool, .description = "Nearest-texel filtering (pixel art)."},
					  {.name = "compression",
					   .type = ArgType::Enum,
					   .description = "Storage in a pack: the source file, or Basis Universal KTX2.",
					   .choices = {"None", "Uastc", "Etc1s"}},
					  {.name = "mips",
					   .type = ArgType::Bool,
					   .description = "Mipmap chain built by the cook (compressed textures)."}},
			 .factory = setImportSettings,
			 .allowedInPlay = true});
	ioRegistry.add({.name = "asset.rename",
					.description = "Rename an asset file or folder with its sidecars: the references follow its GUID.",
					.args = {pathSpec,
							 {.name = "name",
							  .type = ArgType::Text,
							  .required = true,
							  .description = "New file name, in the same folder."}},
					.factory = {},
					.run = renameAsset,
					.allowedInPlay = true});
	ioRegistry.add({.name = "asset.move",
					.description = "Move an asset file or folder with its sidecars into another folder.",
					.args = {pathSpec,
							 {.name = "folder",
							  .type = ArgType::Text,
							  .required = true,
							  .description = "Destination folder.",
							  .projectPath = true}},
					.factory = {},
					.run = moveAsset,
					.allowedInPlay = true});
	ioRegistry.add({.name = "asset.delete",
					.description = "Delete an asset file or folder with its sidecars (not an undo entry).",
					.args = {pathSpec},
					.factory = {},
					.run = deleteAsset,
					.allowedInPlay = true});
	ioRegistry.add({.name = "asset.cook",
					.description = "Cook an asset, or every asset of a folder, into the project cook cache.",
					.args = {pathSpec},
					.factory = {},
					.run = cookAssets,
					.allowedInPlay = true});
	ioRegistry.add({.name = "asset.clear_cache",
					.description = "Delete every cooked asset of the project cook cache.",
					.args = {},
					.factory = {},
					.run = clearCookCache,
					.allowedInPlay = true});
	ioRegistry.add({.name = "asset.upgrade_references",
					.description = "Rewrite the asset references of every Owl document of the project by GUID.",
					.args = {},
					.factory = {},
					.run = upgradeReferences,
					.allowedInPlay = false});
}

}// namespace owl::nest::commands
