/**
 * @file EditorModule.cpp
 * @author Silmaen
 * @date 24/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "EditorModule.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <format>
#include <initializer_list>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace owl::nest::session {

namespace {

auto numbers(const script::HostValue& iValue) -> std::optional<std::vector<double>> {
	if (const auto* list = std::get_if<std::vector<double>>(&iValue))
		return *list;
	return std::nullopt;
}

auto number(const script::HostValue& iValue) -> std::optional<double> {
	if (const auto* value = std::get_if<double>(&iValue))
		return *value;
	if (const auto* value = std::get_if<int64_t>(&iValue))
		return static_cast<double>(*value);
	return std::nullopt;
}

auto text(const script::HostValue& iValue) -> std::optional<std::string> {
	if (const auto* value = std::get_if<std::string>(&iValue))
		return *value;
	if (const auto* value = std::get_if<int64_t>(&iValue))
		return std::format("{}", *value);
	if (const auto* value = std::get_if<double>(&iValue))
		return std::format("{}", *value);
	if (const auto* value = std::get_if<bool>(&iValue))
		return *value ? "true" : "false";
	return std::nullopt;
}

// Largest whole number a Lua number holds exactly.
constexpr int64_t g_exactLimit = int64_t{1} << 53;

}// namespace

auto toHostEntity(const core::UUID iUuid) -> script::HostValue {
	return std::format("{}", static_cast<uint64_t>(iUuid));
}

auto toEntityId(const script::HostValue& iValue) -> std::optional<core::UUID> {
	if (const auto* whole = std::get_if<int64_t>(&iValue))
		return core::UUID{static_cast<uint64_t>(*whole)};
	const auto* digits = std::get_if<std::string>(&iValue);
	if (digits == nullptr || digits->empty())
		return std::nullopt;
	uint64_t id = 0;
	const auto [end, error] = std::from_chars(digits->data(), digits->data() + digits->size(), id);
	if (error != std::errc{} || end != digits->data() + digits->size())
		return std::nullopt;
	return core::UUID{id};
}

auto toArgValue(const commands::ArgSpec& iSpec, const script::HostValue& iValue)
		-> expected<commands::ArgValue, std::string> {
	const auto wrong = [&iSpec](const char* iExpected) -> unexpected<std::string> {
		return unexpected<std::string>{std::format("`{}` takes {}", iSpec.name, iExpected)};
	};
	switch (iSpec.type) {
		case commands::ArgType::Bool:
			if (const auto* value = std::get_if<bool>(&iValue))
				return *value;
			return wrong("a boolean");
		case commands::ArgType::Integer:
			if (const auto value = number(iValue); value.has_value())
				return static_cast<int64_t>(std::llround(*value));
			return wrong("an integer");
		case commands::ArgType::Number:
			if (const auto value = number(iValue); value.has_value())
				return *value;
			return wrong("a number");
		case commands::ArgType::Text:
		case commands::ArgType::Enum:
			if (const auto value = text(iValue); value.has_value())
				return *value;
			return wrong("a string");
		case commands::ArgType::Entity:
			if (const auto id = toEntityId(iValue); id.has_value())
				return static_cast<int64_t>(static_cast<uint64_t>(*id));
			if (const auto* value = std::get_if<std::string>(&iValue))
				return *value;
			return wrong("an entity id or tag");
		case commands::ArgType::Vec2:
			if (const auto list = numbers(iValue); list && list->size() == 2)
				return math::vec2{static_cast<float>((*list)[0]), static_cast<float>((*list)[1])};
			return wrong("{x, y}");
		case commands::ArgType::Vec3:
			if (const auto list = numbers(iValue); list && list->size() == 3)
				return math::vec3{static_cast<float>((*list)[0]), static_cast<float>((*list)[1]),
								  static_cast<float>((*list)[2])};
			return wrong("{x, y, z}");
		case commands::ArgType::Vec4:
		case commands::ArgType::Color:
			if (const auto list = numbers(iValue); list && (list->size() == 4 || list->size() == 3))
				return math::vec4{static_cast<float>((*list)[0]), static_cast<float>((*list)[1]),
								  static_cast<float>((*list)[2]),
								  list->size() == 4 ? static_cast<float>((*list)[3]) : 1.f};
			if (const auto* value = std::get_if<std::string>(&iValue);
				value != nullptr && iSpec.type == commands::ArgType::Color)
				return *value;
			return wrong("{r, g, b[, a]}");
	}
	return wrong("a value");
}

namespace {

auto toDoubles(const std::initializer_list<float> iValues) -> std::vector<double> {
	std::vector<double> out;
	out.reserve(iValues.size());
	for (const float value: iValues) out.push_back(static_cast<double>(value));
	return out;
}

}// namespace

auto toHostValue(const commands::ArgValue& iValue) -> script::HostValue {
	return std::visit(
			[]<typename T>(const T& iItem) -> script::HostValue {
				if constexpr (std::is_same_v<T, math::vec2>)
					return toDoubles({iItem.x(), iItem.y()});
				else if constexpr (std::is_same_v<T, math::vec3>)
					return toDoubles({iItem.x(), iItem.y(), iItem.z()});
				else if constexpr (std::is_same_v<T, math::vec4>)
					return toDoubles({iItem.x(), iItem.y(), iItem.z(), iItem.w()});
				else if constexpr (std::is_same_v<T, int64_t>)
					return iItem > g_exactLimit || iItem < -g_exactLimit
								   ? toHostEntity(core::UUID{static_cast<uint64_t>(iItem)})
								   : script::HostValue{iItem};
				else
					return iItem;
			},
			iValue);
}

EditorModule::EditorModule(script::ScriptConsole& ioLua, const commands::CommandRegistry& iRegistry, Executor iExecutor)
	: m_lua{ioLua}, m_registry{iRegistry}, m_executor{std::move(iExecutor)} {}

void EditorModule::sync() {
	if (m_revision == m_registry.getRevision())
		return;
	m_revision = m_registry.getRevision();
	for (const auto& spec: m_registry.getCommands()) {
		if (!m_bound.insert(spec.name).second)
			continue;
		m_lua.addFunction(
				std::format("editor.{}", spec.name), spec.description,
				[this, name = spec.name](const std::map<std::string, script::HostValue>& iArgs) -> script::HostResult {
					return call(name, iArgs);
				});
	}
}

auto EditorModule::call(const std::string& iName, const std::map<std::string, script::HostValue>& iArgs) const
		-> script::HostResult {
	const auto* command = m_registry.find(iName);
	if (command == nullptr)
		return {.ok = false, .error = std::format("{}: no such command", iName), .fields = {}};
	commands::CommandArgs args;
	for (const auto& [key, value]: iArgs) {
		const auto argSpec = std::ranges::find(command->args, key, &commands::ArgSpec::name);
		if (argSpec == command->args.end())
			return {.ok = false, .error = std::format("{}: no argument `{}`", iName, key), .fields = {}};
		auto converted = toArgValue(*argSpec, value);
		if (!converted)
			return {.ok = false, .error = std::format("{}: {}", iName, converted.error()), .fields = {}};
		args.set(key, std::move(*converted));
	}
	const auto result = m_executor(iName, args);
	if (!result)
		return {.ok = false, .error = std::format("{}: {}", iName, result.error().message), .fields = {}};
	script::HostResult out;
	for (const auto& [key, value]: result->data) out.fields.emplace(key, toHostValue(value));
	out.fields.emplace("description", result->description);
	if (result->entity != core::UUID{0})
		out.fields.insert_or_assign("entity", toHostEntity(result->entity));
	return out;
}

}// namespace owl::nest::session
