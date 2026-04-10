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

namespace owl::script {

/**
 * @brief Private implementation of ScriptEngine.
 */
class ScriptEngine::Impl {
public:
	/// The shared Lua engine.
	LuaEngine engine;
	/// The active scene.
	scene::Scene* activeScene = nullptr;
};

uniq<ScriptEngine::Impl> ScriptEngine::s_impl;

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
	LuaEngine tempEngine;
	if (!tempEngine.isValid() || !tempEngine.loadScript(iPath))
		return {};
	auto* state = tempEngine.getState();
	lua_getglobal(state, "properties");
	if (lua_istable(state, -1) == 0) {
		lua_pop(state, 1);
		return {};
	}
	std::vector<ScriptProperty> props;
	lua_pushnil(state);
	while (lua_next(state, -2) != 0) {
		if (lua_istable(state, -1) != 0) {
			ScriptProperty prop;
			lua_getfield(state, -1, "name");
			if (lua_isstring(state, -1) != 0)
				prop.name = lua_tostring(state, -1);
			lua_pop(state, 1);

			lua_getfield(state, -1, "type");
			if (lua_isstring(state, -1) != 0) {
				const std::string typeName = lua_tostring(state, -1);
				if (typeName == "float")
					prop.type = ScriptPropertyType::Float;
				else if (typeName == "int")
					prop.type = ScriptPropertyType::Int;
				else if (typeName == "string")
					prop.type = ScriptPropertyType::String;
				else if (typeName == "bool")
					prop.type = ScriptPropertyType::Bool;
			}
			lua_pop(state, 1);

			lua_getfield(state, -1, "default");
			switch (prop.type) {
				case ScriptPropertyType::Float:
					prop.value = lua_isnumber(state, -1) != 0 ? static_cast<float>(lua_tonumber(state, -1)) : 0.0f;
					break;
				case ScriptPropertyType::Int:
					prop.value =
							lua_isinteger(state, -1) != 0 ? static_cast<int64_t>(lua_tointeger(state, -1)) : int64_t{0};
					break;
				case ScriptPropertyType::String:
					prop.value = lua_isstring(state, -1) != 0 ? std::string(lua_tostring(state, -1)) : std::string{};
					break;
				case ScriptPropertyType::Bool:
					prop.value = lua_isboolean(state, -1) != 0 ? (lua_toboolean(state, -1) != 0) : false;
					break;
			}
			lua_pop(state, 1);

			if (!prop.name.empty())
				props.push_back(std::move(prop));
		}
		lua_pop(state, 1);
	}
	lua_pop(state, 1);
	return props;
}

auto ScriptEngine::extractPropertiesFromBuffer(const std::vector<uint8_t>& iData, const std::string& iName)
		-> std::vector<ScriptProperty> {
	LuaEngine tempEngine;
	if (!tempEngine.isValid() || !tempEngine.loadBuffer(iData, iName))
		return {};
	auto* state = tempEngine.getState();
	lua_getglobal(state, "properties");
	if (lua_istable(state, -1) == 0) {
		lua_pop(state, 1);
		return {};
	}
	std::vector<ScriptProperty> props;
	lua_pushnil(state);
	while (lua_next(state, -2) != 0) {
		if (lua_istable(state, -1) != 0) {
			ScriptProperty prop;
			lua_getfield(state, -1, "name");
			if (lua_isstring(state, -1) != 0)
				prop.name = lua_tostring(state, -1);
			lua_pop(state, 1);

			lua_getfield(state, -1, "type");
			if (lua_isstring(state, -1) != 0) {
				const std::string typeName = lua_tostring(state, -1);
				if (typeName == "float")
					prop.type = ScriptPropertyType::Float;
				else if (typeName == "int")
					prop.type = ScriptPropertyType::Int;
				else if (typeName == "string")
					prop.type = ScriptPropertyType::String;
				else if (typeName == "bool")
					prop.type = ScriptPropertyType::Bool;
			}
			lua_pop(state, 1);

			lua_getfield(state, -1, "default");
			switch (prop.type) {
				case ScriptPropertyType::Float:
					prop.value = lua_isnumber(state, -1) != 0 ? static_cast<float>(lua_tonumber(state, -1)) : 0.0f;
					break;
				case ScriptPropertyType::Int:
					prop.value =
							lua_isinteger(state, -1) != 0 ? static_cast<int64_t>(lua_tointeger(state, -1)) : int64_t{0};
					break;
				case ScriptPropertyType::String:
					prop.value = lua_isstring(state, -1) != 0 ? std::string(lua_tostring(state, -1)) : std::string{};
					break;
				case ScriptPropertyType::Bool:
					prop.value = lua_isboolean(state, -1) != 0 ? (lua_toboolean(state, -1) != 0) : false;
					break;
			}
			lua_pop(state, 1);

			if (!prop.name.empty())
				props.push_back(std::move(prop));
		}
		lua_pop(state, 1);
	}
	lua_pop(state, 1);
	return props;
}

auto ScriptEngine::getActiveScene() -> scene::Scene* {
	if (!s_impl)
		return nullptr;
	return s_impl->activeScene;
}

}// namespace owl::script
