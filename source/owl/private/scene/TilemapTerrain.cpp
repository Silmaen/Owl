/**
 * @file TilemapTerrain.cpp
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "scene/TilemapTerrain.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace owl::scene::terrain {

namespace {

constexpr std::array<CornerSet, slopeCount> g_profiles{{
		{0, 0, 0, 0},// Flat
		{1, 0, 0, 0},// Corner back
		{0, 1, 0, 0},// Corner right
		{0, 0, 1, 0},// Corner front
		{0, 0, 0, 1},// Corner left
		{0, 1, 1, 1},// Inner back
		{1, 0, 1, 1},// Inner right
		{1, 1, 0, 1},// Inner front
		{1, 1, 1, 0},// Inner left
		{1, 1, 0, 0},// Edge back-right
		{0, 1, 1, 0},// Edge right-front
		{0, 0, 1, 1},// Edge front-left
		{1, 0, 0, 1},// Edge left-back
		{1, 0, 1, 0},// Saddle back-front
		{0, 1, 0, 1},// Saddle right-left
		{2, 1, 0, 1},// Steep back
		{1, 2, 1, 0},// Steep right
		{0, 1, 2, 1},// Steep front
		{1, 0, 1, 2},// Steep left
}};

constexpr std::array<std::string_view, slopeCount> g_names{
		"Flat",           "Corner back",       "Corner right",      "Corner front",
		"Corner left",    "Inner back",        "Inner right",       "Inner front",
		"Inner left",     "Edge back-right",   "Edge right-front",  "Edge front-left",
		"Edge left-back", "Saddle back-front", "Saddle right-left", "Steep back",
		"Steep right",    "Steep front",       "Steep left"};

constexpr uint8_t g_noSlope = 0xFF;

constexpr auto keyOf(const CornerSet& iCorners) -> size_t {
	return static_cast<size_t>(iCorners[0]) * 27 + static_cast<size_t>(iCorners[1]) * 9 +
		   static_cast<size_t>(iCorners[2]) * 3 + static_cast<size_t>(iCorners[3]);
}

constexpr auto buildLookup() -> std::array<uint8_t, 81> {
	std::array<uint8_t, 81> lookup{};
	lookup.fill(g_noSlope);
	for (uint8_t slope = 0; slope < slopeCount; ++slope) lookup[keyOf(g_profiles[slope])] = slope;
	return lookup;
}

constexpr auto g_lookup = buildLookup();

struct Vec {
	float x = 0.f;
	float y = 0.f;
	float z = 0.f;
};

auto minus(const Vec& iFirst, const Vec& iSecond) -> Vec {
	return {iFirst.x - iSecond.x, iFirst.y - iSecond.y, iFirst.z - iSecond.z};
}

auto cross(const Vec& iFirst, const Vec& iSecond) -> Vec {
	return {iFirst.y * iSecond.z - iFirst.z * iSecond.y, iFirst.z * iSecond.x - iFirst.x * iSecond.z,
			iFirst.x * iSecond.y - iFirst.y * iSecond.x};
}

auto dot(const Vec& iFirst, const Vec& iSecond) -> float {
	return iFirst.x * iSecond.x + iFirst.y * iSecond.y + iFirst.z * iSecond.z;
}

// Moller-Trumbore, two-sided; the distance along the ray, or a negative number on a miss.
auto hitTriangle(const Vec& iOrigin, const Vec& iDirection, const Vec& iA, const Vec& iB, const Vec& iC) -> float {
	constexpr float epsilon = 1e-6f;
	const Vec edge1 = minus(iB, iA);
	const Vec edge2 = minus(iC, iA);
	const Vec pvec = cross(iDirection, edge2);
	const float det = dot(edge1, pvec);
	if (std::abs(det) < epsilon)
		return -1.f;
	const float invDet = 1.f / det;
	const Vec tvec = minus(iOrigin, iA);
	const float u = dot(tvec, pvec) * invDet;
	if (u < -epsilon || u > 1.f + epsilon)
		return -1.f;
	const Vec qvec = cross(tvec, edge1);
	const float v = dot(iDirection, qvec) * invDet;
	if (v < -epsilon || u + v > 1.f + epsilon)
		return -1.f;
	return dot(edge2, qvec) * invDet;
}

// Nearest hit on the two triangles of a cell, in the map frame.
auto hitCell(const TilemapAsset& iAsset, const uint32_t iX, const uint32_t iY, const Vec& iOrigin,
			 const Vec& iDirection) -> float {
	const auto corners = iAsset.getCellCorners(iX, iY);
	const auto fx = static_cast<float>(iX);
	const auto fy = static_cast<float>(iY);
	const std::array<Vec, 4> points{{{fx, fy, static_cast<float>(corners[0]) * heightStep},
									 {fx + 1.f, fy, static_cast<float>(corners[1]) * heightStep},
									 {fx + 1.f, fy + 1.f, static_cast<float>(corners[2]) * heightStep},
									 {fx, fy + 1.f, static_cast<float>(corners[3]) * heightStep}}};
	const bool backDiagonal =
			std::abs(int{corners[0]} - int{corners[2]}) <= std::abs(int{corners[1]} - int{corners[3]});
	const std::array<std::array<size_t, 3>, 2> triangles =
			backDiagonal ? std::array<std::array<size_t, 3>, 2>{{{0, 1, 2}, {0, 2, 3}}}
						 : std::array<std::array<size_t, 3>, 2>{{{0, 1, 3}, {1, 2, 3}}};
	float best = std::numeric_limits<float>::max();
	for (const auto& [a, b, c]: triangles) {
		if (const float t = hitTriangle(iOrigin, iDirection, points[a], points[b], points[c]); t >= 0.f)
			best = std::min(best, t);
	}
	return best;
}

struct Clip {
	float enter = 0.f;
	float exit = 0.f;
};

// Range of the ray inside the slab [iLow, iHigh] of one axis; false when it never enters.
auto clipAxis(const float iOrigin, const float iDirection, const float iLow, const float iHigh, Clip& ioClip) -> bool {
	if (std::abs(iDirection) < 1e-9f)
		return iOrigin >= iLow && iOrigin <= iHigh;
	float first = (iLow - iOrigin) / iDirection;
	float second = (iHigh - iOrigin) / iDirection;
	if (first > second)
		std::swap(first, second);
	ioClip.enter = std::max(ioClip.enter, first);
	ioClip.exit = std::min(ioClip.exit, second);
	return ioClip.enter <= ioClip.exit;
}

}// namespace

auto getSlopeProfile(const uint8_t iSlope) -> const CornerSet& {
	return g_profiles[iSlope < slopeCount ? iSlope : flatSlope];
}

auto deriveSlope(const CornerSet& iCorners) -> std::optional<uint8_t> {
	const uint8_t lowest = *std::ranges::min_element(iCorners);
	CornerSet relative{};
	for (size_t i = 0; i < 4; ++i) {
		relative[i] = static_cast<uint8_t>(iCorners[i] - lowest);
		if (relative[i] > 2)
			return std::nullopt;
	}
	const uint8_t slope = g_lookup[keyOf(relative)];
	if (slope == g_noSlope)
		return std::nullopt;
	return slope;
}

auto getSlopeName(const uint8_t iSlope) -> std::string_view {
	return iSlope < slopeCount ? g_names[iSlope] : "Invalid";
}

auto isSteepSlope(const uint8_t iSlope) -> bool { return iSlope >= firstSteepSlope && iSlope < slopeCount; }

auto pickTerrain(const TilemapAsset& iAsset, const math::vec3& iMapOrigin, const math::vec3& iRayOrigin,
				 const math::vec3& iRayDirection) -> std::optional<TerrainHit> {
	if (iRayDirection.z() >= -1e-9f)
		return std::nullopt;
	const Vec origin{iRayOrigin.x() - iMapOrigin.x(), iRayOrigin.y() - iMapOrigin.y(), iRayOrigin.z() - iMapOrigin.z()};
	const Vec direction{iRayDirection.x(), iRayDirection.y(), iRayDirection.z()};
	const float zTop = static_cast<float>(iAsset.getMaxCornerHeight()) * heightStep;
	Clip clip{.enter = 0.f, .exit = std::numeric_limits<float>::max()};
	if (!clipAxis(origin.z, direction.z, 0.f, zTop, clip) ||
		!clipAxis(origin.x, direction.x, 0.f, static_cast<float>(iAsset.width), clip) ||
		!clipAxis(origin.y, direction.y, 0.f, static_cast<float>(iAsset.height), clip))
		return std::nullopt;

	const auto startX = origin.x + direction.x * clip.enter;
	const auto startY = origin.y + direction.y * clip.enter;
	const auto lastX = static_cast<int32_t>(iAsset.width) - 1;
	const auto lastY = static_cast<int32_t>(iAsset.height) - 1;
	auto cellX = std::clamp(static_cast<int32_t>(std::floor(startX)), 0, lastX);
	auto cellY = std::clamp(static_cast<int32_t>(std::floor(startY)), 0, lastY);
	const int32_t stepX = direction.x > 0.f ? 1 : -1;
	const int32_t stepY = direction.y > 0.f ? 1 : -1;
	constexpr float never = std::numeric_limits<float>::max();
	const auto nextBoundary = [](const float iOriginAxis, const float iDirectionAxis, const int32_t iCell,
								 const int32_t iStep) -> float {
		if (std::abs(iDirectionAxis) < 1e-9f)
			return never;
		const auto edge = static_cast<float>(iCell + (iStep > 0 ? 1 : 0));
		return (edge - iOriginAxis) / iDirectionAxis;
	};
	float nextX = nextBoundary(origin.x, direction.x, cellX, stepX);
	float nextY = nextBoundary(origin.y, direction.y, cellY, stepY);
	const float deltaX = std::abs(direction.x) < 1e-9f ? never : std::abs(1.f / direction.x);
	const float deltaY = std::abs(direction.y) < 1e-9f ? never : std::abs(1.f / direction.y);

	float current = clip.enter;
	while (cellX >= 0 && cellY >= 0 && cellX <= lastX && cellY <= lastY && current <= clip.exit) {
		const float leave = std::min({nextX, nextY, clip.exit});
		const auto ux = static_cast<uint32_t>(cellX);
		const auto uy = static_cast<uint32_t>(cellY);
		const float zEnter = origin.z + direction.z * current;
		const float zLeave = origin.z + direction.z * leave;
		const auto corners = iAsset.getCellCorners(ux, uy);
		const auto [lowest, highest] = std::ranges::minmax(corners);
		const float low = static_cast<float>(lowest) * heightStep - 1e-4f;
		const float high = static_cast<float>(highest) * heightStep + 1e-4f;
		if (std::max(zEnter, zLeave) >= low && std::min(zEnter, zLeave) <= high) {
			if (const float t = hitCell(iAsset, ux, uy, origin, direction); t < never) {
				TerrainHit hit;
				hit.cell = {ux, uy};
				hit.point = math::vec3{iMapOrigin.x() + origin.x + direction.x * t,
									   iMapOrigin.y() + origin.y + direction.y * t,
									   iMapOrigin.z() + origin.z + direction.z * t};
				const float fx = origin.x + direction.x * t - static_cast<float>(ux);
				const float fy = origin.y + direction.y * t - static_cast<float>(uy);
				hit.corner = {ux + (fx >= 0.5f ? 1u : 0u), uy + (fy >= 0.5f ? 1u : 0u)};
				return hit;
			}
		}
		if (nextX < nextY) {
			current = nextX;
			nextX += deltaX;
			cellX += stepX;
		} else {
			current = nextY;
			nextY += deltaY;
			cellY += stepY;
		}
	}
	return std::nullopt;
}

void mergeCornerChanges(std::vector<CornerHeightChange>& ioChanges) {
	std::ranges::stable_sort(ioChanges, {}, &CornerHeightChange::index);
	std::vector<CornerHeightChange> merged;
	for (const auto& change: ioChanges) {
		if (!merged.empty() && merged.back().index == change.index)
			merged.back().after = change.after;
		else
			merged.push_back(change);
	}
	std::erase_if(merged, [](const CornerHeightChange& iChange) -> bool { return iChange.before == iChange.after; });
	ioChanges = std::move(merged);
}

auto pickTerrain(const TilemapAsset& iAsset, const renderer::IsometricCamera& iCamera, const math::vec2& iPixel,
				 const math::vec3& iMapOrigin) -> std::optional<TerrainHit> {
	const float top = iMapOrigin.z() + static_cast<float>(iAsset.getMaxCornerHeight()) * heightStep + 1.f;
	const math::vec3 start = iCamera.viewportToWorld(iPixel, top);
	const math::vec3 ground = iCamera.viewportToWorld(iPixel, iMapOrigin.z());
	return pickTerrain(iAsset, iMapOrigin, start, ground - start);
}

}// namespace owl::scene::terrain
