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
#include "script/LuaBindings.h"
#include "script/LuaEngine.h"

#include <cstdint>

namespace owl::script {
class ScriptEngine::Impl {
public:
	// The shared Lua engine.
	LuaEngine engine;
	// The active scene.
	scene::Scene* activeScene = nullptr;
};

uniq<ScriptEngine::Impl> ScriptEngine::s_impl;

namespace {

auto defaultQuotas() -> ScriptQuotas& {
	static ScriptQuotas quotas;
	return quotas;
}

// Raw reads only: a script-provided metatable on the properties table must not run outside a protected call.
auto rawField(lua_State* iState, const int iTableIndex, const char* iKey) -> int {
	const int tableIndex = lua_absindex(iState, iTableIndex);
	lua_pushstring(iState, iKey);
	return lua_rawget(iState, tableIndex);
}

auto parsePropertyType(lua_State* iState) -> ScriptPropertyType {
	if (lua_isstring(iState, -1) == 0)
		return ScriptPropertyType::Float;
	const std::string_view typeName = lua_tostring(iState, -1);
	if (typeName == "int")
		return ScriptPropertyType::Int;
	if (typeName == "string")
		return ScriptPropertyType::String;
	if (typeName == "bool")
		return ScriptPropertyType::Bool;
	return ScriptPropertyType::Float;
}

auto parseProperty(lua_State* iState, const int iEntryIndex) -> ScriptProperty {
	ScriptProperty prop;
	rawField(iState, iEntryIndex, "name");
	if (lua_isstring(iState, -1) != 0)
		prop.name = lua_tostring(iState, -1);
	lua_pop(iState, 1);
	rawField(iState, iEntryIndex, "type");
	prop.type = parsePropertyType(iState);
	lua_pop(iState, 1);
	rawField(iState, iEntryIndex, "default");
	switch (prop.type) {
		case ScriptPropertyType::Float:
			prop.value = lua_isnumber(iState, -1) != 0 ? static_cast<float>(lua_tonumber(iState, -1)) : 0.0f;
			break;
		case ScriptPropertyType::Int:
			prop.value = lua_isinteger(iState, -1) != 0 ? static_cast<int64_t>(lua_tointeger(iState, -1)) : int64_t{0};
			break;
		case ScriptPropertyType::String:
			prop.value = lua_isstring(iState, -1) != 0 ? std::string(lua_tostring(iState, -1)) : std::string{};
			break;
		case ScriptPropertyType::Bool:
			prop.value = lua_isboolean(iState, -1) != 0 ? lua_toboolean(iState, -1) != 0 : false;
			break;
	}
	lua_pop(iState, 1);
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

}// namespace

void ScriptEngine::init(scene::Scene* iScene) {
	OWL_PROFILE_FUNCTION()

	s_impl = mkUniq<Impl>();
	s_impl->activeScene = iScene;
	if (!s_impl->engine.isValid()) {
		OWL_CORE_ERROR("ScriptEngine: Failed to initialize Lua engine.")
		s_impl.reset();
		return;
	}
	registerBindings(s_impl->engine.getState());
	OWL_CORE_TRACE("ScriptEngine: Initialized.")
}

void ScriptEngine::shutdown() {
	OWL_PROFILE_FUNCTION()

	s_impl.reset();
	OWL_CORE_TRACE("ScriptEngine: Shut down.")
}

auto ScriptEngine::isInitialized() -> bool { return s_impl != nullptr && s_impl->engine.isValid(); }

auto ScriptEngine::loadScript(const std::filesystem::path& iPath) -> bool {
	if (!isInitialized())
		return false;
	return s_impl->engine.loadScript(iPath);
}

auto ScriptEngine::loadScriptFromBuffer(const std::vector<uint8_t>& iData, const std::string& iName) -> bool {
	if (!isInitialized())
		return false;
	return s_impl->engine.loadBuffer(iData, iName);
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

auto ScriptEngine::getActiveScene() -> scene::Scene* {
	if (!s_impl)
		return nullptr;
	return s_impl->activeScene;
}

}// namespace owl::script
