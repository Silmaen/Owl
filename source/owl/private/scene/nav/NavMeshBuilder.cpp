/**
 * @file NavMeshBuilder.cpp
 * @author Silmaen
 * @date 08/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "NavMeshBuilder.h"
#include "scene/nav/WalkMesh.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <numbers>
#include <span>
#include <utility>
#include <vector>

namespace owl::scene::nav {

namespace {

// Directions of the four neighbours of a column: -X, +Z, +X, -Z.
constexpr std::array<int32_t, 4> g_dirX{-1, 0, 1, 0};
constexpr std::array<int32_t, 4> g_dirZ{0, 1, 0, -1};
// Top of the open space above the highest span of a column.
constexpr int32_t g_openTop = 0xffff;
constexpr int32_t g_noSpan = -1;
constexpr int32_t g_noRegion = 0;
// Share of a voxel height under which a top is not raised to the next voxel (rounding of the world heights).
constexpr float g_heightTolerance = 1e-3f;

struct Span {
	int32_t bottom = 0;
	int32_t top = 0;
	bool walkable = false;
};

struct Cells {
	float cellSize = 0.f;
	float cellHeight = 0.f;
	int32_t width = 0;
	int32_t depth = 0;
	int32_t maxHeight = 0;
	int32_t climb = 0;
	int32_t height = 0;
	int32_t radius = 0;
	math::vec3 origin;
};

using Polygon = std::vector<math::vec3>;

// Split a polygon by the line `axis = iLine`: the part below and the part above.
void divide(const Polygon& iPolygon, const float iLine, const size_t iAxis, Polygon& oBelow, Polygon& oAbove) {
	oBelow.clear();
	oAbove.clear();
	const size_t count = iPolygon.size();
	for (size_t index = 0, previous = count - 1; index < count; previous = index, ++index) {
		const float distPrevious = iLine - iPolygon[previous][iAxis];
		const float distCurrent = iLine - iPolygon[index][iAxis];
		if ((distPrevious >= 0.f) != (distCurrent >= 0.f)) {
			const float ratio = distPrevious / (distPrevious - distCurrent);
			const math::vec3 cut = iPolygon[previous] + (iPolygon[index] - iPolygon[previous]) * ratio;
			oBelow.push_back(cut);
			oAbove.push_back(cut);
		}
		if (distCurrent > 0.f) {
			oBelow.push_back(iPolygon[index]);
		} else if (distCurrent < 0.f) {
			oAbove.push_back(iPolygon[index]);
		} else {
			oBelow.push_back(iPolygon[index]);
			oAbove.push_back(iPolygon[index]);
		}
	}
}

class Heightfield {
public:
	explicit Heightfield(const Cells& iCells)
		: m_cells{iCells}, m_columns(static_cast<size_t>(iCells.width) * static_cast<size_t>(iCells.depth)) {}

	[[nodiscard]] auto column(const int32_t iX, const int32_t iZ) -> std::vector<Span>& {
		return m_columns[static_cast<size_t>(iX) + static_cast<size_t>(iZ) * static_cast<size_t>(m_cells.width)];
	}

	[[nodiscard]] auto column(const int32_t iX, const int32_t iZ) const -> const std::vector<Span>& {
		return m_columns[static_cast<size_t>(iX) + static_cast<size_t>(iZ) * static_cast<size_t>(m_cells.width)];
	}

	// A new span merges with the spans it overlaps; the flag of the highest top wins, both when the tops are close.
	void addSpan(const int32_t iX, const int32_t iZ, Span iSpan) {
		auto& spans = column(iX, iZ);
		std::vector<Span> kept;
		kept.reserve(spans.size() + 1);
		for (const auto& span: spans) {
			if (span.top < iSpan.bottom || span.bottom > iSpan.top) {
				kept.push_back(span);
				continue;
			}
			const int32_t top = std::max(span.top, iSpan.top);
			const bool spanFlag = top - span.top <= 1 && span.walkable;
			const bool newFlag = top - iSpan.top <= 1 && iSpan.walkable;
			iSpan.walkable = spanFlag || newFlag;
			iSpan.bottom = std::min(span.bottom, iSpan.bottom);
			iSpan.top = top;
		}
		kept.push_back(iSpan);
		std::ranges::sort(kept, [](const Span& iA, const Span& iB) -> bool { return iA.bottom < iB.bottom; });
		spans = std::move(kept);
	}

	void rasterize(const math::vec3& iA, const math::vec3& iB, const math::vec3& iC, const bool iWalkable) {
		const float cellSize = m_cells.cellSize;
		const math::vec3& origin = m_cells.origin;
		const float minZ = std::min({iA.z(), iB.z(), iC.z()});
		const float maxZ = std::max({iA.z(), iB.z(), iC.z()});
		const float maxSpanY = static_cast<float>(m_cells.maxHeight) * m_cells.cellHeight;
		const auto firstZ =
				std::clamp(static_cast<int32_t>(std::floor((minZ - origin.z()) / cellSize)), 0, m_cells.depth - 1);
		const auto lastZ =
				std::clamp(static_cast<int32_t>(std::floor((maxZ - origin.z()) / cellSize)), 0, m_cells.depth - 1);
		Polygon rest{iA, iB, iC};
		Polygon row;
		Polygon cell;
		Polygon scratch;
		divide(Polygon{rest}, origin.z(), 2, scratch, rest);
		for (int32_t z = firstZ; z <= lastZ && rest.size() >= 3; ++z) {
			divide(Polygon{rest}, origin.z() + static_cast<float>(z + 1) * cellSize, 2, row, rest);
			if (row.size() < 3)
				continue;
			float minX = row.front().x();
			float maxX = row.front().x();
			for (const auto& point: row) {
				minX = std::min(minX, point.x());
				maxX = std::max(maxX, point.x());
			}
			const auto firstX =
					std::clamp(static_cast<int32_t>(std::floor((minX - origin.x()) / cellSize)), 0, m_cells.width - 1);
			const auto lastX =
					std::clamp(static_cast<int32_t>(std::floor((maxX - origin.x()) / cellSize)), 0, m_cells.width - 1);
			Polygon line;
			divide(row, origin.x(), 0, scratch, line);
			for (int32_t x = firstX; x <= lastX && line.size() >= 3; ++x) {
				divide(Polygon{line}, origin.x() + static_cast<float>(x + 1) * cellSize, 0, cell, line);
				if (cell.size() < 3)
					continue;
				float minY = cell.front().y();
				float maxY = cell.front().y();
				for (const auto& point: cell) {
					minY = std::min(minY, point.y());
					maxY = std::max(maxY, point.y());
				}
				minY -= origin.y();
				maxY -= origin.y();
				if (maxY < 0.f || minY > maxSpanY)
					continue;
				const auto bottom =
						std::clamp(static_cast<int32_t>(std::floor(std::max(minY, 0.f) / m_cells.cellHeight)), 0,
								   m_cells.maxHeight);
				const auto top = std::clamp(static_cast<int32_t>(std::ceil(
													std::min(maxY, maxSpanY) / m_cells.cellHeight - g_heightTolerance)),
											bottom, m_cells.maxHeight);
				addSpan(x, z, {.bottom = bottom, .top = top, .walkable = iWalkable});
			}
		}
	}

	// A non-walkable span a step above a walkable one (a kerb, a stair edge) is walked over.
	void filterLowHangingObstacles() {
		for (auto& spans: m_columns) {
			bool previousWalkable = false;
			int32_t previousTop = 0;
			for (auto& span: spans) {
				const bool wasWalkable = span.walkable;
				if (!span.walkable && previousWalkable && span.top - previousTop <= m_cells.climb)
					span.walkable = true;
				previousWalkable = wasWalkable;
				previousTop = span.top;
			}
		}
	}

	// A span next to a drop higher than a step, or on a slope steeper than a step between its neighbours, is a ledge.
	void filterLedges() {
		for (int32_t z = 0; z < m_cells.depth; ++z) {
			for (int32_t x = 0; x < m_cells.width; ++x) {
				auto& spans = column(x, z);
				for (size_t index = 0; index < spans.size(); ++index) {
					if (!spans[index].walkable)
						continue;
					const int32_t floor = spans[index].top;
					const int32_t ceiling = index + 1 < spans.size() ? spans[index + 1].bottom : g_openTop;
					if (isLedge(x, z, floor, ceiling))
						spans[index].walkable = false;
				}
			}
		}
	}

	[[nodiscard]] auto isLedge(const int32_t iX, const int32_t iZ, const int32_t iFloor, const int32_t iCeiling) const
			-> bool {
		int32_t lowestDrop = g_openTop;
		int32_t accessibleMin = iFloor;
		int32_t accessibleMax = iFloor;
		for (size_t dir = 0; dir < 4; ++dir) {
			const int32_t nx = iX + g_dirX[dir];
			const int32_t nz = iZ + g_dirZ[dir];
			if (nx < 0 || nz < 0 || nx >= m_cells.width || nz >= m_cells.depth) {
				lowestDrop = std::min(lowestDrop, -m_cells.climb - 1);
				continue;
			}
			const auto& neighbours = column(nx, nz);
			int32_t neighbourFloor = -m_cells.climb;
			int32_t neighbourCeiling = neighbours.empty() ? g_openTop : neighbours.front().bottom;
			if (std::min(iCeiling, neighbourCeiling) - std::max(iFloor, neighbourFloor) > m_cells.height)
				lowestDrop = std::min(lowestDrop, neighbourFloor - iFloor);
			for (size_t index = 0; index < neighbours.size(); ++index) {
				neighbourFloor = neighbours[index].top;
				neighbourCeiling = index + 1 < neighbours.size() ? neighbours[index + 1].bottom : g_openTop;
				if (std::min(iCeiling, neighbourCeiling) - std::max(iFloor, neighbourFloor) <= m_cells.height)
					continue;
				lowestDrop = std::min(lowestDrop, neighbourFloor - iFloor);
				if (std::abs(neighbourFloor - iFloor) <= m_cells.climb) {
					accessibleMin = std::min(accessibleMin, neighbourFloor);
					accessibleMax = std::max(accessibleMax, neighbourFloor);
				}
			}
		}
		return lowestDrop < -m_cells.climb || accessibleMax - accessibleMin > m_cells.climb;
	}

	// A span with less free height above it than an agent is not walkable.
	void filterLowCeilings() {
		for (auto& spans: m_columns) {
			for (size_t index = 0; index < spans.size(); ++index) {
				const int32_t ceiling = index + 1 < spans.size() ? spans[index + 1].bottom : g_openTop;
				if (ceiling - spans[index].top < m_cells.height)
					spans[index].walkable = false;
			}
		}
	}

private:
	Cells m_cells;
	std::vector<std::vector<Span>> m_columns;
};

// An open space above a walkable span, linked to the open spans of the four neighbouring columns.
struct OpenSpan {
	int32_t floor = 0;
	int32_t height = 0;
	std::array<int32_t, 4> links{g_noSpan, g_noSpan, g_noSpan, g_noSpan};
	int32_t region = g_noRegion;
	int32_t distance = 0;
	bool walkable = true;
};

struct Contour {
	int32_t region = g_noRegion;
	// Corners in cells: x, height, z.
	std::vector<std::array<int32_t, 3>> points;
};

struct ContourPoint {
	int32_t x = 0;
	int32_t y = 0;
	int32_t z = 0;
	// Region across the edge leaving this point (0 on a wall).
	int32_t region = g_noRegion;
};

class OpenField {
public:
	OpenField(const Heightfield& iField, const Cells& iCells) : m_cells{iCells} {
		m_columns.reserve(static_cast<size_t>(iCells.width) * static_cast<size_t>(iCells.depth));
		for (int32_t z = 0; z < iCells.depth; ++z) {
			for (int32_t x = 0; x < iCells.width; ++x) {
				const auto& spans = iField.column(x, z);
				const auto first = static_cast<int32_t>(m_spans.size());
				for (size_t index = 0; index < spans.size(); ++index) {
					if (!spans[index].walkable)
						continue;
					const int32_t ceiling = index + 1 < spans.size() ? spans[index + 1].bottom : g_openTop;
					m_spans.push_back(
							{.floor = spans[index].top, .height = std::min(ceiling - spans[index].top, g_openTop)});
				}
				m_columns.emplace_back(first, static_cast<int32_t>(m_spans.size()) - first);
			}
		}
		linkNeighbours();
	}

	void erode() {
		std::vector<int32_t> distance(m_spans.size(), g_openTop);
		for (size_t index = 0; index < m_spans.size(); ++index) {
			if (std::ranges::any_of(m_spans[index].links,
									[](const int32_t iLink) -> bool { return iLink == g_noSpan; }))
				distance[index] = 0;
		}
		const auto relax = [&](const size_t iSpan, const size_t iDir, const size_t iTurn) -> void {
			const int32_t straight = m_spans[iSpan].links[iDir];
			if (straight == g_noSpan)
				return;
			distance[iSpan] = std::min(distance[iSpan], distance[static_cast<size_t>(straight)] + 2);
			if (const int32_t diagonal = m_spans[static_cast<size_t>(straight)].links[iTurn]; diagonal != g_noSpan)
				distance[iSpan] = std::min(distance[iSpan], distance[static_cast<size_t>(diagonal)] + 3);
		};
		for (size_t index = 0; index < m_spans.size(); ++index) {
			relax(index, 0, 3);
			relax(index, 3, 2);
		}
		for (size_t index = m_spans.size(); index-- > 0;) {
			relax(index, 2, 1);
			relax(index, 1, 0);
		}
		const int32_t threshold = m_cells.radius * 2;
		for (size_t index = 0; index < m_spans.size(); ++index) {
			m_spans[index].distance = distance[index];
			if (distance[index] < threshold)
				m_spans[index].walkable = false;
		}
		unlinkBlocked();
	}

	// Islands of floor smaller than the threshold (in cells) are dropped.
	void removeSmallIslands(const int32_t iMinCells) {
		std::vector<int32_t> island(m_spans.size(), -1);
		std::vector<size_t> stack;
		std::vector<size_t> members;
		for (size_t seed = 0; seed < m_spans.size(); ++seed) {
			if (!m_spans[seed].walkable || island[seed] >= 0)
				continue;
			members.clear();
			stack.assign(1, seed);
			island[seed] = static_cast<int32_t>(seed);
			while (!stack.empty()) {
				const size_t current = stack.back();
				stack.pop_back();
				members.push_back(current);
				for (const int32_t link: m_spans[current].links) {
					if (link == g_noSpan || island[static_cast<size_t>(link)] >= 0)
						continue;
					island[static_cast<size_t>(link)] = static_cast<int32_t>(seed);
					stack.push_back(static_cast<size_t>(link));
				}
			}
			if (std::cmp_less(members.size(), iMinCells)) {
				for (const size_t member: members) m_spans[member].walkable = false;
			}
		}
		unlinkBlocked();
	}

	// Rows swept along X: a sweep joins the region of the previous row only when it is its single neighbour there.
	void buildMonotoneRegions() {
		struct Sweep {
			int32_t id = 0;
			int32_t neighbour = 0;
			int32_t samples = 0;
		};
		constexpr int32_t mixed = -1;
		int32_t nextId = 1;
		std::vector<Sweep> sweeps;
		std::vector<int32_t> previousCount;
		for (int32_t z = 0; z < m_cells.depth; ++z) {
			sweeps.assign(1, {});
			previousCount.assign(static_cast<size_t>(nextId) + 1, 0);
			int32_t rowId = 1;
			for (int32_t x = 0; x < m_cells.width; ++x) {
				const auto [first, count] = columnOf(x, z);
				for (int32_t index = first; index < first + count; ++index) {
					auto& span = m_spans[static_cast<size_t>(index)];
					if (!span.walkable)
						continue;
					int32_t sweep = 0;
					if (const int32_t left = span.links[0]; left != g_noSpan)
						sweep = m_spans[static_cast<size_t>(left)].region;
					if (sweep == 0) {
						sweep = rowId++;
						sweeps.push_back({});
					}
					if (const int32_t below = span.links[3]; below != g_noSpan) {
						const int32_t belowRegion = m_spans[static_cast<size_t>(below)].region;
						auto& current = sweeps[static_cast<size_t>(sweep)];
						if (belowRegion != 0 && (current.neighbour == 0 || current.neighbour == belowRegion)) {
							current.neighbour = belowRegion;
							++current.samples;
							++previousCount[static_cast<size_t>(belowRegion)];
						} else if (belowRegion != 0) {
							current.neighbour = mixed;
						}
					}
					span.region = sweep;
				}
			}
			for (size_t sweep = 1; sweep < sweeps.size(); ++sweep) {
				auto& current = sweeps[sweep];
				if (current.neighbour > 0 && previousCount[static_cast<size_t>(current.neighbour)] == current.samples) {
					current.id = current.neighbour;
				} else {
					current.id = nextId++;
					previousCount.push_back(0);
				}
			}
			for (int32_t x = 0; x < m_cells.width; ++x) {
				const auto [first, count] = columnOf(x, z);
				for (int32_t index = first; index < first + count; ++index) {
					auto& span = m_spans[static_cast<size_t>(index)];
					if (span.walkable && span.region > 0)
						span.region = sweeps[static_cast<size_t>(span.region)].id;
				}
			}
		}
	}

	[[nodiscard]] auto buildContours(const float iMaxError) const -> std::vector<Contour> {
		std::vector<uint8_t> edges(m_spans.size(), 0);
		for (size_t index = 0; index < m_spans.size(); ++index) {
			const auto& span = m_spans[index];
			if (!span.walkable || span.region == g_noRegion)
				continue;
			uint8_t open = 0;
			for (size_t dir = 0; dir < 4; ++dir) {
				const int32_t link = span.links[dir];
				if (link == g_noSpan || m_spans[static_cast<size_t>(link)].region != span.region)
					open = static_cast<uint8_t>(open | (1u << dir));
			}
			edges[index] = open == 0x0f ? 0 : open;
		}
		std::vector<Contour> contours;
		std::vector<ContourPoint> raw;
		for (int32_t z = 0; z < m_cells.depth; ++z) {
			for (int32_t x = 0; x < m_cells.width; ++x) {
				const auto [first, count] = columnOf(x, z);
				for (int32_t index = first; index < first + count; ++index) {
					if (edges[static_cast<size_t>(index)] == 0)
						continue;
					raw.clear();
					walkContour(x, z, index, edges, raw);
					auto points = simplify(raw, iMaxError);
					if (points.size() >= 3)
						contours.push_back(
								{.region = m_spans[static_cast<size_t>(index)].region, .points = std::move(points)});
				}
			}
		}
		return contours;
	}

private:
	Cells m_cells;
	std::vector<OpenSpan> m_spans;
	// First open span and count per column.
	std::vector<std::pair<int32_t, int32_t>> m_columns;

	[[nodiscard]] auto columnOf(const int32_t iX, const int32_t iZ) const -> std::pair<int32_t, int32_t> {
		return m_columns[static_cast<size_t>(iX) + static_cast<size_t>(iZ) * static_cast<size_t>(m_cells.width)];
	}

	void linkNeighbours() {
		for (int32_t z = 0; z < m_cells.depth; ++z) {
			for (int32_t x = 0; x < m_cells.width; ++x) {
				const auto [first, count] = columnOf(x, z);
				for (int32_t index = first; index < first + count; ++index) {
					auto& span = m_spans[static_cast<size_t>(index)];
					for (size_t dir = 0; dir < 4; ++dir) {
						const int32_t nx = x + g_dirX[dir];
						const int32_t nz = z + g_dirZ[dir];
						if (nx < 0 || nz < 0 || nx >= m_cells.width || nz >= m_cells.depth)
							continue;
						const auto [nFirst, nCount] = columnOf(nx, nz);
						for (int32_t other = nFirst; other < nFirst + nCount; ++other) {
							const auto& neighbour = m_spans[static_cast<size_t>(other)];
							const int32_t bottom = std::max(span.floor, neighbour.floor);
							const int32_t top = std::min(span.floor + span.height, neighbour.floor + neighbour.height);
							if (top - bottom >= m_cells.height &&
								std::abs(neighbour.floor - span.floor) <= m_cells.climb) {
								span.links[dir] = other;
								break;
							}
						}
					}
				}
			}
		}
	}

	void unlinkBlocked() {
		for (auto& span: m_spans) {
			for (auto& link: span.links) {
				if (link != g_noSpan && (!span.walkable || !m_spans[static_cast<size_t>(link)].walkable))
					link = g_noSpan;
			}
		}
	}

	// Height of the corner between a span, its neighbour in `iDir` and the one after it clockwise: the highest floor.
	[[nodiscard]] auto getCornerHeight(const int32_t iSpan, const size_t iDir) const -> int32_t {
		const auto& span = m_spans[static_cast<size_t>(iSpan)];
		const size_t next = (iDir + 1) & 3u;
		int32_t height = span.floor;
		if (const int32_t side = span.links[iDir]; side != g_noSpan) {
			height = std::max(height, m_spans[static_cast<size_t>(side)].floor);
			if (const int32_t corner = m_spans[static_cast<size_t>(side)].links[next]; corner != g_noSpan)
				height = std::max(height, m_spans[static_cast<size_t>(corner)].floor);
		}
		if (const int32_t side = span.links[next]; side != g_noSpan) {
			height = std::max(height, m_spans[static_cast<size_t>(side)].floor);
			if (const int32_t corner = m_spans[static_cast<size_t>(side)].links[iDir]; corner != g_noSpan)
				height = std::max(height, m_spans[static_cast<size_t>(corner)].floor);
		}
		return height;
	}

	// Follow the border of a region from a span, wall on the left, until back at the start.
	void walkContour(int32_t iX, int32_t iZ, int32_t iSpan, std::vector<uint8_t>& ioEdges,
					 std::vector<ContourPoint>& oPoints) const {
		size_t dir = 0;
		while ((ioEdges[static_cast<size_t>(iSpan)] & (1u << dir)) == 0) ++dir;
		const size_t startDir = dir;
		const int32_t startSpan = iSpan;
		const size_t guard = m_spans.size() * 8 + 16;
		for (size_t step = 0; step < guard; ++step) {
			auto& edge = ioEdges[static_cast<size_t>(iSpan)];
			if ((edge & (1u << dir)) != 0) {
				int32_t px = iX;
				int32_t pz = iZ;
				switch (dir) {
					case 0:
						++pz;
						break;
					case 1:
						++px;
						++pz;
						break;
					case 2:
						++px;
						break;
					default:
						break;
				}
				const int32_t link = m_spans[static_cast<size_t>(iSpan)].links[dir];
				const int32_t region = link == g_noSpan ? g_noRegion : m_spans[static_cast<size_t>(link)].region;
				oPoints.push_back({.x = px, .y = getCornerHeight(iSpan, dir), .z = pz, .region = region});
				edge = static_cast<uint8_t>(edge & ~(1u << dir));
				dir = (dir + 1) & 3u;
			} else {
				iSpan = m_spans[static_cast<size_t>(iSpan)].links[dir];
				iX += g_dirX[dir];
				iZ += g_dirZ[dir];
				dir = (dir + 3) & 3u;
			}
			if (iSpan == startSpan && dir == startDir)
				break;
		}
	}

	// Keep the corners where the neighbouring region changes, then add wall points until the walls are within error.
	[[nodiscard]] static auto simplify(const std::vector<ContourPoint>& iRaw, const float iMaxError)
			-> std::vector<std::array<int32_t, 3>> {
		struct Kept {
			int32_t x;
			int32_t y;
			int32_t z;
			size_t raw;
		};
		const size_t count = iRaw.size();
		if (count < 3)
			return {};
		std::vector<Kept> kept;
		if (std::ranges::any_of(iRaw, [](const ContourPoint& iPoint) -> bool { return iPoint.region != g_noRegion; })) {
			for (size_t index = 0; index < count; ++index) {
				if (iRaw[index].region != iRaw[(index + 1) % count].region)
					kept.push_back({iRaw[index].x, iRaw[index].y, iRaw[index].z, index});
			}
		}
		if (kept.empty()) {
			size_t lowest = 0;
			size_t highest = 0;
			for (size_t index = 1; index < count; ++index) {
				const auto& point = iRaw[index];
				if (point.x < iRaw[lowest].x || (point.x == iRaw[lowest].x && point.z < iRaw[lowest].z))
					lowest = index;
				if (point.x > iRaw[highest].x || (point.x == iRaw[highest].x && point.z > iRaw[highest].z))
					highest = index;
			}
			kept.push_back({iRaw[lowest].x, iRaw[lowest].y, iRaw[lowest].z, lowest});
			kept.push_back({iRaw[highest].x, iRaw[highest].y, iRaw[highest].z, highest});
		}
		const float maxErrorSq = iMaxError * iMaxError;
		for (size_t index = 0; index < kept.size();) {
			const size_t nextIndex = (index + 1) % kept.size();
			int32_t ax = kept[index].x;
			int32_t az = kept[index].z;
			int32_t bx = kept[nextIndex].x;
			int32_t bz = kept[nextIndex].z;
			const size_t startRaw = kept[index].raw;
			const size_t endRaw = kept[nextIndex].raw;
			size_t step = 1;
			size_t current = (startRaw + 1) % count;
			size_t last = endRaw;
			// Walk each segment in the same direction whichever side traces it, so two regions agree on it.
			if (bx < ax || (bx == ax && bz < az)) {
				step = count - 1;
				current = (endRaw + step) % count;
				last = startRaw;
				std::swap(ax, bx);
				std::swap(az, bz);
			}
			float worst = 0.f;
			size_t worstRaw = count;
			if (iRaw[current].region == g_noRegion) {
				while (current != last) {
					const float distance = distanceToSegmentSq(iRaw[current], ax, az, bx, bz);
					if (distance > worst) {
						worst = distance;
						worstRaw = current;
					}
					current = (current + step) % count;
				}
			}
			if (worstRaw != count && worst > maxErrorSq) {
				const auto& point = iRaw[worstRaw];
				kept.insert(kept.begin() + static_cast<std::ptrdiff_t>(index + 1),
							{point.x, point.y, point.z, worstRaw});
			} else {
				++index;
			}
		}
		std::vector<std::array<int32_t, 3>> result;
		result.reserve(kept.size());
		for (const auto& point: kept) {
			if (!result.empty() && result.back()[0] == point.x && result.back()[2] == point.z)
				continue;
			result.push_back({point.x, point.y, point.z});
		}
		while (result.size() > 1 && result.back()[0] == result.front()[0] && result.back()[2] == result.front()[2])
			result.pop_back();
		return result;
	}

	[[nodiscard]] static auto distanceToSegmentSq(const ContourPoint& iPoint, const int32_t iAx, const int32_t iAz,
												  const int32_t iBx, const int32_t iBz) -> float {
		const auto segX = static_cast<float>(iBx - iAx);
		const auto segZ = static_cast<float>(iBz - iAz);
		const auto dx = static_cast<float>(iPoint.x - iAx);
		const auto dz = static_cast<float>(iPoint.z - iAz);
		const float lengthSq = segX * segX + segZ * segZ;
		float ratio = segX * dx + segZ * dz;
		if (lengthSq > 0.f)
			ratio /= lengthSq;
		ratio = std::clamp(ratio, 0.f, 1.f);
		const float ox = segX * ratio - dx;
		const float oz = segZ * ratio - dz;
		return ox * ox + oz * oz;
	}
};

auto signedArea(const std::vector<std::array<int32_t, 3>>& iPoints) -> int64_t {
	int64_t area = 0;
	for (size_t index = 0; index < iPoints.size(); ++index) {
		const auto& a = iPoints[index];
		const auto& b = iPoints[(index + 1) % iPoints.size()];
		area += static_cast<int64_t>(a[0]) * b[2] - static_cast<int64_t>(b[0]) * a[2];
	}
	return area;
}

auto toPlane(const std::vector<std::array<int32_t, 3>>& iPoints) -> std::vector<math::vec2> {
	std::vector<math::vec2> ring;
	ring.reserve(iPoints.size());
	for (const auto& point: iPoints) ring.emplace_back(static_cast<float>(point[0]), static_cast<float>(point[2]));
	return ring;
}

// Corners welded across regions: same column corner and a height within a step.
class VertexWelder {
public:
	VertexWelder(const Cells& iCells, NavBuildOutput& ioOutput) : m_cells{iCells}, m_output{ioOutput} {}

	auto add(const int32_t iX, const int32_t iY, const int32_t iZ) -> uint32_t {
		auto& candidates = m_corners[{iX, iZ}];
		for (const auto& [height, index]: candidates) {
			if (std::abs(height - iY) <= m_cells.climb)
				return index;
		}
		const auto index = static_cast<uint32_t>(m_output.vertices.size());
		m_output.vertices.emplace_back(m_cells.origin.x() + static_cast<float>(iX) * m_cells.cellSize,
									   m_cells.origin.y() + static_cast<float>(iY) * m_cells.cellHeight,
									   m_cells.origin.z() + static_cast<float>(iZ) * m_cells.cellSize);
		candidates.emplace_back(iY, index);
		return index;
	}

private:
	Cells m_cells;
	NavBuildOutput& m_output;
	std::map<std::pair<int32_t, int32_t>, std::vector<std::pair<int32_t, uint32_t>>> m_corners;
};

void triangulateRegions(const std::vector<Contour>& iContours, const Cells& iCells, NavBuildOutput& ioOutput) {
	std::map<int32_t, std::vector<const Contour*>> regions;
	for (const auto& contour: iContours) regions[contour.region].push_back(&contour);
	VertexWelder welder{iCells, ioOutput};
	WalkMesh mesh;
	for (const auto& [region, contours]: regions) {
		const auto outline = std::ranges::max_element(contours, {}, [](const Contour* iContour) -> int64_t {
			return std::abs(signedArea(iContour->points));
		});
		WalkPolygon polygon{.outline = toPlane((*outline)->points), .holes = {}};
		std::map<std::pair<int32_t, int32_t>, int32_t> heights;
		for (const auto* contour: contours) {
			if (contour != *outline)
				polygon.holes.push_back(toPlane(contour->points));
			for (const auto& point: contour->points) heights.try_emplace({point[0], point[2]}, point[1]);
		}
		mesh.build(std::span{&polygon, 1});
		const auto& corners = mesh.getVertices();
		for (const auto& triangle: mesh.getTriangles()) {
			std::array<uint32_t, 3> indices{};
			for (size_t corner = 0; corner < 3; ++corner) {
				const auto& point = corners[triangle.vertices[corner]];
				const auto x = static_cast<int32_t>(std::lround(point.x()));
				const auto z = static_cast<int32_t>(std::lround(point.y()));
				const auto height = heights.find({x, z});
				indices[corner] = welder.add(x, height != heights.end() ? height->second : 0, z);
			}
			if (indices[0] == indices[1] || indices[1] == indices[2] || indices[0] == indices[2])
				continue;
			ioOutput.indices.insert(ioOutput.indices.end(), indices.begin(), indices.end());
		}
	}
}

}// namespace

auto buildNavTriangles(const NavGeometry& iGeometry, const NavMeshSettings& iSettings, const math::vec3& iBoundsMin,
					   const math::vec3& iBoundsMax) -> owl::expected<NavBuildOutput, NavBuildError> {
	OWL_PROFILE_FUNCTION()

	const math::vec3 extent = iBoundsMax - iBoundsMin;
	if (iSettings.cellSize <= 0.f || iSettings.cellHeight <= 0.f || iSettings.agentHeight <= 0.f ||
		iSettings.agentRadius < 0.f || iSettings.maxClimb < 0.f || extent.x() <= 0.f || extent.y() <= 0.f ||
		extent.z() <= 0.f) {
		OWL_CORE_WARN("NavMesh: Invalid settings or empty bounds, nothing built.")
		return owl::unexpected{NavBuildError::InvalidSettings};
	}
	if (iGeometry.isEmpty()) {
		OWL_CORE_WARN("NavMesh: No geometry to build from.")
		return owl::unexpected{NavBuildError::NoGeometry};
	}
	const double columns = std::ceil(static_cast<double>(extent.x() / iSettings.cellSize)) *
						   std::ceil(static_cast<double>(extent.z() / iSettings.cellSize));
	if (columns > static_cast<double>(NavMesh::MaxColumns)) {
		OWL_CORE_WARN("NavMesh: {:.0f} voxel columns, more than the {} accepted: raise the cell size.", columns,
					  NavMesh::MaxColumns)
		return owl::unexpected{NavBuildError::TooLarge};
	}
	const Cells cells{
			.cellSize = iSettings.cellSize,
			.cellHeight = iSettings.cellHeight,
			.width = static_cast<int32_t>(std::ceil(extent.x() / iSettings.cellSize)),
			.depth = static_cast<int32_t>(std::ceil(extent.z() / iSettings.cellSize)),
			.maxHeight = std::min(static_cast<int32_t>(std::ceil(extent.y() / iSettings.cellHeight)), g_openTop - 1),
			.climb = static_cast<int32_t>(std::floor(iSettings.maxClimb / iSettings.cellHeight)),
			.height = static_cast<int32_t>(std::ceil(iSettings.agentHeight / iSettings.cellHeight)),
			.radius = static_cast<int32_t>(std::ceil(iSettings.agentRadius / iSettings.cellSize)),
			.origin = iBoundsMin};

	Heightfield field{cells};
	const float cosSlope = std::cos(std::clamp(iSettings.maxSlope, 0.f, 90.f) * std::numbers::pi_v<float> / 180.f);
	bool anyTriangle = false;
	const auto& vertices = iGeometry.vertices;
	for (size_t index = 0; index + 2 < iGeometry.indices.size(); index += 3) {
		const uint32_t ia = iGeometry.indices[index];
		const uint32_t ib = iGeometry.indices[index + 1];
		const uint32_t ic = iGeometry.indices[index + 2];
		if (ia >= vertices.size() || ib >= vertices.size() || ic >= vertices.size())
			continue;
		const math::vec3& a = vertices[ia];
		const math::vec3& b = vertices[ib];
		const math::vec3& c = vertices[ic];
		const math::vec3 normal = (b - a) ^ (c - a);
		const float length = normal.norm();
		if (length <= 0.f)
			continue;
		if (std::max({a.x(), b.x(), c.x()}) < iBoundsMin.x() || std::min({a.x(), b.x(), c.x()}) > iBoundsMax.x() ||
			std::max({a.z(), b.z(), c.z()}) < iBoundsMin.z() || std::min({a.z(), b.z(), c.z()}) > iBoundsMax.z() ||
			std::max({a.y(), b.y(), c.y()}) < iBoundsMin.y() || std::min({a.y(), b.y(), c.y()}) > iBoundsMax.y())
			continue;
		anyTriangle = true;
		field.rasterize(a, b, c, normal.y() >= cosSlope * length);
	}
	if (!anyTriangle) {
		OWL_CORE_WARN("NavMesh: No triangle inside the bounds.")
		return owl::unexpected{NavBuildError::NoGeometry};
	}
	field.filterLowHangingObstacles();
	field.filterLedges();
	field.filterLowCeilings();

	OpenField open{field, cells};
	open.erode();
	const auto minCells =
			static_cast<int32_t>(std::ceil(iSettings.minRegionArea / (iSettings.cellSize * iSettings.cellSize)));
	open.removeSmallIslands(minCells);
	open.buildMonotoneRegions();
	const auto contours = open.buildContours(iSettings.maxEdgeError / iSettings.cellSize);

	NavBuildOutput output;
	triangulateRegions(contours, cells, output);
	if (output.indices.empty()) {
		OWL_CORE_WARN("NavMesh: No floor is flat, wide and high enough for an agent.")
		return owl::unexpected{NavBuildError::NoWalkableArea};
	}
	OWL_CORE_DEBUG("NavMesh: {} triangles from {} regions on {}x{} columns.", output.indices.size() / 3,
				   contours.size(), cells.width, cells.depth)
	return output;
}

}// namespace owl::scene::nav
