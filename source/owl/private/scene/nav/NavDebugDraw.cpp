/**
 * @file NavDebugDraw.cpp
 * @author Silmaen
 * @date 16/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "scene/nav/NavDebugDraw.h"

#include "renderer/Renderer2D.h"
#include "scene/Entity.h"
#include "scene/Scene.h"
#include "scene/TilemapAsset.h"
#include "scene/TilemapTerrain.h"
#include "scene/component/NavMeshVolume.h"
#include "scene/component/SteeringAgent.h"
#include "scene/component/Tilemap.h"
#include "scene/component/WalkArea.h"
#include "scene/nav/NavBake.h"
#include "scene/nav/NavWorld.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <optional>
#include <utility>

namespace owl::scene::nav {

namespace {

// Edges drawn at most, to keep a huge map from flooding the line batch.
constexpr size_t g_maxEdges = 40000;

// The tilemap entities with a navigation profile, whose graph is up to date, handed to `iDraw`.
template<typename Draw>
void forEachNavigationMap(Scene& ioScene, Draw&& iDraw) {
	for (const auto view = ioScene.registry.view<component::Tilemap>(); const auto entity: view) {
		const auto& tilemap = view.get<component::Tilemap>(entity);
		if (!tilemap.asset || tilemap.navigation.isEmpty())
			continue;
		const Entity sceneEntity{entity, &ioScene};
		auto& entry = ioScene.getNavigation().acquire(static_cast<uint64_t>(sceneEntity.getUUID()), *tilemap.asset,
													  tilemap.navigation);
		iDraw(*tilemap.asset, ioScene.getWorldTransform(sceneEntity).translation(), entry);
	}
}

constexpr math::vec4 g_twoWay{0.30f, 0.90f, 0.40f, 0.9f};
constexpr math::vec4 g_oneWay{1.f, 0.60f, 0.10f, 1.f};
constexpr math::vec4 g_edited{0.95f, 0.35f, 0.95f, 1.f};
constexpr math::vec4 g_closed{0.95f, 0.20f, 0.20f, 1.f};
constexpr math::vec4 g_cost{1.f, 0.90f, 0.20f, 1.f};
constexpr math::vec4 g_picked{1.f, 1.f, 1.f, 1.f};

// Arrow head at `iTip`, the shaft coming from `iTail`.
void drawArrowHead(const math::vec3& iTail, const math::vec3& iTip, const math::vec4& iColor) {
	const auto along = (iTail - iTip) * 0.3f;
	const math::vec3 side{-along.y(), along.x(), 0.f};
	renderer::Renderer2D::drawLine({.point1 = iTip, .point2 = iTip + along + side * 0.5f, .color = iColor});
	renderer::Renderer2D::drawLine({.point1 = iTip, .point2 = iTip + along - side * 0.5f, .color = iColor});
}

// Red cross centred on `iCenter`.
void drawCross(const math::vec3& iCenter, const float iHalf, const math::vec4& iColor) {
	renderer::Renderer2D::drawLine({.point1 = iCenter + math::vec3{-iHalf, -iHalf, 0.f},
									.point2 = iCenter + math::vec3{iHalf, iHalf, 0.f},
									.color = iColor});
	renderer::Renderer2D::drawLine({.point1 = iCenter + math::vec3{-iHalf, iHalf, 0.f},
									.point2 = iCenter + math::vec3{iHalf, -iHalf, 0.f},
									.color = iColor});
}

// Diamond around `iCenter`.
void drawRing(const math::vec3& iCenter, const float iRadius, const math::vec4& iColor) {
	const std::array<math::vec3, 4> ring{
			iCenter + math::vec3{-iRadius, 0.f, 0.f}, iCenter + math::vec3{0.f, -iRadius, 0.f},
			iCenter + math::vec3{iRadius, 0.f, 0.f}, iCenter + math::vec3{0.f, iRadius, 0.f}};
	for (size_t i = 0; i < ring.size(); ++i)
		renderer::Renderer2D::drawLine({.point1 = ring[i], .point2 = ring[(i + 1) % ring.size()], .color = iColor});
}

// Whether the profile cuts the side of a cell (any exception on it, seen from either end of the edge).
auto isEdgeEdited(const NavProfile& iProfile, const uint32_t iX, const uint32_t iY, const Direction iDirection)
		-> bool {
	if (iProfile.overrides.empty())
		return false;
	if (iProfile.getSideState(iX, iY, iDirection) != SideState::TwoWay)
		return true;
	const auto nx = static_cast<int64_t>(iX) + offsetX(iDirection);
	const auto ny = static_cast<int64_t>(iY) + offsetY(iDirection);
	return nx >= 0 && ny >= 0 &&
		   iProfile.getSideState(static_cast<uint32_t>(nx), static_cast<uint32_t>(ny), opposite(iDirection)) !=
				   SideState::TwoWay;
}

// Whether a move crosses the side `iBit` of `iBase` (towards `iNeighbour`), in either direction.
auto isLinked(const NavTileRule& iBase, const NavTileRule& iNeighbour, const uint8_t iBit, const uint8_t iBack)
		-> bool {
	const bool forward = (iBase.exits & iBit) != 0 && (iNeighbour.enters & iBit) != 0;
	const bool backward = (iNeighbour.exits & iBack) != 0 && (iBase.enters & iBack) != 0;
	return forward || backward;
}

// Colour of a side of a cell under the Navigation tool.
auto sideColor(const SideState iState) -> math::vec4 {
	switch (iState) {
		case SideState::Closed:
			return g_closed;
		case SideState::TwoWay:
			return g_twoWay;
		case SideState::ExitOnly:
		case SideState::EnterOnly:
			return g_edited;
	}
	return g_edited;
}

}// namespace

auto getCellCenter(const TilemapAsset& iAsset, const math::vec3& iOrigin, const uint32_t iX, const uint32_t iY)
		-> math::vec3 {
	const auto corners = iAsset.getCellCorners(iX, iY);
	const float height =
			static_cast<float>(corners[0] + corners[1] + corners[2] + corners[3]) * 0.25f * terrain::heightStep;
	return iOrigin + math::vec3{static_cast<float>(iX) + 0.5f, static_cast<float>(iY) + 0.5f, height + 0.06f};
}

void drawNavigationGraph(Scene& ioScene) {
	size_t drawn = 0;
	forEachNavigationMap(
			ioScene, [&](const TilemapAsset& iAsset, const math::vec3& iOrigin, NavWorld::Entry& iEntry) -> void {
				const auto& map = iEntry.map;
				const auto& profile = map.getProfile();
				const auto& size = map.getSize();
				for (uint32_t y = 0; y < size.y() && drawn < g_maxEdges; ++y) {
					for (uint32_t x = 0; x < size.x() && drawn < g_maxEdges; ++x) {
						if (!map.isWalkable(x, y))
							continue;
						for (const auto direction: {Direction::East, Direction::South}) {
							const uint32_t nx = x + static_cast<uint32_t>(offsetX(direction));
							const uint32_t ny = y + static_cast<uint32_t>(offsetY(direction));
							const bool forward = map.getEdgeCost(x, y, direction) != 0;
							const bool backward = map.getEdgeCost(nx, ny, opposite(direction)) != 0;
							const bool edited = isEdgeEdited(profile, x, y, direction);
							if (!forward && !backward) {
								// A side an exception closed: the tiles join, the cross says nobody goes through.
								if (edited && map.isWalkable(nx, ny)) {
									const auto from = getCellCenter(iAsset, iOrigin, x, y);
									const auto to = getCellCenter(iAsset, iOrigin, nx, ny);
									drawCross((from + to) * 0.5f, 0.12f, g_closed);
								}
								continue;
							}
							const auto from = getCellCenter(iAsset, iOrigin, x, y);
							const auto to = getCellCenter(iAsset, iOrigin, nx, ny);
							const auto& oneWayColor = edited ? g_edited : g_oneWay;
							renderer::Renderer2D::drawLine({.point1 = from,
															.point2 = to,
															.color = forward && backward ? g_twoWay : oneWayColor});
							if (forward != backward)
								drawArrowHead(forward ? from : to, forward ? to : from, oneWayColor);
							++drawn;
						}
					}
				}
				for (const auto& entry: profile.overrides) {
					if (entry.cost > 0.f && map.isWalkable(entry.x, entry.y))
						drawRing(getCellCenter(iAsset, iOrigin, entry.x, entry.y), 0.2f, g_cost);
				}
			});
}

void drawNavigationCellEditor(Scene& ioScene, const uint64_t iEntityKey, const uint32_t iX, const uint32_t iY,
							  const std::optional<Direction> iPicked) {
	forEachNavigationMap(
			ioScene, [&](const TilemapAsset& iAsset, const math::vec3& iOrigin, NavWorld::Entry& iEntry) -> void {
				if (&iEntry != ioScene.getNavigation().find(iEntityKey))
					return;
				const auto& map = iEntry.map;
				const auto& profile = map.getProfile();
				const auto base = map.getBaseCell(iAsset, iX, iY);
				if (!base.has_value())
					return;
				drawRing(getCellCenter(iAsset, iOrigin, iX, iY), 0.12f, g_picked);
				const auto center = getCellCenter(iAsset, iOrigin, iX, iY);
				for (const auto side: directions) {
					const auto nx = static_cast<int64_t>(iX) + offsetX(side);
					const auto ny = static_cast<int64_t>(iY) + offsetY(side);
					if (nx < 0 || ny < 0)
						continue;
					const auto neighbour =
							map.getBaseCell(iAsset, static_cast<uint32_t>(nx), static_cast<uint32_t>(ny));
					const uint8_t bit = directionBit(side);
					const uint8_t back = directionBit(opposite(side));
					if (!neighbour.has_value() || !isLinked(*base, *neighbour, bit, back))
						continue;
					const math::vec3 along{static_cast<float>(offsetX(side)), static_cast<float>(offsetY(side)), 0.f};
					const auto inner = center + along * 0.15f;
					const auto outer = center + along * 0.85f;
					const auto state = profile.getSideState(iX, iY, side);
					const bool picked = iPicked.has_value() && *iPicked == side;
					const auto color = picked ? g_picked : sideColor(state);
					renderer::Renderer2D::drawLine({.point1 = inner, .point2 = outer, .color = color});
					if (picked)
						renderer::Renderer2D::drawLine({.point1 = inner + math::vec3{0.02f, 0.02f, 0.f},
														.point2 = outer + math::vec3{0.02f, 0.02f, 0.f},
														.color = color});
					if (state == SideState::Closed) {
						drawCross((inner + outer) * 0.5f, 0.1f, picked ? g_picked : g_closed);
						continue;
					}
					if (state != SideState::EnterOnly)
						drawArrowHead(inner, outer, color);
					if (state != SideState::ExitOnly)
						drawArrowHead(outer, inner, color);
				}
			});
}

void drawNavigationRoutes(Scene& ioScene) {
	forEachNavigationMap(
			ioScene, [&](const TilemapAsset& iAsset, const math::vec3& iOrigin, NavWorld::Entry& iEntry) -> void {
				for (const auto& [id, route]: iEntry.debugRoutes) {
					const math::vec4 color{route.color.x(), route.color.y(), route.color.z(), 1.f};
					// Routes of different vehicles that share a road run side by side instead of on top of each other.
					const float shift = static_cast<float>(id % 5) * 0.05f - 0.1f;
					const math::vec3 offset{shift, -shift, 0.02f};
					for (size_t i = 1; i < route.cells.size(); ++i) {
						const auto from =
								getCellCenter(iAsset, iOrigin, route.cells[i - 1].x(), route.cells[i - 1].y());
						const auto to = getCellCenter(iAsset, iOrigin, route.cells[i].x(), route.cells[i].y());
						renderer::Renderer2D::drawLine(
								{.point1 = from + offset, .point2 = to + offset, .color = color});
					}
					// The stop: a diamond around the last cell.
					const auto& last = route.cells.back();
					const auto c = getCellCenter(iAsset, iOrigin, last.x(), last.y()) + offset;
					const std::array<math::vec3, 4> ring{
							c + math::vec3{-0.3f, 0.f, 0.f}, c + math::vec3{0.f, -0.3f, 0.f},
							c + math::vec3{0.3f, 0.f, 0.f}, c + math::vec3{0.f, 0.3f, 0.f}};
					for (size_t i = 0; i < ring.size(); ++i)
						renderer::Renderer2D::drawLine(
								{.point1 = ring[i], .point2 = ring[(i + 1) % ring.size()], .color = color});
				}
			});
}

void drawWalkAreas(Scene& ioScene) {
	constexpr math::vec4 borderColor{0.3f, 0.95f, 0.45f, 1.f};
	constexpr math::vec4 triangleColor{0.3f, 0.95f, 0.45f, 0.25f};
	constexpr math::vec4 pathColor{1.f, 0.85f, 0.2f, 1.f};
	for (const auto view = ioScene.registry.view<component::WalkArea>(); const auto handle: view) {
		const Entity entity{handle, &ioScene};
		const math::mat4 world = ioScene.getWorldTransform(entity)();
		const auto& mesh = view.get<component::WalkArea>(handle).getMesh();
		const auto point = [&world, &mesh](const uint32_t iIndex) -> math::vec3 {
			const auto& vertex = mesh.getVertices()[iIndex];
			const math::vec4 transformed = world * math::vec4{vertex.x(), vertex.y(), 0.01f, 1.f};
			return {transformed.x(), transformed.y(), transformed.z()};
		};
		for (const auto& triangle: mesh.getTriangles()) {
			for (size_t edge = 0; edge < 3; ++edge) {
				if (triangle.neighbours[edge] < 0)
					continue;
				renderer::Renderer2D::drawLine({.point1 = point(triangle.vertices[edge]),
												.point2 = point(triangle.vertices[(edge + 1) % 3]),
												.color = triangleColor});
			}
		}
		for (const auto& [from, to]: mesh.getBorderEdges())
			renderer::Renderer2D::drawLine({.point1 = point(from), .point2 = point(to), .color = borderColor});
	}
	for (const auto view = ioScene.registry.view<component::WalkAgent>(); const auto handle: view) {
		const auto& agent = view.get<component::WalkAgent>(handle);
		if (!agent.isMoving())
			continue;
		const math::mat4 world = ioScene.getWorldTransform(Entity{handle, &ioScene})();
		math::vec3 previous{world(0, 3), world(1, 3), world(2, 3) + 0.01f};
		for (size_t index = agent.m_next; index < agent.m_path.size(); ++index) {
			const math::vec3 next{agent.m_path[index].x(), agent.m_path[index].y(), previous.z()};
			renderer::Renderer2D::drawLine({.point1 = previous, .point2 = next, .color = pathColor});
			previous = next;
		}
	}
}

namespace {

void drawLine(const math::vec3& iFrom, const math::vec3& iTo, const math::vec4& iColor) {
	renderer::Renderer2D::drawLine({.point1 = iFrom, .point2 = iTo, .color = iColor});
}

void drawBox(const math::vec3& iMin, const math::vec3& iMax, const math::vec4& iColor) {
	const std::array<math::vec3, 8> corners{
			math::vec3{iMin.x(), iMin.y(), iMin.z()}, math::vec3{iMax.x(), iMin.y(), iMin.z()},
			math::vec3{iMax.x(), iMin.y(), iMax.z()}, math::vec3{iMin.x(), iMin.y(), iMax.z()},
			math::vec3{iMin.x(), iMax.y(), iMin.z()}, math::vec3{iMax.x(), iMax.y(), iMin.z()},
			math::vec3{iMax.x(), iMax.y(), iMax.z()}, math::vec3{iMin.x(), iMax.y(), iMax.z()}};
	for (size_t index = 0; index < 4; ++index) {
		drawLine(corners[index], corners[(index + 1) % 4], iColor);
		drawLine(corners[index + 4], corners[(index + 1) % 4 + 4], iColor);
		drawLine(corners[index], corners[index + 4], iColor);
	}
}

void drawCircle(const math::vec3& iCentre, const float iRadius, const math::vec4& iColor) {
	constexpr size_t segments = 16;
	for (size_t index = 0; index < segments; ++index) {
		const float a0 = 2.f * std::numbers::pi_v<float> * static_cast<float>(index) / static_cast<float>(segments);
		const float a1 = 2.f * std::numbers::pi_v<float> * static_cast<float>(index + 1) / static_cast<float>(segments);
		drawLine(iCentre + math::vec3{iRadius * std::cos(a0), 0.f, iRadius * std::sin(a0)},
				 iCentre + math::vec3{iRadius * std::cos(a1), 0.f, iRadius * std::sin(a1)}, iColor);
	}
}

// An arc from start to end, raised by a quarter of its length, with an arrow head at the end when one way.
void drawLink(const NavMeshLink& iLink, const math::vec4& iColor) {
	constexpr size_t segments = 8;
	const float rise = (iLink.end - iLink.start).norm() * 0.25f;
	math::vec3 previous = iLink.start;
	for (size_t index = 1; index <= segments; ++index) {
		const float t = static_cast<float>(index) / static_cast<float>(segments);
		math::vec3 point = iLink.start + (iLink.end - iLink.start) * t;
		point.y() += rise * 4.f * t * (1.f - t);
		drawLine(previous, point, iColor);
		previous = point;
	}
	drawCircle(iLink.start, 0.15f, iColor);
	drawCircle(iLink.end, 0.15f, iColor);
	if (iLink.bidirectional)
		return;
	const math::vec3 back = (iLink.start - iLink.end).normalized() * 0.4f;
	const math::vec3 side{-back.z() * 0.5f, 0.f, back.x() * 0.5f};
	drawLine(iLink.end, iLink.end + back + side, iColor);
	drawLine(iLink.end, iLink.end + back - side, iColor);
}


// A circle on the plane of a steering agent: (x, y) in 2D, (x, z) on the ground and in space.
void drawPlaneCircle(const math::vec3& iCentre, const float iRadius, const ai::SteeringPlane iPlane,
					 const math::vec4& iColor) {
	if (iPlane != ai::SteeringPlane::Plane2D) {
		drawCircle(iCentre, iRadius, iColor);
		return;
	}
	constexpr size_t segments = 16;
	for (size_t index = 0; index < segments; ++index) {
		const float a0 = 2.f * std::numbers::pi_v<float> * static_cast<float>(index) / static_cast<float>(segments);
		const float a1 = 2.f * std::numbers::pi_v<float> * static_cast<float>(index + 1) / static_cast<float>(segments);
		drawLine(iCentre + math::vec3{iRadius * std::cos(a0), iRadius * std::sin(a0), 0.f},
				 iCentre + math::vec3{iRadius * std::cos(a1), iRadius * std::sin(a1), 0.f}, iColor);
	}
}

// Every steering agent: its radius, its velocity (one second ahead) and the line to its slot in a formation.
void drawSteeringAgents(Scene& ioScene) {
	constexpr math::vec4 agentColor{0.55f, 1.f, 0.45f, 1.f};
	constexpr math::vec4 slotColor{0.55f, 1.f, 0.45f, 0.4f};
	for (const auto view = ioScene.registry.view<component::SteeringAgent>(); const auto handle: view) {
		const auto& agent = view.get<component::SteeringAgent>(handle);
		const math::mat4 world = ioScene.getWorldTransform(Entity{handle, &ioScene})();
		const math::vec3 centre{world(0, 3), world(1, 3), world(2, 3)};
		drawPlaneCircle(centre, agent.radius, agent.plane, agentColor);
		if (agent.plane == ai::SteeringPlane::Space)
			drawPlaneCircle(centre, agent.radius, ai::SteeringPlane::Plane2D, agentColor);
		if (agent.m_velocity.normSq() > 1e-4f)
			drawLine(centre, centre + agent.m_velocity, agentColor);
		if (agent.leader == core::UUID{0} || agent.weights.formation <= 0.f)
			continue;
		if (const Entity leader = ioScene.findEntityByUUID(agent.leader); leader) {
			const math::mat4 lead = ioScene.getWorldTransform(leader)();
			const math::vec4 slot = lead * math::vec4{agent.slot.x(), agent.slot.y(), agent.slot.z(), 1.f};
			drawLine(centre, {slot.x(), slot.y(), slot.z()}, slotColor);
		}
	}
}

}// namespace

void drawNavMeshes(Scene& ioScene) {
	constexpr math::vec4 boxColor{0.35f, 0.55f, 1.f, 0.35f};
	constexpr math::vec4 borderColor{0.2f, 0.9f, 1.f, 1.f};
	constexpr math::vec4 triangleColor{0.2f, 0.9f, 1.f, 0.25f};
	constexpr math::vec4 linkColor{1.f, 0.3f, 0.9f, 1.f};
	constexpr math::vec4 obstacleColor{1.f, 0.55f, 0.15f, 1.f};
	constexpr math::vec4 pathColor{1.f, 0.85f, 0.2f, 1.f};
	const math::vec3 lift{0.f, 0.03f, 0.f};
	const auto links = collectNavLinks(ioScene);
	for (const auto view = ioScene.registry.view<component::NavMeshVolume>(); const auto handle: view) {
		const Entity entity{handle, &ioScene};
		const auto& volume = view.get<component::NavMeshVolume>(handle);
		math::vec3 boundsMin;
		math::vec3 boundsMax;
		getNavVolumeBounds(ioScene, entity, boundsMin, boundsMax);
		drawBox(boundsMin, boundsMax, boxColor);
		volume.setLinks(links);
		const auto& mesh = volume.getMesh();
		const auto& vertices = mesh.getVertices();
		const auto& triangles = mesh.getTriangles();
		for (size_t index = 0; index < triangles.size(); ++index) {
			const auto& triangle = triangles[index];
			for (size_t edge = 0; edge < 3; ++edge) {
				// Each inner edge once, from the triangle of the lower index.
				if (triangle.neighbours[edge] < 0 || std::cmp_less(triangle.neighbours[edge], index))
					continue;
				drawLine(vertices[triangle.vertices[edge]] + lift, vertices[triangle.vertices[(edge + 1) % 3]] + lift,
						 triangleColor);
			}
		}
		for (const auto& [from, to]: mesh.getBorderEdges())
			drawLine(vertices[from] + lift, vertices[to] + lift, borderColor);
	}
	for (const auto& link: links) drawLink(link, linkColor);
	for (const auto view = ioScene.registry.view<component::NavObstacle>(); const auto handle: view) {
		const auto& obstacle = view.get<component::NavObstacle>(handle);
		const math::mat4 world = ioScene.getWorldTransform(Entity{handle, &ioScene})();
		const math::vec3 base{world(0, 3), world(1, 3), world(2, 3)};
		const math::vec3 top = base + math::vec3{0.f, obstacle.height, 0.f};
		drawCircle(base, obstacle.radius, obstacleColor);
		drawCircle(top, obstacle.radius, obstacleColor);
		for (const float side: {-1.f, 1.f}) {
			drawLine(base + math::vec3{side * obstacle.radius, 0.f, 0.f},
					 top + math::vec3{side * obstacle.radius, 0.f, 0.f}, obstacleColor);
			drawLine(base + math::vec3{0.f, 0.f, side * obstacle.radius},
					 top + math::vec3{0.f, 0.f, side * obstacle.radius}, obstacleColor);
		}
	}
	for (const auto view = ioScene.registry.view<component::NavAgent>(); const auto handle: view) {
		const auto& agent = view.get<component::NavAgent>(handle);
		const math::mat4 world = ioScene.getWorldTransform(Entity{handle, &ioScene})();
		math::vec3 previous{world(0, 3), world(1, 3), world(2, 3)};
		drawCircle(previous + lift, agent.radius, pathColor);
		for (size_t index = agent.m_next; index < agent.m_path.size(); ++index) {
			drawLine(previous + lift, agent.m_path[index].position + lift, pathColor);
			previous = agent.m_path[index].position;
		}
	}
	drawSteeringAgents(ioScene);
}

}// namespace owl::scene::nav
