/**
 * @file McpPlay.h
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "../session/EditorSession.h"

#include <core/expected.h>
#include <input/ActionMap.h>

#include <nlohmann/json.hpp>

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace owl::nest::mcp {

/// Name of the tool that plays the scene for a number of frames and captures the viewport.
inline constexpr std::string_view playToolName = "play.run";

/**
 * @brief
 *  The `play.run` tool: name, description and input schema.
 * @return The tool, as `tools/list` returns it.
 */
[[nodiscard]] auto makePlayTool() -> nlohmann::json;

/**
 * @brief
 *  Convert the arguments of a `play.run` call into a bounded play request.
 *
 * Held actions are turned into the raw inputs of their first matching binding (`resolveActionHolds`).
 * @param[in] iArguments The `arguments` object of the call.
 * @param[in] iMap The action map the actions are looked up in.
 * @return The request, or why the arguments do not fit.
 */
[[nodiscard]] auto parsePlayRequest(const nlohmann::json& iArguments, const input::ActionMap& iMap)
		-> expected<session::PlayRequest, std::string>;

/**
 * @brief
 *  Tool result of a bounded play: the measurements as structured content and text, the capture as an image.
 * @param[in] iReport What the play did.
 * @return The `tools/call` result (`content` holds the text, then the PNG image when there is one).
 */
[[nodiscard]] auto makePlayResult(const session::PlayReport& iReport) -> nlohmann::json;

/**
 * @brief
 *  Tool result of a play that did not run or did not end (`isError`).
 * @param[in] iMessage Why.
 * @return The `tools/call` result.
 */
[[nodiscard]] auto makePlayError(std::string_view iMessage) -> nlohmann::json;

/**
 * @brief
 *  Encode bytes as base64 (RFC 4648, with padding), as MCP image content carries them.
 * @param[in] iBytes The bytes.
 * @return The text.
 */
[[nodiscard]] auto encodeBase64(std::span<const uint8_t> iBytes) -> std::string;

}// namespace owl::nest::mcp
