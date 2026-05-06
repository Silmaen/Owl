/**
 * @file SceneSettingsCommands.cpp
 * @author Silmaen
 * @date 06/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "SceneSettingsCommands.h"

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
	// All ModifyEnabledRenderersCommand instances merge with each other regardless
	// of which layer was edited — there's only one config per scene, so a single
	// type id is enough.
	constexpr size_t kTypeHash = 0x4D6F644552656E64ULL;// "ModERend"
	return kTypeHash;
}

void ModifyEnabledRenderersCommand::apply(scene::Scene& ioScene, const std::string& iYaml) {
	if (iYaml.empty()) {
		ioScene.getEnabledRenderers() = renderer::EnabledRenderersConfig{};
		return;
	}
	const auto node = YAML::Load(iYaml);
	ioScene.getEnabledRenderers() = renderer::EnabledRenderersConfig::fromYaml(node);
}

}// namespace owl::nest::commands
