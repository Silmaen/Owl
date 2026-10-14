/**
 * @file CommandJournal.h
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "CommandRegistry.h"

#include <core/expected.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace owl::nest::commands {

/**
 * @brief
 *  One command of the journal: what was asked, by whom, and what it did.
 */
struct JournalEntry {
	/// Number of the command in the session (`$n`), from 1.
	size_t index{0};
	/// Who asked for it.
	CommandSource source{CommandSource::Gesture};
	/// Command name.
	std::string name;
	/// Arguments given (defaults not filled in).
	CommandArgs args;
	/// The command as one line of the command syntax.
	std::string line;
	/// True when the command ran.
	bool succeeded{false};
	/// What the command did, when it ran.
	CommandResult result;
	/// Why it did not run, otherwise.
	CommandFailure failure;
};

/**
 * @brief
 *  The commands executed in a session, whatever their source, with their result.
 *
 * Filled by `CommandRegistry::execute` once the registry is bound with `CommandRegistry::setJournal`. The entries are
 * numbered from 1 for the whole session: `$n` designates the result of the entry `n`, `$_` the last one. The oldest
 * entries are dropped beyond `maxEntries`.
 */
class CommandJournal final {
public:
	/// Entries kept; older ones are dropped.
	static constexpr size_t maxEntries = 5000;

	CommandJournal() = default;

	CommandJournal(const CommandJournal&) = delete;

	CommandJournal(CommandJournal&&) = default;

	auto operator=(const CommandJournal&) -> CommandJournal& = delete;

	auto operator=(CommandJournal&&) -> CommandJournal& = default;

	~CommandJournal() = default;

	/**
	 * @brief
	 *  Append an executed command.
	 * @param[in] iSpec The command declaration (null for an unknown command).
	 * @param[in] iName The command name.
	 * @param[in] iArgs The arguments given.
	 * @param[in] iSource Who asked for it.
	 * @param[in] iResult What it did, or why it did not run.
	 */
	void record(const CommandSpec* iSpec, std::string_view iName, const CommandArgs& iArgs, CommandSource iSource,
				const expected<CommandResult, CommandFailure>& iResult);

	/**
	 * @brief
	 *  Get the entries, oldest first.
	 * @return The entries kept.
	 */
	[[nodiscard]] auto getEntries() const -> std::span<const JournalEntry> { return m_entries; }

	/**
	 * @brief
	 *  Get the entries of one source, oldest first.
	 * @param[in] iSource The source kept.
	 * @return Pointers to the matching entries (valid until the next `record` or `clear`).
	 */
	[[nodiscard]] auto filter(CommandSource iSource) const -> std::vector<const JournalEntry*>;

	/**
	 * @brief
	 *  Find an entry by number.
	 * @param[in] iIndex Number of the entry (`$n`).
	 * @return The entry, null when out of the journal.
	 */
	[[nodiscard]] auto find(size_t iIndex) const -> const JournalEntry*;

	/**
	 * @brief
	 *  Resolve a result reference to the entity its command created or changed.
	 * @param[in] iIndex Number of the entry, nothing for `$_` (the last entry).
	 * @return The entity UUID, or why the reference does not resolve.
	 */
	[[nodiscard]] auto resolveReference(std::optional<size_t> iIndex) const -> expected<core::UUID, std::string>;

	/**
	 * @brief
	 *  Number the next entry will get.
	 * @return The number.
	 */
	[[nodiscard]] auto getNextIndex() const -> size_t { return m_nextIndex; }

	/**
	 * @brief
	 *  Increase on every change of the journal, so a view knows when to refresh.
	 * @return The revision.
	 */
	[[nodiscard]] auto getRevision() const -> uint64_t { return m_revision; }

	/**
	 * @brief
	 *  Forget every entry; the numbering goes on.
	 */
	void clear();

private:
	/// Entries, oldest first.
	std::vector<JournalEntry> m_entries;
	/// Number of the next entry.
	size_t m_nextIndex{1};
	/// Change counter.
	uint64_t m_revision{0};
};

}// namespace owl::nest::commands
