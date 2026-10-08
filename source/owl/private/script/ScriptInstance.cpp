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

#include <cstdint>
#include <format>
#include <string>
#include <string_view>

namespace owl::script {
struct ScriptInstance::Impl {
	// Per-instance Lua engine (isolated state).
	LuaEngine engine;
	// Whether the script has been successfully loaded.
	bool loaded = false;
	// Entity UUID.
	uint64_t entityId = 0;
	// Set once a callback exceeded a quota: no callback runs any more.
	bool disabled = false;
	// Script path or chunk name, for diagnostics.
	std::string name;
	// Tag of the owning entity, for diagnostics.
	std::string entityName;

	[[nodiscard]] auto owner() const -> std::string {
		return entityName.empty() ? std::format("entity {}", entityId)
								  : std::format("entity '{}' ({})", entityName, entityId);
	}

	void bind(const std::string& iName, const uint64_t iEntityId) {
		entityId = iEntityId;
		name = iName;
		engine.setErrorContext(std::format("script '{}' on {}", name, owner()));
	}

	void checkQuota(const std::string_view iFunction, const bool iCalled) {
		if (iCalled)
			return;
		if (const auto status = engine.getLastStatus();
			status == LuaStatus::MemoryQuota || status == LuaStatus::TimeQuota) {
			disabled = true;
			OWL_CORE_ERROR("ScriptInstance: Script '{}' on {} disabled, '{}' exceeded its {} quota. Fix: {}.", name,
						   owner(), iFunction, status == LuaStatus::MemoryQuota ? "memory" : "time", fixHint(status))
		}
	}
};

ScriptInstance::ScriptInstance() : mp_impl{mkUniq<Impl>()} {}

ScriptInstance::~ScriptInstance() = default;

ScriptInstance::ScriptInstance(ScriptInstance&& iOther) noexcept = default;

auto ScriptInstance::operator=(ScriptInstance&& iOther) noexcept -> ScriptInstance& = default;

void ScriptInstance::setEntityName(const std::string& iEntityName) const {
	if (mp_impl)
		mp_impl->entityName = iEntityName;
}

auto ScriptInstance::create(const std::string& iScriptPath, const uint64_t iEntityId) const -> bool {
	OWL_PROFILE_FUNCTION()

	if (!mp_impl->engine.isValid())
		return false;
	// Register bindings in this instance's state.
	registerBindings(mp_impl->engine.getState());
	// Store entity_id as a global.
	mp_impl->engine.setGlobal("entity_id", static_cast<int64_t>(iEntityId));
	mp_impl->bind(iScriptPath, iEntityId);
	if (!mp_impl->engine.loadScript(iScriptPath)) {
		OWL_CORE_ERROR("ScriptInstance: Failed to load script '{}' on {}.", iScriptPath, mp_impl->owner())
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
	mp_impl->bind(iName, iEntityId);
	if (!mp_impl->engine.loadBuffer(iData, iName)) {
		OWL_CORE_ERROR("ScriptInstance: Failed to load buffer '{}' on {}.", iName, mp_impl->owner())
		return false;
	}
	mp_impl->loaded = true;
	return true;
}

auto ScriptInstance::isValid() const -> bool { return mp_impl && mp_impl->loaded && mp_impl->engine.isValid(); }

auto ScriptInstance::isDisabled() const -> bool { return mp_impl && mp_impl->disabled; }

void ScriptInstance::setQuotas(const ScriptQuotas& iQuotas) const {
	if (mp_impl)
		mp_impl->engine.setQuotas(iQuotas);
}

auto ScriptInstance::getQuotas() const -> ScriptQuotas {
	return mp_impl ? mp_impl->engine.getQuotas() : ScriptEngine::getDefaultQuotas();
}

void ScriptInstance::onCreate() const {
	if (!isValid() || mp_impl->disabled)
		return;
	mp_impl->checkQuota("on_create", mp_impl->engine.callFunction("on_create"));
}

void ScriptInstance::onUpdate(const float iDeltaTime) const {
	if (!isValid() || mp_impl->disabled)
		return;
	// Store delta time in Lua registry for the time.delta() binding.
	lua_pushnumber(mp_impl->engine.getState(), static_cast<lua_Number>(iDeltaTime));
	lua_setfield(mp_impl->engine.getState(), LUA_REGISTRYINDEX, "owl_dt");
	mp_impl->checkQuota("on_update", mp_impl->engine.callFunction("on_update", iDeltaTime));
}

void ScriptInstance::onDestroy() const {
	if (!isValid() || mp_impl->disabled)
		return;
	mp_impl->checkQuota("on_destroy", mp_impl->engine.callFunction("on_destroy"));
}

void ScriptInstance::onCollision(const uint64_t iOtherEntityId) const {
	if (!isValid() || mp_impl->disabled)
		return;
	mp_impl->checkQuota("on_collision", mp_impl->engine.callFunction("on_collision", iOtherEntityId));
}

auto ScriptInstance::callFunction(const std::string& iName) const -> bool {
	if (!isValid() || mp_impl->disabled)
		return false;
	const bool called = mp_impl->engine.callFunction(iName);
	mp_impl->checkQuota(iName, called);
	return called;
}

auto ScriptInstance::callFunction(const std::string& iName, const uint64_t iEntityId) const -> bool {
	if (!isValid())
		return false;
	return mp_impl->engine.callFunction(iName, iEntityId);
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
