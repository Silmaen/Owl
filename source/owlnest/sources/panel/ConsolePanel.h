/**
 * @file ConsolePanel.h
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "../session/Console.h"
#include "../session/LuaConsole.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace owl::nest::panel {

/**
 * @brief
 *  The Console panel: a command line over the session, and the journal of every command executed in it.
 *
 * Shows the journal entries as they arrive (filterable by source), the help and errors of the typed lines, and
 * saves the selected entries as a `.owlmacro`. ↑ / ↓ browse the history, Tab completes.
 */
class ConsolePanel final {
public:
	/// Lines kept in the view.
	static constexpr size_t maxItems = 2000;

	ConsolePanel(const ConsolePanel&) = delete;

	ConsolePanel(ConsolePanel&&) = delete;

	auto operator=(const ConsolePanel&) -> ConsolePanel& = delete;

	auto operator=(ConsolePanel&&) -> ConsolePanel& = delete;

	/**
	 * @brief
	 *  Bind the panel to a session.
	 * @param[in,out] ioSession The session (outlives the panel).
	 */
	explicit ConsolePanel(session::EditorSession& ioSession) : m_session{ioSession}, m_console{ioSession} {}

	~ConsolePanel() = default;

	/**
	 * @brief
	 *  Draw the panel.
	 */
	void onImGuiRender();

	/**
	 * @brief
	 *  Get the console behind the panel.
	 * @return The console.
	 */
	[[nodiscard]] auto getConsole() -> session::Console& { return m_console; }

	/**
	 * @brief
	 *  Get the console behind the panel.
	 * @return The console.
	 */
	[[nodiscard]] auto getConsole() const -> const session::Console& { return m_console; }

	/**
	 * @brief
	 *  Run an editor script and print its output in the console.
	 * @param[in] iPath Path of the `.lua` on disk.
	 */
	void runEditorScript(const std::filesystem::path& iPath);

	/**
	 * @brief
	 *  Print a line in the console.
	 * @param[in] iOutput The line.
	 */
	void print(session::ConsoleOutput iOutput) { addItem({.journalIndex = 0, .output = std::move(iOutput)}); }

private:
	/**
	 * @brief
	 *  One line of the view: printed text, or a journal entry.
	 */
	struct Item {
		/// Number of the journal entry shown (0: a printed line).
		size_t journalIndex{0};
		/// Printed line, when `journalIndex` is 0.
		session::ConsoleOutput output;
	};

	/**
	 * @brief
	 *  Append the journal entries recorded since the last frame.
	 */
	void collectJournal();

	/**
	 * @brief
	 *  Append a line to the view.
	 * @param[in] iItem The line.
	 */
	void addItem(Item iItem);

	/**
	 * @brief
	 *  Draw the source filters and the macro buttons.
	 */
	void renderToolbar();

	/**
	 * @brief
	 *  Draw one journal entry (selectable).
	 * @param[in] iIndex Number of the entry.
	 */
	void renderEntry(size_t iIndex);

	/**
	 * @brief
	 *  Draw the menu of the editor scripts of the project.
	 */
	void renderScriptsMenu();

	/**
	 * @brief
	 *  Draw the input line.
	 */
	void renderInput();

	/**
	 * @brief
	 *  Ask for a file and write the selected entries as a macro.
	 */
	void saveSelection();

	/// The session the commands run on.
	session::EditorSession& m_session;
	/// Parser, history and completion.
	session::Console m_console;
	/// Lua evaluator and editor scripts.
	session::LuaConsole m_lua{m_session, m_console};
	/// Whether the input line is Lua.
	bool m_luaMode{false};
	/// Lines of the view.
	std::vector<Item> m_items;
	/// Last journal entry appended to the view.
	size_t m_lastJournalIndex{0};
	/// Journal entries selected for a macro.
	std::set<size_t> m_selected;
	/// Text being typed.
	std::string m_input;
	/// Sources shown, by `CommandSource` value (bit set).
	uint32_t m_sourceMask{0xff};
	/// Scroll to the bottom on the next frame.
	bool m_scrollToBottom{false};
	/// Give the focus back to the input on the next frame.
	bool m_refocus{false};
};

}// namespace owl::nest::panel
