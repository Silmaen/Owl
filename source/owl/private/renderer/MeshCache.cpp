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
#include "data/assets/AssetMeta.h"
#include "data/geometry/MeshLoader.h"
#include "data/geometry/MeshLod.h"
#include "data/geometry/StaticMesh.h"
#include "data/geometry/extradata/TriangleNormals.h"
#include "data/geometry/extradata/TriangleUVCoordinate.h"
#include "platform/FileWatcher.h"
#include "renderer/TextureDecoder.h"

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
#include <span>
#include <string>
#include <system_error>
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

// Levels of detail of an uncooked file: the settings of its sidecar, none without one.
auto lodSettingsOf(const std::filesystem::path& iFile) -> data::geometry::LodSettings {
	if (iFile.empty())
		return {};
	const auto metaFile = data::assets::metaPathOf(iFile);
	if (std::error_code error; !std::filesystem::is_regular_file(metaFile, error))
		return {};
	const auto meta = data::assets::readMeta(metaFile);
	if (!meta || meta->importer != data::assets::Importer::Mesh)
		return {};
	return {.levelCount = meta->mesh.lods,
			.reduction = meta->mesh.lodReduction,
			.screenSize = meta->mesh.lodScreenSize};
}

}// namespace

auto MeshGeometry::fromStaticMesh(const data::geometry::StaticMesh& iMesh) -> MeshGeometry {
	return fromCorners(iMesh, {}, {});
}

auto MeshGeometry::fromCorners(const data::geometry::StaticMesh& iMesh,
							   const std::span<const data::geometry::VertexSkin> iSkin,
							   const std::span<const uint32_t> iCorners) -> MeshGeometry {
	OWL_PROFILE_FUNCTION()

	MeshGeometry geometry;
	const auto& triangles = iMesh.getTriangles();
	if (triangles.empty())
		return geometry;
	const auto* firstVertex = iMesh.getVertices().data();
	const bool skinned = !iSkin.empty() && iSkin.size() == iMesh.getVertexCount();
	const auto uvs = iMesh.createTriangleExtraDataRange<data::geometry::extradata::TriangleUVCoordinate>();
	const auto normals = iMesh.createTriangleExtraDataRange<data::geometry::extradata::TriangleNormals>();
	const size_t cornerCount = iCorners.empty() ? triangles.size() * 3 : iCorners.size() / 3 * 3;
	std::unordered_map<CornerKey, uint32_t, CornerKeyHash> seen;
	seen.reserve(cornerCount);
	geometry.indices.reserve(cornerCount);
	constexpr float big = std::numeric_limits<float>::max();
	geometry.boundsMin = math::vec3{big, big, big};
	geometry.boundsMax = math::vec3{-big, -big, -big};
	for (size_t face = 0; face < cornerCount / 3; ++face) {
		const size_t firstCorner = iCorners.empty() ? face * 3 : iCorners[face * 3];
		const size_t tri = firstCorner / 3;
		if (tri >= triangles.size())
			continue;
		std::array<math::vec3, 3> corners{};
		std::array<uint8_t, 3> cornerIds{0, 1, 2};
		std::array<size_t, 3> cornerTris{tri, tri, tri};
		if (!iCorners.empty()) {
			for (uint8_t corner = 0; corner < 3; ++corner) {
				const size_t ref = iCorners[face * 3 + corner];
				cornerTris[corner] = std::min(ref / 3, triangles.size() - 1);
				cornerIds[corner] = static_cast<uint8_t>(ref % 3);
			}
		}
		for (uint8_t corner = 0; corner < 3; ++corner)
			corners[corner] = triangles[cornerTris[corner]].getVertex(cornerIds[corner])->getPosition();
		const math::vec3 flat = faceNormal(corners[0], corners[1], corners[2]);
		for (uint8_t slot = 0; slot < 3; ++slot) {
			const size_t sourceTri = cornerTris[slot];
			const uint8_t corner = cornerIds[slot];
			math::vec3 normal = flat;
			if (sourceTri < normals.size() && normals[sourceTri]) {
				if (const auto& given = normals[sourceTri]->getNormal(corner); given.normSq() > 1e-12f)
					normal = given;
			}
			math::vec2 uv{0.f, 0.f};
			if (sourceTri < uvs.size() && uvs[sourceTri])
				uv = uvs[sourceTri]->getUvCoord(corner);
			const math::vec3& position = corners[slot];
			const auto [found, inserted] =
					seen.try_emplace(makeKey(position, normal, uv), static_cast<uint32_t>(geometry.vertices.size()));
			if (inserted) {
				geometry.vertices.push_back(
						Mesh3DVertex{.position = position, .normal = normal, .uv = uv, .textureIndex = 1});
				if (skinned) {
					// NOLINTNEXTLINE(bugprone-pointer-arithmetic-on-polymorphic-object): one vector of one type.
					const auto vertex = static_cast<size_t>(triangles[sourceTri].getVertex(corner) - firstVertex);
					geometry.skin.push_back(vertex < iSkin.size() ? iSkin[vertex] : data::geometry::VertexSkin{});
				}
				for (size_t axis = 0; axis < 3; ++axis) {
					geometry.boundsMin[axis] = std::min(geometry.boundsMin[axis], position[axis]);
					geometry.boundsMax[axis] = std::max(geometry.boundsMax[axis], position[axis]);
				}
			}
			geometry.indices.push_back(found->second);
		}
	}
	if (geometry.vertices.empty()) {
		geometry.boundsMin = math::vec3{0.f, 0.f, 0.f};
		geometry.boundsMax = math::vec3{0.f, 0.f, 0.f};
	}
	return geometry;
}

auto MeshGeometry::extract(const uint32_t iFirstIndex, const uint32_t iIndexCount) const -> MeshGeometry {
	MeshGeometry part;
	part.boundsMin = boundsMin;
	part.boundsMax = boundsMax;
	std::unordered_map<uint32_t, uint32_t> remap;
	part.indices.reserve(iIndexCount);
	const bool skinned = skin.size() == vertices.size() && !skin.empty();
	for (uint32_t offset = 0; offset < iIndexCount && iFirstIndex + offset < indices.size(); ++offset) {
		const uint32_t source = indices[iFirstIndex + offset];
		const auto [found, inserted] = remap.try_emplace(source, static_cast<uint32_t>(part.vertices.size()));
		if (inserted) {
			part.vertices.push_back(vertices[source]);
			if (skinned)
				part.skin.push_back(skin[source]);
		}
		part.indices.push_back(found->second);
	}
	return part;
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

namespace {
// Luminance given to a glTF emissive factor of 1 (glTF leaves its unit open; 1 reads as a lit sign).
constexpr float g_gltfEmissiveNits = 1000.f;

auto linearToSrgb(const float iValue) -> float {
	const float clamped = std::clamp(iValue, 0.f, 1.f);
	return clamped <= 0.0031308f ? clamped * 12.92f : 1.055f * std::pow(clamped, 1.f / 2.4f) - 0.055f;
}

auto toSrgb(const math::vec3& iLinear) -> math::vec3 {
	return {linearToSrgb(iLinear.x()), linearToSrgb(iLinear.y()), linearToSrgb(iLinear.z())};
}

auto createTexture(const data::geometry::MeshImage& iImage) -> shared<gpu::Texture2D> {
	if (iImage.isEmpty())
		return nullptr;
	if (!iImage.cooked.empty()) {
		const auto levels = transcodeKtx2(iImage.cooked, pickKtx2Target());
		auto texture = levels ? gpu::Texture2D::create(gpu::Texture2D::Specification{.size = levels->size,
																					 .format = gpu::ImageFormat::Rgba8})
							  : nullptr;
		if (!texture || !texture->setLevels(*levels)) {
			OWL_CORE_WARN("MeshCache: Cannot upload the cooked image {}.", iImage.name)
			return nullptr;
		}
		return texture;
	}
	auto texture = gpu::Texture2D::create(
			gpu::Texture2D::Specification{.size = iImage.size, .format = gpu::ImageFormat::Rgba8});
	if (!texture)
		return nullptr;
	auto pixels = iImage.pixels;
	texture->setData(pixels.data(), static_cast<uint32_t>(pixels.size()));
	return texture;
}

// A material of a mesh file as the renderer draws it: glTF colour factors are linear, the material keeps sRGB.
auto convertMaterial(const data::geometry::MeshMaterial& iSource) -> shared<const Material> {
	MaterialParameters values;
	const auto& base = iSource.baseColorFactor;
	const auto baseSrgb = toSrgb({base.x(), base.y(), base.z()});
	values.baseColor = math::vec4{baseSrgb.x(), baseSrgb.y(), baseSrgb.z(), base.w()};
	values.metallic = iSource.metallicFactor;
	values.roughness = iSource.roughnessFactor;
	values.emissive = toSrgb(iSource.emissiveFactor);
	values.emissiveLuminance = iSource.emissiveStrength * g_gltfEmissiveNits;
	values.normalScale = iSource.normalScale;
	values.occlusionStrength = iSource.occlusionStrength;
	values.alphaCutoff = iSource.alphaCutoff;
	values.doubleSided = iSource.doubleSided;
	values.clearcoat = iSource.clearcoat;
	values.clearcoatRoughness = iSource.clearcoatRoughness;
	values.sheenColor = toSrgb(iSource.sheenColor);
	values.sheenRoughness = iSource.sheenRoughness;
	values.anisotropy = iSource.anisotropy;
	values.anisotropyRotation = iSource.anisotropyRotation;
	using SourceAlpha = data::geometry::MeshMaterial::AlphaMode;
	switch (iSource.alphaMode) {
		case SourceAlpha::Opaque:
			values.alphaMode = AlphaMode::Opaque;
			break;
		case SourceAlpha::Mask:
			values.alphaMode = AlphaMode::Mask;
			break;
		case SourceAlpha::Blend:
			values.alphaMode = AlphaMode::Blend;
			break;
	}
	// The glTF metallic-roughness image serves both maps, read on the default channels B and G.
	const auto metallicRoughness = createTexture(iSource.metallicRoughness);
	const std::array<shared<gpu::Texture2D>, g_materialMapCount> textures{
			createTexture(iSource.baseColor), metallicRoughness,
			createTexture(iSource.normal),    createTexture(iSource.occlusion),
			createTexture(iSource.emissive),  metallicRoughness};
	return mkShared<const Material>(values, textures);
}

}// namespace

MeshResource::MeshResource(std::filesystem::path iFile, const data::geometry::LoadedMesh& iLoaded,
						   const data::geometry::LodSettings& iLods)
	: m_file{std::move(iFile)}, m_baseColorFactor{iLoaded.materials.front().baseColorFactor} {
	const bool skinned = iLoaded.isSkinned() && iLoaded.skeleton->isValid();
	if (iLoaded.isSkinned() && !skinned)
		OWL_CORE_WARN("MeshCache: Skeleton of {} cannot be drawn ({} bones), drawn static.", m_file.generic_string(),
					  iLoaded.skeleton->getBoneCount())
	if (iLoaded.mesh)
		m_geometry = MeshGeometry::fromCorners(
				*iLoaded.mesh, skinned ? std::span{iLoaded.skin} : std::span<const data::geometry::VertexSkin>{}, {});
	if (skinned) {
		m_skeleton = iLoaded.skeleton;
		m_clips = iLoaded.clips;
	}
	m_sockets = iLoaded.sockets;
	m_fileMaterials.reserve(iLoaded.materials.size());
	for (const auto& material: iLoaded.materials) m_fileMaterials.push_back(convertMaterial(material));
	const auto indexCount = static_cast<uint32_t>(m_geometry.indices.size());
	for (const auto& submesh: iLoaded.submeshes) {
		if (submesh.firstIndex >= indexCount || submesh.indexCount == 0)
			continue;
		m_submeshes.push_back({.firstIndex = submesh.firstIndex,
							   .indexCount = std::min(submesh.indexCount, indexCount - submesh.firstIndex),
							   .materialIndex = std::min<uint32_t>(submesh.materialIndex,
																   static_cast<uint32_t>(m_fileMaterials.size() - 1))});
	}
	if (m_submeshes.empty() && indexCount > 0)
		m_submeshes.push_back({.firstIndex = 0, .indexCount = indexCount, .materialIndex = 0});
	if (iLoaded.mesh) {
		const auto lods = !iLoaded.lods.empty() ? iLoaded.lods : data::geometry::buildMeshLods(iLoaded, iLods);
		const bool matches = std::ranges::all_of(lods, [this](const data::geometry::MeshLodLevel& iLevel) -> bool {
			return iLevel.submeshCorners.size() == m_submeshes.size();
		});
		if (matches) {
			for (const auto& level: lods) {
				auto& parts = m_lodParts.emplace_back();
				for (const auto& corners: level.submeshCorners) {
					parts.push_back(MeshGeometry::fromCorners(*iLoaded.mesh,
															  skinned ? std::span{iLoaded.skin}
																	  : std::span<const data::geometry::VertexSkin>{},
															  corners));
				}
				m_lodScreenSizes.push_back(level.screenSize);
			}
		} else if (!lods.empty()) {
			OWL_CORE_WARN("MeshCache: Levels of detail of {} do not match its submeshes, ignored.",
						  m_file.generic_string())
		}
	}
	m_submeshMeshes.resize(m_submeshes.size() * (1 + m_lodParts.size()));
	if (!m_fileMaterials.empty())
		m_baseColorTexture = m_fileMaterials.front()->getTexture(MaterialMap::BaseColor);
}

MeshResource::~MeshResource() = default;

auto MeshResource::getMesh(const bool iTransparent) -> const Renderer3D::MeshHandle& {
	auto& mesh = iTransparent ? m_transparent : m_opaque;
	if (!mesh && !m_geometry.isEmpty())
		mesh = Renderer3D::createMesh(m_geometry.vertices, m_geometry.indices, "mesh3d",
									  iTransparent ? Renderer3D::transparentMeshState : Renderer3D::opaqueMeshState);
	return mesh;
}

auto MeshResource::getFileMaterial(const size_t iSubmesh) const -> const shared<const Material>& {
	static const shared<const Material> s_default = mkShared<const Material>();
	if (iSubmesh >= m_submeshes.size() || m_fileMaterials.empty())
		return s_default;
	return m_fileMaterials[m_submeshes[iSubmesh].materialIndex];
}

auto MeshResource::getSubmeshMesh(const size_t iSubmesh, const bool iTransparent) -> const Renderer3D::MeshHandle& {
	return getSubmeshMesh(iSubmesh, iTransparent, 0, false);
}

auto MeshResource::getSubmeshMesh(const size_t iSubmesh, const bool iTransparent, const size_t iLod,
								  const bool iSkinned) -> const Renderer3D::MeshHandle& {
	static const Renderer3D::MeshHandle s_none;
	if (iSubmesh >= m_submeshes.size() || (iSkinned && !isSkinned()))
		return s_none;
	const size_t lod = std::min(iLod, m_lodParts.size());
	auto& mesh = m_submeshMeshes[lod * m_submeshes.size() + iSubmesh][(iTransparent ? 1 : 0) + (iSkinned ? 2 : 0)];
	if (mesh)
		return mesh;
	// The part gets its own compact vertices: a draw's vertex buffer is sized from its index count.
	MeshGeometry extracted;
	if (lod == 0) {
		const auto& part = m_submeshes[iSubmesh];
		extracted = m_geometry.extract(part.firstIndex, part.indexCount);
	}
	const MeshGeometry& geometry = lod == 0 ? extracted : m_lodParts[lod - 1][iSubmesh];
	if (geometry.isEmpty())
		return s_none;
	const auto& state = iTransparent ? Renderer3D::transparentMeshState : Renderer3D::opaqueMeshState;
	if (!iSkinned) {
		mesh = Renderer3D::createMesh(geometry.vertices, geometry.indices, "mesh3d", state);
		return mesh;
	}
	std::vector<SkinnedMesh3DVertex> vertices;
	vertices.reserve(geometry.vertices.size());
	for (size_t index = 0; index < geometry.vertices.size(); ++index) {
		const auto& skin = index < geometry.skin.size() ? geometry.skin[index] : data::geometry::VertexSkin{};
		vertices.push_back({.vertex = geometry.vertices[index],
							.joints = {skin.joints[0], skin.joints[1], skin.joints[2], skin.joints[3]},
							.weights = skin.weights});
	}
	mesh = Renderer3D::createSkinnedMesh(vertices, geometry.indices, state);
	return mesh;
}

auto MeshResource::getLodScreenSize(const size_t iLod) const -> float {
	if (iLod == 0 || iLod > m_lodScreenSizes.size())
		return 1.f;
	return m_lodScreenSizes[iLod - 1];
}

auto MeshResource::getLodTriangleCount(const size_t iLod) const -> size_t {
	if (iLod == 0)
		return m_geometry.indices.size() / 3;
	if (iLod > m_lodParts.size())
		return 0;
	size_t count = 0;
	for (const auto& part: m_lodParts[iLod - 1]) count += part.indices.size() / 3;
	return count;
}

auto MeshResource::selectLod(const float iScreenSize) const -> size_t {
	size_t lod = 0;
	for (size_t level = 0; level < m_lodScreenSizes.size(); ++level) {
		if (iScreenSize < m_lodScreenSizes[level])
			lod = level + 1;
	}
	return lod;
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
		entry.resource = mkShared<MeshResource>(loaded->first, loaded->second, lodSettingsOf(loaded->first));
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
