/**
 * @file SystemSchedule.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "scene/SystemSchedule.h"

#include "systems/EngineSystems.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace owl::scene {

namespace {

auto isValid(const SceneSystem& iSystem) -> bool { return !iSystem.name.empty() && iSystem.update != nullptr; }

}// namespace

auto SystemSchedule::add(SceneSystem iSystem) -> bool {
	if (!isValid(iSystem) || has(iSystem.name)) {
		OWL_CORE_WARN("SystemSchedule: System '{}' not added (empty, or name already used).", iSystem.name)
		return false;
	}
	const auto after = std::ranges::find_if(
			m_systems, [&iSystem](const SceneSystem& iOther) -> bool { return iOther.phase > iSystem.phase; });
	m_systems.insert(after, std::move(iSystem));
	return true;
}

auto SystemSchedule::insertBefore(const std::string& iBefore, SceneSystem iSystem) -> bool {
	const auto it = std::ranges::find(m_systems, iBefore, &SceneSystem::name);
	if (it == m_systems.end() || !isValid(iSystem) || has(iSystem.name)) {
		OWL_CORE_WARN("SystemSchedule: System '{}' not inserted before '{}' (unknown, empty or duplicate).",
					  iSystem.name, iBefore)
		return false;
	}
	iSystem.phase = it->phase;
	m_systems.insert(it, std::move(iSystem));
	return true;
}

auto SystemSchedule::replace(const std::string& iName, SystemFunction iUpdate) -> bool {
	const auto it = std::ranges::find(m_systems, iName, &SceneSystem::name);
	if (it == m_systems.end() || iUpdate == nullptr) {
		OWL_CORE_WARN("SystemSchedule: System '{}' not replaced (unknown, or empty function).", iName)
		return false;
	}
	it->update = std::move(iUpdate);
	return true;
}

auto SystemSchedule::remove(const std::string& iName) -> bool {
	if (std::erase_if(m_systems, [&iName](const SceneSystem& iSystem) -> bool { return iSystem.name == iName; }) == 0) {
		OWL_CORE_WARN("SystemSchedule: System '{}' not removed (unknown).", iName)
		return false;
	}
	return true;
}

auto SystemSchedule::has(const std::string& iName) const -> bool {
	return std::ranges::find(m_systems, iName, &SceneSystem::name) != m_systems.end();
}

auto SystemSchedule::getNames(const SystemPhase iPhase) const -> std::vector<std::string> {
	std::vector<std::string> names;
	for (const auto& [name, phase, update]: m_systems)
		if (phase == iPhase)
			names.push_back(name);
	return names;
}

void SystemSchedule::run(const SystemPhase iPhase, Scene& ioScene, const SystemContext& iContext) const {
	for (const auto& [name, phase, update]: m_systems)
		if (phase == iPhase)
			update(ioScene, iContext);
}

auto SystemSchedule::getDefault() -> SystemSchedule& {
	static SystemSchedule schedule = makeEngineDefault();
	return schedule;
}

auto SystemSchedule::makeEngineDefault() -> SystemSchedule {
	SystemSchedule schedule;
	systems::registerEngineSystems(schedule);
	return schedule;
}

}// namespace owl::scene
