/**
 * @file ScriptInstance.cpp
 * @author Silmaen
 * @date 09/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "script/ScriptInstance.h"

#include "core/external/lua.h"
#include "script/LuaBindings.h"
#include "script/LuaEngine.h"

namespace owl::script {
/**
 * @brief
 *  Private implementation of ScriptInstance.
 *
 * Each instance owns its own LuaEngine (isolated state) so that
 * global variables in one script don't pollute another entity's state.
 */
struct ScriptInstance::Impl {
	/// Per-instance Lua engine (isolated state).
	LuaEngine engine;
	/// Whether the script has been successfully loaded.
	bool loaded = false;
	/// Entity UUID.
	uint64_t entityId = 0;
};

ScriptInstance::ScriptInstance() : mp_impl{mkUniq<Impl>()} {}

ScriptInstance::~ScriptInstance() = default;

ScriptInstance::ScriptInstance(ScriptInstance&& iOther) noexcept = default;

auto ScriptInstance::operator=(ScriptInstance&& iOther) noexcept -> ScriptInstance& = default;

auto ScriptInstance::create(const std::string& iScriptPath, const uint64_t iEntityId) const -> bool {
	OWL_PROFILE_FUNCTION()

	if (!mp_impl->engine.isValid())
		return false;
	// Register bindings in this instance's state.
	registerBindings(mp_impl->engine.getState());
	// Store entity_id as a global.
	mp_impl->engine.setGlobal("entity_id", static_cast<int64_t>(iEntityId));
	mp_impl->entityId = iEntityId;
	if (!mp_impl->engine.loadScript(iScriptPath)) {
		OWL_CORE_ERROR("ScriptInstance: Failed to load script '{}'.", iScriptPath)
		return false;
	}
	mp_impl->loaded = true;
	return true;
}

auto ScriptInstance::createFromBuffer(const std::vector<uint8_t>& iData, const std::string& iName,
									  const uint64_t iEntityId) const -> bool {
	OWL_PROFILE_FUNCTION()

	if (!mp_impl->engine.isValid())
		return false;
	registerBindings(mp_impl->engine.getState());
	mp_impl->engine.setGlobal("entity_id", static_cast<int64_t>(iEntityId));
	mp_impl->entityId = iEntityId;
	if (!mp_impl->engine.loadBuffer(iData, iName)) {
		OWL_CORE_ERROR("ScriptInstance: Failed to load buffer '{}'.", iName)
		return false;
	}
	mp_impl->loaded = true;
	return true;
}

auto ScriptInstance::isValid() const -> bool { return mp_impl && mp_impl->loaded && mp_impl->engine.isValid(); }

void ScriptInstance::onCreate() const {
	if (!isValid())
		return;
	std::ignore = mp_impl->engine.callFunction("on_create");
}

void ScriptInstance::onUpdate(const float iDeltaTime) const {
	if (!isValid())
		return;
	// Store delta time in Lua registry for the time.delta() binding.
	lua_pushnumber(mp_impl->engine.getState(), static_cast<lua_Number>(iDeltaTime));
	lua_setfield(mp_impl->engine.getState(), LUA_REGISTRYINDEX, "owl_dt");
	std::ignore = mp_impl->engine.callFunction("on_update", iDeltaTime);
}

void ScriptInstance::onDestroy() const {
	if (!isValid())
		return;
	std::ignore = mp_impl->engine.callFunction("on_destroy");
}

void ScriptInstance::onCollision(const uint64_t iOtherEntityId) const {
	if (!isValid())
		return;
	std::ignore = mp_impl->engine.callFunction("on_collision", iOtherEntityId);
}

auto ScriptInstance::callFunction(const std::string& iName) const -> bool {
	if (!isValid())
		return false;
	return mp_impl->engine.callFunction(iName);
}

// ---- Property access ----
void ScriptInstance::setProperty(const std::string& iName, const float iValue) const {
	if (!isValid())
		return;
	mp_impl->engine.setGlobal(iName, iValue);
}

void ScriptInstance::setProperty(const std::string& iName, const int64_t iValue) const {
	if (!isValid())
		return;
	mp_impl->engine.setGlobal(iName, iValue);
}

void ScriptInstance::setProperty(const std::string& iName, const std::string& iValue) const {
	if (!isValid())
		return;
	mp_impl->engine.setGlobal(iName, iValue);
}

void ScriptInstance::setProperty(const std::string& iName, const bool iValue) const {
	if (!isValid())
		return;
	mp_impl->engine.setGlobal(iName, iValue);
}

auto ScriptInstance::getPropertyFloat(const std::string& iName) const -> std::optional<float> {
	if (!isValid())
		return std::nullopt;
	return mp_impl->engine.getGlobalFloat(iName);
}

auto ScriptInstance::getPropertyInt(const std::string& iName) const -> std::optional<int64_t> {
	if (!isValid())
		return std::nullopt;
	return mp_impl->engine.getGlobalInt(iName);
}

auto ScriptInstance::getPropertyString(const std::string& iName) const -> std::optional<std::string> {
	if (!isValid())
		return std::nullopt;
	return mp_impl->engine.getGlobalString(iName);
}

auto ScriptInstance::getPropertyBool(const std::string& iName) const -> std::optional<bool> {
	if (!isValid())
		return std::nullopt;
	return mp_impl->engine.getGlobalBool(iName);
}

}// namespace owl::script
