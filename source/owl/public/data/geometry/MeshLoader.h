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
 *  Base colour of the material of a loaded mesh (glTF `pbrMetallicRoughness`).
 */
struct OWL_API MeshMaterial {
	/// Colour factor multiplied with the base-colour texture.
	math::vec4 baseColorFactor{1.f, 1.f, 1.f, 1.f};
	/// Size of the base-colour image, `{0, 0}` without one.
	math::vec2ui baseColorSize{0, 0};
	/// RGBA8 pixels of the base-colour image, bottom row first (the engine texture convention).
	std::vector<uint8_t> baseColorPixels;
	/// Name of the base-colour image (its URI, or its name when embedded).
	std::string baseColorName;

	/**
	 * @brief
	 *  Check if the material has a base-colour image.
	 * @return True when pixels were decoded.
	 */
	[[nodiscard]] auto hasBaseColorTexture() const -> bool { return !baseColorPixels.empty(); }
};

/**
 * @brief
 *  A mesh read from a file with the material of its first primitive.
 */
struct OWL_API LoadedMesh {
	/// The geometry (its texture coordinates follow the engine convention: `v = 0` at the bottom of the image).
	shared<StaticMesh> mesh;
	/// The material of the first primitive that has one; the default material otherwise.
	MeshMaterial material;
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
