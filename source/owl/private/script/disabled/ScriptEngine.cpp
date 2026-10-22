/**
 * @file ScriptEngine.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "script/ScriptEngine.h"

#include <cstdint>

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
	static const std::vector<std::string> names{"on_create",       "on_update",    "on_tick",         "on_destroy",
												"on_collision",    "on_trigger_enter", "on_trigger_exit", "on_triggered",
												"on_interact",     "on_timer"};
	return names;
}

void ScriptEngine::setDefaultQuotas(const ScriptQuotas& iQuotas) { defaultQuotas() = iQuotas; }

auto ScriptEngine::getDefaultQuotas() -> ScriptQuotas { return defaultQuotas(); }

}// namespace owl::script
