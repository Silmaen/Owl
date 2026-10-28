/**
 * @file Console.h
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "EditorSession.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace owl::nest::session {

/**
 * @brief
 *  Kind of a console output line.
 */
enum struct ConsoleLineKind : uint8_t {
	Input,///< The line typed, echoed.
	Result,///< What a command did.
	Error,///< A line that does not read, or a command that failed.
	Refused,///< A command refused because the scene plays.
	Help///< Text printed by `help`.
};

/**
 * @brief
 *  One line printed by the console.
 */
struct ConsoleOutput {
	/// Kind of line.
	ConsoleLineKind kind{ConsoleLineKind::Result};
	/// The text.
	std::string text;
	/// Number of the journal entry the line reports (0: none).
	size_t journalIndex{0};
};

/**
 * @brief
 *  Completions of the text typed so far.
 */
struct ConsoleCompletion {
	/// The candidates for the word under completion (command names, `argument=`, enum choices).
	std::vector<std::string> candidates;
	/// The input with the word completed as far as the candidates agree.
	std::string completed;
};

/**
 * @brief
 *  The editor console, without UI: reads command lines, runs them on a session, keeps the history and completes.
 *
 * Lines are in the command syntax (`commands::parseCommandLine`), `$n` resolved through the session journal, and run
 * with the `Console` source. `help` lists the commands, `help <command>` describes one.
 */
class Console final {
public:
	/// Lines kept in the history.
	static constexpr size_t maxHistory = 100;

	Console(const Console&) = delete;

	Console(Console&&) = delete;

	auto operator=(const Console&) -> Console& = delete;

	auto operator=(Console&&) -> Console& = delete;

	/**
	 * @brief
	 *  Bind a console to a session.
	 * @param[in,out] ioSession The session the commands run on (outlives the console).
	 */
	explicit Console(EditorSession& ioSession) : m_session{ioSession} {}

	~Console() = default;

	/**
	 * @brief
	 *  Run a line typed by the user and add it to the history.
	 * @param[in] iLine The line.
	 * @return What to print: the echoed line, then the result, the error or the help text.
	 */
	auto submit(std::string_view iLine) -> std::vector<ConsoleOutput>;

	/**
	 * @brief
	 *  Complete the last word of the input.
	 * @param[in] iInput The text typed so far.
	 * @return The candidates and the completed input.
	 */
	[[nodiscard]] auto complete(std::string_view iInput) const -> ConsoleCompletion;

	/**
	 * @brief
	 *  Step back in the history (↑).
	 * @return The older line, nothing when the history is empty.
	 */
	auto previousHistory() -> std::optional<std::string>;

	/**
	 * @brief
	 *  Step forward in the history (↓).
	 * @return The newer line, an empty line past the newest, nothing when not browsing the history.
	 */
	auto nextHistory() -> std::optional<std::string>;

	/**
	 * @brief
	 *  Get the history, oldest first.
	 * @return The lines.
	 */
	[[nodiscard]] auto getHistory() const -> const std::vector<std::string>& { return m_history; }

	/**
	 * @brief
	 *  Replace the history (restored from the editor settings).
	 * @param[in] iHistory The lines, oldest first; only the newest `maxHistory` are kept.
	 */
	void setHistory(std::vector<std::string> iHistory);

	/**
	 * @brief
	 *  Add a line to the history.
	 * @param[in] iLine The line.
	 */
	void remember(const std::string& iLine);

private:
	/**
	 * @brief
	 *  Print the help of every command or of one.
	 * @param[in] iTopic The command name, empty for the list.
	 * @param[in,out] ioOutput The lines printed.
	 */
	void help(std::string_view iTopic, std::vector<ConsoleOutput>& ioOutput) const;


	/// The session the commands run on.
	EditorSession& m_session;
	/// Lines typed, oldest first.
	std::vector<std::string> m_history;
	/// Position while browsing the history (`m_history.size()` when not browsing).
	size_t m_cursor{0};
};

}// namespace owl::nest::session
