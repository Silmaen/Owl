/**
 * @file SceneSettingsCommands.cpp
 * @author Silmaen
 * @date 06/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "SceneSettingsCommands.h"

#include <optional>
#include <utility>

namespace owl::nest::commands {

ModifyEnabledRenderersCommand::ModifyEnabledRenderersCommand(std::string iBeforeYaml, std::string iAfterYaml)
	: m_beforeYaml{std::move(iBeforeYaml)}, m_afterYaml{std::move(iAfterYaml)} {}

ModifyEnabledRenderersCommand::~ModifyEnabledRenderersCommand() = default;

void ModifyEnabledRenderersCommand::undo(scene::Scene& ioScene) { apply(ioScene, m_beforeYaml); }

void ModifyEnabledRenderersCommand::redo(scene::Scene& ioScene) { apply(ioScene, m_afterYaml); }

auto ModifyEnabledRenderersCommand::description() const -> std::string { return "Edit scene renderers"; }

auto ModifyEnabledRenderersCommand::mergeWith(const SceneUndoCommand& iOther) -> bool {
	const auto* other = dynamic_cast<const ModifyEnabledRenderersCommand*>(&iOther);
	if (other == nullptr)
		return false;
	// Keep our "before", absorb the newer "after" + timestamp.
	m_afterYaml = other->m_afterYaml;
	m_timestamp = other->m_timestamp;
	return true;
}

auto ModifyEnabledRenderersCommand::typeId() const -> size_t {
	constexpr size_t kTypeHash = 0x4D6F644552656E64ULL;// "ModERend"
	return kTypeHash;
}

void ModifyEnabledRenderersCommand::apply(scene::Scene& ioScene, const std::string& iYaml) {
	if (iYaml.empty()) {
		ioScene.getEnabledRenderers() = renderer::EnabledRenderersConfig{};
		return;
	}
	ioScene.getEnabledRenderers() = renderer::EnabledRenderersConfig::fromYaml(iYaml);
}

ModifyPhysicsSettingsCommand::ModifyPhysicsSettingsCommand(const scene::PhysicsSettings& iBefore,
														   const scene::PhysicsSettings& iAfter)
	: m_before{iBefore}, m_after{iAfter} {}

ModifyPhysicsSettingsCommand::~ModifyPhysicsSettingsCommand() = default;

void ModifyPhysicsSettingsCommand::undo(scene::Scene& ioScene) { ioScene.getPhysicsSettings() = m_before; }

void ModifyPhysicsSettingsCommand::redo(scene::Scene& ioScene) { ioScene.getPhysicsSettings() = m_after; }

auto ModifyPhysicsSettingsCommand::description() const -> std::string { return "Edit scene physics"; }

auto ModifyPhysicsSettingsCommand::mergeWith(const SceneUndoCommand& iOther) -> bool {
	const auto* other = dynamic_cast<const ModifyPhysicsSettingsCommand*>(&iOther);
	if (other == nullptr)
		return false;
	m_after = other->m_after;
	m_timestamp = other->m_timestamp;
	return true;
}

auto ModifyPhysicsSettingsCommand::typeId() const -> size_t {
	constexpr size_t kTypeHash = 0x4D6F645068797353ULL;// "ModPhysS"
	return kTypeHash;
}

SetSceneHudCommand::SetSceneHudCommand(std::optional<std::string> iBefore, std::optional<std::string> iAfter)
	: m_before{std::move(iBefore)}, m_after{std::move(iAfter)} {}

SetSceneHudCommand::~SetSceneHudCommand() = default;

void SetSceneHudCommand::undo(scene::Scene& ioScene) { ioScene.setHudPath(m_before); }

void SetSceneHudCommand::redo(scene::Scene& ioScene) { ioScene.setHudPath(m_after); }

auto SetSceneHudCommand::description() const -> std::string {
	if (!m_after)
		return "Use the default HUD";
	return m_after->empty() ? "Remove the HUD" : "Set the HUD";
}

auto SetSceneHudCommand::typeId() const -> size_t {
	constexpr size_t kTypeHash = 0x536574487564ULL;// "SetHud"
	return kTypeHash;
}

SetSceneScriptCommand::SetSceneScriptCommand(std::string iBefore, std::string iAfter)
	: m_before{std::move(iBefore)}, m_after{std::move(iAfter)} {}

SetSceneScriptCommand::~SetSceneScriptCommand() = default;

void SetSceneScriptCommand::undo(scene::Scene& ioScene) { apply(ioScene, m_before); }

void SetSceneScriptCommand::redo(scene::Scene& ioScene) { apply(ioScene, m_after); }

auto SetSceneScriptCommand::description() const -> std::string {
	return m_after.empty() ? "Remove the scene script" : "Edit the scene script";
}

auto SetSceneScriptCommand::mergeWith(const SceneUndoCommand& iOther) -> bool {
	const auto* other = dynamic_cast<const SetSceneScriptCommand*>(&iOther);
	if (other == nullptr)
		return false;
	m_after = other->m_after;
	m_timestamp = other->m_timestamp;
	return true;
}

auto SetSceneScriptCommand::typeId() const -> size_t {
	constexpr size_t kTypeHash = 0x536574536353ULL;// "SetScS"
	return kTypeHash;
}

void SetSceneScriptCommand::apply(scene::Scene& ioScene, const std::string& iYaml) {
	auto script = iYaml.empty() ? std::optional<scene::component::LuaScript>{} : scene::component::LuaScript::fromYaml(iYaml);
	ioScene.getSceneScript() = script ? std::move(*script) : scene::component::LuaScript{};
}

}// namespace owl::nest::commands
