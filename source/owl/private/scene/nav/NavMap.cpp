/**
 * @file NavMap.cpp
 * @author Silmaen
 * @date 16/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "scene/nav/NavMap.h"

#include "scene/TilemapAsset.h"
#include "scene/TilemapTerrain.h"
#include "scene/component/Tilemap.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace owl::scene::nav {

static_assert(chunkSide == component::TilemapChunk::side, "navigation chunks follow the tilemap chunks");

namespace {

struct BorderTables {
	std::array<int16_t, chunkCellCount> indexOfCell{};
	std::array<uint16_t, borderCellCount> cellOfIndex{};
};

auto makeBorderTables() -> BorderTables {
	BorderTables tables;
	tables.indexOfCell.fill(-1);
	uint32_t next = 0;
	for (uint32_t y = 0; y < chunkSide; ++y) {
		for (uint32_t x = 0; x < chunkSide; ++x) {
			if (x != 0 && y != 0 && x != chunkSide - 1 && y != chunkSide - 1)
				continue;
			const uint32_t cell = y * chunkSide + x;
			tables.indexOfCell[cell] = static_cast<int16_t>(next);
			tables.cellOfIndex[next] = static_cast<uint16_t>(cell);
			++next;
		}
	}
	return tables;
}

auto borderTables() -> const BorderTables& {
	static const BorderTables s_tables = makeBorderTables();
	return s_tables;
}

auto toCostUnits(const float iCost) -> uint16_t {
	const float clamped = std::clamp(iCost, 1.f / 16.f, 64.f);
	return static_cast<uint16_t>(std::lround(clamped * 16.f));
}

}// namespace

NavMap::NavMap() = default;

NavMap::~NavMap() = default;

NavMap::NavMap(NavMap&&) noexcept = default;

auto NavMap::operator=(NavMap&&) noexcept -> NavMap& = default;

auto NavMap::getBorderIndex(const uint32_t iLocal) -> int32_t {
	return iLocal < chunkCellCount ? borderTables().indexOfCell[iLocal] : -1;
}

auto NavMap::getBorderCell(const uint32_t iBorder) -> uint32_t {
	return iBorder < borderCellCount ? borderTables().cellOfIndex[iBorder] : 0u;
}

void NavMap::setProfile(const NavProfile& iProfile) {
	m_profile = iProfile;
	m_profile.maxRise = std::min(m_profile.maxRise, NavProfile::maxRiseLimit);
	m_profile.climbCost = std::max(0.f, m_profile.climbCost);
	m_ruleOfTile.clear();
	uint16_t lowest = std::numeric_limits<uint16_t>::max();
	for (size_t i = 0; i < m_profile.rules.size(); ++i) {
		const auto& rule = m_profile.rules[i];
		if (rule.tile < 0 || rule.tile > component::g_MaxTileIndex)
			continue;
		const auto tile = static_cast<size_t>(rule.tile);
		if (m_ruleOfTile.size() <= tile)
			m_ruleOfTile.resize(tile + 1, 0);
		if (m_ruleOfTile[tile] == 0)
			m_ruleOfTile[tile] = static_cast<uint16_t>(i + 1);
		lowest = std::min(lowest, toCostUnits(rule.cost));
	}
	m_minEdgeCost = lowest == std::numeric_limits<uint16_t>::max() ? edgeUnit : 2u * lowest;
	m_climbUnits = static_cast<uint32_t>(std::lround(m_profile.climbCost * 8.f));
	m_profileDirty = true;
}

void NavMap::reset() {
	m_chunks.clear();
	m_stamps.clear();
	m_chunkVersions.clear();
	m_chunkWalkable.clear();
	m_walkableCount = 0;
	m_rows.clear();
	m_rowVersions.clear();
}

auto NavMap::getAllocatedChunkCount() const -> size_t {
	return static_cast<size_t>(
			std::ranges::count_if(m_chunks, [](const uniq<NavChunk>& iChunk) -> bool { return iChunk != nullptr; }));
}

auto NavMap::sync(const TilemapAsset& iAsset) -> uint32_t {
	OWL_PROFILE_FUNCTION()

	const math::vec2ui size{iAsset.width, iAsset.height};
	const math::vec2ui grid{(size.x() + chunkSide - 1) / chunkSide, (size.y() + chunkSide - 1) / chunkSide};
	const bool everything = m_profileDirty || size != m_size;
	if (everything) {
		reset();
		m_size = size;
		m_chunkGrid = grid;
		const size_t count = static_cast<size_t>(grid.x()) * grid.y();
		m_chunks.resize(count);
		m_stamps.assign(count, Stamp{});
		m_chunkVersions.assign(count, 0);
		m_chunkWalkable.assign(count, 0);
		m_rows.resize(count);
		m_rowVersions.assign(count, 0);
		m_profileDirty = false;
	}
	const bool hasLayer = m_profile.layer < iAsset.layers.size();
	const component::TilemapLayer* layer = hasLayer ? &iAsset.layers[m_profile.layer] : nullptr;
	uint32_t rebuilt = 0;
	for (uint32_t cy = 0; cy < grid.y(); ++cy) {
		for (uint32_t cx = 0; cx < grid.x(); ++cx) {
			const size_t index = static_cast<size_t>(cy) * grid.x() + cx;
			const Stamp current{.tiles = layer != nullptr ? layer->getChunkGeneration({cx, cy}) : 0,
								.heights = iAsset.getChunkHeightGeneration({cx, cy})};
			if (!everything && current.tiles == m_stamps[index].tiles && current.heights == m_stamps[index].heights)
				continue;
			rebuildChunk(iAsset, cx, cy);
			m_stamps[index] = current;
			++rebuilt;
		}
	}
	if (rebuilt != 0 || everything) {
		++m_version;
		m_rebuiltChunks += rebuilt;
	}
	return rebuilt;
}

void NavMap::rebuildChunk(const TilemapAsset& iAsset, const uint32_t iChunkX, const uint32_t iChunkY) {
	const size_t index = static_cast<size_t>(iChunkY) * m_chunkGrid.x() + iChunkX;
	m_chunkVersions[index] = m_version + 1;
	m_chunks[index].reset();
	m_walkableCount -= m_chunkWalkable[index];
	m_chunkWalkable[index] = 0;
	if (m_profile.layer >= iAsset.layers.size() || m_ruleOfTile.empty())
		return;
	const auto& layer = iAsset.layers[m_profile.layer];
	const auto* source = layer.getChunk({iChunkX, iChunkY});
	if (source == nullptr)
		return;
	const bool overlay = m_profile.source == NavSource::Overlay;
	const auto& ids = overlay ? source->overlay : source->terrain;
	const bool heights = iAsset.hasHeights();
	const auto riseLimit = static_cast<int32_t>(m_profile.maxRise) * 2;
	uniq<NavChunk> built;
	uint16_t walkable = 0;
	for (uint32_t ly = 0; ly < chunkSide; ++ly) {
		const uint32_t y = iChunkY * chunkSide + ly;
		if (y >= m_size.y())
			break;
		for (uint32_t lx = 0; lx < chunkSide; ++lx) {
			const uint32_t x = iChunkX * chunkSide + lx;
			if (x >= m_size.x())
				break;
			const uint32_t local = ly * chunkSide + lx;
			const uint16_t id = ids[local];
			if (id == component::TilemapChunk::emptyId || id >= m_ruleOfTile.size() || m_ruleOfTile[id] == 0)
				continue;
			const auto& rule = m_profile.rules[m_ruleOfTile[id] - 1u];
			int32_t riseX = 0;
			int32_t riseY = 0;
			if (heights) {
				if (!m_profile.allowSteep && terrain::isSteepSlope(iAsset.getCellSlope(x, y)))
					continue;
				const auto corners = iAsset.getCellCorners(x, y);
				riseX = (corners[1] + corners[2]) - (corners[0] + corners[3]);
				riseY = (corners[3] + corners[2]) - (corners[0] + corners[1]);
			}
			uint8_t slopeOk = allDirections;
			if (std::abs(riseX) > riseLimit)
				slopeOk = static_cast<uint8_t>(slopeOk &
											   ~(directionBit(Direction::East) | directionBit(Direction::West)));
			if (std::abs(riseY) > riseLimit)
				slopeOk = static_cast<uint8_t>(slopeOk &
											   ~(directionBit(Direction::South) | directionBit(Direction::North)));
			if (built == nullptr)
				built = mkUniq<NavChunk>();
			built->cost[local] = toCostUnits(rule.cost);
			built->exits[local] = static_cast<uint8_t>(rule.exits & slopeOk & allDirections);
			built->enters[local] = static_cast<uint8_t>(rule.enters & slopeOk & allDirections);
			built->riseX[local] = static_cast<int8_t>(riseX);
			built->riseY[local] = static_cast<int8_t>(riseY);
			++walkable;
		}
	}
	m_chunks[index] = std::move(built);
	m_chunkWalkable[index] = walkable;
	m_walkableCount += walkable;
}

auto NavMap::isWalkable(const uint32_t iX, const uint32_t iY) const -> bool {
	if (iX >= m_size.x() || iY >= m_size.y())
		return false;
	const auto* chunk = getChunk(static_cast<size_t>(iY / chunkSide) * m_chunkGrid.x() + iX / chunkSide);
	return chunk != nullptr && chunk->cost[(iY % chunkSide) * chunkSide + iX % chunkSide] != 0;
}

auto NavMap::getCellCost(const uint32_t iX, const uint32_t iY) const -> float {
	if (iX >= m_size.x() || iY >= m_size.y())
		return 0.f;
	const auto* chunk = getChunk(static_cast<size_t>(iY / chunkSide) * m_chunkGrid.x() + iX / chunkSide);
	if (chunk == nullptr)
		return 0.f;
	return static_cast<float>(chunk->cost[(iY % chunkSide) * chunkSide + iX % chunkSide]) / 16.f;
}

auto NavMap::getEdgeCost(const uint32_t iX, const uint32_t iY, const Direction iDirection) const -> uint32_t {
	const auto nx = static_cast<int64_t>(iX) + offsetX(iDirection);
	const auto ny = static_cast<int64_t>(iY) + offsetY(iDirection);
	if (iX >= m_size.x() || iY >= m_size.y() || nx < 0 || ny < 0 || nx >= static_cast<int64_t>(m_size.x()) ||
		ny >= static_cast<int64_t>(m_size.y()))
		return 0;
	const auto bx = static_cast<uint32_t>(nx);
	const auto by = static_cast<uint32_t>(ny);
	const auto* from = getChunk(static_cast<size_t>(iY / chunkSide) * m_chunkGrid.x() + iX / chunkSide);
	const auto* to = getChunk(static_cast<size_t>(by / chunkSide) * m_chunkGrid.x() + bx / chunkSide);
	if (from == nullptr || to == nullptr)
		return 0;
	const uint32_t a = (iY % chunkSide) * chunkSide + iX % chunkSide;
	const uint32_t b = (by % chunkSide) * chunkSide + bx % chunkSide;
	const uint8_t bit = directionBit(iDirection);
	if (from->cost[a] == 0 || to->cost[b] == 0 || (from->exits[a] & bit) == 0 || (to->enters[b] & bit) == 0)
		return 0;
	uint32_t total = static_cast<uint32_t>(from->cost[a]) + to->cost[b];
	if (m_climbUnits != 0) {
		const auto rise = [iDirection](const NavChunk& iChunk, const uint32_t iLocal) -> int32_t {
			switch (iDirection) {
				case Direction::East:
					return iChunk.riseX[iLocal];
				case Direction::West:
					return -iChunk.riseX[iLocal];
				case Direction::South:
					return iChunk.riseY[iLocal];
				case Direction::North:
					return -iChunk.riseY[iLocal];
			}
			return 0;
		};
		const int32_t up = std::max(0, rise(*from, a)) + std::max(0, rise(*to, b));
		total += static_cast<uint32_t>(up) * m_climbUnits;
	}
	return total;
}

auto NavMap::getRows(const size_t iIndex) const -> NavRows& {
	auto& rows = m_rows[iIndex];
	if (rows == nullptr) {
		rows = mkUniq<NavRows>();
		rows->distance.assign(static_cast<size_t>(borderCellCount) * borderCellCount, unreachable);
		rows->valid.assign(borderCellCount, 0);
		rows->targets.assign(static_cast<size_t>(borderCellCount) * borderCellCount, 0);
		rows->count.assign(borderCellCount, 0);
		m_rowVersions[iIndex] = m_chunkVersions[iIndex];
	} else if (m_rowVersions[iIndex] != m_chunkVersions[iIndex]) {
		std::ranges::fill(rows->valid, uint8_t{0});
		m_rowVersions[iIndex] = m_chunkVersions[iIndex];
	}
	return *rows;
}

void NavMap::releaseSearchCaches() const {
	for (auto& rows: m_rows) rows.reset();
}

auto NavProfile::findRule(const int32_t iTile) const -> const NavTileRule* {
	const auto found =
			std::ranges::find_if(rules, [iTile](const NavTileRule& iRule) -> bool { return iRule.tile == iTile; });
	return found == rules.end() ? nullptr : &*found;
}

}// namespace owl::scene::nav
