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

}// namespace

auto getCellCenter(const TilemapAsset& iAsset, const math::vec3& iOrigin, const uint32_t iX, const uint32_t iY)
		-> math::vec3 {
	const auto corners = iAsset.getCellCorners(iX, iY);
	const float height =
			static_cast<float>(corners[0] + corners[1] + corners[2] + corners[3]) * 0.25f * terrain::heightStep;
	return iOrigin + math::vec3{static_cast<float>(iX) + 0.5f, static_cast<float>(iY) + 0.5f, height + 0.06f};
}

void drawNavigationGraph(Scene& ioScene) {
	constexpr math::vec4 twoWay{0.30f, 0.90f, 0.40f, 0.9f};
	constexpr math::vec4 oneWay{1.f, 0.60f, 0.10f, 1.f};
	size_t drawn = 0;
	forEachNavigationMap(ioScene, [&](const TilemapAsset& iAsset, const math::vec3& iOrigin, NavWorld::Entry& iEntry) {
		const auto& map = iEntry.map;
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
					if (!forward && !backward)
						continue;
					const auto from = getCellCenter(iAsset, iOrigin, x, y);
					const auto to = getCellCenter(iAsset, iOrigin, nx, ny);
					renderer::Renderer2D::drawLine(
							{.point1 = from, .point2 = to, .color = forward && backward ? twoWay : oneWay});
					if (forward != backward) {
						const auto tip = forward ? to : from;
						const auto tail = forward ? from : to;
						const auto along = (tail - tip) * 0.3f;
						const math::vec3 side{-along.y(), along.x(), 0.f};
						renderer::Renderer2D::drawLine(
								{.point1 = tip, .point2 = tip + along + side * 0.5f, .color = oneWay});
						renderer::Renderer2D::drawLine(
								{.point1 = tip, .point2 = tip + along - side * 0.5f, .color = oneWay});
					}
					++drawn;
				}
			}
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
