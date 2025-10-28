/**
 * @file MeshLoader.h
 * @author Silmaen
 * @date 26/10/2025
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "data/geometry/StaticMesh.h"

namespace owl::data {

/**
 * @brief Class MeshLoader.
 */
class OWL_API MeshLoader {
public:
	/**
	 * @brief Default constructor.
	 */
	MeshLoader() = delete;
	/**
	 * @brief Default destructor.
	 */
	~MeshLoader() = delete;

	MeshLoader(const MeshLoader&) = delete;
	MeshLoader(MeshLoader&&) = delete;
	auto operator=(const MeshLoader&) -> MeshLoader& = delete;
	auto operator=(MeshLoader&&) -> MeshLoader& = delete;

	/**
	 * @brief Get the supported file extensions.
	 * @return The supported file extensions.
	 */
	[[nodiscard]] static auto getSupportedExtensions() -> std::vector<std::string> {
		return {".obj", ".fbx", ".gltf", ".glb"};
	}

	/**
	 * @brief Load a static mesh from a file.
	 * @param[in] iFilePath Path to the mesh file.
	 * @return The loaded static mesh or nullptr if loading failed.
	 */
	[[nodiscard]] static auto loadStaticMesh(const std::filesystem::path& iFilePath) -> shared<geometry::StaticMesh> {
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
	 * @brief Load a static mesh from an OBJ file.
	 * @param[in] iFilePath Path to the OBJ file.
	 * @return The loaded static mesh.
	 */
	static auto loadObj(const std::filesystem::path& iFilePath) -> shared<geometry::StaticMesh>;
	/**
	 * @brief Load a static mesh from an FBX file.
	 * @param[in] iFilePath Path to the FBX file.
	 * @return The loaded static mesh.
	 */
	static auto loadFbx(const std::filesystem::path& iFilePath) -> shared<geometry::StaticMesh>;
	/**
	 * @brief Load a static mesh from a GLTF/GLB file.
	 * @param[in] iFilePath Path to the GLTF/GLB file.
	 * @return The loaded static mesh.
	 */
	static auto loadGltf(const std::filesystem::path& iFilePath) -> shared<geometry::StaticMesh>;
};

}// namespace owl::data
