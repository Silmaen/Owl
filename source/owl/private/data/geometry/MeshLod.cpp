/**
 * @file MeshLod.cpp
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "data/geometry/MeshLod.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <optional>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace owl::data::geometry {

namespace {

constexpr std::array<uint8_t, 4> g_cookedMagic{'O', 'W', 'L', 'M'};
constexpr uint32_t g_cookedVersion = 1;
constexpr uint32_t g_maxGridCells = 1024;
constexpr double g_gridStep = 1.25;
// Grids tried, down to one cell: 1.25^32 exceeds the finest grid.
constexpr uint32_t g_gridSteps = 33;

struct TriangleKeyHash {
	auto operator()(const std::array<uint32_t, 3>& iKey) const noexcept -> size_t {
		size_t seed = 0;
		for (const uint32_t value: iKey)
			seed ^= std::hash<uint32_t>{}(value) + 0x9e3779b9u + (seed << 6u) + (seed >> 2u);
		return seed;
	}
};

struct Grid {
	math::vec3 origin{0.f, 0.f, 0.f};
	math::vec3 extent{0.f, 0.f, 0.f};
};

auto boundsOf(const std::span<const math::vec3> iPositions) -> Grid {
	constexpr float big = std::numeric_limits<float>::max();
	math::vec3 lo{big, big, big};
	math::vec3 hi{-big, -big, -big};
	for (const auto& p: iPositions) {
		for (size_t axis = 0; axis < 3; ++axis) {
			lo[axis] = std::min(lo[axis], p[axis]);
			hi[axis] = std::max(hi[axis], p[axis]);
		}
	}
	return {.origin = lo, .extent = hi - lo};
}

// The triangles left by a grid of `iCells` cells per axis, their corners written to `oCorners`.
auto cluster(const std::span<const math::vec3> iPositions, const Grid& iGrid, const uint32_t iCells,
			 std::vector<uint32_t>& oCorners) -> size_t {
	oCorners.clear();
	const math::vec3 cell = iGrid.extent * (1.f / static_cast<float>(iCells));
	const auto cellOf = [&](const math::vec3& iP) -> uint64_t {
		uint64_t key = 0;
		for (size_t axis = 0; axis < 3; ++axis) {
			const float offset = cell[axis] > 0.f ? (iP[axis] - iGrid.origin[axis]) / cell[axis] : 0.f;
			const auto index = std::min(static_cast<uint64_t>(std::max(offset, 0.f)), uint64_t{iCells - 1});
			key = (key << 21u) | index;
		}
		return key;
	};
	std::unordered_map<uint64_t, uint32_t> clusters;
	std::vector<uint32_t> clusterOfCorner(iPositions.size());
	std::vector<math::vec3> sums;
	std::vector<uint32_t> counts;
	for (size_t corner = 0; corner < iPositions.size(); ++corner) {
		const auto [found, inserted] =
				clusters.try_emplace(cellOf(iPositions[corner]), static_cast<uint32_t>(sums.size()));
		if (inserted) {
			sums.emplace_back(0.f, 0.f, 0.f);
			counts.push_back(0);
		}
		clusterOfCorner[corner] = found->second;
		sums[found->second] += iPositions[corner];
		++counts[found->second];
	}
	std::vector<uint32_t> representative(sums.size(), std::numeric_limits<uint32_t>::max());
	std::vector<float> bestDistance(sums.size(), std::numeric_limits<float>::max());
	for (size_t corner = 0; corner < iPositions.size(); ++corner) {
		const uint32_t id = clusterOfCorner[corner];
		const math::vec3 mean = sums[id] * (1.f / static_cast<float>(counts[id]));
		if (const float distance = (iPositions[corner] - mean).normSq(); distance < bestDistance[id]) {
			bestDistance[id] = distance;
			representative[id] = static_cast<uint32_t>(corner);
		}
	}
	std::unordered_set<std::array<uint32_t, 3>, TriangleKeyHash> seen;
	for (size_t tri = 0; tri + 2 < iPositions.size(); tri += 3) {
		const std::array<uint32_t, 3> ids{clusterOfCorner[tri], clusterOfCorner[tri + 1], clusterOfCorner[tri + 2]};
		if (ids[0] == ids[1] || ids[1] == ids[2] || ids[0] == ids[2])
			continue;
		auto sorted = ids;
		std::ranges::sort(sorted);
		if (!seen.insert(sorted).second)
			continue;
		for (const uint32_t id: ids) oCorners.push_back(representative[id]);
	}
	return oCorners.size() / 3;
}

void appendU32(std::vector<uint8_t>& ioOut, const uint32_t iValue) {
	const auto at = ioOut.size();
	ioOut.resize(at + sizeof(uint32_t));
	std::memcpy(ioOut.data() + at, &iValue, sizeof(uint32_t));
}

class Reader {
public:
	explicit Reader(const std::span<const uint8_t> iData) : m_data{iData} {}

	auto readU32() -> std::optional<uint32_t> {
		uint32_t value = 0;
		if (!read(&value, sizeof(uint32_t)))
			return std::nullopt;
		return value;
	}

	auto readU64() -> std::optional<uint64_t> {
		uint64_t value = 0;
		if (!read(&value, sizeof(uint64_t)))
			return std::nullopt;
		return value;
	}

	auto readF32() -> std::optional<float> {
		float value = 0;
		if (!read(&value, sizeof(float)))
			return std::nullopt;
		return value;
	}

	auto take(const size_t iSize) -> std::optional<std::span<const uint8_t>> {
		if (iSize > m_data.size() - m_offset)
			return std::nullopt;
		const auto view = m_data.subspan(m_offset, iSize);
		m_offset += iSize;
		return view;
	}

	[[nodiscard]] auto remaining() const -> size_t { return m_data.size() - m_offset; }

private:
	auto read(void* oValue, const size_t iSize) -> bool {
		if (iSize > m_data.size() - m_offset)
			return false;
		std::memcpy(oValue, m_data.data() + m_offset, iSize);
		m_offset += iSize;
		return true;
	}

	// The bytes read.
	std::span<const uint8_t> m_data;
	// Position of the next read.
	size_t m_offset = 0;
};

}// namespace

auto simplifyCorners(const std::span<const math::vec3> iCornerPositions, const float iRatio) -> std::vector<uint32_t> {
	OWL_PROFILE_FUNCTION()

	std::vector<uint32_t> corners;
	const size_t triangles = iCornerPositions.size() / 3;
	if (triangles == 0)
		return corners;
	const auto target = static_cast<size_t>(
			std::floor(static_cast<double>(triangles) * static_cast<double>(std::clamp(iRatio, 0.f, 1.f))));
	const Grid grid = boundsOf(iCornerPositions);
	// The kept count is not monotonic in the grid size: the grids are tried from the finest one, a quarter coarser each.
	for (uint32_t step = 0; step < g_gridSteps; ++step) {
		const auto cells = static_cast<uint32_t>(
				std::max(1.0, static_cast<double>(g_maxGridCells) / std::pow(g_gridStep, static_cast<double>(step))));
		if (cluster(iCornerPositions, grid, cells, corners) <= target)
			return corners;
	}
	corners.clear();
	return corners;
}

auto buildMeshLods(const LoadedMesh& iMesh, const LodSettings& iSettings) -> std::vector<MeshLodLevel> {
	OWL_PROFILE_FUNCTION()

	std::vector<MeshLodLevel> levels;
	if (!iMesh.mesh || iSettings.levelCount == 0)
		return levels;
	const auto& triangles = iMesh.mesh->getTriangles();
	std::vector<MeshSubmesh> submeshes = iMesh.submeshes;
	if (submeshes.empty())
		submeshes.push_back(
				{.firstIndex = 0, .indexCount = static_cast<uint32_t>(triangles.size() * 3), .materialIndex = 0});
	const float reduction = std::clamp(iSettings.reduction, 0.05f, 0.95f);
	const uint32_t levelCount = std::min(iSettings.levelCount, LodSettings::g_maxLevels);
	size_t previous = triangles.size();
	float ratio = 1.f;
	float screenSize = std::max(iSettings.screenSize, 0.f);
	std::vector<math::vec3> positions;
	for (uint32_t level = 0; level < levelCount; ++level) {
		ratio *= reduction;
		MeshLodLevel lod;
		lod.screenSize = screenSize;
		for (const auto& submesh: submeshes) {
			const size_t firstTriangle = submesh.firstIndex / 3;
			const size_t count = std::min<size_t>(submesh.indexCount / 3,
												  triangles.size() - std::min(firstTriangle, triangles.size()));
			positions.clear();
			for (size_t tri = firstTriangle; tri < firstTriangle + count; ++tri) {
				for (uint8_t corner = 0; corner < 3; ++corner)
					positions.push_back(triangles[tri].getVertex(corner)->getPosition());
			}
			auto corners = simplifyCorners(positions, ratio);
			for (auto& corner: corners) corner += static_cast<uint32_t>(firstTriangle * 3);
			lod.submeshCorners.push_back(std::move(corners));
		}
		const size_t kept = lod.getTriangleCount();
		if (kept == 0 || kept >= previous)
			break;
		previous = kept;
		levels.push_back(std::move(lod));
		screenSize *= 0.5f;
	}
	return levels;
}

auto isCookedMesh(const std::span<const uint8_t> iData) -> bool {
	return iData.size() >= g_cookedMagic.size() && std::ranges::equal(iData.first(g_cookedMagic.size()), g_cookedMagic);
}

auto writeCookedMesh(const std::span<const uint8_t> iSource, const std::span<const MeshLodLevel> iLods)
		-> std::vector<uint8_t> {
	std::vector<uint8_t> out{g_cookedMagic.begin(), g_cookedMagic.end()};
	appendU32(out, g_cookedVersion);
	const uint64_t size = iSource.size();
	const auto at = out.size();
	out.resize(at + sizeof(uint64_t));
	std::memcpy(out.data() + at, &size, sizeof(uint64_t));
	out.insert(out.end(), iSource.begin(), iSource.end());
	appendU32(out, static_cast<uint32_t>(iLods.size()));
	for (const auto& lod: iLods) {
		uint32_t bits = 0;
		std::memcpy(&bits, &lod.screenSize, sizeof(float));
		appendU32(out, bits);
		appendU32(out, static_cast<uint32_t>(lod.submeshCorners.size()));
		for (const auto& corners: lod.submeshCorners) {
			appendU32(out, static_cast<uint32_t>(corners.size()));
			for (const uint32_t corner: corners) appendU32(out, corner);
		}
	}
	return out;
}

auto readCookedMesh(const std::span<const uint8_t> iData) -> std::optional<CookedMesh> {
	if (!isCookedMesh(iData)) {
		OWL_CORE_WARN("MeshLoader: Not a cooked mesh.")
		return std::nullopt;
	}
	Reader reader{iData.subspan(g_cookedMagic.size())};
	const auto version = reader.readU32();
	if (!version || *version != g_cookedVersion) {
		OWL_CORE_WARN("MeshLoader: Cooked mesh of version {} not supported.", version.value_or(0))
		return std::nullopt;
	}
	const auto size = reader.readU64();
	const auto source = size && *size <= reader.remaining() ? reader.take(static_cast<size_t>(*size)) : std::nullopt;
	const auto levelCount = reader.readU32();
	if (!source || !levelCount || *levelCount > LodSettings::g_maxLevels) {
		OWL_CORE_WARN("MeshLoader: Corrupted cooked mesh header.")
		return std::nullopt;
	}
	CookedMesh cooked{.source = *source, .lods = {}};
	for (uint32_t level = 0; level < *levelCount; ++level) {
		MeshLodLevel lod;
		const auto screenSize = reader.readF32();
		const auto submeshCount = reader.readU32();
		if (!screenSize || !submeshCount || *submeshCount > reader.remaining() / sizeof(uint32_t)) {
			OWL_CORE_WARN("MeshLoader: Corrupted level {} of a cooked mesh.", level)
			return std::nullopt;
		}
		lod.screenSize = *screenSize;
		for (uint32_t submesh = 0; submesh < *submeshCount; ++submesh) {
			const auto count = reader.readU32();
			if (!count || *count % 3 != 0 || *count > reader.remaining() / sizeof(uint32_t)) {
				OWL_CORE_WARN("MeshLoader: Corrupted level {} of a cooked mesh.", level)
				return std::nullopt;
			}
			std::vector<uint32_t> corners(*count);
			for (auto& corner: corners) corner = reader.readU32().value_or(0);
			lod.submeshCorners.push_back(std::move(corners));
		}
		cooked.lods.push_back(std::move(lod));
	}
	return cooked;
}

}// namespace owl::data::geometry
