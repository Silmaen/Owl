/**
 * @file NavMesh.cpp
 * @author Silmaen
 * @date 08/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "NavMeshBuilder.h"
#include "math/linAlgebra.h"
#include "scene/nav/NavMesh.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <numbers>
#include <optional>
#include <queue>
#include <span>
#include <tuple>
#include <utility>
#include <vector>

namespace owl::scene::nav {

namespace {

constexpr float g_epsilon = 1e-6f;
// Most triangles a move along the surface looks through.
constexpr size_t g_maxMoveTriangles = 64;

auto transformPoint(const math::mat4& iTransform, const math::vec3& iPoint) -> math::vec3 {
	const math::vec4 point = iTransform * math::vec4{iPoint.x(), iPoint.y(), iPoint.z(), 1.f};
	return {point.x(), point.y(), point.z()};
}

// Twice the signed area of (a, b, c) in the plane (x, z): positive when c is left of a -> b.
auto cross(const math::vec3& iA, const math::vec3& iB, const math::vec3& iC) -> float {
	return (iB.x() - iA.x()) * (iC.z() - iA.z()) - (iB.z() - iA.z()) * (iC.x() - iA.x());
}

auto distanceSq(const math::vec3& iA, const math::vec3& iB) -> float { return (iA - iB).normSq(); }

auto isSamePlanar(const math::vec3& iA, const math::vec3& iB) -> bool {
	const float dx = iA.x() - iB.x();
	const float dz = iA.z() - iB.z();
	return dx * dx + dz * dz <= g_epsilon * g_epsilon;
}

// Closest point of a triangle to a point, in 3D.
auto closestOnTriangle(const math::vec3& iPoint, const math::vec3& iA, const math::vec3& iB, const math::vec3& iC)
		-> math::vec3 {
	const math::vec3 ab = iB - iA;
	const math::vec3 ac = iC - iA;
	const math::vec3 ap = iPoint - iA;
	const float d1 = ab * ap;
	const float d2 = ac * ap;
	if (d1 <= 0.f && d2 <= 0.f)
		return iA;
	const math::vec3 bp = iPoint - iB;
	const float d3 = ab * bp;
	const float d4 = ac * bp;
	if (d3 >= 0.f && d4 <= d3)
		return iB;
	const float vc = d1 * d4 - d3 * d2;
	if (vc <= 0.f && d1 >= 0.f && d3 <= 0.f)
		return iA + ab * (d1 / (d1 - d3));
	const math::vec3 cp = iPoint - iC;
	const float d5 = ab * cp;
	const float d6 = ac * cp;
	if (d6 >= 0.f && d5 <= d6)
		return iC;
	const float vb = d5 * d2 - d1 * d6;
	if (vb <= 0.f && d2 >= 0.f && d6 <= 0.f)
		return iA + ac * (d2 / (d2 - d6));
	const float va = d3 * d6 - d5 * d4;
	if (va <= 0.f && d4 - d3 >= 0.f && d5 - d6 >= 0.f)
		return iB + (iC - iB) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)));
	const float denominator = 1.f / (va + vb + vc);
	return iA + ab * (vb * denominator) + ac * (vc * denominator);
}

auto distancePlanarSq(const math::vec3& iA, const math::vec3& iB) -> float {
	const float dx = iA.x() - iB.x();
	const float dz = iA.z() - iB.z();
	return dx * dx + dz * dz;
}

auto closestOnSegmentPlanar(const math::vec3& iA, const math::vec3& iB, const math::vec3& iPoint) -> math::vec3 {
	const float edgeX = iB.x() - iA.x();
	const float edgeZ = iB.z() - iA.z();
	const float lengthSq = edgeX * edgeX + edgeZ * edgeZ;
	float along = lengthSq > 0.f ? ((iPoint.x() - iA.x()) * edgeX + (iPoint.z() - iA.z()) * edgeZ) / lengthSq : 0.f;
	along = std::clamp(along, 0.f, 1.f);
	return {iA.x() + edgeX * along, 0.f, iA.z() + edgeZ * along};
}

auto isInsidePlanar(const math::vec3& iA, const math::vec3& iB, const math::vec3& iC, const math::vec3& iPoint)
		-> bool {
	return cross(iA, iB, iPoint) >= -g_epsilon && cross(iB, iC, iPoint) >= -g_epsilon &&
		   cross(iC, iA, iPoint) >= -g_epsilon;
}

}// namespace

void NavGeometry::addTriangles(const std::span<const math::vec3> iVertices, const std::span<const uint32_t> iIndices,
							   const math::mat4& iTransform) {
	const auto base = static_cast<uint32_t>(vertices.size());
	vertices.reserve(vertices.size() + iVertices.size());
	for (const auto& vertex: iVertices) vertices.push_back(transformPoint(iTransform, vertex));
	for (size_t index = 0; index + 2 < iIndices.size(); index += 3) {
		if (iIndices[index] >= iVertices.size() || iIndices[index + 1] >= iVertices.size() ||
			iIndices[index + 2] >= iVertices.size())
			continue;
		indices.push_back(base + iIndices[index]);
		indices.push_back(base + iIndices[index + 1]);
		indices.push_back(base + iIndices[index + 2]);
	}
}

void NavGeometry::addBox(const math::vec3& iSize, const math::mat4& iTransform) {
	const math::vec3 half = iSize * 0.5f;
	const std::array<math::vec3, 8> corners{
			math::vec3{-half.x(), -half.y(), -half.z()}, math::vec3{half.x(), -half.y(), -half.z()},
			math::vec3{half.x(), -half.y(), half.z()},   math::vec3{-half.x(), -half.y(), half.z()},
			math::vec3{-half.x(), half.y(), -half.z()},  math::vec3{half.x(), half.y(), -half.z()},
			math::vec3{half.x(), half.y(), half.z()},    math::vec3{-half.x(), half.y(), half.z()}};
	// Outward faces, counter-clockwise seen from outside.
	constexpr std::array<uint32_t, 36> faces{4, 7, 6, 4, 6, 5, 0, 1, 2, 0, 2, 3, 0, 4, 5, 0, 5, 1,
											 3, 2, 6, 3, 6, 7, 0, 3, 7, 0, 7, 4, 1, 5, 6, 1, 6, 2};
	addTriangles(corners, faces, iTransform);
}

void NavGeometry::addCylinder(const float iDiameter, const float iHeight, const math::mat4& iTransform) {
	constexpr uint32_t sides = 12;
	const float radius = iDiameter * 0.5f;
	const float half = iHeight * 0.5f;
	std::vector<math::vec3> corners;
	corners.reserve(sides * 2 + 2);
	for (uint32_t side = 0; side < sides; ++side) {
		const float angle = 2.f * std::numbers::pi_v<float> * static_cast<float>(side) / static_cast<float>(sides);
		corners.emplace_back(radius * std::cos(angle), -half, radius * std::sin(angle));
		corners.emplace_back(radius * std::cos(angle), half, radius * std::sin(angle));
	}
	const uint32_t bottomCentre = sides * 2;
	const uint32_t topCentre = bottomCentre + 1;
	corners.emplace_back(0.f, -half, 0.f);
	corners.emplace_back(0.f, half, 0.f);
	std::vector<uint32_t> faces;
	faces.reserve(size_t{sides} * 12u);
	for (uint32_t side = 0; side < sides; ++side) {
		const uint32_t b0 = side * 2;
		const uint32_t t0 = b0 + 1;
		const uint32_t b1 = ((side + 1) % sides) * 2;
		const uint32_t t1 = b1 + 1;
		faces.insert(faces.end(), {b0, t0, t1, b0, t1, b1, topCentre, t1, t0, bottomCentre, b0, b1});
	}
	addTriangles(corners, faces, iTransform);
}

auto NavMesh::build(const NavGeometry& iGeometry, const NavMeshSettings& iSettings, const math::vec3& iBoundsMin,
					const math::vec3& iBoundsMax) -> owl::expected<void, NavBuildError> {
	clear();
	auto output = buildNavTriangles(iGeometry, iSettings, iBoundsMin, iBoundsMax);
	if (!output.has_value())
		return owl::unexpected{output.error()};
	setTriangles(output->vertices, output->indices);
	return {};
}

void NavMesh::setTriangles(const std::span<const math::vec3> iVertices, const std::span<const uint32_t> iIndices) {
	clear();
	m_vertices.assign(iVertices.begin(), iVertices.end());
	m_triangles.reserve(iIndices.size() / 3);
	for (size_t index = 0; index + 2 < iIndices.size(); index += 3) {
		const std::array<uint32_t, 3> corners{iIndices[index], iIndices[index + 1], iIndices[index + 2]};
		if (std::ranges::any_of(corners, [&](const uint32_t iCorner) -> bool { return iCorner >= m_vertices.size(); }))
			continue;
		m_triangles.push_back({.vertices = corners, .neighbours = {-1, -1, -1}});
	}
	linkNeighbours();
	m_linksFrom.assign(m_triangles.size(), {});
}

void NavMesh::linkNeighbours() {
	std::map<std::pair<uint32_t, uint32_t>, std::pair<uint32_t, size_t>> open;
	for (size_t triangle = 0; triangle < m_triangles.size(); ++triangle) {
		for (size_t edge = 0; edge < 3; ++edge) {
			const uint32_t a = m_triangles[triangle].vertices[edge];
			const uint32_t b = m_triangles[triangle].vertices[(edge + 1) % 3];
			const auto key = std::minmax(a, b);
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

void NavMesh::setLinks(const std::span<const NavMeshLink> iLinks, const float iReach) {
	m_links.clear();
	m_linksFrom.assign(m_triangles.size(), {});
	for (const auto& link: iLinks) {
		const auto start = findNearest(link.start, iReach);
		const auto end = findNearest(link.end, iReach);
		if (!start.has_value() || !end.has_value()) {
			OWL_CORE_WARN("NavMesh: A link from ({:.2f}, {:.2f}, {:.2f}) is off the mesh, skipped.", link.start.x(),
						  link.start.y(), link.start.z())
			continue;
		}
		const auto index = static_cast<int32_t>(m_links.size());
		m_links.push_back({.link = {.start = start->position,
									.end = end->position,
									.bidirectional = link.bidirectional,
									.costScale = std::max(link.costScale, 1.f)},
						   .startTriangle = start->triangle,
						   .endTriangle = end->triangle});
		m_linksFrom[start->triangle].push_back(index);
		if (link.bidirectional)
			m_linksFrom[end->triangle].push_back(-1 - index);
	}
}

void NavMesh::clear() {
	m_vertices.clear();
	m_triangles.clear();
	m_links.clear();
	m_linksFrom.clear();
}

auto NavMesh::getIndices() const -> std::vector<uint32_t> {
	std::vector<uint32_t> indices;
	indices.reserve(m_triangles.size() * 3);
	for (const auto& triangle: m_triangles)
		indices.insert(indices.end(), triangle.vertices.begin(), triangle.vertices.end());
	return indices;
}

auto NavMesh::getArea() const -> float {
	float area = 0.f;
	for (const auto& triangle: m_triangles) {
		area += std::abs(cross(m_vertices[triangle.vertices[0]], m_vertices[triangle.vertices[1]],
							   m_vertices[triangle.vertices[2]])) *
				0.5f;
	}
	return area;
}

auto NavMesh::findNearest(const math::vec3& iPoint, const float iReach) const -> std::optional<NavPoint> {
	std::optional<NavPoint> best;
	float bestDistance = iReach * iReach;
	for (size_t index = 0; index < m_triangles.size(); ++index) {
		const auto& corners = m_triangles[index].vertices;
		const auto& a = m_vertices[corners[0]];
		const auto& b = m_vertices[corners[1]];
		const auto& c = m_vertices[corners[2]];
		math::vec3 candidate;
		if (isInsidePlanar(a, b, c, iPoint))
			candidate =
					math::vec3{iPoint.x(), getHeight(static_cast<uint32_t>(index), iPoint.x(), iPoint.z()), iPoint.z()};
		else
			candidate = closestOnTriangle(iPoint, a, b, c);
		const float distance = distanceSq(candidate, iPoint);
		if (distance > bestDistance || (best.has_value() && distance >= bestDistance))
			continue;
		bestDistance = distance;
		best = NavPoint{.position = candidate, .triangle = static_cast<uint32_t>(index)};
	}
	return best;
}

auto NavMesh::getHeight(const uint32_t iTriangle, const float iX, const float iZ) const -> float {
	const auto& corners = m_triangles[iTriangle].vertices;
	const auto& a = m_vertices[corners[0]];
	const auto& b = m_vertices[corners[1]];
	const auto& c = m_vertices[corners[2]];
	const math::vec3 point{iX, 0.f, iZ};
	const float area = cross(a, b, c);
	if (std::abs(area) <= g_epsilon)
		return (a.y() + b.y() + c.y()) / 3.f;
	const float wa = cross(b, c, point) / area;
	const float wb = cross(c, a, point) / area;
	return a.y() * wa + b.y() * wb + c.y() * (1.f - wa - wb);
}

auto NavMesh::findCorridor(const NavPoint& iFrom, const NavPoint& iTo,
						   std::vector<std::pair<uint32_t, int32_t>>& oCorridor) const -> bool {
	oCorridor.clear();
	if (iFrom.triangle == iTo.triangle) {
		oCorridor.emplace_back(iFrom.triangle, -1);
		return true;
	}
	struct Node {
		float cost = std::numeric_limits<float>::max();
		math::vec3 position;
		int32_t parent = -1;
		int32_t link = -1;
		bool closed = false;
	};
	std::vector<Node> nodes(m_triangles.size());
	using Entry = std::tuple<float, uint32_t>;
	std::priority_queue<Entry, std::vector<Entry>, std::greater<>> open;
	nodes[iFrom.triangle].cost = 0.f;
	nodes[iFrom.triangle].position = iFrom.position;
	open.emplace(std::sqrt(distanceSq(iFrom.position, iTo.position)), iFrom.triangle);
	const auto relax = [&](const uint32_t iCurrent, const uint32_t iNext, const math::vec3& iEntry, const float iStep,
						   const int32_t iLink) -> void {
		auto& next = nodes[iNext];
		if (next.closed)
			return;
		const float cost = nodes[iCurrent].cost + iStep;
		if (cost >= next.cost)
			return;
		next.cost = cost;
		next.position = iEntry;
		next.parent = static_cast<int32_t>(iCurrent);
		next.link = iLink;
		open.emplace(cost + std::sqrt(distanceSq(iEntry, iTo.position)), iNext);
	};
	while (!open.empty()) {
		const uint32_t current = std::get<1>(open.top());
		open.pop();
		if (nodes[current].closed)
			continue;
		nodes[current].closed = true;
		if (current == iTo.triangle)
			break;
		const auto& triangle = m_triangles[current];
		for (size_t edge = 0; edge < 3; ++edge) {
			const int32_t neighbour = triangle.neighbours[edge];
			if (neighbour < 0)
				continue;
			const auto next = static_cast<uint32_t>(neighbour);
			const math::vec3 entry =
					next == iTo.triangle
							? iTo.position
							: (m_vertices[triangle.vertices[edge]] + m_vertices[triangle.vertices[(edge + 1) % 3]]) *
									  0.5f;
			relax(current, next, entry, std::sqrt(distanceSq(nodes[current].position, entry)), -1);
		}
		for (const int32_t code: m_linksFrom[current]) {
			const bool reverse = code < 0;
			const auto index = static_cast<size_t>(reverse ? -1 - code : code);
			const auto& attached = m_links[index];
			const math::vec3& start = reverse ? attached.link.end : attached.link.start;
			const math::vec3& end = reverse ? attached.link.start : attached.link.end;
			const uint32_t target = reverse ? attached.startTriangle : attached.endTriangle;
			const float step = std::sqrt(distanceSq(nodes[current].position, start)) +
							   std::sqrt(distanceSq(start, end)) * attached.link.costScale;
			relax(current, target, end, step, code);
		}
	}
	if (!nodes[iTo.triangle].closed)
		return false;
	for (auto current = static_cast<int32_t>(iTo.triangle); current >= 0;
		 current = nodes[static_cast<size_t>(current)].parent)
		oCorridor.emplace_back(static_cast<uint32_t>(current), nodes[static_cast<size_t>(current)].link);
	std::ranges::reverse(oCorridor);
	return true;
}

void NavMesh::pullTight(const std::span<const uint32_t> iCorridor, const math::vec3& iFrom, const math::vec3& iTo,
						std::vector<NavPathPoint>& ioPath) const {
	struct Portal {
		math::vec3 left;
		math::vec3 right;
	};
	std::vector<Portal> portals;
	portals.reserve(iCorridor.size() + 1);
	portals.push_back({iFrom, iFrom});
	for (size_t index = 0; index + 1 < iCorridor.size(); ++index) {
		const auto& triangle = m_triangles[iCorridor[index]];
		for (size_t edge = 0; edge < 3; ++edge) {
			if (triangle.neighbours[edge] != static_cast<int32_t>(iCorridor[index + 1]))
				continue;
			portals.push_back({.left = m_vertices[triangle.vertices[(edge + 1) % 3]],
							   .right = m_vertices[triangle.vertices[edge]]});
			break;
		}
	}
	portals.push_back({iTo, iTo});

	math::vec3 apex = iFrom;
	math::vec3 left = portals[0].left;
	math::vec3 right = portals[0].right;
	size_t apexIndex = 0;
	size_t leftIndex = 0;
	size_t rightIndex = 0;
	const auto addCorner = [&](const math::vec3& iCorner) -> void {
		if (!isSamePlanar(ioPath.back().position, iCorner))
			ioPath.push_back({.position = iCorner, .link = -1});
	};
	for (size_t index = 1; index < portals.size(); ++index) {
		const auto& portal = portals[index];
		if (cross(apex, right, portal.right) >= 0.f) {
			if (isSamePlanar(apex, right) || cross(apex, left, portal.right) < 0.f) {
				right = portal.right;
				rightIndex = index;
			} else {
				addCorner(left);
				apex = left;
				apexIndex = leftIndex;
				right = apex;
				rightIndex = apexIndex;
				index = apexIndex;
				continue;
			}
		}
		if (cross(apex, left, portal.left) <= 0.f) {
			if (isSamePlanar(apex, left) || cross(apex, right, portal.left) > 0.f) {
				left = portal.left;
				leftIndex = index;
			} else {
				addCorner(right);
				apex = right;
				apexIndex = rightIndex;
				left = apex;
				leftIndex = apexIndex;
				index = apexIndex;
				continue;
			}
		}
	}
	addCorner(iTo);
	if (isSamePlanar(ioPath.back().position, iTo))
		ioPath.back().position = iTo;
}

auto NavMesh::findPath(const math::vec3& iFrom, const math::vec3& iTo, std::vector<NavPathPoint>& oPath,
					   const float iReach) const -> bool {
	OWL_PROFILE_FUNCTION()

	oPath.clear();
	const auto from = findNearest(iFrom, iReach);
	const auto to = findNearest(iTo, iReach);
	if (!from.has_value() || !to.has_value())
		return false;
	std::vector<std::pair<uint32_t, int32_t>> corridor;
	if (!findCorridor(*from, *to, corridor))
		return false;
	oPath.push_back({.position = from->position, .link = -1});
	std::vector<uint32_t> stretch;
	math::vec3 stretchStart = from->position;
	for (const auto& [triangle, link]: corridor) {
		if (link == -1) {
			stretch.push_back(triangle);
			continue;
		}
		const bool reverse = link < 0;
		const auto& attached = m_links[static_cast<size_t>(reverse ? -1 - link : link)].link;
		const math::vec3& start = reverse ? attached.end : attached.start;
		const math::vec3& end = reverse ? attached.start : attached.end;
		pullTight(stretch, stretchStart, start, oPath);
		oPath.back().link = link;
		oPath.push_back({.position = end, .link = -1});
		stretch.assign(1, triangle);
		stretchStart = end;
	}
	pullTight(stretch, stretchStart, to->position, oPath);
	return true;
}

auto NavMesh::moveAlongSurface(const NavPoint& iFrom, const math::vec3& iTarget) const -> NavPoint {
	if (m_triangles.empty() || iFrom.triangle >= m_triangles.size())
		return iFrom;
	const math::vec3 target{iTarget.x(), 0.f, iTarget.z()};
	const math::vec3 centre{(iFrom.position.x() + target.x()) * 0.5f, 0.f, (iFrom.position.z() + target.z()) * 0.5f};
	const float radius = std::sqrt(distancePlanarSq(iFrom.position, target)) * 0.5f + 1e-3f;
	// Triangles near the move, nearest first: the one holding the target, else the nearest point of their walls.
	std::vector<uint32_t> visited{iFrom.triangle};
	std::vector<bool> seen(m_triangles.size(), false);
	seen[iFrom.triangle] = true;
	NavPoint best = iFrom;
	float bestDistance = std::numeric_limits<float>::max();
	for (size_t index = 0; index < visited.size() && index < g_maxMoveTriangles; ++index) {
		const uint32_t current = visited[index];
		const auto& triangle = m_triangles[current];
		const auto& a = m_vertices[triangle.vertices[0]];
		const auto& b = m_vertices[triangle.vertices[1]];
		const auto& c = m_vertices[triangle.vertices[2]];
		if (isInsidePlanar(a, b, c, target))
			return {.position = {target.x(), getHeight(current, target.x(), target.z()), target.z()},
					.triangle = current};
		for (size_t edge = 0; edge < 3; ++edge) {
			const auto& start = m_vertices[triangle.vertices[edge]];
			const auto& end = m_vertices[triangle.vertices[(edge + 1) % 3]];
			const int32_t neighbour = triangle.neighbours[edge];
			if (neighbour < 0) {
				const math::vec3 closest = closestOnSegmentPlanar(start, end, target);
				if (const float distance = distancePlanarSq(closest, target); distance < bestDistance) {
					bestDistance = distance;
					best = {.position = closest, .triangle = current};
				}
				continue;
			}
			const auto next = static_cast<size_t>(neighbour);
			if (seen[next] || distancePlanarSq(closestOnSegmentPlanar(start, end, centre), centre) > radius * radius)
				continue;
			seen[next] = true;
			visited.push_back(static_cast<uint32_t>(next));
		}
	}
	best.position.y() = getHeight(best.triangle, best.position.x(), best.position.z());
	return best;
}

auto NavMesh::getBorderEdges() const -> std::vector<std::array<uint32_t, 2>> {
	std::vector<std::array<uint32_t, 2>> edges;
	for (const auto& triangle: m_triangles) {
		for (size_t edge = 0; edge < 3; ++edge) {
			if (triangle.neighbours[edge] < 0)
				edges.push_back({triangle.vertices[edge], triangle.vertices[(edge + 1) % 3]});
		}
	}
	return edges;
}

}// namespace owl::scene::nav
