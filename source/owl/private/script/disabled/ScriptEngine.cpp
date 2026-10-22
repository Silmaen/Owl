/**
 * @file ScriptEngine.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "script/ScriptEngine.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// Built instead of the Lua implementation when OWL_MODULE_SCRIPT is OFF: the scene still registers, no script loads.

namespace owl::script {
namespace {

auto defaultQuotas() -> ScriptQuotas& {
	static ScriptQuotas quotas;
	return quotas;
}

}// namespace

auto ScriptEngine::extractProperties(const std::filesystem::path& iPath) -> std::vector<ScriptProperty> {
	OWL_CORE_WARN("ScriptEngine: Lua not built in (OWL_MODULE_SCRIPT=OFF), no property read from '{}'.", iPath.string())
	return {};
}

auto ScriptEngine::extractPropertiesFromBuffer([[maybe_unused]] const std::vector<uint8_t>& iData,
											   const std::string& iName) -> std::vector<ScriptProperty> {
	OWL_CORE_WARN("ScriptEngine: Lua not built in (OWL_MODULE_SCRIPT=OFF), no property read from '{}'.", iName)
	return {};
}

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
	OWL_CORE_WARN("ScriptEngine: Lua not built in (OWL_MODULE_SCRIPT=OFF), no function read from '{}'.", iPath.string())
	return {};
}

auto ScriptEngine::extractFunctionsFromBuffer([[maybe_unused]] const std::vector<uint8_t>& iData,
											  const std::string& iName) -> std::vector<ScriptFunction> {
	OWL_CORE_WARN("ScriptEngine: Lua not built in (OWL_MODULE_SCRIPT=OFF), no function read from '{}'.", iName)
	return {};
}

auto ScriptEngine::getHookNames() -> const std::vector<std::string>& {
	static const std::vector<std::string> names{"on_create",    "on_update",        "on_tick",         "on_destroy",
												"on_collision", "on_trigger_enter", "on_trigger_exit", "on_triggered",
												"on_interact",  "on_timer"};
	return names;
}

void ScriptEngine::setDefaultQuotas(const ScriptQuotas& iQuotas) { defaultQuotas() = iQuotas; }

auto ScriptEngine::getDefaultQuotas() -> ScriptQuotas { return defaultQuotas(); }

}// namespace owl::script
