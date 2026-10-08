/**
 * @file ScriptInstance.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "script/ScriptInstance.h"

#include <cstdint>

// Built instead of the Lua implementation when OWL_MODULE_SCRIPT is OFF: an instance never loads, every call is a no-op.

namespace owl::script {
struct ScriptInstance::Impl {
	// Quotas kept for getQuotas(), as the Lua implementation does.
	ScriptQuotas quotas = ScriptEngine::getDefaultQuotas();
	// Scene given to setScene(), kept for getScene().
	scene::Scene* boundScene = nullptr;
};

ScriptInstance::ScriptInstance() : mp_impl{mkUniq<Impl>()} {}

ScriptInstance::~ScriptInstance() = default;

ScriptInstance::ScriptInstance(ScriptInstance&& iOther) noexcept = default;

auto ScriptInstance::operator=(ScriptInstance&& iOther) noexcept -> ScriptInstance& = default;

void ScriptInstance::setEntityName([[maybe_unused]] const std::string& iEntityName) const {}

void ScriptInstance::setScene(scene::Scene* iScene) const {
	if (mp_impl)
		mp_impl->boundScene = iScene;
}

auto ScriptInstance::getScene() const -> scene::Scene* { return mp_impl ? mp_impl->boundScene : nullptr; }

auto ScriptInstance::create(const std::string& iScriptPath, [[maybe_unused]] const uint64_t iEntityId) const -> bool {
	OWL_CORE_WARN("ScriptInstance: Lua not built in (OWL_MODULE_SCRIPT=OFF), script '{}' not loaded.", iScriptPath)
	return false;
}

auto ScriptInstance::createFromBuffer([[maybe_unused]] const std::vector<uint8_t>& iData, const std::string& iName,
									  [[maybe_unused]] const uint64_t iEntityId) const -> bool {
	OWL_CORE_WARN("ScriptInstance: Lua not built in (OWL_MODULE_SCRIPT=OFF), script '{}' not loaded.", iName)
	return false;
}

auto ScriptInstance::isValid() const -> bool { return false; }

auto ScriptInstance::isDisabled() const -> bool { return false; }

void ScriptInstance::setQuotas(const ScriptQuotas& iQuotas) const {
	if (mp_impl)
		mp_impl->quotas = iQuotas;
}

auto ScriptInstance::getQuotas() const -> ScriptQuotas {
	return mp_impl ? mp_impl->quotas : ScriptEngine::getDefaultQuotas();
}

void ScriptInstance::onCreate() const {}

void ScriptInstance::onUpdate([[maybe_unused]] const float iDeltaTime) const {}

void ScriptInstance::onDestroy() const {}

void ScriptInstance::onCollision([[maybe_unused]] const uint64_t iOtherEntityId) const {}

auto ScriptInstance::callFunction([[maybe_unused]] const std::string& iName) const -> bool { return false; }

auto ScriptInstance::callFunction([[maybe_unused]] const std::string& iName,
								  [[maybe_unused]] const uint64_t iEntityId) const -> bool {
	return false;
}

void ScriptInstance::setProperty([[maybe_unused]] const std::string& iName, [[maybe_unused]] const float iValue) const {
}

void ScriptInstance::setProperty([[maybe_unused]] const std::string& iName,
								 [[maybe_unused]] const int64_t iValue) const {}

void ScriptInstance::setProperty([[maybe_unused]] const std::string& iName,
								 [[maybe_unused]] const std::string& iValue) const {}

void ScriptInstance::setProperty([[maybe_unused]] const std::string& iName, [[maybe_unused]] const bool iValue) const {}

auto ScriptInstance::getPropertyFloat([[maybe_unused]] const std::string& iName) const -> std::optional<float> {
	return std::nullopt;
}

auto ScriptInstance::getPropertyInt([[maybe_unused]] const std::string& iName) const -> std::optional<int64_t> {
	return std::nullopt;
}

auto ScriptInstance::getPropertyString([[maybe_unused]] const std::string& iName) const -> std::optional<std::string> {
	return std::nullopt;
}

auto ScriptInstance::getPropertyBool([[maybe_unused]] const std::string& iName) const -> std::optional<bool> {
	return std::nullopt;
}

}// namespace owl::script
