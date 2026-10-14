/**
 * @file CommandJournal.cpp
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "CommandJournal.h"

#include "CommandLine.h"

#include <cstddef>
#include <format>
#include <utility>

namespace owl::nest::commands {

void CommandJournal::record(const CommandSpec* iSpec, const std::string_view iName, const CommandArgs& iArgs,
							const CommandSource iSource, const expected<CommandResult, CommandFailure>& iResult) {
	JournalEntry entry{.index = m_nextIndex++,
					   .source = iSource,
					   .name = std::string{iName},
					   .args = iArgs,
					   .line = printCommandLine(iName, iArgs, iSpec),
					   .succeeded = iResult.has_value(),
					   .result = iResult.has_value() ? *iResult : CommandResult{},
					   .failure = iResult.has_value() ? CommandFailure{} : iResult.error()};
	if (m_entries.size() >= maxEntries)
		m_entries.erase(m_entries.begin(),
						m_entries.begin() + static_cast<std::ptrdiff_t>(m_entries.size() - maxEntries + 1));
	m_entries.push_back(std::move(entry));
	++m_revision;
}

auto CommandJournal::filter(const CommandSource iSource) const -> std::vector<const JournalEntry*> {
	std::vector<const JournalEntry*> entries;
	for (const auto& entry: m_entries) {
		if (entry.source == iSource)
			entries.push_back(&entry);
	}
	return entries;
}

auto CommandJournal::find(const size_t iIndex) const -> const JournalEntry* {
	if (m_entries.empty() || iIndex < m_entries.front().index || iIndex > m_entries.back().index)
		return nullptr;
	return &m_entries[iIndex - m_entries.front().index];
}

auto CommandJournal::resolveReference(const std::optional<size_t> iIndex) const -> expected<core::UUID, std::string> {
	const auto name = iIndex ? std::format("${}", *iIndex) : std::string{"$_"};
	const JournalEntry* entry = nullptr;
	if (iIndex)
		entry = find(*iIndex);
	else if (!m_entries.empty())
		entry = &m_entries.back();
	if (entry == nullptr) {
		OWL_WARN("Journal: {} designates no command of the journal.", name)
		return unexpected<std::string>{std::format("{} designates no command of the journal", name)};
	}
	if (!entry->succeeded || entry->result.entity == core::UUID{0}) {
		OWL_WARN("Journal: {} ({}) gave no entity.", name, entry->name)
		return unexpected<std::string>{std::format("{} ({}) gave no entity", name, entry->name)};
	}
	return entry->result.entity;
}

void CommandJournal::clear() {
	m_entries.clear();
	++m_revision;
}

}// namespace owl::nest::commands
