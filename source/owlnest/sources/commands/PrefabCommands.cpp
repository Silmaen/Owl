/**
 * @file PrefabCommands.cpp
 * @author Silmaen
 * @date 13/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "PrefabCommands.h"

#include <algorithm>
#include <format>
#include <ranges>
#include <scene/PrefabSerializer.h>
#include <scene/component/PrefabLink.h>
#include <utility>
#include <vector>

namespace owl::nest::commands {

namespace {

void setOverrides(scene::Scene& ioScene, const core::UUID iRootUuid, const std::vector<std::string>& iOverrides) {
	const auto root = ioScene.findEntityByUUID(iRootUuid);
	if (!root || !root.hasComponent<scene::component::PrefabLink>())
		return;
	root.getComponent<scene::component::PrefabLink>().overriddenComponents = iOverrides;
}

void destroyAbsent(scene::Scene& ioScene, const SubtreeSnapshot& iCurrent, const SubtreeSnapshot& iTarget) {
	for (const auto& snapshot: std::views::reverse(iCurrent.entities)) {
		if (std::ranges::find(iTarget.entities, snapshot.uuid, &EntitySnapshot::uuid) != iTarget.entities.end())
			continue;
		if (auto stale = ioScene.findEntityByUUID(snapshot.uuid); stale)
			ioScene.destroyEntity(stale);
	}
}

}// namespace

// --- PrefabOverrideChange ---

auto PrefabOverrideChange::capture(const scene::Entity& iEntity, const scene::Scene& iScene) -> PrefabOverrideChange {
	PrefabOverrideChange change;
	const auto root = scene::PrefabSerializer::findInstanceRoot(iEntity, iScene);
	if (!root)
		return change;
	change.rootUuid = root.getUUID();
	change.before = root.getComponent<scene::component::PrefabLink>().overriddenComponents;
	change.after = change.before;
	return change;
}

auto PrefabOverrideChange::record(const scene::Entity& iEntity, const scene::Scene& iScene,
								  const std::string& iBeforeYaml) -> PrefabOverrideChange {
	auto change = capture(iEntity, iScene);
	if (change.rootUuid == core::UUID{0})
		return change;
	scene::PrefabSerializer::recordOverrides(iEntity, iScene, iBeforeYaml);
	change.captureAfter(iScene);
	return change;
}

void PrefabOverrideChange::captureAfter(const scene::Scene& iScene) {
	const auto root = iScene.findEntityByUUID(rootUuid);
	if (!root || !root.hasComponent<scene::component::PrefabLink>())
		return;
	after = root.getComponent<scene::component::PrefabLink>().overriddenComponents;
}

auto PrefabOverrideChange::isEmpty() const -> bool { return rootUuid == core::UUID{0} || before == after; }

void PrefabOverrideChange::restoreBefore(scene::Scene& ioScene) const {
	if (!isEmpty())
		setOverrides(ioScene, rootUuid, before);
}

void PrefabOverrideChange::restoreAfter(scene::Scene& ioScene) const {
	if (!isEmpty())
		setOverrides(ioScene, rootUuid, after);
}

void PrefabOverrideChange::mergeWith(const PrefabOverrideChange& iLater) {
	if (iLater.isEmpty())
		return;
	if (isEmpty()) {
		*this = iLater;
		return;
	}
	after = iLater.after;
}

// --- InstantiatePrefabCommand ---

InstantiatePrefabCommand::InstantiatePrefabCommand(const scene::Entity& iInstanceRoot, const scene::Scene& iScene,
												   std::string iPrefabName)
	: m_snapshot{SubtreeSnapshot::capture(iInstanceRoot, iScene)}, m_prefabName{std::move(iPrefabName)} {
	m_selectAfterRedo = iInstanceRoot.getUUID();
}

InstantiatePrefabCommand::~InstantiatePrefabCommand() = default;

void InstantiatePrefabCommand::undo(scene::Scene& ioScene) {
	if (m_snapshot.entities.empty())
		return;
	if (auto root = ioScene.findEntityByUUID(m_snapshot.entities[0].uuid); root)
		ioScene.destroyEntityWithChildren(root);
}

void InstantiatePrefabCommand::redo(scene::Scene& ioScene) { m_snapshot.restore(ioScene); }

auto InstantiatePrefabCommand::description() const -> std::string {
	return std::format("Instantiate '{}'", m_prefabName);
}

// --- ApplyPrefabCommand ---
ApplyPrefabCommand::ApplyPrefabCommand(SubtreeSnapshot iBefore, SubtreeSnapshot iAfter, std::string iDescription)
	: m_before{std::move(iBefore)}, m_after{std::move(iAfter)}, m_description{std::move(iDescription)} {
	if (!m_before.entities.empty())
		m_selectAfterUndo = m_before.entities[0].uuid;
	if (!m_after.entities.empty())
		m_selectAfterRedo = m_after.entities[0].uuid;
}

ApplyPrefabCommand::~ApplyPrefabCommand() = default;

void ApplyPrefabCommand::undo(scene::Scene& ioScene) {
	destroyAbsent(ioScene, m_after, m_before);
	m_before.restore(ioScene);
}

void ApplyPrefabCommand::redo(scene::Scene& ioScene) {
	destroyAbsent(ioScene, m_before, m_after);
	m_after.restore(ioScene);
}

auto ApplyPrefabCommand::description() const -> std::string { return m_description; }

}// namespace owl::nest::commands
