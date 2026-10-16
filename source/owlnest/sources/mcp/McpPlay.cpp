/**
 * @file McpPlay.cpp
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "McpPlay.h"

#include "McpTools.h"

#include <core/Log.h>
#include <input/InputNames.h>

#include <array>
#include <cstddef>
#include <format>
#include <optional>
#include <utility>
#include <vector>

namespace owl::nest::mcp {

namespace {

using json = nlohmann::json;

constexpr std::string_view g_base64Alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

auto invalid(std::string iMessage) -> unexpected<std::string> {
	OWL_CORE_WARN("Mcp: play.run: {}.", iMessage)
	return unexpected{std::move(iMessage)};
}

auto readCount(const json& iValue, const std::string_view iName) -> expected<uint32_t, std::string> {
	if (!iValue.is_number_integer() || iValue.get<int64_t>() < 0 ||
		iValue.get<int64_t>() > static_cast<int64_t>(session::PlayRequest::maxCount))
		return invalid(std::format("`{}` is an integer between 0 and {}", iName, session::PlayRequest::maxCount));
	return iValue.get<uint32_t>();
}

auto readActions(const json& iValue) -> expected<std::vector<session::ActionHold>, std::string> {
	if (!iValue.is_object())
		return invalid("`actions` is an object: action name to true, a number or [x, y]");
	std::vector<session::ActionHold> holds;
	for (const auto& [name, value]: iValue.items()) {
		session::ActionHold hold{.name = name};
		if (value.is_boolean()) {
			hold.value = math::vec2{value.get<bool>() ? 1.f : 0.f, 0.f};
		} else if (value.is_number()) {
			hold.value = math::vec2{value.get<float>(), 0.f};
		} else if (value.is_array() && value.size() == 2 && value[0].is_number() && value[1].is_number()) {
			hold.value = math::vec2{value[0].get<float>(), value[1].get<float>()};
		} else {
			return invalid(std::format("action `{}`: give true, a number or [x, y]", name));
		}
		holds.push_back(std::move(hold));
	}
	return holds;
}

auto readKeys(const json& iValue) -> expected<std::vector<uint16_t>, std::string> {
	if (!iValue.is_array())
		return invalid("`keys` is an array of key names");
	std::vector<uint16_t> keys;
	for (const auto& item: iValue) {
		const auto code = item.is_string() ? input::names::parseKey(item.get<std::string>()) : std::nullopt;
		if (!code)
			return invalid(std::format("unknown key {}", item.dump()));
		keys.push_back(*code);
	}
	return keys;
}

auto readFlag(const json& iValue, const std::string_view iName, bool& oFlag) -> expected<void, std::string> {
	if (!iValue.is_boolean())
		return invalid(std::format("`{}` is a boolean", iName));
	oFlag = iValue.get<bool>();
	return {};
}

auto readField(const std::string& iName, const json& iValue, session::PlayRequest& oRequest,
			   std::vector<session::ActionHold>& oHolds) -> expected<void, std::string> {
	if (iName == "frames" || iName == "ticks") {
		auto count = readCount(iValue, iName);
		if (!count)
			return unexpected{std::move(count.error())};
		(iName == "frames" ? oRequest.frames : oRequest.ticks) = *count;
		return {};
	}
	if (iName == "timestep_ms") {
		if (!iValue.is_number())
			return invalid("`timestep_ms` is a number");
		oRequest.timestepMs = iValue.get<double>();
		return {};
	}
	if (iName == "actions") {
		auto holds = readActions(iValue);
		if (!holds)
			return unexpected{std::move(holds.error())};
		oHolds = std::move(*holds);
		return {};
	}
	if (iName == "keys") {
		auto keys = readKeys(iValue);
		if (!keys)
			return unexpected{std::move(keys.error())};
		oRequest.keys = std::move(*keys);
		return {};
	}
	if (iName == "capture")
		return readFlag(iValue, iName, oRequest.capture);
	if (iName == "stop_at_end")
		return readFlag(iValue, iName, oRequest.stopAtEnd);
	return invalid(std::format("unknown argument `{}`", iName));
}

auto phasesJson(const scene::RuntimeTimings& iPhases) -> json {
	return {{"scripts_ms", iPhases.scriptsMs},
			{"physics_ms", iPhases.physicsMs},
			{"render_ms", iPhases.renderMs},
			{"total_ms", iPhases.totalMs}};
}

}// namespace

auto makePlayTool() -> json {
	const json properties{
			{"frames",
			 {{"type", "integer"},
			  {"minimum", 0},
			  {"maximum", session::PlayRequest::maxCount},
			  {"description", "Frames to play; the play ends at the first of `frames` and `ticks` reached."}}},
			{"ticks",
			 {{"type", "integer"},
			  {"minimum", 0},
			  {"maximum", session::PlayRequest::maxCount},
			  {"description", "Simulation ticks to run (1 steps the simulation once); at least one of frames and "
							  "ticks is required."}}},
			{"timestep_ms",
			 {{"type", "number"},
			  {"minimum", 0},
			  {"maximum", 1000},
			  {"default", 0},
			  {"description", "Duration of every frame in milliseconds; 0 plays one simulation tick per frame."}}},
			{"actions",
			 {{"type", "object"},
			  {"additionalProperties",
			   {{"anyOf",
				 json::array(
						 {{{"type", "boolean"}},
						  {{"type", "number"}},
						  {{"type", "array"}, {"items", {{"type", "number"}}}, {"minItems", 2}, {"maxItems", 2}}})}}},
			  {"description", "Input actions held during the whole play: name to true (button), a number (axis) or "
							  "[x, y] (vector), as in a scenario `actions:` step."}}},
			{"keys",
			 {{"type", "array"},
			  {"items", {{"type", "string"}}},
			  {"description", "Keyboard keys held during the whole play, by name (`Space`, `D`)."}}},
			{"capture",
			 {{"type", "boolean"},
			  {"default", true},
			  {"description", "Return the last frame of the viewport as a PNG image."}}},
			{"stop_at_end",
			 {{"type", "boolean"},
			  {"default", true},
			  {"description", "Stop the scene at the end (back to the edited scene); false leaves it paused on "
							  "the last frame."}}}};
	return {{"name", playToolName},
			{"description", "Play the scene for a number of frames or simulation ticks at a fixed time step, with "
							"held input actions, then return the viewport as a PNG image and the phase timings. "
							"Enters Play when the scene is edited; the edited scene is left untouched."},
			{"inputSchema", {{"type", "object"}, {"properties", properties}, {"additionalProperties", false}}}};
}

auto parsePlayRequest(const json& iArguments, const input::ActionMap& iMap)
		-> expected<session::PlayRequest, std::string> {
	session::PlayRequest request{.capture = true};
	std::vector<session::ActionHold> holds;
	for (const auto& [name, value]: iArguments.items()) {
		if (const auto read = readField(name, value, request, holds); !read)
			return unexpected{read.error()};
	}
	session::HeldInput held{.keys = std::move(request.keys)};
	if (auto error = session::resolveActionHolds(iMap, holds, held); !error.empty())
		return invalid(error);
	request.keys = std::move(held.keys);
	request.mouseButtons = std::move(held.mouseButtons);
	request.gamepad = held.gamepad;
	return request;
}

auto makePlayResult(const session::PlayReport& iReport) -> json {
	json structured{{"frames", iReport.frames},
					{"ticks", iReport.ticks},
					{"timestep_ms", iReport.timestepMs},
					{"total_ms", iReport.totalMs},
					{"mean_frame_ms", iReport.getMeanFrameMs()},
					{"max_frame_ms", iReport.maxFrameMs},
					{"draw_calls", iReport.drawCalls},
					{"entity_count", iReport.entityCount},
					{"interrupted", iReport.interrupted},
					{"phases", phasesJson(iReport.lastPhases)}};
	std::vector<uint8_t> png;
	if (iReport.capture)
		png = iReport.capture->encodePng();
	if (!png.empty()) {
		structured["capture"] = {{"width", iReport.capture->size.x()},
								 {"height", iReport.capture->size.y()},
								 {"png_bytes", png.size()}};
	}
	json content = json::array({{{"type", "text"}, {"text", dumpJson(structured)}}});
	if (!png.empty())
		content.push_back({{"type", "image"}, {"data", encodeBase64(png)}, {"mimeType", "image/png"}});
	return {{"content", std::move(content)}, {"structuredContent", std::move(structured)}, {"isError", false}};
}

auto makePlayError(const std::string_view iMessage) -> json {
	const json structured{{"error", "PlayFailed"}, {"message", iMessage}};
	return {{"content", json::array({{{"type", "text"}, {"text", iMessage}}})},
			{"structuredContent", structured},
			{"isError", true}};
}

auto encodeBase64(const std::span<const uint8_t> iBytes) -> std::string {
	std::string text;
	text.reserve((iBytes.size() + 2) / 3 * 4);
	for (size_t i = 0; i < iBytes.size(); i += 3) {
		const size_t left = iBytes.size() - i;
		const uint32_t chunk = (static_cast<uint32_t>(iBytes[i]) << 16u) |
							   (left > 1 ? static_cast<uint32_t>(iBytes[i + 1]) << 8u : 0u) |
							   (left > 2 ? static_cast<uint32_t>(iBytes[i + 2]) : 0u);
		text.push_back(g_base64Alphabet[(chunk >> 18u) & 63u]);
		text.push_back(g_base64Alphabet[(chunk >> 12u) & 63u]);
		text.push_back(left > 1 ? g_base64Alphabet[(chunk >> 6u) & 63u] : '=');
		text.push_back(left > 2 ? g_base64Alphabet[chunk & 63u] : '=');
	}
	return text;
}

}// namespace owl::nest::mcp
