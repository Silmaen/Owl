/**
 * @file EditorModule.h
 * @author Silmaen
 * @date 24/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "../commands/CommandRegistry.h"

#include <core/UUID.h>
#include <core/expected.h>
#include <script/ScriptConsole.h>

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>

namespace owl::nest::session {

/**
 * @brief
 *  An entity id as Lua receives it: its decimal digits, since a Lua number only holds 53 bits exactly.
 * @param[in] iUuid The id.
 * @return The text of the id.
 */
[[nodiscard]] auto toHostEntity(core::UUID iUuid) -> script::HostValue;

/**
 * @brief
 *  The entity id a Lua value holds.
 * @param[in] iValue Decimal digits (what `toHostEntity` gives), or a whole number.
 * @return The id, or nothing when the value is neither.
 */
[[nodiscard]] auto toEntityId(const script::HostValue& iValue) -> std::optional<core::UUID>;

/**
 * @brief
 *  The value of a Lua argument as a command expects it.
 * @param[in] iSpec Declaration of the argument.
 * @param[in] iValue The value read from Lua.
 * @return The argument value, or why the value does not fit.
 */
[[nodiscard]] auto toArgValue(const commands::ArgSpec& iSpec, const script::HostValue& iValue)
		-> expected<commands::ArgValue, std::string>;

/**
 * @brief
 *  A command value as Lua receives it: a vector becomes a list of numbers, an integer a Lua number cannot hold (an
 *  entity id) its decimal digits.
 * @param[in] iValue The command value.
 * @return The Lua value.
 */
[[nodiscard]] auto toHostValue(const commands::ArgValue& iValue) -> script::HostValue;

/**
 * @brief
 *  The `editor` module of a Lua state: one function per registry command (`editor.entity.create{name = "Tree"}`),
 *  with the names, arguments and refusals of the console and MCP.
 *
 * The registry may gain commands while the state lives (Owl Nest plugins): `sync` binds the new ones.
 */
class EditorModule final {
public:
	/// How the module runs a command: its name and arguments, already converted.
	using Executor = std::function<expected<commands::CommandResult, commands::CommandFailure>(
			std::string_view iName, const commands::CommandArgs& iArgs)>;

	/**
	 * @brief
	 *  Create the module of a Lua state.
	 * @param[in,out] ioLua The Lua state (outlives the module).
	 * @param[in] iRegistry The registry whose commands are bound (outlives the module).
	 * @param[in] iExecutor Runs a command asked by a script.
	 */
	EditorModule(script::ScriptConsole& ioLua, const commands::CommandRegistry& iRegistry, Executor iExecutor);

	/**
	 * @brief
	 *  Destructor.
	 */
	~EditorModule() = default;

	EditorModule(const EditorModule&) = delete;

	EditorModule(EditorModule&&) = delete;

	auto operator=(const EditorModule&) -> EditorModule& = delete;

	auto operator=(EditorModule&&) -> EditorModule& = delete;

	/**
	 * @brief
	 *  Give the Lua state a function for every command of the registry it has none for; nothing is done while the
	 *  registry did not change.
	 */
	void sync();

private:
	/**
	 * @brief
	 *  Run a command asked by a script.
	 * @param[in] iName Command name.
	 * @param[in] iArgs Fields of the Lua table.
	 * @return The fields of the result, or the refusal.
	 */
	[[nodiscard]] auto call(const std::string& iName, const std::map<std::string, script::HostValue>& iArgs) const
			-> script::HostResult;

	/// The Lua state.
	script::ScriptConsole& m_lua;
	/// The registry.
	const commands::CommandRegistry& m_registry;
	/// Runs a command.
	Executor m_executor;
	/// Names of the commands the state has a function for.
	std::set<std::string, std::less<>> m_bound;
	/// Revision of the registry at the last `sync`.
	uint64_t m_revision{UINT64_MAX};
};

}// namespace owl::nest::session
