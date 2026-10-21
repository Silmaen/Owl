/**
 * @file MeshCache.cpp
 * @author Silmaen
 * @date 21/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/MeshCache.h"

#include "app/Application.h"
#include "data/geometry/MeshLoader.h"
#include "data/geometry/StaticMesh.h"
#include "data/geometry/extradata/TriangleNormals.h"
#include "data/geometry/extradata/TriangleUVCoordinate.h"
#include "platform/FileWatcher.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <limits>
#include <optional>
#include <ranges>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace owl::renderer {

namespace {

using data::geometry::LoadedMesh;
using data::geometry::MeshLoader;

// Bit pattern of one vertex (position, normal, uv), to share identical corners.
using CornerKey = std::array<uint32_t, 8>;

struct CornerKeyHash {
	auto operator()(const CornerKey& iKey) const noexcept -> size_t {
		size_t seed = 0;
		for (const uint32_t value: iKey)
			seed ^= std::hash<uint32_t>{}(value) + 0x9e3779b9u + (seed << 6u) + (seed >> 2u);
		return seed;
	}
};

// Adding zero turns -0 into +0, so equal values give equal bits.
auto bitsOf(const float iValue) -> uint32_t { return std::bit_cast<uint32_t>(iValue + 0.f); }

auto makeKey(const math::vec3& iPosition, const math::vec3& iNormal, const math::vec2& iUv) -> CornerKey {
	return {bitsOf(iPosition.x()), bitsOf(iPosition.y()), bitsOf(iPosition.z()), bitsOf(iNormal.x()),
			bitsOf(iNormal.y()),   bitsOf(iNormal.z()),   bitsOf(iUv.x()),       bitsOf(iUv.y())};
}

auto faceNormal(const math::vec3& iA, const math::vec3& iB, const math::vec3& iC) -> math::vec3 {
	const math::vec3 normal = (iB - iA) ^ (iC - iA);
	const float length = normal.norm();
	return length > std::numeric_limits<float>::epsilon() ? normal * (1.f / length) : math::vec3{0.f, 1.f, 0.f};
}

// Möller-Trumbore, both faces; the distance along the direction, when the ray hits in front of its origin.
auto hitTriangle(const math::vec3& iOrigin, const math::vec3& iDirection, const math::vec3& iA, const math::vec3& iB,
				 const math::vec3& iC) -> std::optional<float> {
	constexpr float epsilon = 1e-7f;
	const math::vec3 edge1 = iB - iA;
	const math::vec3 edge2 = iC - iA;
	const math::vec3 p = iDirection ^ edge2;
	const float det = edge1 * p;
	if (std::abs(det) < epsilon)
		return std::nullopt;
	const float invDet = 1.f / det;
	const math::vec3 s = iOrigin - iA;
	const float u = (s * p) * invDet;
	if (u < 0.f || u > 1.f)
		return std::nullopt;
	const math::vec3 q = s ^ edge1;
	const float v = (iDirection * q) * invDet;
	if (v < 0.f || u + v > 1.f)
		return std::nullopt;
	const float t = (edge2 * q) * invDet;
	if (t < 0.f)
		return std::nullopt;
	return t;
}

// The mesh of an asset path and the file it came from: the open pack first, then the asset directories, then as is.
auto loadAsset(const std::filesystem::path& iAssetPath) -> std::optional<std::pair<std::filesystem::path, LoadedMesh>> {
	const auto extension = iAssetPath.extension().string();
	if (!MeshLoader::isMeshAsset(iAssetPath)) {
		OWL_CORE_WARN("MeshCache: {} is not a mesh file (.obj, .gltf, .glb).", iAssetPath.generic_string())
		return std::nullopt;
	}
	const auto tryFile =
			[](const std::filesystem::path& iFile) -> std::optional<std::pair<std::filesystem::path, LoadedMesh>> {
		auto loaded = MeshLoader::loadMesh(iFile);
		if (!loaded.mesh)
			return std::nullopt;
		return std::pair{iFile, std::move(loaded)};
	};
	if (app::Application::instanced()) {
		const auto& app = app::Application::get();
		if (const std::string rel = iAssetPath.generic_string(); app.hasOpenPack() && app.packContains(rel)) {
			if (const auto bytes = app.loadFromPack(rel); bytes) {
				const auto readFromPack = [&app](const std::string& iPath) -> std::optional<std::vector<uint8_t>> {
					return app.loadFromPack(std::filesystem::path{iPath}.lexically_normal().generic_string());
				};
				auto loaded =
						MeshLoader::loadMeshFromMemory(*bytes, extension, rel, iAssetPath.parent_path(), readFromPack);
				if (loaded.mesh)
					return std::pair{std::filesystem::path{}, std::move(loaded)};
			}
		}
		if (iAssetPath.is_relative()) {
			for (const auto& [title, assetsPath]: app.getAssetDirectories()) {
				if (const auto full = assetsPath / iAssetPath; exists(full))
					return tryFile(full);
			}
		}
	}
	if (exists(iAssetPath))
		return tryFile(iAssetPath);
	OWL_CORE_WARN("MeshCache: Mesh {} not found.", iAssetPath.generic_string())
	return std::nullopt;
}

}// namespace

auto MeshGeometry::fromStaticMesh(const data::geometry::StaticMesh& iMesh) -> MeshGeometry {
	OWL_PROFILE_FUNCTION()

	MeshGeometry geometry;
	const auto& triangles = iMesh.getTriangles();
	if (triangles.empty())
		return geometry;
	const auto uvs = iMesh.createTriangleExtraDataRange<data::geometry::extradata::TriangleUVCoordinate>();
	const auto normals = iMesh.createTriangleExtraDataRange<data::geometry::extradata::TriangleNormals>();
	std::unordered_map<CornerKey, uint32_t, CornerKeyHash> seen;
	seen.reserve(triangles.size() * 3);
	geometry.indices.reserve(triangles.size() * 3);
	constexpr float big = std::numeric_limits<float>::max();
	geometry.boundsMin = math::vec3{big, big, big};
	geometry.boundsMax = math::vec3{-big, -big, -big};
	for (size_t tri = 0; tri < triangles.size(); ++tri) {
		const auto& triangle = triangles[tri];
		std::array<math::vec3, 3> corners{};
		for (uint8_t corner = 0; corner < 3; ++corner) corners[corner] = triangle.getVertex(corner)->getPosition();
		const math::vec3 flat = faceNormal(corners[0], corners[1], corners[2]);
		for (uint8_t corner = 0; corner < 3; ++corner) {
			math::vec3 normal = flat;
			if (tri < normals.size() && normals[tri]) {
				if (const auto& given = normals[tri]->getNormal(corner); given.normSq() > 1e-12f)
					normal = given;
			}
			math::vec2 uv{0.f, 0.f};
			if (tri < uvs.size() && uvs[tri])
				uv = uvs[tri]->getUvCoord(corner);
			const math::vec3& position = corners[corner];
			const auto [found, inserted] =
					seen.try_emplace(makeKey(position, normal, uv), static_cast<uint32_t>(geometry.vertices.size()));
			if (inserted) {
				geometry.vertices.push_back(
						Mesh3DVertex{.position = position, .normal = normal, .uv = uv, .textureIndex = 1});
				for (size_t axis = 0; axis < 3; ++axis) {
					geometry.boundsMin[axis] = std::min(geometry.boundsMin[axis], position[axis]);
					geometry.boundsMax[axis] = std::max(geometry.boundsMax[axis], position[axis]);
				}
			}
			geometry.indices.push_back(found->second);
		}
	}
	return geometry;
}

auto MeshGeometry::raycast(const math::vec3& iOrigin, const math::vec3& iDirection) const -> std::optional<math::vec3> {
	std::optional<float> nearest;
	for (size_t i = 0; i + 2 < indices.size(); i += 3) {
		const auto hit = hitTriangle(iOrigin, iDirection, vertices[indices[i]].position,
									 vertices[indices[i + 1]].position, vertices[indices[i + 2]].position);
		if (hit && (!nearest || *hit < *nearest))
			nearest = hit;
	}
	if (!nearest)
		return std::nullopt;
	return iOrigin + iDirection * *nearest;
}

MeshResource::MeshResource(std::filesystem::path iFile, const data::geometry::LoadedMesh& iLoaded)
	: m_file{std::move(iFile)}, m_baseColorFactor{iLoaded.material.baseColorFactor} {
	if (iLoaded.mesh)
		m_geometry = MeshGeometry::fromStaticMesh(*iLoaded.mesh);
	const auto& material = iLoaded.material;
	if (!material.hasBaseColorTexture())
		return;
	m_baseColorTexture = gpu::Texture2D::create(
			gpu::Texture2D::Specification{.size = material.baseColorSize, .format = gpu::ImageFormat::Rgba8});
	if (!m_baseColorTexture)
		return;
	auto pixels = material.baseColorPixels;
	m_baseColorTexture->setData(pixels.data(), static_cast<uint32_t>(pixels.size()));
}

MeshResource::~MeshResource() = default;

auto MeshResource::getMesh(const bool iTransparent) -> const Renderer3D::MeshHandle& {
	auto& mesh = iTransparent ? m_transparent : m_opaque;
	if (!mesh && !m_geometry.isEmpty())
		mesh = Renderer3D::createMesh(m_geometry.vertices, m_geometry.indices, "mesh3d",
									  iTransparent ? Renderer3D::transparentMeshState : Renderer3D::opaqueMeshState);
	return mesh;
}

MeshCache::MeshCache() = default;

MeshCache::~MeshCache() = default;

auto MeshCache::get(const std::filesystem::path& iAssetPath) -> shared<MeshResource> {
	if (iAssetPath.empty())
		return nullptr;
	const auto key = iAssetPath.generic_string();
	if (const auto found = m_entries.find(key); found != m_entries.end())
		return found->second.resource;
	auto& entry = m_entries[key];
	if (auto loaded = loadAsset(iAssetPath); loaded)
		entry.resource = mkShared<MeshResource>(std::move(loaded->first), loaded->second);
	return entry.resource;
}

auto MeshCache::find(const std::filesystem::path& iAssetPath) const -> shared<MeshResource> {
	const auto found = m_entries.find(iAssetPath.generic_string());
	return found == m_entries.end() ? nullptr : found->second.resource;
}

auto MeshCache::invalidate(const std::filesystem::path& iFile) -> uint32_t {
	const auto changed = iFile.generic_string();
	const auto matches = [&](const std::string& iKey, const Entry& iEntry) -> bool {
		if (iKey == changed || changed.ends_with("/" + iKey))
			return true;
		return iEntry.resource && !iEntry.resource->getFile().empty() &&
			   platform::isSameFile(iEntry.resource->getFile(), iFile);
	};
	uint32_t dropped = 0;
	for (auto it = m_entries.begin(); it != m_entries.end();) {
		if (!matches(it->first, it->second)) {
			++it;
			continue;
		}
		if (it->second.resource)
			it->second.resource->markStale();
		it = m_entries.erase(it);
		++dropped;
	}
	if (dropped > 0)
		OWL_CORE_INFO("MeshCache: {} reloads on its next draw.", changed)
	return dropped;
}

void MeshCache::clear() {
	for (auto& entry: m_entries | std::views::values) {
		if (entry.resource)
			entry.resource->markStale();
	}
	m_entries.clear();
}

}// namespace owl::renderer
