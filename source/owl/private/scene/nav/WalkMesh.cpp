/**
 * @file WalkMesh.cpp
 * @author Silmaen
 * @date 31/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "math/elementaryFunctions.h"
#include "scene/nav/WalkMesh.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <queue>
#include <ranges>
#include <span>
#include <tuple>
#include <utility>
#include <vector>

namespace owl::scene::nav {

namespace {

// Tolerance of the orientation tests, in squared units of the plane.
constexpr float g_epsilon = 1e-6f;
// Distance under which two points are the same vertex.
constexpr float g_weld = 1e-5f;

// Twice the signed area of (a, b, c): positive when c is to the left of a -> b.
auto cross(const math::vec2& iA, const math::vec2& iB, const math::vec2& iC) -> float {
	return (iB.x() - iA.x()) * (iC.y() - iA.y()) - (iB.y() - iA.y()) * (iC.x() - iA.x());
}

auto isSame(const math::vec2& iA, const math::vec2& iB) -> bool { return (iA - iB).normSq() <= g_weld * g_weld; }

auto signedArea(const std::vector<math::vec2>& iRing) -> float {
	float area = 0.f;
	for (size_t index = 0; index < iRing.size(); ++index) {
		const auto& a = iRing[index];
		const auto& b = iRing[(index + 1) % iRing.size()];
		area += a.x() * b.y() - b.x() * a.y();
	}
	return area * 0.5f;
}

// The ring without repeated consecutive points nor a closing point equal to the first.
auto cleanRing(const std::vector<math::vec2>& iRing) -> std::vector<math::vec2> {
	std::vector<math::vec2> ring;
	ring.reserve(iRing.size());
	for (const auto& point: iRing) {
		if (ring.empty() || !isSame(ring.back(), point))
			ring.push_back(point);
	}
	while (ring.size() > 1 && isSame(ring.front(), ring.back())) ring.pop_back();
	return ring;
}

// Inside or on the border of the counter-clockwise triangle (a, b, c).
auto isInTriangle(const math::vec2& iA, const math::vec2& iB, const math::vec2& iC, const math::vec2& iPoint,
				  const float iTolerance = g_epsilon) -> bool {
	return cross(iA, iB, iPoint) >= -iTolerance && cross(iB, iC, iPoint) >= -iTolerance &&
		   cross(iC, iA, iPoint) >= -iTolerance;
}

// Inside or on the border of a triangle of either winding.
auto isInAnyTriangle(const math::vec2& iA, const math::vec2& iB, const math::vec2& iC, const math::vec2& iPoint)
		-> bool {
	if (cross(iA, iB, iC) >= 0.f)
		return isInTriangle(iA, iB, iC, iPoint);
	return isInTriangle(iA, iC, iB, iPoint);
}

// Whether the direction from a ring corner to a point goes into the ring (counter-clockwise ring).
auto isInSector(const math::vec2& iPrevious, const math::vec2& iCorner, const math::vec2& iNext,
				const math::vec2& iPoint) -> bool {
	if (cross(iPrevious, iCorner, iNext) >= 0.f)
		return cross(iPrevious, iCorner, iPoint) >= 0.f && cross(iCorner, iNext, iPoint) >= 0.f;
	return cross(iPrevious, iCorner, iPoint) >= 0.f || cross(iCorner, iNext, iPoint) >= 0.f;
}

// Whether a point lies on a segment, within the weld distance.
auto isOnSegment(const math::vec2& iA, const math::vec2& iB, const math::vec2& iPoint) -> bool {
	const math::vec2 segment = iB - iA;
	const float lengthSq = segment.normSq();
	if (lengthSq <= 0.f)
		return isSame(iA, iPoint);
	const float share = ((iPoint - iA) * segment) / lengthSq;
	return share >= 0.f && share <= 1.f && (iA + segment * share - iPoint).normSq() <= g_weld * g_weld;
}

auto closestOnSegment(const math::vec2& iA, const math::vec2& iB, const math::vec2& iPoint) -> math::vec2 {
	const math::vec2 segment = iB - iA;
	const float lengthSq = segment.normSq();
	if (lengthSq <= 0.f)
		return iA;
	const float share = std::clamp(((iPoint - iA) * segment) / lengthSq, 0.f, 1.f);
	return iA + segment * share;
}

}// namespace

auto WalkMesh::addVertex(const math::vec2& iPoint) -> uint32_t {
	for (size_t index = 0; index < m_vertices.size(); ++index) {
		if (isSame(m_vertices[index], iPoint))
			return static_cast<uint32_t>(index);
	}
	m_vertices.push_back(iPoint);
	return static_cast<uint32_t>(m_vertices.size() - 1);
}

void WalkMesh::clear() {
	m_vertices.clear();
	m_triangles.clear();
}

namespace {

using Ring = std::vector<uint32_t>;
using Vertices = std::vector<math::vec2>;

// The ring edge the ray from a point to +x meets first, and the x it meets it at.
auto findHitEdge(const Vertices& iVertices, const Ring& iRing, const math::vec2& iStart)
		-> std::optional<std::pair<size_t, float>> {
	float hitX = std::numeric_limits<float>::max();
	std::optional<size_t> hitEdge;
	for (size_t index = 0; index < iRing.size(); ++index) {
		const auto& a = iVertices[iRing[index]];
		const auto& b = iVertices[iRing[(index + 1) % iRing.size()]];
		if (std::abs(a.y() - b.y()) <= 0.f || iStart.y() < std::min(a.y(), b.y()) ||
			iStart.y() > std::max(a.y(), b.y()))
			continue;
		const float x = a.x() + (iStart.y() - a.y()) * (b.x() - a.x()) / (b.y() - a.y());
		if (x >= iStart.x() - g_weld && x < hitX) {
			hitX = x;
			hitEdge = index;
		}
	}
	if (!hitEdge)
		return std::nullopt;
	return std::pair{*hitEdge, hitX};
}

// The ring vertex a hole vertex is bridged to: the end of the hit edge, or a reflex vertex hiding it.
auto findBridge(const Vertices& iVertices, const Ring& iRing, const math::vec2& iStart, const size_t iHitEdge,
				const float iHitX) -> size_t {
	const size_t edgeEnd = (iHitEdge + 1) % iRing.size();
	size_t bridge = iVertices[iRing[iHitEdge]].x() > iVertices[iRing[edgeEnd]].x() ? iHitEdge : edgeEnd;
	const math::vec2 hit{iHitX, iStart.y()};
	const math::vec2 candidate = iVertices[iRing[bridge]];
	const uint32_t candidateVertex = iRing[bridge];
	float bestAngle = std::numeric_limits<float>::max();
	float bestDistance = std::numeric_limits<float>::max();
	for (size_t index = 0; !isSame(hit, candidate) && index < iRing.size(); ++index) {
		const auto& point = iVertices[iRing[index]];
		const auto& previous = iVertices[iRing[(index + iRing.size() - 1) % iRing.size()]];
		const auto& next = iVertices[iRing[(index + 1) % iRing.size()]];
		if (iRing[index] == candidateVertex || cross(previous, point, next) >= 0.f ||
			!isInAnyTriangle(iStart, hit, candidate, point))
			continue;
		const math::vec2 offset = point - iStart;
		const float angle = math::atan2(std::abs(offset.y()), offset.x());
		const float distance = offset.normSq();
		if (angle < bestAngle || (angle <= bestAngle && distance < bestDistance)) {
			bestAngle = angle;
			bestDistance = distance;
			bridge = index;
		}
	}
	// A vertex met twice (an earlier bridge): the copy whose corner opens towards the hole.
	for (size_t index = 0; index < iRing.size(); ++index) {
		if (iRing[index] != iRing[bridge])
			continue;
		const auto& previous = iVertices[iRing[(index + iRing.size() - 1) % iRing.size()]];
		const auto& next = iVertices[iRing[(index + 1) % iRing.size()]];
		if (isInSector(previous, iVertices[iRing[index]], next, iStart))
			return index;
	}
	return bridge;
}

// Splice a clockwise hole into a counter-clockwise ring through a bridge from its rightmost vertex.
auto bridgeHole(const Vertices& iVertices, Ring& ioRing, const Ring& iHole) -> bool {
	size_t holeStart = 0;
	for (size_t index = 1; index < iHole.size(); ++index) {
		if (iVertices[iHole[index]].x() > iVertices[iHole[holeStart]].x())
			holeStart = index;
	}
	const math::vec2 start = iVertices[iHole[holeStart]];
	const auto hit = findHitEdge(iVertices, ioRing, start);
	if (!hit) {
		OWL_CORE_WARN("WalkMesh: A hole lies outside its outline, skipped.")
		return false;
	}
	const size_t bridge = findBridge(iVertices, ioRing, start, hit->first, hit->second);
	Ring spliced;
	spliced.reserve(ioRing.size() + iHole.size() + 2);
	spliced.insert(spliced.end(), ioRing.begin(), ioRing.begin() + static_cast<std::ptrdiff_t>(bridge) + 1);
	for (size_t step = 0; step <= iHole.size(); ++step) spliced.push_back(iHole[(holeStart + step) % iHole.size()]);
	spliced.push_back(ioRing[bridge]);
	spliced.insert(spliced.end(), ioRing.begin() + static_cast<std::ptrdiff_t>(bridge) + 1, ioRing.end());
	ioRing = std::move(spliced);
	return true;
}

// Whether the corner of a ring is an ear: convex, no other vertex in its triangle.
auto isEar(const Vertices& iVertices, const Ring& iRing, const size_t iIndex) -> bool {
	const size_t previous = (iIndex + iRing.size() - 1) % iRing.size();
	const size_t next = (iIndex + 1) % iRing.size();
	const auto& a = iVertices[iRing[previous]];
	const auto& b = iVertices[iRing[iIndex]];
	const auto& c = iVertices[iRing[next]];
	if (cross(a, b, c) <= g_epsilon)
		return false;
	return std::ranges::none_of(iRing, [&](const uint32_t iOther) -> bool {
		return iOther != iRing[previous] && iOther != iRing[iIndex] && iOther != iRing[next] &&
			   isInTriangle(a, b, c, iVertices[iOther]);
	});
}

// Cut a counter-clockwise ring into triangles, ear by ear.
void clipEars(const Vertices& iVertices, Ring& ioRing, std::vector<WalkMesh::Triangle>& oTriangles) {
	while (ioRing.size() > 3) {
		const auto found =
				std::ranges::find_if(std::views::iota(size_t{0}, ioRing.size()),
									 [&](const size_t iIndex) -> bool { return isEar(iVertices, ioRing, iIndex); });
		size_t index = 0;
		if (found != std::views::iota(size_t{0}, ioRing.size()).end()) {
			index = *found;
			const size_t previous = (index + ioRing.size() - 1) % ioRing.size();
			const size_t next = (index + 1) % ioRing.size();
			oTriangles.push_back(
					{.vertices = {ioRing[previous], ioRing[index], ioRing[next]}, .neighbours = {-1, -1, -1}});
		} else {
			// No ear left (a flat or crossing outline): drop the flattest corner.
			float flatness = std::numeric_limits<float>::max();
			for (size_t corner = 0; corner < ioRing.size(); ++corner) {
				const float area =
						std::abs(cross(iVertices[ioRing[(corner + ioRing.size() - 1) % ioRing.size()]],
									   iVertices[ioRing[corner]], iVertices[ioRing[(corner + 1) % ioRing.size()]]));
				if (area < flatness) {
					flatness = area;
					index = corner;
				}
			}
		}
		ioRing.erase(ioRing.begin() + static_cast<std::ptrdiff_t>(index));
	}
	if (ioRing.size() == 3 && cross(iVertices[ioRing[0]], iVertices[ioRing[1]], iVertices[ioRing[2]]) > g_epsilon)
		oTriangles.push_back({.vertices = {ioRing[0], ioRing[1], ioRing[2]}, .neighbours = {-1, -1, -1}});
}

}// namespace

void WalkMesh::build(const std::span<const WalkPolygon> iPolygons) {
	clear();
	for (const auto& polygon: iPolygons) {
		auto outline = cleanRing(polygon.outline);
		if (outline.size() < 3 || std::abs(signedArea(outline)) <= g_epsilon)
			continue;
		if (signedArea(outline) < 0.f)
			std::ranges::reverse(outline);
		std::vector<uint32_t> ring;
		ring.reserve(outline.size());
		for (const auto& point: outline) ring.push_back(addVertex(point));
		// Holes, clockwise, the rightmost first.
		std::vector<std::vector<uint32_t>> holes;
		for (const auto& source: polygon.holes) {
			auto hole = cleanRing(source);
			if (hole.size() < 3 || std::abs(signedArea(hole)) <= g_epsilon)
				continue;
			if (signedArea(hole) > 0.f)
				std::ranges::reverse(hole);
			std::vector<uint32_t> indices;
			indices.reserve(hole.size());
			for (const auto& point: hole) indices.push_back(addVertex(point));
			holes.push_back(std::move(indices));
		}
		const auto maxX = [this](const std::vector<uint32_t>& iHole) -> float {
			float result = -std::numeric_limits<float>::max();
			for (const auto index: iHole) result = std::max(result, m_vertices[index].x());
			return result;
		};
		std::ranges::sort(holes, std::greater{}, maxX);
		for (const auto& hole: holes) std::ignore = bridgeHole(m_vertices, ring, hole);
		clipEars(m_vertices, ring, m_triangles);
	}
	linkNeighbours();
}

void WalkMesh::linkNeighbours() {
	std::map<uint64_t, std::pair<uint32_t, uint32_t>> open;
	for (size_t triangle = 0; triangle < m_triangles.size(); ++triangle) {
		for (uint32_t edge = 0; edge < 3; ++edge) {
			const uint32_t a = m_triangles[triangle].vertices[edge];
			const uint32_t b = m_triangles[triangle].vertices[(edge + 1) % 3];
			const uint64_t key = (static_cast<uint64_t>(std::min(a, b)) << 32u) | std::max(a, b);
			if (const auto found = open.find(key); found != open.end()) {
				const auto [other, otherEdge] = found->second;
				m_triangles[triangle].neighbours[edge] = static_cast<int32_t>(other);
				m_triangles[other].neighbours[otherEdge] = static_cast<int32_t>(triangle);
				open.erase(found);
			} else {
				open.emplace(key, std::pair{static_cast<uint32_t>(triangle), edge});
			}
		}
	}
}

auto WalkMesh::getArea() const -> float {
	float area = 0.f;
	for (const auto& triangle: m_triangles)
		area += std::abs(cross(m_vertices[triangle.vertices[0]], m_vertices[triangle.vertices[1]],
							   m_vertices[triangle.vertices[2]]));
	return area * 0.5f;
}

auto WalkMesh::findTriangle(const math::vec2& iPoint) const -> std::optional<uint32_t> {
	for (size_t index = 0; index < m_triangles.size(); ++index) {
		const auto& corners = m_triangles[index].vertices;
		if (isInTriangle(m_vertices[corners[0]], m_vertices[corners[1]], m_vertices[corners[2]], iPoint, 1e-5f))
			return static_cast<uint32_t>(index);
	}
	return std::nullopt;
}

auto WalkMesh::locate(math::vec2& ioPoint) const -> std::optional<uint32_t> {
	if (const auto found = findTriangle(ioPoint); found)
		return found;
	std::optional<uint32_t> best;
	float bestDistance = std::numeric_limits<float>::max();
	math::vec2 bestPoint = ioPoint;
	for (size_t index = 0; index < m_triangles.size(); ++index) {
		const auto& triangle = m_triangles[index];
		for (uint32_t edge = 0; edge < 3; ++edge) {
			if (triangle.neighbours[edge] >= 0)
				continue;
			const math::vec2 point = closestOnSegment(m_vertices[triangle.vertices[edge]],
													  m_vertices[triangle.vertices[(edge + 1) % 3]], ioPoint);
			if (const float distance = (point - ioPoint).normSq(); distance < bestDistance) {
				bestDistance = distance;
				bestPoint = point;
				best = static_cast<uint32_t>(index);
			}
		}
	}
	ioPoint = bestPoint;
	return best;
}

auto WalkMesh::getClosestPoint(const math::vec2& iPoint) const -> std::optional<math::vec2> {
	math::vec2 point = iPoint;
	if (!locate(point))
		return std::nullopt;
	return point;
}

auto WalkMesh::getBorderEdges() const -> std::vector<std::array<uint32_t, 2>> {
	std::vector<std::array<uint32_t, 2>> edges;
	for (const auto& triangle: m_triangles) {
		for (uint32_t edge = 0; edge < 3; ++edge) {
			if (triangle.neighbours[edge] < 0)
				edges.push_back({triangle.vertices[edge], triangle.vertices[(edge + 1) % 3]});
		}
	}
	return edges;
}

auto WalkMesh::findPath(const math::vec2& iFrom, const math::vec2& iTo, std::vector<math::vec2>& oPath) const -> bool {
	oPath.clear();
	math::vec2 from = iFrom;
	math::vec2 to = iTo;
	const auto start = locate(from);
	const auto goal = locate(to);
	if (!start || !goal) {
		OWL_CORE_WARN("WalkMesh: No floor to find a path on.")
		return false;
	}
	if (*start == *goal) {
		oPath = {from, to};
		return true;
	}
	// A* over the triangles, entered at the middle of an edge.
	const size_t count = m_triangles.size();
	std::vector<float> cost(count, std::numeric_limits<float>::max());
	std::vector<int32_t> parent(count, -1);
	std::vector<math::vec2> entry(count, from);
	std::vector<bool> closed(count, false);
	using Node = std::pair<float, uint32_t>;
	std::priority_queue<Node, std::vector<Node>, std::greater<>> open;
	cost[*start] = 0.f;
	open.emplace((to - from).norm(), *start);
	while (!open.empty()) {
		const auto current = open.top().second;
		open.pop();
		if (closed[current])
			continue;
		closed[current] = true;
		if (current == *goal)
			break;
		const auto& triangle = m_triangles[current];
		for (uint32_t edge = 0; edge < 3; ++edge) {
			const int32_t neighbour = triangle.neighbours[edge];
			if (neighbour < 0 || closed[static_cast<size_t>(neighbour)])
				continue;
			const math::vec2 middle =
					(m_vertices[triangle.vertices[edge]] + m_vertices[triangle.vertices[(edge + 1) % 3]]) * 0.5f;
			const float reached = cost[current] + (middle - entry[current]).norm();
			const auto next = static_cast<size_t>(neighbour);
			if (reached >= cost[next])
				continue;
			cost[next] = reached;
			entry[next] = middle;
			parent[next] = static_cast<int32_t>(current);
			open.emplace(reached + (to - middle).norm(), static_cast<uint32_t>(neighbour));
		}
	}
	if (!closed[*goal]) {
		OWL_CORE_TRACE("WalkMesh: The goal lies on a floor the start does not reach.")
		return false;
	}
	std::vector<uint32_t> corridor;
	for (auto triangle = static_cast<int32_t>(*goal); triangle >= 0; triangle = parent[static_cast<size_t>(triangle)])
		corridor.push_back(static_cast<uint32_t>(triangle));
	std::ranges::reverse(corridor);
	// Portals as (left, right) seen when walking along the corridor.
	std::vector<std::pair<math::vec2, math::vec2>> portals;
	portals.reserve(corridor.size() + 1);
	portals.emplace_back(from, from);
	for (size_t index = 0; index + 1 < corridor.size(); ++index) {
		const auto& triangle = m_triangles[corridor[index]];
		for (uint32_t edge = 0; edge < 3; ++edge) {
			if (triangle.neighbours[edge] != static_cast<int32_t>(corridor[index + 1]))
				continue;
			portals.emplace_back(m_vertices[triangle.vertices[(edge + 1) % 3]], m_vertices[triangle.vertices[edge]]);
			break;
		}
	}
	portals.emplace_back(to, to);
	// Funnel: tighten the left and right sides from the apex, a corner each time they cross.
	oPath.push_back(from);
	math::vec2 apex = from;
	math::vec2 left = from;
	math::vec2 right = from;
	size_t apexIndex = 0;
	size_t leftIndex = 0;
	size_t rightIndex = 0;
	for (size_t index = 1; index < portals.size(); ++index) {
		const auto& [portalLeft, portalRight] = portals[index];
		// The apex on the portal itself (a start on the edge of two triangles) gives no direction: skip it.
		if (!isSame(apex, portalLeft) && !isSame(apex, portalRight) && isOnSegment(portalLeft, portalRight, apex))
			continue;
		if (cross(apex, right, portalRight) >= 0.f) {
			if (isSame(apex, right) || cross(apex, left, portalRight) < 0.f) {
				right = portalRight;
				rightIndex = index;
			} else {
				if (!isSame(oPath.back(), left))
					oPath.push_back(left);
				apex = left;
				apexIndex = leftIndex;
				right = apex;
				rightIndex = apexIndex;
				index = apexIndex;
				continue;
			}
		}
		if (cross(apex, left, portalLeft) <= 0.f) {
			if (isSame(apex, left) || cross(apex, right, portalLeft) > 0.f) {
				left = portalLeft;
				leftIndex = index;
			} else {
				if (!isSame(oPath.back(), right))
					oPath.push_back(right);
				apex = right;
				apexIndex = rightIndex;
				left = apex;
				leftIndex = apexIndex;
				index = apexIndex;
				continue;
			}
		}
	}
	if (!isSame(oPath.back(), to))
		oPath.push_back(to);
	return true;
}

}// namespace owl::scene::nav
