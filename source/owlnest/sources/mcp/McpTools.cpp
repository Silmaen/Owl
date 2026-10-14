/**
 * @file McpTools.cpp
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "McpTools.h"

#include <core/Log.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace owl::nest::mcp {

namespace {

using json = nlohmann::json;

auto numberArray(const size_t iMin, const size_t iMax) -> json {
	return {{"type", "array"}, {"items", {{"type", "number"}}}, {"minItems", iMin}, {"maxItems", iMax}};
}

auto typeSchema(const commands::ArgSpec& iSpec) -> json {
	switch (iSpec.type) {
		case commands::ArgType::Bool:
			return {{"type", "boolean"}};
		case commands::ArgType::Integer:
			return {{"type", "integer"}};
		case commands::ArgType::Number:
			return {{"type", "number"}};
		case commands::ArgType::Text:
			return {{"type", "string"}};
		case commands::ArgType::Entity:
			return {{"type", json::array({"integer", "string"})}};
		case commands::ArgType::Vec2:
			return numberArray(2, 2);
		case commands::ArgType::Vec3:
			return numberArray(3, 3);
		case commands::ArgType::Vec4:
			return numberArray(4, 4);
		case commands::ArgType::Color:
			return {{"anyOf", json::array({numberArray(3, 4),
										   {{"type", "string"}, {"pattern", "^#([0-9a-fA-F]{6}|[0-9a-fA-F]{8})$"}}})}};
		case commands::ArgType::Enum:
			return {{"type", "string"}, {"enum", iSpec.choices}};
	}
	return json::object();
}

template<size_t N>
auto parseNumbers(const json& iValue) -> std::optional<std::array<float, N>> {
	if (!iValue.is_array() || iValue.size() != N)
		return std::nullopt;
	std::array<float, N> numbers{};
	for (size_t i = 0; i < N; ++i) {
		if (!iValue[i].is_number())
			return std::nullopt;
		numbers[i] = iValue[i].get<float>();
	}
	return numbers;
}

auto parseColor(const json& iValue) -> std::optional<commands::ArgValue> {
	if (iValue.is_string())
		return commands::CommandRegistry::parseArg(commands::ArgType::Color, iValue.get<std::string>());
	if (const auto rgba = parseNumbers<4>(iValue); rgba)
		return math::vec4{(*rgba)[0], (*rgba)[1], (*rgba)[2], (*rgba)[3]};
	if (const auto rgb = parseNumbers<3>(iValue); rgb)
		return math::vec4{(*rgb)[0], (*rgb)[1], (*rgb)[2], 1.f};
	return std::nullopt;
}

auto parseInteger(const json& iValue) -> std::optional<commands::ArgValue> {
	if (iValue.is_number_unsigned())
		return static_cast<int64_t>(iValue.get<uint64_t>());
	if (iValue.is_number_integer())
		return iValue.get<int64_t>();
	return std::nullopt;
}

template<typename T>
auto vectorJson(const T& iVector, const size_t iSize) -> json {
	auto array = json::array();
	for (size_t i = 0; i < iSize; ++i) array.push_back(iVector[i]);
	return array;
}

}// namespace

auto getErrorName(const commands::CommandError iError) -> std::string_view {
	switch (iError) {
		case commands::CommandError::UnknownCommand:
			return "UnknownCommand";
		case commands::CommandError::MissingArgument:
			return "MissingArgument";
		case commands::CommandError::WrongArgumentType:
			return "WrongArgumentType";
		case commands::CommandError::UnknownArgument:
			return "UnknownArgument";
		case commands::CommandError::UnknownEntity:
			return "UnknownEntity";
		case commands::CommandError::InvalidArgument:
			return "InvalidArgument";
		case commands::CommandError::NotApplicable:
			return "NotApplicable";
		case commands::CommandError::RefusedDuringPlay:
			return "RefusedDuringPlay";
	}
	return "";
}

auto makeArgSchema(const commands::ArgSpec& iSpec) -> json {
	auto schema = typeSchema(iSpec);
	if (!iSpec.description.empty())
		schema["description"] = iSpec.description;
	if (iSpec.defaultValue)
		schema["default"] = toJson(*iSpec.defaultValue);
	return schema;
}

auto makeTool(const commands::CommandSpec& iSpec) -> json {
	auto properties = json::object();
	auto required = json::array();
	for (const auto& arg: iSpec.args) {
		properties[arg.name] = makeArgSchema(arg);
		if (arg.required)
			required.push_back(arg.name);
	}
	json schema{{"type", "object"}, {"properties", std::move(properties)}, {"additionalProperties", false}};
	if (!required.empty())
		schema["required"] = std::move(required);
	return {{"name", iSpec.name}, {"description", iSpec.description}, {"inputSchema", std::move(schema)}};
}

auto parseArgValue(const commands::ArgSpec& iSpec, const json& iValue) -> std::optional<commands::ArgValue> {
	switch (iSpec.type) {
		case commands::ArgType::Bool:
			return iValue.is_boolean() ? std::optional<commands::ArgValue>{iValue.get<bool>()} : std::nullopt;
		case commands::ArgType::Integer:
			return parseInteger(iValue);
		case commands::ArgType::Number:
			return iValue.is_number() ? std::optional<commands::ArgValue>{iValue.get<double>()} : std::nullopt;
		case commands::ArgType::Text:
		case commands::ArgType::Enum:
			return iValue.is_string() ? std::optional<commands::ArgValue>{iValue.get<std::string>()} : std::nullopt;
		case commands::ArgType::Entity:
			if (iValue.is_string())
				return iValue.get<std::string>();
			return parseInteger(iValue);
		case commands::ArgType::Vec2:
			if (const auto n = parseNumbers<2>(iValue); n)
				return math::vec2{(*n)[0], (*n)[1]};
			return std::nullopt;
		case commands::ArgType::Vec3:
			if (const auto n = parseNumbers<3>(iValue); n)
				return math::vec3{(*n)[0], (*n)[1], (*n)[2]};
			return std::nullopt;
		case commands::ArgType::Vec4:
			if (const auto n = parseNumbers<4>(iValue); n)
				return math::vec4{(*n)[0], (*n)[1], (*n)[2], (*n)[3]};
			return std::nullopt;
		case commands::ArgType::Color:
			return parseColor(iValue);
	}
	return std::nullopt;
}

auto parseArguments(const commands::CommandSpec& iSpec, const json& iArguments)
		-> expected<commands::CommandArgs, commands::CommandFailure> {
	commands::CommandArgs args;
	for (const auto& [name, value]: iArguments.items()) {
		const auto spec = std::ranges::find(iSpec.args, name, &commands::ArgSpec::name);
		if (spec == iSpec.args.end()) {
			OWL_CORE_WARN("Mcp: {} has no argument `{}`.", iSpec.name, name)
			return unexpected{
					commands::CommandFailure{.error = commands::CommandError::UnknownArgument,
											 .message = std::format("{}: unknown argument `{}`", iSpec.name, name)}};
		}
		auto parsed = parseArgValue(*spec, value);
		if (!parsed) {
			OWL_CORE_WARN("Mcp: Argument `{}` of {} does not fit its type.", name, iSpec.name)
			return unexpected{commands::CommandFailure{
					.error = commands::CommandError::WrongArgumentType,
					.message = std::format("{}: `{}` does not fit the schema of the argument", iSpec.name, name)}};
		}
		args.set(name, std::move(*parsed));
	}
	return args;
}

auto toJson(const commands::ArgValue& iValue) -> json {
	return std::visit(
			[](const auto& iAlternative) -> json {
				using T = std::decay_t<decltype(iAlternative)>;
				if constexpr (std::is_same_v<T, math::vec2>)
					return vectorJson(iAlternative, 2);
				else if constexpr (std::is_same_v<T, math::vec3>)
					return vectorJson(iAlternative, 3);
				else if constexpr (std::is_same_v<T, math::vec4>)
					return vectorJson(iAlternative, 4);
				else
					return iAlternative;
			},
			iValue);
}

auto makeToolResult(const commands::CommandResult& iResult) -> json {
	auto structured = json::object();
	for (const auto& [name, value]: iResult.data) structured[name] = toJson(value);
	// Entity ids leave as the unsigned UUIDs of the scene files (the registry stores them as signed integers).
	if (iResult.entity != 0)
		structured["entity"] = static_cast<uint64_t>(iResult.entity);
	if (!iResult.description.empty())
		structured["description"] = iResult.description;
	return {{"content", json::array({{{"type", "text"}, {"text", dumpJson(structured)}}})},
			{"structuredContent", structured},
			{"isError", false}};
}

auto makeToolError(const commands::CommandFailure& iFailure) -> json {
	const json structured{{"error", getErrorName(iFailure.error)}, {"message", iFailure.message}};
	return {{"content", json::array({{{"type", "text"}, {"text", iFailure.message}}})},
			{"structuredContent", structured},
			{"isError", true}};
}

auto dumpJson(const json& iValue) -> std::string { return iValue.dump(-1, ' ', false, json::error_handler_t::replace); }

}// namespace owl::nest::mcp
