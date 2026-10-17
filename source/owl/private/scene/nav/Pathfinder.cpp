/**
 * @file Pathfinder.cpp
 * @author Silmaen
 * @date 16/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "scene/nav/Pathfinder.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace owl::scene::nav {

namespace {

auto openLess(const auto& iLeft, const auto& iRight) -> bool {
	if (iLeft.f != iRight.f)
		return iLeft.f > iRight.f;
	return iLeft.g < iRight.g;
}

auto absDiff(const uint32_t iLeft, const uint32_t iRight) -> uint32_t {
	return iLeft > iRight ? iLeft - iRight : iRight - iLeft;
}

}// namespace

Pathfinder::Pathfinder() = default;

Pathfinder::~Pathfinder() = default;

void Pathfinder::newStamp() {
	++m_stamp;
	if (m_stamp == 0) {
		std::ranges::fill(m_visit, 0u);
		std::ranges::fill(m_stateVisit, 0u);
		m_stamp = 1;
	}
}

auto Pathfinder::heuristic(const uint32_t iNode) const -> uint32_t {
	if (iNode == m_goalNode)
		return 0;
	return (absDiff(iNode % m_width, m_toX) + absDiff(iNode / m_width, m_toY)) * m_minEdge;
}

void Pathfinder::relax(const uint32_t iNode, const uint32_t iCost, const uint32_t iParent, const Kind iKind) {
	if (m_visit[iNode] == m_stamp && m_cost[iNode] <= iCost)
		return;
	m_visit[iNode] = m_stamp;
	m_cost[iNode] = iCost;
	m_parent[iNode] = iParent;
	m_kind[iNode] = static_cast<uint8_t>(iKind);
	m_open.push_back({.f = iCost + heuristic(iNode), .g = iCost, .node = iNode});
	std::ranges::push_heap(m_open, [](const OpenNode& iL, const OpenNode& iR) -> bool { return openLess(iL, iR); });
}

auto Pathfinder::begin(const NavMap& iMap, const math::vec2ui& iFrom, const math::vec2ui& iTo, const PathMode iMode,
					   const PathOptions& iOptions) -> PathState {
	mp_map = &iMap;
	m_mapVersion = iMap.getVersion();
	m_work = 0;
	m_total = 0;
	m_cells.clear();
	m_chain.clear();
	m_open.clear();
	m_state = PathState::NotFound;
	if (!iMap.isWalkable(iFrom.x(), iFrom.y()) || !iMap.isWalkable(iTo.x(), iTo.y()))
		return m_state;
	const auto& size = iMap.getSize();
	m_width = size.x();
	m_goalNode = size.x() * size.y();
	if (m_visit.size() < static_cast<size_t>(m_goalNode) + 1) {
		const size_t count = static_cast<size_t>(m_goalNode) + 1;
		m_visit.assign(count, 0);
		m_cost.assign(count, 0);
		m_parent.assign(count, 0);
		m_kind.assign(count, 0);
	}
	m_minEdge = iMap.getMinEdgeCost();
	m_from = iFrom.y() * m_width + iFrom.x();
	m_to = iTo.y() * m_width + iTo.x();
	m_toX = iTo.x();
	m_toY = iTo.y();
	newStamp();
	if (iOptions.noReverse) {
		const size_t count = static_cast<size_t>(m_goalNode) * (noHeading + 1);
		if (m_stateVisit.size() < count) {
			m_stateVisit.assign(count, 0);
			m_stateCost.assign(count, 0);
			m_stateParent.assign(count, 0);
		}
		m_phase = Phase::Heading;
		m_state = PathState::Searching;
		const uint32_t entry = iOptions.heading ? static_cast<uint32_t>(*iOptions.heading) : noHeading;
		relaxState(m_from * (noHeading + 1) + entry, 0, m_from * (noHeading + 1) + entry);
		return m_state;
	}
	auto mode = iMode;
	if (mode == PathMode::Auto) {
		const auto chunkDistance = absDiff(iFrom.x() / chunkSide, iTo.x() / chunkSide) +
								   absDiff(iFrom.y() / chunkSide, iTo.y() / chunkSide);
		const bool sparse = iMap.getWalkableCount() * 5 <= static_cast<uint64_t>(m_goalNode);
		mode = chunkDistance >= 3 && sparse ? PathMode::Hierarchical : PathMode::Flat;
	}
	m_state = PathState::Searching;
	if (mode == PathMode::Flat) {
		m_phase = Phase::Flat;
		relax(m_from, 0, m_from, Kind::Seed);
	} else {
		m_phase = Phase::StartChunk;
	}
	return m_state;
}

auto Pathfinder::step(const uint32_t iBudget) -> PathState {
	OWL_PROFILE_FUNCTION()

	if (m_state != PathState::Searching)
		return m_state;
	const uint64_t limit = m_work + iBudget;
	const auto fromX = m_from % m_width;
	const auto fromY = m_from / m_width;
	const auto toX = m_toX;
	const auto toY = m_toY;
	if (m_phase == Phase::StartChunk) {
		searchChunk(fromX / chunkSide, fromY / chunkSide, (fromY % chunkSide) * chunkSide + fromX % chunkSide, false,
					m_startDistance, m_startDirection);
		m_phase = Phase::GoalChunk;
	}
	if (m_phase == Phase::GoalChunk && m_work < limit) {
		searchChunk(toX / chunkSide, toY / chunkSide, (toY % chunkSide) * chunkSide + toX % chunkSide, true,
					m_goalDistance, m_goalDirection);
		newStamp();
		m_open.clear();
		const uint32_t startChunkX = fromX / chunkSide;
		const uint32_t startChunkY = fromY / chunkSide;
		for (uint32_t border = 0; border < borderCellCount; ++border) {
			const uint32_t local = NavMap::getBorderCell(border);
			const uint32_t distance = m_startDistance[local];
			if (distance == unreachable)
				continue;
			const uint32_t node = (startChunkY * chunkSide + local / chunkSide) * m_width + startChunkX * chunkSide +
								  local % chunkSide;
			relax(node, distance, node, Kind::Seed);
		}
		if (startChunkX == toX / chunkSide && startChunkY == toY / chunkSide) {
			if (const uint32_t direct = m_startDistance[(toY % chunkSide) * chunkSide + toX % chunkSide];
				direct != unreachable)
				relax(m_goalNode, direct, m_to, Kind::Direct);
		}
		m_phase = Phase::Abstract;
	}
	if (m_phase == Phase::Heading)
		stepHeading(static_cast<uint32_t>(std::min<uint64_t>(limit - m_work, 0xFFFFFFFFu)));
	else if (m_phase == Phase::Flat)
		stepFlat(static_cast<uint32_t>(std::min<uint64_t>(limit - m_work, 0xFFFFFFFFu)));
	else if (m_phase == Phase::Abstract)
		stepAbstract(static_cast<uint32_t>(std::min<uint64_t>(limit > m_work ? limit - m_work : 0, 0xFFFFFFFFu)));
	while (m_phase == Phase::Refine && m_state == PathState::Searching && m_work < limit) stepRefine();
	return m_state;
}

void Pathfinder::stepFlat(const uint32_t iBudget) {
	const NavMap& map = *mp_map;
	const uint64_t limit = m_work + iBudget;
	while (!m_open.empty() && m_work < limit) {
		std::ranges::pop_heap(m_open, [](const OpenNode& iL, const OpenNode& iR) -> bool { return openLess(iL, iR); });
		const OpenNode current = m_open.back();
		m_open.pop_back();
		if (current.g != m_cost[current.node])
			continue;
		++m_work;
		if (current.node == m_to) {
			finishFlat();
			return;
		}
		const uint32_t x = current.node % m_width;
		const uint32_t y = current.node / m_width;
		for (const auto direction: directions) {
			const uint32_t edge = map.getEdgeCost(x, y, direction);
			if (edge == 0)
				continue;
			const auto nx = static_cast<uint32_t>(static_cast<int32_t>(x) + offsetX(direction));
			const auto ny = static_cast<uint32_t>(static_cast<int32_t>(y) + offsetY(direction));
			relax(ny * m_width + nx, current.g + edge, current.node, Kind::Step);
		}
	}
	if (m_open.empty() && m_state == PathState::Searching)
		m_state = PathState::NotFound;
}

void Pathfinder::relaxState(const uint32_t iState, const uint32_t iCost, const uint32_t iParent) {
	if (m_stateVisit[iState] == m_stamp && m_stateCost[iState] <= iCost)
		return;
	m_stateVisit[iState] = m_stamp;
	m_stateCost[iState] = iCost;
	m_stateParent[iState] = iParent;
	m_open.push_back({.f = iCost + heuristic(iState / (noHeading + 1)), .g = iCost, .node = iState});
	std::ranges::push_heap(m_open, [](const OpenNode& iL, const OpenNode& iR) -> bool { return openLess(iL, iR); });
}

void Pathfinder::stepHeading(const uint32_t iBudget) {
	const NavMap& map = *mp_map;
	const uint64_t limit = m_work + iBudget;
	constexpr uint32_t states = noHeading + 1;
	while (!m_open.empty() && m_work < limit) {
		std::ranges::pop_heap(m_open, [](const OpenNode& iL, const OpenNode& iR) -> bool { return openLess(iL, iR); });
		const OpenNode current = m_open.back();
		m_open.pop_back();
		if (current.g != m_stateCost[current.node])
			continue;
		++m_work;
		const uint32_t cell = current.node / states;
		const uint32_t entry = current.node % states;
		if (cell == m_to) {
			m_total = current.g;
			uint32_t state = current.node;
			while (true) {
				m_cells.emplace_back((state / states) % m_width, (state / states) / m_width);
				if (m_stateParent[state] == state)
					break;
				state = m_stateParent[state];
			}
			std::ranges::reverse(m_cells);
			m_state = PathState::Found;
			return;
		}
		const uint32_t x = cell % m_width;
		const uint32_t y = cell / m_width;
		uint32_t exits = 0;
		for (const auto direction: directions) exits += map.getEdgeCost(x, y, direction) != 0 ? 1u : 0u;
		for (const auto direction: directions) {
			const uint32_t edge = map.getEdgeCost(x, y, direction);
			if (edge == 0)
				continue;
			if (entry != noHeading && exits > 1 && direction == opposite(static_cast<Direction>(entry)))
				continue;
			const auto nx = static_cast<uint32_t>(static_cast<int32_t>(x) + offsetX(direction));
			const auto ny = static_cast<uint32_t>(static_cast<int32_t>(y) + offsetY(direction));
			relaxState((ny * m_width + nx) * states + static_cast<uint32_t>(direction), current.g + edge, current.node);
		}
	}
	if (m_open.empty() && m_state == PathState::Searching)
		m_state = PathState::NotFound;
}

void Pathfinder::finishFlat() {
	m_total = m_cost[m_to];
	uint32_t node = m_to;
	while (true) {
		m_cells.emplace_back(node % m_width, node / m_width);
		if (node == m_from)
			break;
		node = m_parent[node];
	}
	std::ranges::reverse(m_cells);
	m_state = PathState::Found;
}

void Pathfinder::stepAbstract(const uint32_t iBudget) {
	const uint64_t limit = m_work + iBudget;
	while (!m_open.empty() && m_work < limit) {
		std::ranges::pop_heap(m_open, [](const OpenNode& iL, const OpenNode& iR) -> bool { return openLess(iL, iR); });
		const OpenNode current = m_open.back();
		m_open.pop_back();
		if (current.g != m_cost[current.node])
			continue;
		++m_work;
		if (current.node == m_goalNode) {
			prepareRefine();
			m_phase = Phase::Refine;
			return;
		}
		expandBorder(current.node);
	}
	if (m_open.empty() && m_phase == Phase::Abstract)
		m_state = PathState::NotFound;
}

auto Pathfinder::hasCrossExit(const uint32_t iX, const uint32_t iY) const -> bool {
	return std::ranges::any_of(directions, [this, iX, iY](const Direction iDirection) -> bool {
		const auto nx = static_cast<int64_t>(iX) + offsetX(iDirection);
		const auto ny = static_cast<int64_t>(iY) + offsetY(iDirection);
		if (nx < 0 || ny < 0)
			return false;
		if (static_cast<uint32_t>(nx) / chunkSide == iX / chunkSide &&
			static_cast<uint32_t>(ny) / chunkSide == iY / chunkSide)
			return false;
		return mp_map->getEdgeCost(iX, iY, iDirection) != 0;
	});
}

void Pathfinder::expandBorder(const uint32_t iNode) {
	const NavMap& map = *mp_map;
	const uint32_t x = iNode % m_width;
	const uint32_t y = iNode / m_width;
	const uint32_t chunkX = x / chunkSide;
	const uint32_t chunkY = y / chunkSide;
	const uint32_t local = (y % chunkSide) * chunkSide + x % chunkSide;
	const uint32_t base = m_cost[iNode];
	if (static_cast<Kind>(m_kind[iNode]) == Kind::Cross) {
		NavRows* rows = nullptr;
		size_t index = 0;
		borderRow(chunkX, chunkY, local, rows, index);
		const uint32_t* row = rows->distance.data() + index * borderCellCount;
		const uint16_t* targets = rows->targets.data() + index * borderCellCount;
		const bool inGoalChunk = chunkX == m_toX / chunkSide && chunkY == m_toY / chunkSide;
		for (uint16_t k = 0; k < rows->count[index]; ++k) {
			const uint32_t border = targets[k];
			const uint32_t cell = NavMap::getBorderCell(border);
			const uint32_t cellX = chunkX * chunkSide + cell % chunkSide;
			const uint32_t cellY = chunkY * chunkSide + cell / chunkSide;
			if (!inGoalChunk && !hasCrossExit(cellX, cellY))
				continue;
			relax(cellY * m_width + cellX, base + row[border], iNode, Kind::Intra);
		}
	}
	for (const auto direction: directions) {
		const auto nx = static_cast<int64_t>(x) + offsetX(direction);
		const auto ny = static_cast<int64_t>(y) + offsetY(direction);
		if (nx < 0 || ny < 0)
			continue;
		const auto ux = static_cast<uint32_t>(nx);
		const auto uy = static_cast<uint32_t>(ny);
		if (ux / chunkSide == chunkX && uy / chunkSide == chunkY)
			continue;
		const uint32_t edge = map.getEdgeCost(x, y, direction);
		if (edge != 0)
			relax(uy * m_width + ux, base + edge, iNode, Kind::Cross);
	}
	if (chunkX == m_toX / chunkSide && chunkY == m_toY / chunkSide) {
		if (const uint32_t tail = m_goalDistance[local]; tail != unreachable)
			relax(m_goalNode, base + tail, iNode, Kind::Goal);
	}
}

void Pathfinder::searchChunk(const uint32_t iChunkX, const uint32_t iChunkY, const uint32_t iStart, const bool iReverse,
							 std::array<uint32_t, chunkCellCount>& oDistance,
							 std::array<uint8_t, chunkCellCount>& oDirection) {
	const NavMap& map = *mp_map;
	oDistance.fill(unreachable);
	oDirection.fill(0);
	m_localOpen.clear();
	const auto greater = [](const LocalNode& iL, const LocalNode& iR) -> bool { return iL.distance > iR.distance; };
	oDistance[iStart] = 0;
	m_localOpen.push_back({.distance = 0, .cell = iStart});
	while (!m_localOpen.empty()) {
		std::ranges::pop_heap(m_localOpen, greater);
		const LocalNode current = m_localOpen.back();
		m_localOpen.pop_back();
		if (current.distance != oDistance[current.cell])
			continue;
		++m_work;
		const auto lx = static_cast<int32_t>(current.cell % chunkSide);
		const auto ly = static_cast<int32_t>(current.cell / chunkSide);
		for (const auto direction: directions) {
			const int32_t dx = offsetX(direction);
			const int32_t dy = offsetY(direction);
			const int32_t otherX = iReverse ? lx - dx : lx + dx;
			const int32_t otherY = iReverse ? ly - dy : ly + dy;
			if (otherX < 0 || otherY < 0 || otherX >= static_cast<int32_t>(chunkSide) ||
				otherY >= static_cast<int32_t>(chunkSide))
				continue;
			const int32_t fromX = iReverse ? otherX : lx;
			const int32_t fromY = iReverse ? otherY : ly;
			const uint32_t edge = map.getEdgeCost(iChunkX * chunkSide + static_cast<uint32_t>(fromX),
												  iChunkY * chunkSide + static_cast<uint32_t>(fromY), direction);
			if (edge == 0)
				continue;
			const auto other = static_cast<uint32_t>(otherY) * chunkSide + static_cast<uint32_t>(otherX);
			const uint32_t candidate = current.distance + edge;
			if (candidate >= oDistance[other])
				continue;
			oDistance[other] = candidate;
			oDirection[other] = static_cast<uint8_t>(direction);
			m_localOpen.push_back({.distance = candidate, .cell = other});
			std::ranges::push_heap(m_localOpen, greater);
		}
	}
}

void Pathfinder::borderRow(const uint32_t iChunkX, const uint32_t iChunkY, const uint32_t iLocal, NavRows*& oRows,
						   size_t& oIndex) {
	const NavMap& map = *mp_map;
	oRows = &map.getRows(static_cast<size_t>(iChunkY) * map.getChunkGridSize().x() + iChunkX);
	oIndex = static_cast<size_t>(NavMap::getBorderIndex(iLocal));
	if (oRows->valid[oIndex] != 0)
		return;
	searchChunk(iChunkX, iChunkY, iLocal, false, m_scratchDistance, m_scratchDirection);
	uint32_t* row = oRows->distance.data() + oIndex * borderCellCount;
	uint16_t* targets = oRows->targets.data() + oIndex * borderCellCount;
	uint16_t count = 0;
	for (uint32_t border = 0; border < borderCellCount; ++border) {
		const uint32_t distance = m_scratchDistance[NavMap::getBorderCell(border)];
		row[border] = distance;
		if (distance != unreachable && border != oIndex)
			targets[count++] = static_cast<uint16_t>(border);
	}
	oRows->count[oIndex] = count;
	oRows->valid[oIndex] = 1;
}

void Pathfinder::prepareRefine() {
	m_total = m_cost[m_goalNode];
	m_chain.clear();
	m_cells.clear();
	m_refineIndex = 0;
	if (static_cast<Kind>(m_kind[m_goalNode]) == Kind::Direct)
		return;
	uint32_t node = m_parent[m_goalNode];
	m_chain.push_back(node);
	while (static_cast<Kind>(m_kind[node]) != Kind::Seed) {
		node = m_parent[node];
		m_chain.push_back(node);
	}
	std::ranges::reverse(m_chain);
}

void Pathfinder::appendForward(const std::array<uint8_t, chunkCellCount>& iDirection, const uint32_t iChunkX,
							   const uint32_t iChunkY, const uint32_t iStart, const uint32_t iEnd,
							   const bool iSkipFirst) {
	const size_t first = m_cells.size();
	uint32_t local = iEnd;
	for (uint32_t guard = 0; guard <= chunkCellCount; ++guard) {
		m_cells.emplace_back(iChunkX * chunkSide + local % chunkSide, iChunkY * chunkSide + local / chunkSide);
		if (local == iStart)
			break;
		const auto direction = static_cast<Direction>(iDirection[local]);
		local = static_cast<uint32_t>(static_cast<int32_t>(local / chunkSide) - offsetY(direction)) * chunkSide +
				static_cast<uint32_t>(static_cast<int32_t>(local % chunkSide) - offsetX(direction));
	}
	std::reverse(m_cells.begin() + static_cast<std::ptrdiff_t>(first), m_cells.end());
	if (iSkipFirst)
		m_cells.erase(m_cells.begin() + static_cast<std::ptrdiff_t>(first));
}

void Pathfinder::stepRefine() {
	++m_work;
	const uint32_t fromX = m_from % m_width;
	const uint32_t fromY = m_from / m_width;
	const auto localOf = [this](const uint32_t iNode) -> uint32_t {
		return ((iNode / m_width) % chunkSide) * chunkSide + (iNode % m_width) % chunkSide;
	};
	if (m_chain.empty()) {
		appendForward(m_startDirection, fromX / chunkSide, fromY / chunkSide, localOf(m_from), localOf(m_to), false);
		m_state = PathState::Found;
		return;
	}
	const size_t index = m_refineIndex++;
	if (index == 0) {
		appendForward(m_startDirection, fromX / chunkSide, fromY / chunkSide, localOf(m_from), localOf(m_chain[0]),
					  false);
		return;
	}
	if (index < m_chain.size()) {
		const uint32_t node = m_chain[index];
		const uint32_t nodeX = node % m_width;
		const uint32_t nodeY = node / m_width;
		if (static_cast<Kind>(m_kind[node]) == Kind::Cross) {
			m_cells.emplace_back(nodeX, nodeY);
			return;
		}
		const uint32_t previous = m_chain[index - 1];
		searchChunk(nodeX / chunkSide, nodeY / chunkSide, localOf(previous), false, m_scratchDistance,
					m_scratchDirection);
		appendForward(m_scratchDirection, nodeX / chunkSide, nodeY / chunkSide, localOf(previous), localOf(node), true);
		return;
	}
	uint32_t x = m_chain.back() % m_width;
	uint32_t y = m_chain.back() / m_width;
	for (uint32_t guard = 0; guard <= chunkCellCount && (x != m_toX || y != m_toY); ++guard) {
		const auto direction = static_cast<Direction>(m_goalDirection[(y % chunkSide) * chunkSide + x % chunkSide]);
		x = static_cast<uint32_t>(static_cast<int32_t>(x) + offsetX(direction));
		y = static_cast<uint32_t>(static_cast<int32_t>(y) + offsetY(direction));
		m_cells.emplace_back(x, y);
	}
	m_state = PathState::Found;
}

auto Pathfinder::takeResult(PathResult& ioResult) -> bool {
	if (m_state != PathState::Found)
		return false;
	ioResult.cells.swap(m_cells);
	ioResult.cost = m_total;
	m_state = PathState::Idle;
	return true;
}

auto isPathValid(const NavMap& iMap, const std::span<const math::vec2ui> iCells) -> bool {
	if (iCells.empty() || !iMap.isWalkable(iCells.front().x(), iCells.front().y()))
		return false;
	for (size_t i = 1; i < iCells.size(); ++i) {
		const auto& from = iCells[i - 1];
		const auto& to = iCells[i];
		const bool valid = std::ranges::any_of(directions, [&](const Direction iDirection) -> bool {
			return static_cast<int64_t>(from.x()) + offsetX(iDirection) == static_cast<int64_t>(to.x()) &&
				   static_cast<int64_t>(from.y()) + offsetY(iDirection) == static_cast<int64_t>(to.y()) &&
				   iMap.getEdgeCost(from.x(), from.y(), iDirection) != 0;
		});
		if (!valid)
			return false;
	}
	return true;
}

auto Pathfinder::find(const NavMap& iMap, const math::vec2ui& iFrom, const math::vec2ui& iTo, const PathMode iMode,
					  PathResult& oResult, const PathOptions& iOptions) -> bool {
	begin(iMap, iFrom, iTo, iMode, iOptions);
	step(0xFFFFFFFFu);
	return takeResult(oResult);
}

}// namespace owl::scene::nav
