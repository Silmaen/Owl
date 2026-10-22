/**
 * @file MeshLoader.h
 * @author Silmaen
 * @date 26/10/2025
 * Copyright (c) 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "data/geometry/StaticMesh.h"
#include "math/vectors.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace owl::data::geometry {

/**
 * @brief
 *  A decoded image of a material texture.
 */
struct OWL_API MeshImage {
	/// Size of the image, `{0, 0}` without one.
	math::vec2ui size{0, 0};
	/// RGBA8 pixels, bottom row first (the engine texture convention).
	std::vector<uint8_t> pixels;
	/// Name of the image (its URI, or its name when embedded).
	std::string name;

	/**
	 * @brief
	 *  Check if the image holds no pixel.
	 * @return True when nothing was decoded.
	 */
	[[nodiscard]] auto isEmpty() const -> bool { return pixels.empty(); }
};

/**
 * @brief
 *  Material of a mesh primitive (glTF metallic-roughness model and its common extensions).
 *
 * Images keep the glTF channel conventions: metallic in blue, roughness in green, occlusion in red.
 */
struct OWL_API MeshMaterial {
	/// How the alpha of the base colour is used.
	enum struct AlphaMode : uint8_t {
		Opaque,///< Alpha ignored.
		Mask,///< Fragments under the cutoff are discarded.
		Blend///< Alpha blended with the background.
	};

	/// Name of the material, empty when the file gives none.
	std::string name;
	/// Colour factor multiplied with the base-colour image.
	math::vec4 baseColorFactor{1.f, 1.f, 1.f, 1.f};
	/// Metallic factor multiplied with the metallic channel.
	float metallicFactor = 1.f;
	/// Roughness factor multiplied with the roughness channel.
	float roughnessFactor = 1.f;
	/// Emitted colour multiplied with the emissive image.
	math::vec3 emissiveFactor{0.f, 0.f, 0.f};
	/// Emissive intensity (`KHR_materials_emissive_strength`).
	float emissiveStrength = 1.f;
	/// Scale of the normal-map perturbation.
	float normalScale = 1.f;
	/// Strength of the occlusion image.
	float occlusionStrength = 1.f;
	/// Use of the base-colour alpha.
	AlphaMode alphaMode = AlphaMode::Opaque;
	/// Alpha under which a fragment is discarded in `Mask` mode.
	float alphaCutoff = 0.5f;
	/// True when both faces of a triangle are drawn.
	bool doubleSided = false;
	/// Clear-coat layer intensity (`KHR_materials_clearcoat`), 0 without one.
	float clearcoat = 0.f;
	/// Clear-coat layer roughness.
	float clearcoatRoughness = 0.f;
	/// Sheen colour (`KHR_materials_sheen`), black without one.
	math::vec3 sheenColor{0.f, 0.f, 0.f};
	/// Sheen roughness.
	float sheenRoughness = 0.f;
	/// Anisotropy strength (`KHR_materials_anisotropy`), 0 without one.
	float anisotropy = 0.f;
	/// Anisotropy direction, in radians.
	float anisotropyRotation = 0.f;
	/// Base-colour image.
	MeshImage baseColor;
	/// Metallic (blue) and roughness (green) image.
	MeshImage metallicRoughness;
	/// Tangent-space normal image.
	MeshImage normal;
	/// Occlusion (red) image.
	MeshImage occlusion;
	/// Emissive image.
	MeshImage emissive;

	/**
	 * @brief
	 *  Check if the material has a base-colour image.
	 * @return True when pixels were decoded.
	 */
	[[nodiscard]] auto hasBaseColorTexture() const -> bool { return !baseColor.isEmpty(); }
};

/**
 * @brief
 *  A run of the indices of a mesh drawn with one material.
 */
struct OWL_API MeshSubmesh {
	/// Position of the first index of the run in the merged index buffer (a multiple of 3).
	uint32_t firstIndex = 0;
	/// Number of indices of the run (a multiple of 3).
	uint32_t indexCount = 0;
	/// Position of the material in `LoadedMesh::materials`.
	uint32_t materialIndex = 0;
};

/**
 * @brief
 *  A mesh read from a file with its materials and the submeshes using them.
 */
struct OWL_API LoadedMesh {
	/// The geometry (its texture coordinates follow the engine convention: `v = 0` at the bottom of the image).
	shared<StaticMesh> mesh;
	/// The materials of the file; the default material alone when the file has none.
	std::vector<MeshMaterial> materials{MeshMaterial{}};
	/// One run of indices per glTF primitive or OBJ material group, contiguous in the merged index buffer.
	std::vector<MeshSubmesh> submeshes;
};

/// Reads a file a mesh names (the external buffers and images of a `.gltf`) by its path; nothing when it is missing.
using MeshFileReader = std::function<std::optional<std::vector<uint8_t>>(const std::string& iPath)>;

/**
 * @brief
 *  Loads static meshes from OBJ, glTF and FBX files.
 */
class OWL_API MeshLoader {
public:
	/**
	 * @brief
	 *  Default constructor.
	 */
	MeshLoader() = delete;

	/**
	 * @brief
	 *  Default destructor.
	 */
	~MeshLoader() = delete;

	MeshLoader(const MeshLoader&) = delete;

	MeshLoader(MeshLoader&&) = delete;

	auto operator=(const MeshLoader&) -> MeshLoader& = delete;

	auto operator=(MeshLoader&&) -> MeshLoader& = delete;

	/**
	 * @brief
	 *  Get the supported file extensions.
	 * @return The supported file extensions.
	 */
	[[nodiscard]] static auto getSupportedExtensions() -> std::vector<std::string> {
		return {".obj", ".fbx", ".gltf", ".glb"};
	}

	/**
	 * @brief
	 *  Get the extensions `loadMesh` reads (FBX is left out: its loader is not complete).
	 * @return The extensions, with their dot.
	 */
	[[nodiscard]] static auto getMeshAssetExtensions() -> std::vector<std::string> { return {".obj", ".gltf", ".glb"}; }

	/**
	 * @brief
	 *  Check if a file is a mesh `loadMesh` reads, by its extension.
	 * @param[in] iFilePath The file path.
	 * @return True for `.obj`, `.gltf` and `.glb`.
	 */
	[[nodiscard]] static auto isMeshAsset(const std::filesystem::path& iFilePath) -> bool;

	/**
	 * @brief
	 *  Load a mesh and its material from an OBJ, glTF or GLB file.
	 * @param[in] iFilePath Path to the mesh file.
	 * @return The mesh, or a null mesh when the file cannot be read (logged).
	 */
	[[nodiscard]] static auto loadMesh(const std::filesystem::path& iFilePath) -> LoadedMesh;

	/**
	 * @brief
	 *  Load a mesh and its material from the bytes of an OBJ, glTF or GLB file (an asset pack entry).
	 *
	 * The external files a `.gltf` names (buffers, images) are looked up in `iBaseDirectory`, on disk or through
	 * `iReadFile` when one is given.
	 * @param[in] iData The file content.
	 * @param[in] iExtension The extension of the file, with its dot.
	 * @param[in] iName Name of the file, for the logs.
	 * @param[in] iBaseDirectory Folder of the external files of a `.gltf`.
	 * @param[in] iReadFile Reader of the external files (an asset pack); empty to read them from the disk.
	 * @return The mesh, or a null mesh when the bytes cannot be read (logged).
	 */
	[[nodiscard]] static auto loadMeshFromMemory(std::span<const uint8_t> iData, const std::string& iExtension,
												 const std::string& iName,
												 const std::filesystem::path& iBaseDirectory = {},
												 const MeshFileReader& iReadFile = {}) -> LoadedMesh;

	/**
	 * @brief
	 *  List the external files a `.gltf` names (buffer and image URIs that are not embedded `data:` URIs).
	 * @param[in] iFilePath Path to the `.gltf` file.
	 * @return The URIs, relative to the folder of the file; empty for another format or an unreadable file.
	 */
	[[nodiscard]] static auto getExternalFiles(const std::filesystem::path& iFilePath) -> std::vector<std::string>;

	/**
	 * @brief
	 *  Load a static mesh from a file.
	 * @param[in] iFilePath Path to the mesh file.
	 * @return The loaded static mesh or nullptr if loading failed.
	 */
	[[nodiscard]] static auto loadStaticMesh(const std::filesystem::path& iFilePath) -> shared<StaticMesh> {
		if (!std::filesystem::exists(iFilePath)) {
			OWL_CORE_ERROR("MeshLoader::loadStaticMesh: File {} does not exist!", iFilePath.string())
			return nullptr;
		}
		const auto extension = iFilePath.extension().string();
		if (extension == ".obj") {
			return loadObj(iFilePath);
		}
		if (extension == ".fbx") {
			return loadFbx(iFilePath);
		}
		if (extension == ".gltf" || extension == ".glb") {
			return loadGltf(iFilePath);
		}
		OWL_CORE_ERROR("MeshLoader::loadStaticMesh: Unsupported file extension {}!", extension)
		return nullptr;
	}

private:
	/**
	 * @brief
	 *  Load a static mesh from an OBJ file.
	 * @param[in] iFilePath Path to the OBJ file.
	 * @return The loaded static mesh.
	 */
	static auto loadObj(const std::filesystem::path& iFilePath) -> shared<StaticMesh>;

	/**
	 * @brief
	 *  Load a static mesh from an FBX file.
	 * @param[in] iFilePath Path to the FBX file.
	 * @return The loaded static mesh.
	 */
	static auto loadFbx(const std::filesystem::path& iFilePath) -> shared<StaticMesh>;

	/**
	 * @brief
	 *  Load a static mesh from a GLTF/GLB file.
	 * @param[in] iFilePath Path to the GLTF/GLB file.
	 * @return The loaded static mesh.
	 */
	static auto loadGltf(const std::filesystem::path& iFilePath) -> shared<StaticMesh>;
};

}// namespace owl::data::geometry

namespace owl::data {
/// Former location of the loader, kept for the code written before it joined `data::geometry`.
using geometry::MeshLoader;
}// namespace owl::data
