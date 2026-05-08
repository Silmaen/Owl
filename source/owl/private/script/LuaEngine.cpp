/**
 * @file LuaEngine.cpp
 * @author Silmaen
 * @date 09/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "LuaEngine.h"

#include "core/external/lua.h"

namespace owl::script {

LuaEngine::LuaEngine() : mp_state{luaL_newstate()} {

	OWL_PROFILE_FUNCTION()

	if (mp_state == nullptr) {
		OWL_CORE_ERROR("LuaEngine: Failed to create Lua state.")
		return;
	}

	// Open only safe standard libraries (no io, no os, no loadfile/dofile for sandboxing).
	luaL_requiref(mp_state, LUA_GNAME, luaopen_base, 1);

	lua_pop(mp_state, 1);

	luaL_requiref(mp_state, LUA_TABLIBNAME, luaopen_table, 1);

	lua_pop(mp_state, 1);

	luaL_requiref(mp_state, LUA_STRLIBNAME, luaopen_string, 1);

	lua_pop(mp_state, 1);

	luaL_requiref(mp_state, LUA_MATHLIBNAME, luaopen_math, 1);

	lua_pop(mp_state, 1);

	luaL_requiref(mp_state, LUA_UTF8LIBNAME, luaopen_utf8, 1);

	lua_pop(mp_state, 1);

	luaL_requiref(mp_state, LUA_COLIBNAME, luaopen_coroutine, 1);

	lua_pop(mp_state, 1);

	// Remove dangerous base functions.
	lua_getglobal(mp_state, "_G");
	for (const auto* func: {"dofile", "loadfile"}) {
		lua_pushnil(mp_state);

		lua_setfield(mp_state, -2, func);
	}

	lua_pop(mp_state, 1);

	OWL_CORE_TRACE("LuaEngine: Lua state created successfully.")
}

LuaEngine::~LuaEngine() {
	if (mp_state != nullptr) {
		lua_close(mp_state);
		mp_state = nullptr;
	}
}

auto LuaEngine::isValid() const -> bool { return mp_state != nullptr; }

auto LuaEngine::loadScript(const std::filesystem::path& iPath) const -> bool {
	OWL_PROFILE_FUNCTION()

	if (mp_state == nullptr) {
		OWL_CORE_ERROR("LuaEngine: Cannot load script, Lua state is invalid.")
		return false;
	}
	if (const int result = luaL_dofile(mp_state, iPath.string().c_str()); result != LUA_OK) {
		OWL_CORE_ERROR("LuaEngine: Error loading '{}': {}", iPath.string(), lua_tostring(mp_state, -1))
		lua_pop(mp_state, 1);
		return false;
	}
	return true;
}

auto LuaEngine::loadBuffer(const std::vector<uint8_t>& iData, const std::string& iName) const -> bool {
	OWL_PROFILE_FUNCTION()

	if (mp_state == nullptr) {
		OWL_CORE_ERROR("LuaEngine: Cannot load buffer, Lua state is invalid.")
		return false;
	}
	const auto* data = reinterpret_cast<const char*>(iData.data());
	if (const int result = luaL_loadbuffer(mp_state, data, iData.size(), iName.c_str()); result != LUA_OK) {
		OWL_CORE_ERROR("LuaEngine: Error loading buffer '{}': {}", iName, lua_tostring(mp_state, -1))
		lua_pop(mp_state, 1);
		return false;
	}
	// Execute the loaded chunk.
	if (const int result = lua_pcall(mp_state, 0, LUA_MULTRET, 0); result != LUA_OK) {
		OWL_CORE_ERROR("LuaEngine: Error executing buffer '{}': {}", iName, lua_tostring(mp_state, -1))
		lua_pop(mp_state, 1);
		return false;
	}
	return true;
}

auto LuaEngine::hasFunction(const std::string& iName) const -> bool {
	if (mp_state == nullptr)
		return false;
	lua_getglobal(mp_state, iName.c_str());
	const bool isFunc = lua_isfunction(mp_state, -1) != 0;
	lua_pop(mp_state, 1);
	return isFunc;
}

auto LuaEngine::callFunction(const std::string& iName) const -> bool {
	OWL_PROFILE_FUNCTION()

	if (mp_state == nullptr)
		return false;
	lua_getglobal(mp_state, iName.c_str());
	if (lua_isfunction(mp_state, -1) == 0) {
		lua_pop(mp_state, 1);
		return false;
	}
	if (const int result = lua_pcall(mp_state, 0, 0, 0); result != LUA_OK) {
		OWL_CORE_ERROR("LuaEngine: Error calling '{}': {}", iName, lua_tostring(mp_state, -1))
		lua_pop(mp_state, 1);
		return false;
	}
	return true;
}

auto LuaEngine::callFunction(const std::string& iName, const float iArg) const -> bool {
	OWL_PROFILE_FUNCTION()

	if (mp_state == nullptr)
		return false;
	lua_getglobal(mp_state, iName.c_str());
	if (lua_isfunction(mp_state, -1) == 0) {
		lua_pop(mp_state, 1);
		return false;
	}
	lua_pushnumber(mp_state, static_cast<lua_Number>(iArg));
	if (const int result = lua_pcall(mp_state, 1, 0, 0); result != LUA_OK) {
		OWL_CORE_ERROR("LuaEngine: Error calling '{}': {}", iName, lua_tostring(mp_state, -1))
		lua_pop(mp_state, 1);
		return false;
	}
	return true;
}

auto LuaEngine::callFunction(const std::string& iName, const uint64_t iArg) const -> bool {
	OWL_PROFILE_FUNCTION()

	if (mp_state == nullptr)
		return false;
	lua_getglobal(mp_state, iName.c_str());
	if (lua_isfunction(mp_state, -1) == 0) {
		lua_pop(mp_state, 1);
		return false;
	}
	lua_pushinteger(mp_state, static_cast<lua_Integer>(iArg));
	if (const int result = lua_pcall(mp_state, 1, 0, 0); result != LUA_OK) {
		OWL_CORE_ERROR("LuaEngine: Error calling '{}': {}", iName, lua_tostring(mp_state, -1))
		lua_pop(mp_state, 1);
		return false;
	}
	return true;
}

// ---- Global variable getters ----
auto LuaEngine::getGlobalFloat(const std::string& iName) const -> std::optional<float> {
	if (mp_state == nullptr)
		return std::nullopt;
	lua_getglobal(mp_state, iName.c_str());
	if (lua_isnumber(mp_state, -1) == 0) {
		lua_pop(mp_state, 1);
		return std::nullopt;
	}
	const auto val = static_cast<float>(lua_tonumber(mp_state, -1));
	lua_pop(mp_state, 1);
	return val;
}

auto LuaEngine::getGlobalInt(const std::string& iName) const -> std::optional<int64_t> {
	if (mp_state == nullptr)
		return std::nullopt;
	lua_getglobal(mp_state, iName.c_str());
	if (lua_isinteger(mp_state, -1) == 0) {
		lua_pop(mp_state, 1);
		return std::nullopt;
	}
	const auto val = static_cast<int64_t>(lua_tointeger(mp_state, -1));
	lua_pop(mp_state, 1);
	return val;
}

auto LuaEngine::getGlobalString(const std::string& iName) const -> std::optional<std::string> {
	if (mp_state == nullptr)
		return std::nullopt;
	lua_getglobal(mp_state, iName.c_str());
	if (lua_isstring(mp_state, -1) == 0) {
		lua_pop(mp_state, 1);
		return std::nullopt;
	}
	std::string val = lua_tostring(mp_state, -1);
	lua_pop(mp_state, 1);
	return val;
}

auto LuaEngine::getGlobalBool(const std::string& iName) const -> std::optional<bool> {
	if (mp_state == nullptr)
		return std::nullopt;
	lua_getglobal(mp_state, iName.c_str());
	if (lua_isboolean(mp_state, -1) == 0) {
		lua_pop(mp_state, 1);
		return std::nullopt;
	}
	const bool val = lua_toboolean(mp_state, -1) != 0;
	lua_pop(mp_state, 1);
	return val;
}

// ---- Global variable setters ----
void LuaEngine::setGlobal(const std::string& iName, const float iValue) const {
	if (mp_state == nullptr)
		return;
	lua_pushnumber(mp_state, static_cast<lua_Number>(iValue));
	lua_setglobal(mp_state, iName.c_str());
}

void LuaEngine::setGlobal(const std::string& iName, const int64_t iValue) const {
	if (mp_state == nullptr)
		return;
	lua_pushinteger(mp_state, static_cast<lua_Integer>(iValue));
	lua_setglobal(mp_state, iName.c_str());
}

void LuaEngine::setGlobal(const std::string& iName, const std::string& iValue) const {
	if (mp_state == nullptr)
		return;
	lua_pushstring(mp_state, iValue.c_str());
	lua_setglobal(mp_state, iName.c_str());
}

void LuaEngine::setGlobal(const std::string& iName, const bool iValue) const {
	if (mp_state == nullptr)
		return;
	lua_pushboolean(mp_state, iValue ? 1 : 0);
	lua_setglobal(mp_state, iName.c_str());
}

auto LuaEngine::getState() const -> lua_State* { return mp_state; }

}// namespace owl::script
