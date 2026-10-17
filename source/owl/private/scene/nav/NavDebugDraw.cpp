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
#include "scene/component/Tilemap.h"
#include "scene/nav/NavWorld.h"

#include <array>
#include <cstddef>
#include <optional>

namespace owl::scene::nav {

namespace {

/// Edges drawn at most, to keep a huge map from flooding the line batch.
constexpr size_t g_maxEdges = 40000;

/// The tilemap entities with a navigation profile, whose graph is up to date, handed to `iDraw`.
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

/// Arrow head at `iTip`, the shaft coming from `iTail`.
void drawArrowHead(const math::vec3& iTail, const math::vec3& iTip, const math::vec4& iColor) {
	const auto along = (iTail - iTip) * 0.3f;
	const math::vec3 side{-along.y(), along.x(), 0.f};
	renderer::Renderer2D::drawLine({.point1 = iTip, .point2 = iTip + along + side * 0.5f, .color = iColor});
	renderer::Renderer2D::drawLine({.point1 = iTip, .point2 = iTip + along - side * 0.5f, .color = iColor});
}

/// Red cross centred on `iCenter`.
void drawCross(const math::vec3& iCenter, const float iHalf, const math::vec4& iColor) {
	renderer::Renderer2D::drawLine({.point1 = iCenter + math::vec3{-iHalf, -iHalf, 0.f},
									.point2 = iCenter + math::vec3{iHalf, iHalf, 0.f},
									.color = iColor});
	renderer::Renderer2D::drawLine({.point1 = iCenter + math::vec3{-iHalf, iHalf, 0.f},
									.point2 = iCenter + math::vec3{iHalf, -iHalf, 0.f},
									.color = iColor});
}

/// Diamond around `iCenter`.
void drawRing(const math::vec3& iCenter, const float iRadius, const math::vec4& iColor) {
	const std::array<math::vec3, 4> ring{
			iCenter + math::vec3{-iRadius, 0.f, 0.f}, iCenter + math::vec3{0.f, -iRadius, 0.f},
			iCenter + math::vec3{iRadius, 0.f, 0.f}, iCenter + math::vec3{0.f, iRadius, 0.f}};
	for (size_t i = 0; i < ring.size(); ++i)
		renderer::Renderer2D::drawLine({.point1 = ring[i], .point2 = ring[(i + 1) % ring.size()], .color = iColor});
}

/// Whether the profile cuts the side of a cell (any exception on it, seen from either end of the edge).
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
	forEachNavigationMap(ioScene, [&](const TilemapAsset& iAsset, const math::vec3& iOrigin, NavWorld::Entry& iEntry) {
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
					renderer::Renderer2D::drawLine(
							{.point1 = from, .point2 = to, .color = forward && backward ? g_twoWay : oneWayColor});
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
	forEachNavigationMap(ioScene, [&](const TilemapAsset& iAsset, const math::vec3& iOrigin, NavWorld::Entry& iEntry) {
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
			const auto neighbour = map.getBaseCell(iAsset, static_cast<uint32_t>(nx), static_cast<uint32_t>(ny));
			const uint8_t bit = directionBit(side);
			const uint8_t back = directionBit(opposite(side));
			if (!neighbour.has_value() || !(((base->exits & bit) != 0 && (neighbour->enters & bit) != 0) ||
											((neighbour->exits & back) != 0 && (base->enters & back) != 0)))
				continue;
			const math::vec3 along{static_cast<float>(offsetX(side)), static_cast<float>(offsetY(side)), 0.f};
			const auto inner = center + along * 0.15f;
			const auto outer = center + along * 0.85f;
			const auto state = profile.getSideState(iX, iY, side);
			const bool picked = iPicked.has_value() && *iPicked == side;
			const auto color =
					picked ? g_picked
						   : (state == SideState::Closed ? g_closed
														 : (state == SideState::TwoWay ? g_twoWay : g_edited));
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
	forEachNavigationMap(ioScene, [&](const TilemapAsset& iAsset, const math::vec3& iOrigin, NavWorld::Entry& iEntry) {
		for (const auto& [id, route]: iEntry.debugRoutes) {
			const math::vec4 color{route.color.x(), route.color.y(), route.color.z(), 1.f};
			// Routes of different vehicles that share a road run side by side instead of on top of each other.
			const float shift = static_cast<float>(id % 5) * 0.05f - 0.1f;
			const math::vec3 offset{shift, -shift, 0.02f};
			for (size_t i = 1; i < route.cells.size(); ++i) {
				const auto from = getCellCenter(iAsset, iOrigin, route.cells[i - 1].x(), route.cells[i - 1].y());
				const auto to = getCellCenter(iAsset, iOrigin, route.cells[i].x(), route.cells[i].y());
				renderer::Renderer2D::drawLine({.point1 = from + offset, .point2 = to + offset, .color = color});
			}
			// The stop: a diamond around the last cell.
			const auto& last = route.cells.back();
			const auto c = getCellCenter(iAsset, iOrigin, last.x(), last.y()) + offset;
			const std::array<math::vec3, 4> ring{c + math::vec3{-0.3f, 0.f, 0.f}, c + math::vec3{0.f, -0.3f, 0.f},
												 c + math::vec3{0.3f, 0.f, 0.f}, c + math::vec3{0.f, 0.3f, 0.f}};
			for (size_t i = 0; i < ring.size(); ++i)
				renderer::Renderer2D::drawLine(
						{.point1 = ring[i], .point2 = ring[(i + 1) % ring.size()], .color = color});
		}
	});
}

}// namespace owl::scene::nav
