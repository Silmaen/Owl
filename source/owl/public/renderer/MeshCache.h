/**
 * @file MeshCache.h
 * @author Silmaen
 * @date 21/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "data/animation/SkeletalClip.h"
#include "data/animation/Skeleton.h"
#include "data/geometry/MeshLoader.h"
#include "math/vectors.h"
#include "renderer/Material.h"
#include "renderer/Renderer3D.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace owl::data::geometry {
class StaticMesh;
}// namespace owl::data::geometry

namespace owl::renderer {

/**
 * @brief
 *  Geometry of a static mesh in the `Renderer3D` vertex layout, with its object-space bounds.
 */
struct OWL_API MeshGeometry {
	/// Vertices, one per distinct (position, normal, UV) corner; their `textureIndex` is `1` (the base-colour slot).
	std::vector<Mesh3DVertex> vertices;
	/// Triangle indices into `vertices`.
	std::vector<uint32_t> indices;
	/// Joints and weights of each vertex of a skinned mesh, empty for a static one.
	std::vector<data::geometry::VertexSkin> skin;
	/// Smallest corner of the object-space bounding box.
	math::vec3 boundsMin{0.f, 0.f, 0.f};
	/// Largest corner of the object-space bounding box.
	math::vec3 boundsMax{0.f, 0.f, 0.f};

	/**
	 * @brief
	 *  Check if the geometry has no triangle.
	 * @return True without triangles.
	 */
	[[nodiscard]] auto isEmpty() const -> bool { return indices.empty(); }

	/**
	 * @brief
	 *  Build the geometry of a static mesh.
	 *
	 * The per-corner normals and UVs of the triangles are used when the mesh has them; a corner without a normal
	 * takes the normal of its triangle, a corner without UV gets `(0, 0)`. Identical corners share one vertex.
	 * @param[in] iMesh The mesh.
	 * @return The geometry.
	 */
	[[nodiscard]] static auto fromStaticMesh(const data::geometry::StaticMesh& iMesh) -> MeshGeometry;

	/**
	 * @brief
	 *  Build the geometry of some corners of a mesh, with the skin of their vertices.
	 * @param[in] iMesh The mesh.
	 * @param[in] iSkin Joints and weights of each vertex of the mesh; empty for a static geometry.
	 * @param[in] iCorners The corners (`triangle * 3 + corner`), three per triangle; empty for every corner.
	 * @return The geometry.
	 */
	[[nodiscard]] static auto fromCorners(const data::geometry::StaticMesh& iMesh,
										  std::span<const data::geometry::VertexSkin> iSkin,
										  std::span<const uint32_t> iCorners) -> MeshGeometry;

	/**
	 * @brief
	 *  Extract a run of indices with its own compact vertices (and skin).
	 * @param[in] iFirstIndex First index of the run.
	 * @param[in] iIndexCount Number of indices.
	 * @return The geometry of the run (bounds copied from this one).
	 */
	[[nodiscard]] auto extract(uint32_t iFirstIndex, uint32_t iIndexCount) const -> MeshGeometry;

	/**
	 * @brief
	 *  Intersect a ray with the triangles, both faces counting.
	 * @param[in] iOrigin Origin of the ray, in object space.
	 * @param[in] iDirection Direction of the ray, in object space (need not be normalized).
	 * @return The nearest hit point in object space, or nothing when the ray misses.
	 */
	[[nodiscard]] auto raycast(const math::vec3& iOrigin, const math::vec3& iDirection) const
			-> std::optional<math::vec3>;
};

/**
 * @brief
 *  A static mesh file loaded once: its geometry, its GPU meshes and its base-colour material.
 *
 * Shared by every entity drawing the same file. The opaque and the transparent GPU meshes are uploaded on first
 * request (`getMesh`), before the layers draw: an upload closes the render pass.
 */
class OWL_API MeshResource final {
public:
	/**
	 * @brief
	 *  Build the resource of a loaded mesh, uploading its base-colour image when it has one.
	 * @param[in] iFile The file the mesh was read from (empty for an asset pack entry).
	 * @param[in] iLoaded The loaded mesh and material.
	 * @param[in] iLods Levels of detail built when the loaded mesh has none (an uncooked file).
	 */
	MeshResource(std::filesystem::path iFile, const data::geometry::LoadedMesh& iLoaded,
				 const data::geometry::LodSettings& iLods = {});

	/**
	 * @brief
	 *  Destroy the resource and its GPU meshes.
	 */
	~MeshResource();

	MeshResource(const MeshResource&) = delete;

	MeshResource(MeshResource&&) = delete;

	auto operator=(const MeshResource&) -> MeshResource& = delete;

	auto operator=(MeshResource&&) -> MeshResource& = delete;

	/**
	 * @brief
	 *  Get the geometry.
	 * @return The geometry.
	 */
	[[nodiscard]] auto getGeometry() const -> const MeshGeometry& { return m_geometry; }

	/**
	 * @brief
	 *  Get the file the mesh was read from.
	 * @return The file, empty for an asset pack entry.
	 */
	[[nodiscard]] auto getFile() const -> const std::filesystem::path& { return m_file; }

	/**
	 * @brief
	 *  Get the base-colour texture of the material.
	 * @return The texture, null when the material has none.
	 */
	[[nodiscard]] auto getBaseColorTexture() const -> const shared<gpu::Texture2D>& { return m_baseColorTexture; }

	/**
	 * @brief
	 *  Get the base-colour factor of the material.
	 * @return The factor (white for a file without material).
	 */
	[[nodiscard]] auto getBaseColorFactor() const -> const math::vec4& { return m_baseColorFactor; }

	/**
	 * @brief
	 *  Get the GPU mesh drawn with the opaque or the blended pipeline state, uploading it on the first call.
	 * @param[in] iTransparent True for the blended mesh (`Renderer3D::transparentMeshState`).
	 * @return The mesh handle, null for an empty geometry.
	 */
	[[nodiscard]] auto getMesh(bool iTransparent) -> const Renderer3D::MeshHandle&;

	/**
	 * @brief
	 *  A part of the mesh drawn with one material (a glTF primitive, an OBJ material group).
	 */
	struct Submesh {
		/// First index of the part in `getGeometry().indices`.
		uint32_t firstIndex = 0;
		/// Number of indices of the part.
		uint32_t indexCount = 0;
		/// Index of its material in the file's materials.
		uint32_t materialIndex = 0;
	};

	/**
	 * @brief
	 *  The parts of the mesh, one material slot each.
	 * @return The submeshes (at least one for a mesh with triangles).
	 */
	[[nodiscard]] auto getSubmeshes() const -> const std::vector<Submesh>& { return m_submeshes; }

	/**
	 * @brief
	 *  The material the file gives a submesh (its glTF or OBJ material).
	 * @param[in] iSubmesh Index of the submesh.
	 * @return The material.
	 */
	[[nodiscard]] auto getFileMaterial(size_t iSubmesh) const -> const shared<const Material>&;

	/**
	 * @brief
	 *  The GPU mesh of one submesh, created on its first request.
	 * @param[in] iSubmesh Index of the submesh.
	 * @param[in] iTransparent True for the blended pipeline state.
	 * @return The mesh, null for an empty submesh.
	 */
	[[nodiscard]] auto getSubmeshMesh(size_t iSubmesh, bool iTransparent) -> const Renderer3D::MeshHandle&;

	/**
	 * @brief
	 *  Get the GPU mesh of one submesh at a level of detail, static or skinned, uploading it on the first call.
	 * @param[in] iSubmesh Index of the submesh.
	 * @param[in] iTransparent True for the blended pipeline state.
	 * @param[in] iLod Level of detail (0 is the full mesh), clamped to the levels of the mesh.
	 * @param[in] iSkinned True for the `skinned3d` mesh (a skinned resource only).
	 * @return The mesh handle, null for an invalid submesh or an empty part.
	 */
	[[nodiscard]] auto getSubmeshMesh(size_t iSubmesh, bool iTransparent, size_t iLod, bool iSkinned)
			-> const Renderer3D::MeshHandle&;

	/**
	 * @brief
	 *  Check if the mesh has a skeleton and skinned vertices.
	 * @return True for a skinned mesh.
	 */
	[[nodiscard]] auto isSkinned() const -> bool { return m_skeleton != nullptr && !m_geometry.skin.empty(); }

	/**
	 * @brief
	 *  Get the skeleton.
	 * @return The skeleton, null for a static mesh.
	 */
	[[nodiscard]] auto getSkeleton() const -> const shared<const data::animation::Skeleton>& { return m_skeleton; }

	/**
	 * @brief
	 *  Get the animations of the file.
	 * @return The clips.
	 */
	[[nodiscard]] auto getClips() const -> const std::vector<data::animation::SkeletalClip>& { return m_clips; }

	/**
	 * @brief
	 *  Get the sockets of the file.
	 * @return The sockets.
	 */
	[[nodiscard]] auto getSockets() const -> const std::vector<data::geometry::MeshSocket>& { return m_sockets; }

	/**
	 * @brief
	 *  Get the number of levels of detail besides the full mesh.
	 * @return The level count.
	 */
	[[nodiscard]] auto getLodCount() const -> size_t { return m_lodScreenSizes.size(); }

	/**
	 * @brief
	 *  Get the screen size under which a level is drawn.
	 * @param[in] iLod The level (1 for the first reduced one).
	 * @return The fraction of the screen height, 1 for level 0 or a level out of range.
	 */
	[[nodiscard]] auto getLodScreenSize(size_t iLod) const -> float;

	/**
	 * @brief
	 *  Get the number of triangles of a level.
	 * @param[in] iLod The level (0 is the full mesh).
	 * @return The triangle count, 0 for a level out of range.
	 */
	[[nodiscard]] auto getLodTriangleCount(size_t iLod) const -> size_t;

	/**
	 * @brief
	 *  Pick the level drawn at a screen size: the coarsest one whose screen size is above it.
	 * @param[in] iScreenSize Fraction of the screen height the mesh covers.
	 * @return The level, 0 for the full mesh.
	 */
	[[nodiscard]] auto selectLod(float iScreenSize) const -> size_t;

	/**
	 * @brief
	 *  Check if the file changed since the resource was loaded (the holders fetch a new one from the cache).
	 * @return True once invalidated.
	 */
	[[nodiscard]] auto isStale() const -> bool { return m_stale; }

	/**
	 * @brief
	 *  Mark the resource as outdated.
	 */
	void markStale() { m_stale = true; }

private:
	/// File the mesh was read from.
	std::filesystem::path m_file;
	/// Geometry in the renderer layout.
	MeshGeometry m_geometry;
	/// Base-colour texture of the material, null without one.
	shared<gpu::Texture2D> m_baseColorTexture;
	/// Base-colour factor of the material.
	math::vec4 m_baseColorFactor{1.f, 1.f, 1.f, 1.f};
	/// Parts of the mesh.
	std::vector<Submesh> m_submeshes;
	/// Materials of the file, by its material index.
	std::vector<shared<const Material>> m_fileMaterials;
	/// GPU meshes of the submeshes per level (level * submesh count + submesh): opaque, blended, then skinned ones.
	std::vector<std::array<Renderer3D::MeshHandle, 4>> m_submeshMeshes;
	/// Compact geometry of every submesh of every reduced level (level - 1, then submesh).
	std::vector<std::vector<MeshGeometry>> m_lodParts;
	/// Screen size of every reduced level.
	std::vector<float> m_lodScreenSizes;
	/// The skeleton of a skinned mesh.
	shared<const data::animation::Skeleton> m_skeleton;
	/// The animations of the file.
	std::vector<data::animation::SkeletalClip> m_clips;
	/// The sockets of the file.
	std::vector<data::geometry::MeshSocket> m_sockets;
	/// GPU mesh with the opaque pipeline state.
	Renderer3D::MeshHandle m_opaque;
	/// GPU mesh with the blended pipeline state.
	Renderer3D::MeshHandle m_transparent;
	/// True once the file changed.
	bool m_stale = false;
};

/**
 * @brief
 *  The static meshes of one engine context, keyed by asset path and shared between the entities drawing them.
 *
 * Owned by `app::EngineContext`. A path is looked up in the open asset pack, then in the asset directories of the
 * application, then as given; a file that cannot be read is remembered as missing until it is invalidated. Release
 * the cache before the renderer shuts down: it holds GPU buffers.
 */
class OWL_API MeshCache final {
public:
	/**
	 * @brief
	 *  Create an empty cache.
	 */
	MeshCache();

	/**
	 * @brief
	 *  Destroy the cache and the meshes nobody else holds.
	 */
	~MeshCache();

	MeshCache(const MeshCache&) = delete;

	MeshCache(MeshCache&&) = delete;

	auto operator=(const MeshCache&) -> MeshCache& = delete;

	auto operator=(MeshCache&&) -> MeshCache& = delete;

	/**
	 * @brief
	 *  Get the mesh of an asset path, loading it on the first request.
	 * @param[in] iAssetPath Path of an `.obj`, `.gltf` or `.glb` file, relative to the asset directories or absolute.
	 * @return The shared resource, null when the file cannot be read.
	 */
	[[nodiscard]] auto get(const std::filesystem::path& iAssetPath) -> shared<MeshResource>;

	/**
	 * @brief
	 *  Get the mesh of an asset path if it is loaded already.
	 * @param[in] iAssetPath The asset path.
	 * @return The resource, null when it is not loaded (or failed to load).
	 */
	[[nodiscard]] auto find(const std::filesystem::path& iAssetPath) const -> shared<MeshResource>;

	/**
	 * @brief
	 *  Forget the meshes read from a file that changed; their holders reload them on their next draw.
	 * @param[in] iFile The changed file (absolute, as the hot reload reports it, or an asset path).
	 * @return The number of entries dropped.
	 */
	auto invalidate(const std::filesystem::path& iFile) -> uint32_t;

	/**
	 * @brief
	 *  Forget every mesh.
	 */
	void clear();

	/**
	 * @brief
	 *  Get the number of asset paths looked up so far, the missing files included.
	 * @return The number of entries.
	 */
	[[nodiscard]] auto size() const -> size_t { return m_entries.size(); }

private:
	/// One looked-up asset path.
	struct Entry {
		/// The mesh, null when the file could not be read.
		shared<MeshResource> resource;
	};
	/// Entries keyed by the generic form of their asset path.
	std::unordered_map<std::string, Entry> m_entries;
};

}// namespace owl::renderer
