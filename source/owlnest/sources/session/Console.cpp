/**
 * @file Console.cpp
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "Console.h"

#include "../commands/CommandLine.h"

#include <algorithm>
#include <cstddef>
#include <format>
#include <utility>

namespace owl::nest::session {

namespace {

constexpr std::string_view g_spaces = " \t";

auto trim(const std::string_view iText) -> std::string_view {
	const auto first = iText.find_first_not_of(g_spaces);
	if (first == std::string_view::npos)
		return {};
	const auto last = iText.find_last_not_of(g_spaces);
	return iText.substr(first, last - first + 1);
}

auto firstWord(const std::string_view iText) -> std::string_view {
	const auto text = trim(iText);
	return text.substr(0, text.find_first_of(g_spaces));
}

auto commonPrefix(const std::vector<std::string>& iCandidates) -> std::string {
	if (iCandidates.empty())
		return {};
	std::string prefix = iCandidates.front();
	for (const auto& candidate: iCandidates) {
		const auto mismatch = std::ranges::mismatch(prefix, candidate);
		prefix.resize(static_cast<size_t>(mismatch.in1 - prefix.begin()));
	}
	return prefix;
}

auto describeResult(const commands::CommandResult& iResult) -> std::string {
	std::string fields;
	for (const auto& [key, value]: iResult.data)
		fields += std::format("{}{}={}", fields.empty() ? "" : " ", key, commands::formatArgValue(value));
	if (fields.empty())
		return iResult.description;
	return iResult.description.empty() ? fields : std::format("{}  ({})", iResult.description, fields);
}

auto completeValue(const commands::ArgSpec& iArg, const std::string_view iPrefix) -> std::vector<std::string> {
	std::vector<std::string> values;
	if (iArg.type == commands::ArgType::Enum)
		values = iArg.choices;
	else if (iArg.type == commands::ArgType::Bool)
		values = {"true", "false"};
	std::vector<std::string> candidates;
	for (const auto& value: values) {
		if (value.starts_with(iPrefix))
			candidates.push_back(std::format("{}={}", iArg.name, value));
	}
	return candidates;
}

}// namespace

auto Console::submit(const std::string_view iLine) -> std::vector<ConsoleOutput> {
	const auto line = trim(iLine);
	m_cursor = m_history.size();
	if (line.empty())
		return {};
	remember(std::string{line});
	std::vector<ConsoleOutput> output{{.kind = ConsoleLineKind::Input, .text = std::format("> {}", line)}};
	if (firstWord(line) == "help") {
		help(trim(line.substr(4)), output);
		return output;
	}
	const auto& journal = m_session.getJournal();
	const commands::ReferenceResolver resolver =
			[&journal](const std::optional<size_t> iIndex) -> expected<core::UUID, std::string> {
		return journal.resolveReference(iIndex);
	};
	const auto parsed = commands::parseCommandLine(line, m_session.getRegistry(), resolver);
	if (!parsed) {
		output.push_back({.kind = ConsoleLineKind::Error,
						  .text = std::format("column {}: {}", parsed.error().column, parsed.error().message)});
		return output;
	}
	if (!parsed->has_value())
		return output;
	const auto& command = **parsed;
	const auto result = m_session.execute(command.name, command.args, commands::CommandSource::Console);
	if (!result) {
		const bool refused = result.error().error == commands::CommandError::RefusedDuringPlay;
		output.push_back({.kind = refused ? ConsoleLineKind::Refused : ConsoleLineKind::Error,
						  .text = refused ? std::format("refused during Play: {}", result.error().message)
										  : result.error().message,
						  .journalIndex = journal.getNextIndex() - 1});
		return output;
	}
	const auto index = journal.getNextIndex() - 1;
	output.push_back({.kind = ConsoleLineKind::Result,
					  .text = std::format("${}  {}", index, describeResult(*result)),
					  .journalIndex = index});
	return output;
}

auto Console::complete(const std::string_view iInput) const -> ConsoleCompletion {
	const std::string text{iInput};
	const auto lastSpace = text.find_last_of(g_spaces);
	const size_t wordStart = lastSpace == std::string::npos ? 0 : lastSpace + 1;
	const std::string head = text.substr(0, wordStart);
	const std::string word = text.substr(wordStart);
	const auto& registry = m_session.getRegistry();
	const auto commandName = firstWord(head);
	std::vector<std::string> candidates;
	bool endsWord = true;
	if (commandName.empty() || commandName == "help") {
		if (commandName.empty() && std::string_view{"help"}.starts_with(word))
			candidates.emplace_back("help");
		for (const auto& spec: registry.getCommands()) {
			if (spec.name.starts_with(word))
				candidates.push_back(spec.name);
		}
	} else if (const auto* spec = registry.find(commandName); spec != nullptr) {
		if (const auto equal = word.find('='); equal != std::string::npos) {
			const auto key = word.substr(0, equal);
			if (const auto arg = std::ranges::find(spec->args, key, &commands::ArgSpec::name); arg != spec->args.end())
				candidates = completeValue(*arg, std::string_view{word}.substr(equal + 1));
		} else {
			endsWord = false;
			for (const auto& arg: spec->args) {
				if (arg.name.starts_with(word) && !head.contains(std::format(" {}=", arg.name)))
					candidates.push_back(std::format("{}=", arg.name));
			}
		}
	}
	ConsoleCompletion completion{.candidates = std::move(candidates), .completed = text};
	if (completion.candidates.empty())
		return completion;
	completion.completed = head + commonPrefix(completion.candidates);
	if (completion.candidates.size() == 1 && endsWord)
		completion.completed += ' ';
	return completion;
}

auto Console::previousHistory() -> std::optional<std::string> {
	if (m_history.empty())
		return std::nullopt;
	if (m_cursor > 0)
		--m_cursor;
	return m_history[m_cursor];
}

auto Console::nextHistory() -> std::optional<std::string> {
	if (m_cursor >= m_history.size())
		return std::nullopt;
	++m_cursor;
	return m_cursor < m_history.size() ? m_history[m_cursor] : std::string{};
}

void Console::setHistory(std::vector<std::string> iHistory) {
	m_history = std::move(iHistory);
	if (m_history.size() > maxHistory)
		m_history.erase(m_history.begin(), m_history.end() - static_cast<std::ptrdiff_t>(maxHistory));
	m_cursor = m_history.size();
}

void Console::help(const std::string_view iTopic, std::vector<ConsoleOutput>& ioOutput) const {
	const auto& registry = m_session.getRegistry();
	if (iTopic.empty()) {
		ioOutput.push_back({.kind = ConsoleLineKind::Help,
							.text = "Commands (`help <command>` for its arguments; `$n` is the result of command n, "
									"`$_` the last one):"});
		for (const auto& spec: registry.getCommands())
			ioOutput.push_back(
					{.kind = ConsoleLineKind::Help, .text = std::format("  {}  {}", spec.name, spec.description)});
		return;
	}
	const auto* spec = registry.find(iTopic);
	if (spec == nullptr) {
		ioOutput.push_back({.kind = ConsoleLineKind::Error, .text = std::format("unknown command `{}`", iTopic)});
		return;
	}
	ioOutput.push_back({.kind = ConsoleLineKind::Help, .text = commands::describeCommand(*spec)});
}

void Console::remember(const std::string& iLine) {
	if (m_history.empty() || m_history.back() != iLine)
		m_history.push_back(iLine);
	if (m_history.size() > maxHistory)
		m_history.erase(m_history.begin());
	m_cursor = m_history.size();
}

}// namespace owl::nest::session
