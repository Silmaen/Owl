/**
 * @file Macro.cpp
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "Macro.h"

#include "CommandLine.h"

#include <app/Application.h>

#include <format>
#include <fstream>
#include <map>
#include <optional>

namespace owl::nest::commands {

namespace {

auto macroFail(const MacroError iError, const size_t iLine, std::string iMessage) -> unexpected<MacroFailure> {
	OWL_WARN("Macro: {}.", iMessage)
	return unexpected<MacroFailure>{MacroFailure{.error = iError, .line = iLine, .message = std::move(iMessage)}};
}

auto replayDepth() -> size_t& {
	static size_t s_depth = 0;
	return s_depth;
}

class DepthGuard final {
public:
	DepthGuard() { ++replayDepth(); }

	DepthGuard(const DepthGuard&) = delete;

	DepthGuard(DepthGuard&&) = delete;

	auto operator=(const DepthGuard&) -> DepthGuard& = delete;

	auto operator=(DepthGuard&&) -> DepthGuard& = delete;

	~DepthGuard() { --replayDepth(); }
};

auto runMacro(const CommandContext& iContext, const CommandArgs& iArgs) -> expected<CommandResult, CommandFailure> {
	if (iContext.registry == nullptr)
		return unexpected<CommandFailure>{
				CommandFailure{.error = CommandError::NotApplicable, .message = "no registry to replay with"}};
	const auto path = resolveMacroPath(iArgs.getText("path"));
	const auto macro = Macro::load(path);
	if (!macro)
		return unexpected<CommandFailure>{
				CommandFailure{.error = CommandError::InvalidArgument, .message = macro.error().message}};
	const auto description = std::format("Macro {}", path.filename().string());
	const auto report = macro->replay(*iContext.registry, iContext, description);
	if (!report)
		return unexpected<CommandFailure>{CommandFailure{.error = report.error().error == MacroError::CommandFailed
																		  ? CommandError::NotApplicable
																		  : CommandError::InvalidArgument,
														 .message = report.error().message}};
	CommandResult result{.entity = report->lastEntity, .description = description, .data = {}};
	result.data = {{"path", path.generic_string()}, {"commands", static_cast<int64_t>(report->commands)}};
	return result;
}

}// namespace

auto Macro::load(const std::filesystem::path& iPath) -> expected<Macro, MacroFailure> {
	if (!exists(iPath))
		return macroFail(MacroError::FileNotFound, 0, std::format("macro {} not found", iPath.string()));
	std::ifstream file{iPath};
	if (!file)
		return macroFail(MacroError::ReadFailed, 0, std::format("cannot read {}", iPath.string()));
	std::vector<std::string> lines;
	for (std::string line; std::getline(file, line);) {
		if (line.ends_with('\r'))
			line.pop_back();
		lines.push_back(std::move(line));
	}
	return Macro{std::move(lines)};
}

auto Macro::fromJournal(const std::span<const JournalEntry* const> iEntries, const CommandRegistry& iRegistry)
		-> Macro {
	std::vector<std::string> lines;
	std::map<int64_t, size_t> references;
	const ReferenceNamer namer = [&references](const int64_t iUuid) -> std::optional<std::string> {
		if (const auto found = references.find(iUuid); found != references.end())
			return std::format("${}", found->second);
		return std::nullopt;
	};
	size_t count = 0;
	for (const auto* entry: iEntries) {
		if (entry == nullptr || !entry->succeeded)
			continue;
		lines.push_back(printCommandLine(entry->name, entry->args, iRegistry.find(entry->name), namer));
		++count;
		if (entry->result.entity != core::UUID{0})
			references.insert_or_assign(static_cast<int64_t>(static_cast<uint64_t>(entry->result.entity)), count);
	}
	return Macro{std::move(lines)};
}

auto Macro::save(const std::filesystem::path& iPath) const -> expected<void, MacroFailure> {
	std::ofstream file{iPath};
	if (!file)
		return macroFail(MacroError::WriteFailed, 0, std::format("cannot write {}", iPath.string()));
	for (const auto& line: m_lines) file << line << '\n';
	if (!file)
		return macroFail(MacroError::WriteFailed, 0, std::format("cannot write {}", iPath.string()));
	return {};
}

auto Macro::replay(const CommandRegistry& iRegistry, const CommandContext& iContext,
				   const std::string& iDescription) const -> expected<MacroReport, MacroFailure> {
	if (!iContext.scene || iContext.undo == nullptr)
		return macroFail(MacroError::NoScene, 0, "no scene or undo history to replay on");
	if (replayDepth() >= maxDepth)
		return macroFail(MacroError::TooDeep, 0, std::format("macros nest deeper than {}", maxDepth));
	const DepthGuard depth;
	std::vector<core::UUID> results;
	const ReferenceResolver resolver =
			[&results](const std::optional<size_t> iIndex) -> expected<core::UUID, std::string> {
		const auto name = iIndex ? std::format("${}", *iIndex) : std::string{"$_"};
		if (results.empty() || (iIndex && *iIndex > results.size()))
			return unexpected<std::string>{std::format("{} designates no previous command of the macro", name)};
		const auto uuid = iIndex ? results[*iIndex - 1] : results.back();
		if (uuid == core::UUID{0})
			return unexpected<std::string>{std::format("{} gave no entity", name)};
		return uuid;
	};
	CommandContext context = iContext;
	context.source = CommandSource::Replay;
	MacroReport report;
	iContext.undo->beginGroup(iDescription);
	for (size_t i = 0; i < m_lines.size(); ++i) {
		const size_t lineNumber = i + 1;
		const auto parsed = parseCommandLine(m_lines[i], iRegistry, resolver);
		if (!parsed) {
			iContext.undo->cancelGroup(*iContext.scene);
			return macroFail(
					MacroError::InvalidLine, lineNumber,
					std::format("line {}, column {}: {}", lineNumber, parsed.error().column, parsed.error().message));
		}
		if (!parsed->has_value())
			continue;
		const auto& command = **parsed;
		const auto result = iRegistry.execute(command.name, command.args, context);
		if (!result) {
			iContext.undo->cancelGroup(*iContext.scene);
			return macroFail(MacroError::CommandFailed, lineNumber,
							 std::format("line {}: {}", lineNumber, result.error().message));
		}
		results.push_back(result->entity);
		++report.commands;
		if (result->entity != core::UUID{0})
			report.lastEntity = result->entity;
	}
	iContext.undo->endGroup();
	return report;
}

auto resolveMacroPath(const std::filesystem::path& iPath) -> std::filesystem::path {
	if (iPath.is_absolute() || exists(iPath) || !app::Application::instanced())
		return iPath;
	for (const auto& [title, assetsPath]: app::Application::get().getAssetDirectories()) {
		if (const auto candidate = assetsPath / iPath; exists(candidate))
			return candidate;
	}
	return iPath;
}

auto getMacroRunSpec() -> CommandSpec {
	return {.name = "macro.run",
			.description = "Replay a macro file (.owlmacro) as one undo step; the first failing line stops it and "
						   "undoes what it did.",
			.args = {{.name = "path",
					  .type = ArgType::Text,
					  .required = true,
					  .description = "Macro file, absolute or relative to an asset folder."}},
			.factory = {},
			.run = runMacro};
}

}// namespace owl::nest::commands
