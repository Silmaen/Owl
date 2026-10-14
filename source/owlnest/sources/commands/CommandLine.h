/**
 * @file CommandLine.h
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
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace owl::nest::commands {

/**
 * @brief
 *  A command read from one line of the command syntax.
 */
struct CommandLine {
	/// Command name.
	std::string name;
	/// Typed arguments, result references resolved.
	CommandArgs args;
};

/**
 * @brief
 *  Why a line does not read as a command.
 */
struct LineFailure {
	/// Column of the problem in the line, from 1.
	size_t column{1};
	/// Human-readable explanation.
	std::string message;
};

/**
 * @brief
 *  Resolves a result reference: `$n` (the number given) or `$_` (nothing given) to an entity UUID.
 */
using ReferenceResolver = std::function<expected<core::UUID, std::string>(std::optional<size_t> iIndex)>;

/**
 * @brief
 *  Substitutes an entity argument by a result reference when printing: returns the text to write (`$2`), or
 *  nothing to write the UUID.
 */
using ReferenceNamer = std::function<std::optional<std::string>(int64_t iUuid)>;

/**
 * @brief
 *  Read one line of the command syntax: `entity.create name="Tree" parent=$1  # a comment`.
 *
 * The command name comes first, then `name=value` pairs separated by spaces. Values follow the type the command
 * declares: `true` / `false`, numbers, text (a bare word, or quoted with backslash escapes for the quote, the
 * backslash, a new line and a tab), vectors as `(1, 2, 3)`, colours as `#rrggbb[aa]` or `(r, g, b[, a])`, enum
 * choices by name, entities by UUID, by tag or by a result reference `$n` / `$_`. A `#` at the start of a word begins
 * a comment.
 * @param[in] iLine The line.
 * @param[in] iRegistry The registry declaring the commands.
 * @param[in] iResolver Resolves the `$n` references (null: a reference is an error).
 * @return The command, nothing for an empty or comment line, or why the line is wrong.
 */
[[nodiscard]] auto parseCommandLine(std::string_view iLine, const CommandRegistry& iRegistry,
									const ReferenceResolver& iResolver = {})
		-> expected<std::optional<CommandLine>, LineFailure>;

/**
 * @brief
 *  Write one argument value in the command syntax.
 * @param[in] iValue The value.
 * @param[in] iType The declared type, when known (colours as `#rrggbb[aa]` when exact, enums bare).
 * @return The text, read back to the same value by `parseCommandLine`.
 */
[[nodiscard]] auto formatArgValue(const ArgValue& iValue, std::optional<ArgType> iType = std::nullopt) -> std::string;

/**
 * @brief
 *  Write a command as one line of the command syntax, arguments in their declaration order.
 * @param[in] iName Command name.
 * @param[in] iArgs Arguments.
 * @param[in] iSpec The command declaration (null: arguments by name, typed by their value).
 * @param[in] iNamer Substitutes entity UUIDs by result references (null: none).
 * @return The line, read back to the same command by `parseCommandLine`.
 */
[[nodiscard]] auto printCommandLine(std::string_view iName, const CommandArgs& iArgs, const CommandSpec* iSpec,
									const ReferenceNamer& iNamer = {}) -> std::string;

/**
 * @brief
 *  Describe a command for `help <command>`: its syntax, description and arguments.
 * @param[in] iSpec The command declaration.
 * @return Several lines of text.
 */
[[nodiscard]] auto describeCommand(const CommandSpec& iSpec) -> std::string;

}// namespace owl::nest::commands
