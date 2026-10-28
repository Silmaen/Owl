/**
 * @file LuaConsole.h
 * @author Silmaen
 * @date 28/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Console.h"

#include <script/ScriptConsole.h>

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace owl::nest::commands {
class CommandRegistry;
}// namespace owl::nest::commands

namespace owl::nest::session {

class EditorSession;

/**
 * @brief
 *  The Lua side of the editor console and the editor scripts: a Lua state of its own whose `editor` module holds one
 *  function per registry command (`editor.entity.create{name = "Tree"}`), with the names, arguments and refusals of
 *  the console and MCP.
 *
 * The state never runs inside Play and cannot reach the game: a command refused during Play raises its refusal. A line
 * in command syntax (`entity.create name="Tree"`) runs as a command, as in the command mode.
 */
class LuaConsole final {
public:
	/**
	 * @brief
	 *  Create the console of a session.
	 * @param[in,out] ioSession The session whose commands the scripts run.
	 * @param[in,out] ioCommandConsole The command console, for the lines in command syntax.
	 */
	LuaConsole(EditorSession& ioSession, Console& ioCommandConsole);

	~LuaConsole() = default;

	LuaConsole(const LuaConsole&) = delete;

	LuaConsole(LuaConsole&&) = delete;

	auto operator=(const LuaConsole&) -> LuaConsole& = delete;

	auto operator=(LuaConsole&&) -> LuaConsole& = delete;

	/**
	 * @brief
	 *  Evaluate one typed line: Lua, or a command when it starts with a command name.
	 * @param[in] iLine The line.
	 * @return The lines to print, the input echoed first.
	 */
	auto submit(std::string_view iLine) -> std::vector<ConsoleOutput>;

	/**
	 * @brief
	 *  Whether a Lua block is open: the next line continues it.
	 * @return True while lines wait for the end of a block.
	 */
	[[nodiscard]] auto hasPending() const -> bool { return m_lua.hasPending(); }

	/**
	 * @brief
	 *  Forget the lines of an open block.
	 */
	void cancelPending() { m_lua.cancelPending(); }

	/**
	 * @brief
	 *  Run an editor script, as one undo step; an error undoes what it did.
	 * @param[in] iPath Path of the `.lua` on disk.
	 * @return What it printed, then its error if any.
	 */
	auto runScript(const std::filesystem::path& iPath) -> std::vector<ConsoleOutput>;

	/**
	 * @brief
	 *  The Lua script equivalent to the lines of a macro: one `editor` call per command.
	 * @param[in] iLines Lines of the macro.
	 * @param[in] iRegistry The registry the lines name commands of.
	 * @return The script, the lines it cannot translate kept as comments.
	 */
	[[nodiscard]] static auto macroToLua(const std::vector<std::string>& iLines,
										 const commands::CommandRegistry& iRegistry) -> std::string;

	/**
	 * @brief
	 *  The editor scripts of a project: the `.lua` files under `editor/` of its asset folders.
	 * @param[in] iAssetDirectories The asset folders.
	 * @return Their paths on disk, sorted.
	 */
	[[nodiscard]] static auto listEditorScripts(const std::vector<std::filesystem::path>& iAssetDirectories)
			-> std::vector<std::filesystem::path>;

private:
	/// The session whose commands run.
	EditorSession& m_session;
	/// The command console, for the lines in command syntax.
	Console& m_commands;
	/// The Lua state.
	script::ScriptConsole m_lua;
	/// Whether the `editor` module holds the commands of the registry.
	bool m_registered{false};

	/**
	 * @brief
	 *  Give the Lua state one function per registry command, on first use.
	 */
	void registerCommands();
};

}// namespace owl::nest::session
