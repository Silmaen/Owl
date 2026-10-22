/**
 * @file ScriptEngine.cpp
 * @author Silmaen
 * @date 09/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "script/ScriptEngine.h"

#include "core/external/lua.h"
#include "script/LuaEngine.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace owl::script {
namespace {

// `OWL_SCRIPT_TIME_SCALE` multiplies the time quota: tests under an emulator run every call many times slower.
auto scaledTimeQuota(const uint32_t iTimeMs) -> uint32_t {
	const char* scale = std::getenv("OWL_SCRIPT_TIME_SCALE");
	if (scale == nullptr)
		return iTimeMs;
	uint32_t factor = 1;
	const std::string_view text{scale};
	if (const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), factor);
		error != std::errc{} || factor == 0)
		return iTimeMs;
	return iTimeMs * factor;
}

auto defaultQuotas() -> ScriptQuotas& {
	static ScriptQuotas quotas = [] -> ScriptQuotas {
		ScriptQuotas initial;
		initial.timePerCallMs = scaledTimeQuota(initial.timePerCallMs);
		return initial;
	}();
	return quotas;
}

// Raw reads only: a script-provided metatable on the properties table must not run outside a protected call.
auto rawField(lua_State* iState, const int iTableIndex, const char* iKey) -> int {
	const int tableIndex = lua_absindex(iState, iTableIndex);
	lua_pushstring(iState, iKey);
	return lua_rawget(iState, tableIndex);
}

// Numbers of the table on top of the stack, at most iCount (missing entries and non-numbers read as 0).
template<size_t Count>
auto readComponents(lua_State* iState, const std::array<float, Count>& iFallback) -> std::array<float, Count> {
	auto values = iFallback;
	if (lua_type(iState, -1) != LUA_TTABLE)
		return values;
	for (size_t index = 0; index < Count; ++index) {
		lua_rawgeti(iState, -1, static_cast<lua_Integer>(index + 1));
		if (lua_isnumber(iState, -1) != 0)
			values[index] = static_cast<float>(lua_tonumber(iState, -1));
		lua_pop(iState, 1);
	}
	return values;
}

// The value of the declared type read from the top of the stack.
auto readValue(lua_State* iState, const ScriptPropertyType iType) -> ScriptValue {
	switch (iType) {
		case ScriptPropertyType::Float:
			return lua_isnumber(iState, -1) != 0 ? static_cast<float>(lua_tonumber(iState, -1)) : 0.0f;
		case ScriptPropertyType::Int:
			return lua_isinteger(iState, -1) != 0 ? static_cast<int64_t>(lua_tointeger(iState, -1)) : int64_t{0};
		case ScriptPropertyType::String:
			return lua_isstring(iState, -1) != 0 ? std::string(lua_tostring(iState, -1)) : std::string{};
		case ScriptPropertyType::Bool:
			return lua_isboolean(iState, -1) != 0 ? lua_toboolean(iState, -1) != 0 : false;
		case ScriptPropertyType::Color:
			return readComponents<4>(iState, {1.f, 1.f, 1.f, 1.f});
		case ScriptPropertyType::Vec2:
			return readComponents<2>(iState, {0.f, 0.f});
		case ScriptPropertyType::Vec3:
			return readComponents<3>(iState, {0.f, 0.f, 0.f});
		case ScriptPropertyType::Entity:
			return lua_isinteger(iState, -1) != 0 ? static_cast<uint64_t>(lua_tointeger(iState, -1)) : uint64_t{0};
		case ScriptPropertyType::Numbers:
			{
				std::vector<double> numbers;
				if (lua_type(iState, -1) == LUA_TTABLE) {
					for (lua_Integer index = 1;; ++index) {
						lua_rawgeti(iState, -1, index);
						const bool isNumber = lua_isnumber(iState, -1) != 0;
						if (isNumber)
							numbers.push_back(static_cast<double>(lua_tonumber(iState, -1)));
						lua_pop(iState, 1);
						if (!isNumber)
							break;
					}
				}
				return numbers;
			}
		case ScriptPropertyType::Strings:
			{
				std::vector<std::string> strings;
				if (lua_type(iState, -1) == LUA_TTABLE) {
					for (lua_Integer index = 1;; ++index) {
						lua_rawgeti(iState, -1, index);
						const bool isString = lua_type(iState, -1) == LUA_TSTRING;
						if (isString)
							strings.emplace_back(lua_tostring(iState, -1));
						lua_pop(iState, 1);
						if (!isString)
							break;
					}
				}
				return strings;
			}
	}
	return 0.0f;
}

auto readOptionalNumber(lua_State* iState, const int iEntryIndex, const char* iKey) -> std::optional<float> {
	rawField(iState, iEntryIndex, iKey);
	std::optional<float> number;
	if (lua_isnumber(iState, -1) != 0)
		number = static_cast<float>(lua_tonumber(iState, -1));
	lua_pop(iState, 1);
	return number;
}

auto parseProperty(lua_State* iState, const int iEntryIndex) -> ScriptProperty {
	ScriptProperty prop;
	rawField(iState, iEntryIndex, "name");
	if (lua_isstring(iState, -1) != 0)
		prop.name = lua_tostring(iState, -1);
	lua_pop(iState, 1);
	rawField(iState, iEntryIndex, "type");
	if (lua_isstring(iState, -1) != 0)
		prop.type = ScriptEngine::parsePropertyType(lua_tostring(iState, -1)).value_or(ScriptPropertyType::Float);
	lua_pop(iState, 1);
	rawField(iState, iEntryIndex, "default");
	prop.value = readValue(iState, prop.type);
	prop.defaultValue = prop.value;
	lua_pop(iState, 1);
	prop.min = readOptionalNumber(iState, iEntryIndex, "min");
	prop.max = readOptionalNumber(iState, iEntryIndex, "max");
	prop.step = readOptionalNumber(iState, iEntryIndex, "step");
	return prop;
}

auto readProperties(lua_State* iState) -> std::vector<ScriptProperty> {
	lua_pushglobaltable(iState);
	if (rawField(iState, -1, "properties") != LUA_TTABLE) {
		lua_pop(iState, 2);
		return {};
	}
	const int tableIndex = lua_gettop(iState);
	std::vector<ScriptProperty> props;
	lua_pushnil(iState);
	while (lua_next(iState, tableIndex) != 0) {
		if (lua_type(iState, -1) == LUA_TTABLE) {
			if (auto prop = parseProperty(iState, lua_gettop(iState)); !prop.name.empty())
				props.push_back(std::move(prop));
		}
		lua_pop(iState, 1);
	}
	lua_pop(iState, 2);
	return props;
}

auto readFunctions(lua_State* iState) -> std::vector<ScriptFunction> {
	std::vector<ScriptFunction> functions;
	lua_pushglobaltable(iState);
	const int tableIndex = lua_gettop(iState);
	lua_pushnil(iState);
	while (lua_next(iState, tableIndex) != 0) {
		if (lua_type(iState, -2) == LUA_TSTRING && lua_type(iState, -1) == LUA_TFUNCTION) {
			const std::string name = lua_tostring(iState, -2);
			lua_pushvalue(iState, -1);
			lua_Debug info{};
			if (lua_getinfo(iState, ">S", &info) != 0 && std::string_view{info.what} == "Lua")
				functions.push_back({.name = name, .line = static_cast<uint32_t>(std::max(info.linedefined, 0))});
		}
		lua_pop(iState, 1);
	}
	lua_pop(iState, 1);
	std::ranges::sort(functions, [](const ScriptFunction& iLeft, const ScriptFunction& iRight) -> bool {
		return iLeft.line != iRight.line ? iLeft.line < iRight.line : iLeft.name < iRight.name;
	});
	return functions;
}

}// namespace

auto ScriptEngine::propertyTypeName(const ScriptPropertyType iType) -> std::string_view {
	switch (iType) {
		case ScriptPropertyType::Float:
			return "float";
		case ScriptPropertyType::Int:
			return "int";
		case ScriptPropertyType::String:
			return "string";
		case ScriptPropertyType::Bool:
			return "bool";
		case ScriptPropertyType::Color:
			return "color";
		case ScriptPropertyType::Vec2:
			return "vec2";
		case ScriptPropertyType::Vec3:
			return "vec3";
		case ScriptPropertyType::Entity:
			return "entity";
		case ScriptPropertyType::Numbers:
			return "numbers";
		case ScriptPropertyType::Strings:
			return "strings";
	}
	return "float";
}

auto ScriptEngine::parsePropertyType(const std::string_view iName) -> std::optional<ScriptPropertyType> {
	for (const auto type:
		 {ScriptPropertyType::Float, ScriptPropertyType::Int, ScriptPropertyType::String, ScriptPropertyType::Bool,
		  ScriptPropertyType::Color, ScriptPropertyType::Vec2, ScriptPropertyType::Vec3, ScriptPropertyType::Entity,
		  ScriptPropertyType::Numbers, ScriptPropertyType::Strings}) {
		if (propertyTypeName(type) == iName)
			return type;
	}
	return std::nullopt;
}

auto ScriptEngine::defaultValueFor(const ScriptPropertyType iType) -> ScriptValue {
	switch (iType) {
		case ScriptPropertyType::Float:
			return 0.0f;
		case ScriptPropertyType::Int:
			return int64_t{0};
		case ScriptPropertyType::String:
			return std::string{};
		case ScriptPropertyType::Bool:
			return false;
		case ScriptPropertyType::Color:
			return std::array<float, 4>{1.f, 1.f, 1.f, 1.f};
		case ScriptPropertyType::Vec2:
			return std::array<float, 2>{};
		case ScriptPropertyType::Vec3:
			return std::array<float, 3>{};
		case ScriptPropertyType::Entity:
			return uint64_t{0};
		case ScriptPropertyType::Numbers:
			return std::vector<double>{};
		case ScriptPropertyType::Strings:
			return std::vector<std::string>{};
	}
	return 0.0f;
}

auto ScriptEngine::extractFunctions(const std::filesystem::path& iPath) -> std::vector<ScriptFunction> {
	const LuaEngine tempEngine;
	if (!tempEngine.isValid() || !tempEngine.loadScript(iPath))
		return {};
	return readFunctions(tempEngine.getState());
}

auto ScriptEngine::extractFunctionsFromBuffer(const std::vector<uint8_t>& iData, const std::string& iName)
		-> std::vector<ScriptFunction> {
	const LuaEngine tempEngine;
	if (!tempEngine.isValid() || !tempEngine.loadBuffer(iData, iName))
		return {};
	return readFunctions(tempEngine.getState());
}

auto ScriptEngine::getHookNames() -> const std::vector<std::string>& {
	static const std::vector<std::string> names{"on_create",    "on_update",        "on_tick",         "on_destroy",
												"on_collision", "on_trigger_enter", "on_trigger_exit", "on_triggered",
												"on_interact",  "on_timer"};
	return names;
}

auto ScriptEngine::extractProperties(const std::filesystem::path& iPath) -> std::vector<ScriptProperty> {
	const LuaEngine tempEngine;
	if (!tempEngine.isValid() || !tempEngine.loadScript(iPath))
		return {};
	return readProperties(tempEngine.getState());
}

auto ScriptEngine::extractPropertiesFromBuffer(const std::vector<uint8_t>& iData, const std::string& iName)
		-> std::vector<ScriptProperty> {
	const LuaEngine tempEngine;
	if (!tempEngine.isValid() || !tempEngine.loadBuffer(iData, iName))
		return {};
	return readProperties(tempEngine.getState());
}

void ScriptEngine::setDefaultQuotas(const ScriptQuotas& iQuotas) { defaultQuotas() = iQuotas; }

auto ScriptEngine::getDefaultQuotas() -> ScriptQuotas { return defaultQuotas(); }

}// namespace owl::script
