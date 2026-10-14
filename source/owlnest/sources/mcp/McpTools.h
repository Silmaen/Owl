/**
 * @file McpTools.h
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "../commands/CommandRegistry.h"

#include <core/expected.h>

#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <string_view>

namespace owl::nest::mcp {

/**
 * @brief
 *  Name of a command error, as tools report it (`RefusedDuringPlay`).
 * @param[in] iError The error.
 * @return Its name.
 */
[[nodiscard]] auto getErrorName(commands::CommandError iError) -> std::string_view;

/**
 * @brief
 *  JSON Schema of one command argument: type, description, default value, enum choices.
 * @param[in] iSpec The argument declaration.
 * @return The schema.
 */
[[nodiscard]] auto makeArgSchema(const commands::ArgSpec& iSpec) -> nlohmann::json;

/**
 * @brief
 *  MCP tool description of a command: name, description and input schema (an object of its arguments).
 * @param[in] iSpec The command declaration.
 * @return The tool, as `tools/list` returns it.
 */
[[nodiscard]] auto makeTool(const commands::CommandSpec& iSpec) -> nlohmann::json;

/**
 * @brief
 *  Convert a JSON value into a command argument of the declared type.
 * @param[in] iSpec The argument declaration.
 * @param[in] iValue The value given by the client.
 * @return The argument, or nothing when the value does not fit the type.
 */
[[nodiscard]] auto parseArgValue(const commands::ArgSpec& iSpec, const nlohmann::json& iValue)
		-> std::optional<commands::ArgValue>;

/**
 * @brief
 *  Convert the JSON arguments of a tool call into command arguments.
 * @param[in] iSpec The command declaration.
 * @param[in] iArguments The `arguments` object of the call.
 * @return The arguments, or why they do not fit the declaration (unknown argument, wrong type).
 */
[[nodiscard]] auto parseArguments(const commands::CommandSpec& iSpec, const nlohmann::json& iArguments)
		-> expected<commands::CommandArgs, commands::CommandFailure>;

/**
 * @brief
 *  Convert a command argument or result field into JSON (vectors become arrays).
 * @param[in] iValue The value.
 * @return The JSON value.
 */
[[nodiscard]] auto toJson(const commands::ArgValue& iValue) -> nlohmann::json;

/**
 * @brief
 *  Tool result of a command that ran: its data as structured content, also serialised as text.
 * @param[in] iResult What the command did.
 * @return The `tools/call` result.
 */
[[nodiscard]] auto makeToolResult(const commands::CommandResult& iResult) -> nlohmann::json;

/**
 * @brief
 *  Tool result of a command that did not run (`isError`), with the error name and the message.
 * @param[in] iFailure Why the command did not run.
 * @return The `tools/call` result.
 */
[[nodiscard]] auto makeToolError(const commands::CommandFailure& iFailure) -> nlohmann::json;

/**
 * @brief
 *  Serialise JSON for the wire: compact, invalid UTF-8 replaced instead of throwing.
 * @param[in] iValue The value.
 * @return The text.
 */
[[nodiscard]] auto dumpJson(const nlohmann::json& iValue) -> std::string;

}// namespace owl::nest::mcp
