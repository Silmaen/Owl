/**
 * @file SceneSettingsCommands.cpp
 * @author Silmaen
 * @date 06/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "SceneSettingsCommands.h"

#include <scene/Entity.h>
#include <scene/component/Hierarchy.h>
#include <scene/component/Transform.h>
#include <scene/component/WorldPosition.h>
#include <sound/SoundCommand.h>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <format>
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

namespace {

auto getTouchedParts(const sound::MixSettings& iBefore, const sound::MixSettings& iAfter) -> uint32_t {
	uint32_t touched = 0;
	for (size_t index = 0; index < sound::g_busCount; ++index) {
		if (iBefore.buses.at(index) != iAfter.buses.at(index))
			touched |= 1u << index;
	}
	if (std::bit_cast<uint32_t>(iBefore.dopplerFactor) != std::bit_cast<uint32_t>(iAfter.dopplerFactor))
		touched |= 1u << sound::g_busCount;
	return touched;
}

}// namespace

SetMixCommand::SetMixCommand(const sound::MixSettings& iBefore, const sound::MixSettings& iAfter)
	: m_before{iBefore}, m_after{iAfter}, m_touched{getTouchedParts(iBefore, iAfter)} {}

SetMixCommand::~SetMixCommand() = default;

void SetMixCommand::undo(scene::Scene& ioScene) {
	ioScene.getMixSettings() = m_before;
	if (auto* mixer = sound::SoundCommand::getMixer(); mixer != nullptr)
		mixer->setMix(m_before);
}

void SetMixCommand::redo(scene::Scene& ioScene) {
	ioScene.getMixSettings() = m_after;
	if (auto* mixer = sound::SoundCommand::getMixer(); mixer != nullptr)
		mixer->setMix(m_after);
}

auto SetMixCommand::description() const -> std::string { return "Edit the scene mix"; }

auto SetMixCommand::mergeWith(const SceneUndoCommand& iOther) -> bool {
	const auto* other = dynamic_cast<const SetMixCommand*>(&iOther);
	if (other == nullptr || other->m_touched != m_touched)
		return false;
	m_after = other->m_after;
	m_timestamp = other->m_timestamp;
	return true;
}

auto SetMixCommand::typeId() const -> size_t {
	constexpr size_t kTypeHash = 0x5365744d6978ULL;// "SetMix"
	return kTypeHash;
}

SetWorldPartitionCommand::SetWorldPartitionCommand(scene::WorldPartitionSettings iBefore,
												   scene::WorldPartitionSettings iAfter, std::string iDescription)
	: m_before{std::move(iBefore)}, m_after{std::move(iAfter)}, m_description{std::move(iDescription)} {}

SetWorldPartitionCommand::~SetWorldPartitionCommand() = default;

void SetWorldPartitionCommand::undo(scene::Scene& ioScene) { ioScene.getWorldPartitionSettings() = m_before; }

void SetWorldPartitionCommand::redo(scene::Scene& ioScene) { ioScene.getWorldPartitionSettings() = m_after; }

auto SetWorldPartitionCommand::description() const -> std::string { return m_description; }

auto SetWorldPartitionCommand::mergeWith(const SceneUndoCommand& iOther) -> bool {
	const auto* other = dynamic_cast<const SetWorldPartitionCommand*>(&iOther);
	if (other == nullptr || other->m_before.cells != other->m_after.cells || m_before.cells != m_after.cells)
		return false;
	m_after = other->m_after;
	m_timestamp = other->m_timestamp;
	return true;
}

auto SetWorldPartitionCommand::typeId() const -> size_t {
	constexpr size_t kTypeHash = 0x536574576f50ULL;// "SetWoP"
	return kTypeHash;
}

SetFloatingOriginCommand::SetFloatingOriginCommand(const scene::FloatingOriginSettings& iBefore,
												   const scene::FloatingOriginSettings& iAfter)
	: m_before{iBefore}, m_after{iAfter} {}

SetFloatingOriginCommand::~SetFloatingOriginCommand() = default;

void SetFloatingOriginCommand::undo(scene::Scene& ioScene) { ioScene.getFloatingOriginSettings() = m_before; }

void SetFloatingOriginCommand::redo(scene::Scene& ioScene) { ioScene.getFloatingOriginSettings() = m_after; }

auto SetFloatingOriginCommand::description() const -> std::string { return "Edit the scene floating origin"; }

auto SetFloatingOriginCommand::mergeWith(const SceneUndoCommand& iOther) -> bool {
	const auto* other = dynamic_cast<const SetFloatingOriginCommand*>(&iOther);
	if (other == nullptr)
		return false;
	m_after = other->m_after;
	m_timestamp = other->m_timestamp;
	return true;
}

auto SetFloatingOriginCommand::typeId() const -> size_t {
	constexpr size_t kTypeHash = 0x536574466c4fULL;// "SetFlO"
	return kTypeHash;
}

SetWorldOriginCommand::SetWorldOriginCommand(const math::vec3d& iBefore, const math::vec3d& iAfter)
	: m_before{iBefore}, m_after{iAfter} {}

SetWorldOriginCommand::~SetWorldOriginCommand() = default;

void SetWorldOriginCommand::undo(scene::Scene& ioScene) {
	ioScene.shiftWorldOrigin(m_before);
	for (const auto& [uuid, translation]: m_roots)
		if (const auto entity = ioScene.findEntityByUUID(uuid); entity)
			entity.getComponent<scene::component::Transform>().transform.translation() = translation;
}

void SetWorldOriginCommand::redo(scene::Scene& ioScene) {
	m_roots.clear();
	for (const auto view = ioScene.registry.view<scene::component::Transform, scene::component::Hierarchy>(
				 entt::exclude<scene::component::WorldPosition>);
		 const auto handle: view) {
		if (view.get<scene::component::Hierarchy>(handle).parentId != core::UUID{0})
			continue;
		const scene::Entity entity{handle, &ioScene};
		m_roots.emplace_back(entity.getUUID(), view.get<scene::component::Transform>(handle).transform.translation());
	}
	ioScene.shiftWorldOrigin(m_after);
}

auto SetWorldOriginCommand::description() const -> std::string {
	return std::format("Move the world origin to {:.3f}, {:.3f}, {:.3f}", m_after.x(), m_after.y(), m_after.z());
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
	auto script =
			iYaml.empty() ? std::optional<scene::component::LuaScript>{} : scene::component::LuaScript::fromYaml(iYaml);
	ioScene.getSceneScript() = script ? std::move(*script) : scene::component::LuaScript{};
}

}// namespace owl::nest::commands
