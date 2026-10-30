/**
 * @file WorldPartition.cpp
 * @author Silmaen
 * @date 30/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "scene/WorldPartitionSettings.h"
#include "scene/WorldStreamer.h"

#include "core/Macros.h"
#include "core/task/Scheduler.h"
#include "core/task/Task.h"
#include "data/assets/AssetSearchPaths.h"
#include "scene/Entity.h"
#include "scene/Scene.h"
#include "scene/SceneSerializer.h"
#include "scene/component/Hierarchy.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <mutex>
#include <set>
#include <string>
#include <utility>

namespace owl::scene {

namespace {

auto toFloat(const math::vec3d& iVector) -> math::vec3 {
	return {static_cast<float>(iVector.x()), static_cast<float>(iVector.y()), static_cast<float>(iVector.z())};
}

auto finiteOr(const double iValue, const double iFallback) -> double {
	return std::isfinite(iValue) ? iValue : iFallback;
}

auto coordsLess(const math::vec3i& iA, const math::vec3i& iB) -> bool {
	if (iA.x() != iB.x())
		return iA.x() < iB.x();
	if (iA.y() != iB.y())
		return iA.y() < iB.y();
	return iA.z() < iB.z();
}

auto nearestDistance(const WorldPartitionSettings& iSettings, const math::vec3i& iCoords,
					 const std::span<const StreamPoint> iSources) -> double {
	double nearest = std::numeric_limits<double>::max();
	for (const auto& source: iSources) {
		const double scale = source.radiusScale > 0.0 ? source.radiusScale : 1.0;
		nearest = std::min(nearest, iSettings.getDistanceToCell(iCoords, source.position) / scale);
	}
	return nearest;
}

// Load and unload radii of the settings, brought into their accepted range without copying the cells.
auto streamRadii(const WorldPartitionSettings& iSettings) -> std::pair<double, double> {
	const double load = std::max(finiteOr(iSettings.loadRadius, WorldPartitionSettings{}.loadRadius), 0.0);
	return {load, std::max(finiteOr(iSettings.unloadRadius, WorldPartitionSettings{}.unloadRadius), load)};
}

}// namespace

auto WorldPartitionSettings::clamped() const -> WorldPartitionSettings {
	WorldPartitionSettings out = *this;
	const WorldPartitionSettings defaults;
	out.cellSize = std::max(finiteOr(cellSize, defaults.cellSize), minCellSize);
	out.loadRadius = std::max(finiteOr(loadRadius, defaults.loadRadius), 0.0);
	out.unloadRadius = std::max(finiteOr(unloadRadius, defaults.unloadRadius), out.loadRadius);
	out.cells.clear();
	std::set<math::vec3i, decltype(&coordsLess)> seen{&coordsLess};
	for (const auto& cell: cells) {
		if (seen.insert(cell.coords).second)
			out.cells.push_back(cell);
	}
	return out;
}

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wfloat-equal")
auto WorldPartitionSettings::operator==(const WorldPartitionSettings& iOther) const -> bool = default;
OWL_DIAG_POP

auto WorldPartitionSettings::findCell(const math::vec3i& iCoords) const -> const WorldCell* {
	const auto found =
			std::ranges::find_if(cells, [&](const WorldCell& iCell) -> bool { return iCell.coords == iCoords; });
	return found == cells.end() ? nullptr : &*found;
}

auto WorldPartitionSettings::getCellOf(const math::vec3d& iPosition) const -> math::vec3i {
	const double size = std::max(cellSize, minCellSize);
	return {static_cast<int32_t>(std::floor(iPosition.x() / size)),
			static_cast<int32_t>(std::floor(iPosition.y() / size)),
			static_cast<int32_t>(std::floor(iPosition.z() / size))};
}

auto WorldPartitionSettings::getCellCorner(const math::vec3i& iCoords) const -> math::vec3d {
	const double size = std::max(cellSize, minCellSize);
	return {static_cast<double>(iCoords.x()) * size, static_cast<double>(iCoords.y()) * size,
			static_cast<double>(iCoords.z()) * size};
}

auto WorldPartitionSettings::getDistanceToCell(const math::vec3i& iCoords, const math::vec3d& iPosition) const
		-> double {
	const double size = std::max(cellSize, minCellSize);
	const math::vec3d corner = getCellCorner(iCoords);
	double sum = 0.0;
	for (size_t axis = 0; axis < 3; ++axis) {
		const double low = corner[axis];
		const double outside = std::max({low - iPosition[axis], iPosition[axis] - (low + size), 0.0});
		sum += outside * outside;
	}
	return std::sqrt(sum);
}

struct WorldStreamer::FinishedLoad {
	math::vec3i coords{0, 0, 0};
	uint64_t ticket = 0;
	shared<ParsedScene> parsed;
};

struct WorldStreamer::Inbox {
	std::mutex mutex;
	std::vector<FinishedLoad> done;
};

auto WorldStreamer::CoordsLess::operator()(const math::vec3i& iA, const math::vec3i& iB) const -> bool {
	return coordsLess(iA, iB);
}

WorldStreamer::WorldStreamer() : m_inbox{mkShared<Inbox>()} {}

WorldStreamer::~WorldStreamer() = default;

auto WorldStreamer::getWantedCells(const WorldPartitionSettings& iSettings, const std::span<const StreamPoint> iSources)
		-> std::vector<math::vec3i> {
	std::vector<math::vec3i> wanted;
	for (const auto& cell: iSettings.cells) {
		if (!cell.scene.empty() && nearestDistance(iSettings, cell.coords, iSources) <= streamRadii(iSettings).first)
			wanted.push_back(cell.coords);
	}
	std::ranges::sort(wanted, coordsLess);
	return wanted;
}

void WorldStreamer::startLoad(const WorldCell& iCell, CellRuntime& ioRuntime, core::task::Scheduler* ioScheduler) {
	ioRuntime.state = WorldCellState::Loading;
	ioRuntime.scene = iCell.scene;
	ioRuntime.ticket = m_nextTicket++;
	++m_stats.loadsStarted;
	const auto load = [inbox = m_inbox, coords = iCell.coords, ticket = ioRuntime.ticket,
					   scene = iCell.scene]() -> void {
		auto parsed = mkShared<ParsedScene>();
		if (const auto bytes = data::assets::readAssetBytes(scene); bytes) {
			*parsed = SceneSerializer::parseBuffer(*bytes, scene);
		} else {
			parsed->sourceName = scene;
			parsed->error = "the file is not found";
			parsed->failure = SceneLoadError::FileUnreadable;
		}
		const std::lock_guard<std::mutex> lock{inbox->mutex};
		inbox->done.push_back({.coords = coords, .ticket = ticket, .parsed = std::move(parsed)});
	};
	if (ioScheduler == nullptr)
		load();
	else
		ioScheduler->pushTask(core::task::Task{load});
}

void WorldStreamer::mergeFinished(Scene& ioScene, const WorldPartitionSettings& iSettings) {
	std::vector<FinishedLoad> done;
	{
		const std::lock_guard<std::mutex> lock{m_inbox->mutex};
		done.swap(m_inbox->done);
	}
	uint32_t budget = g_mergeBudget;
	std::vector<FinishedLoad> later;
	for (auto& load: done) {
		const auto found = m_cells.find(load.coords);
		if (found == m_cells.end() || found->second.ticket != load.ticket ||
			found->second.state != WorldCellState::Loading)
			continue;
		if (budget == 0) {
			later.push_back(std::move(load));
			continue;
		}
		--budget;
		auto& runtime = found->second;
		if (!load.parsed || !load.parsed->valid) {
			OWL_CORE_WARN("WorldStreamer: Cell ({}, {}, {}) cannot load '{}': {}.", load.coords.x(), load.coords.y(),
						  load.coords.z(), runtime.scene, load.parsed ? load.parsed->error : std::string{"no data"})
			runtime.state = WorldCellState::Failed;
			++m_stats.failures;
			continue;
		}
		const math::vec3 offset = toFloat(iSettings.getCellCorner(load.coords) - ioScene.getWorldOrigin());
		// A non-owning handle: the serializer only works on the scene during the call.
		const SceneSerializer serializer{shared<Scene>{shared<Scene>{}, &ioScene}};
		const auto merged = serializer.mergeParsed(*load.parsed, offset);
		if (!merged) {
			OWL_CORE_WARN("WorldStreamer: Cell ({}, {}, {}) cannot merge '{}'.", load.coords.x(), load.coords.y(),
						  load.coords.z(), runtime.scene)
			runtime.state = WorldCellState::Failed;
			++m_stats.failures;
			continue;
		}
		runtime.entities.clear();
		for (const auto& entity: *merged) {
			runtime.entities.push_back(entity.getUUID());
			const bool root = !entity.hasComponent<component::Hierarchy>() ||
							  entity.getComponent<component::Hierarchy>().parentId == core::UUID{0};
			if (root && ioScene.status != Scene::Status::Editing)
				ioScene.startSpawnedSubtree(entity);
		}
		runtime.state = WorldCellState::Loaded;
		++m_stats.merges;
	}
	if (!later.empty()) {
		const std::lock_guard<std::mutex> lock{m_inbox->mutex};
		m_inbox->done.insert(m_inbox->done.end(), std::make_move_iterator(later.begin()),
							 std::make_move_iterator(later.end()));
	}
}

void WorldStreamer::unloadCell(Scene& ioScene, CellRuntime& ioRuntime) {
	for (const auto& uuid: ioRuntime.entities) {
		auto entity = ioScene.findEntityByUUID(uuid);
		if (!entity)
			continue;
		if (ioScene.status != Scene::Status::Editing)
			ioScene.destroyEntityDeferred(entity);
		else
			ioScene.destroyEntityWithChildren(entity);
	}
	ioRuntime.entities.clear();
	ioRuntime.state = WorldCellState::Unloaded;
	ioRuntime.ticket = 0;
}

void WorldStreamer::update(Scene& ioScene, const std::span<const StreamPoint> iSources,
						   core::task::Scheduler* ioScheduler) {
	OWL_PROFILE_FUNCTION()

	const auto& settings = ioScene.getWorldPartitionSettings();
	if (!settings.enabled) {
		unloadAll(ioScene);
		return;
	}
	const auto [load, unload] = streamRadii(settings);
	mergeFinished(ioScene, settings);
	for (auto& [coords, runtime]: m_cells) {
		const auto* cell = settings.findCell(coords);
		const bool changed = cell == nullptr || cell->scene != runtime.scene;
		const bool far = nearestDistance(settings, coords, iSources) > unload;
		if (runtime.state == WorldCellState::Loaded && (changed || far)) {
			unloadCell(ioScene, runtime);
			++m_stats.unloads;
		} else if (runtime.state != WorldCellState::Unloaded && (changed || far)) {
			runtime.state = WorldCellState::Unloaded;
			runtime.ticket = 0;
		}
	}
	std::erase_if(m_cells, [](const auto& iEntry) -> bool { return iEntry.second.state == WorldCellState::Unloaded; });
	for (const auto& cell: settings.cells) {
		if (cell.scene.empty() || nearestDistance(settings, cell.coords, iSources) > load)
			continue;
		auto& runtime = m_cells[cell.coords];
		if (runtime.state == WorldCellState::Unloaded)
			startLoad(cell, runtime, ioScheduler);
	}
	if (ioScheduler == nullptr)
		mergeFinished(ioScene, settings);
	countStates();
}

void WorldStreamer::unloadAll(Scene& ioScene) {
	for (auto& [coords, runtime]: m_cells) {
		if (runtime.state == WorldCellState::Loaded) {
			unloadCell(ioScene, runtime);
			++m_stats.unloads;
		}
	}
	m_cells.clear();
	countStates();
}

void WorldStreamer::waitForLoads(core::task::Scheduler* ioScheduler) {
	if (ioScheduler != nullptr)
		ioScheduler->waitEmptyQueue();
}

auto WorldStreamer::getState(const math::vec3i& iCoords) const -> WorldCellState {
	const auto found = m_cells.find(iCoords);
	return found == m_cells.end() ? WorldCellState::Unloaded : found->second.state;
}

auto WorldStreamer::getLoadedCells() const -> std::vector<math::vec3i> {
	std::vector<math::vec3i> loaded;
	for (const auto& [coords, runtime]: m_cells)
		if (runtime.state == WorldCellState::Loaded)
			loaded.push_back(coords);
	return loaded;
}

auto WorldStreamer::getCellEntities(const math::vec3i& iCoords) const -> std::span<const core::UUID> {
	const auto found = m_cells.find(iCoords);
	if (found == m_cells.end())
		return {};
	return found->second.entities;
}

void WorldStreamer::countStates() {
	m_stats.loaded = 0;
	m_stats.loading = 0;
	for (const auto& [coords, runtime]: m_cells) {
		if (runtime.state == WorldCellState::Loaded)
			++m_stats.loaded;
		else if (runtime.state == WorldCellState::Loading)
			++m_stats.loading;
	}
}

}// namespace owl::scene
