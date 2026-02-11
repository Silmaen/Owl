/**
 * @file MeshLoader.cpp
 * @author Silmaen
 * @date 26/10/2025
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "data/geometry/MeshLoader.h"
#include "data/geometry/MeshRange.h"
#include "data/geometry/StaticMesh.h"
#include "data/geometry/extradata/TriangleNormals.h"
#include "data/geometry/extradata/TriangleUVCoordinate.h"

// TinyGLTF + implementation
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>
#undef STB_IMAGE_WRITE_IMPLEMENTATION
#undef TINYGLTF_IMPLEMENTATION

#include <tiny_obj_loader.h>
#include <ufbx.h>

using namespace owl::core;
using namespace owl::math;
using namespace owl::data::geometry;
using namespace owl::data::geometry::extradata;

namespace owl::data {

auto MeshLoader::loadObj([[maybe_unused]] const std::filesystem::path& iFilePath) -> shared<StaticMesh> {
	shared<StaticMesh> mesh = mkShared<StaticMesh>();
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warn;
	std::string err;

	if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, iFilePath.string().c_str())) {
		OWL_CORE_ERROR("MeshLoader::loadObj: Failed to load {}: {}", iFilePath.string(), err)
		return mesh;
	}
	if (!warn.empty()) {
		OWL_CORE_WARN("MeshLoader::loadObj: {}", warn)
	}
	// Reserve space for vertices
	mesh->reserveVertices(attrib.vertices.size() / 3);
	// insert all vertices
	uint8_t coordIdx = 0;
	vec3 position;
	for (const float coord: attrib.vertices) {
		position[coordIdx] = coord;
		if (coordIdx == 2) {
			coordIdx = 0;
			mesh->addVertex(position);
		} else {
			coordIdx++;
		}
	}
	std::vector<std::array<vec2, 3>> uvCoords;
	std::vector<std::array<vec3, 3>> normals;
	const bool hasUv = !attrib.texcoords.empty();
	const bool hasNormals = !attrib.normals.empty();
	// Estimate triangle count for reserve
	size_t estimatedTriangles = 0;
	for (const auto& shape: shapes) { estimatedTriangles += shape.mesh.num_face_vertices.size(); }
	mesh->reserveTriangles(estimatedTriangles);
	uvCoords.reserve(estimatedTriangles);
	normals.reserve(estimatedTriangles);
	// insert all triangles
	for (const auto& shape: shapes) {
		size_t indexOffset = 0;
		for (size_t fv: shape.mesh.num_face_vertices) {
			if (fv != 3) {
				OWL_CORE_WARN(
						"MeshLoader::loadObj: Face with {} vertices found, only triangles are supported, skipping.", fv)
				indexOffset += fv;
				continue;
			}
			std::array<uint32_t, 3> vertexIndices{};
			for (size_t v = 0; v < fv; v++) {
				const tinyobj::index_t idx = shape.mesh.indices[indexOffset + v];
				vertexIndices[v] = static_cast<uint32_t>(idx.vertex_index);
			}
			std::array<vec2, 3> faceUv{};
			std::array<vec3, 3> faceNormals{};
			for (size_t v = 0; v < fv; v++) {
				const tinyobj::index_t idx = shape.mesh.indices[indexOffset + v];
				if (idx.texcoord_index >= 0) {
					faceUv[v].x() = attrib.texcoords[2u * static_cast<uint32_t>(idx.texcoord_index) + 0u];
					faceUv[v].y() = attrib.texcoords[2u * static_cast<uint32_t>(idx.texcoord_index) + 1u];
				} else {
					faceUv[v] = vec2(0.0f, 0.0f);
				}
				if (idx.normal_index >= 0) {
					faceNormals[v].x() = attrib.normals[3u * static_cast<uint32_t>(idx.normal_index) + 0u];
					faceNormals[v].y() = attrib.normals[3u * static_cast<uint32_t>(idx.normal_index) + 1u];
					faceNormals[v].z() = attrib.normals[3u * static_cast<uint32_t>(idx.normal_index) + 2u];
				}
			}
			if (hasUv)
				uvCoords.push_back(faceUv);
			if (hasNormals)
				normals.push_back(faceNormals);
			mesh->addTriangle(vertexIndices);
			indexOffset += fv;
		}
	}

	if (hasUv)
		mesh->addTriangleExtraData<TriangleUVCoordinate>();
	if (hasNormals)
		mesh->addTriangleExtraData<TriangleNormals>();
	// Fill UVs and normals
	size_t triIndex = 0;
	for (auto& [uv, m_normals]: MeshTriangleRange(*mesh, component::WriteUvCoords, component::WriteTriangleNormals)) {
		if (uv.hasValue()) {
			uv.value()->setUvCoord(0, uvCoords[triIndex][0]);
			uv.value()->setUvCoord(1, uvCoords[triIndex][1]);
		}
		if (m_normals.hasValue()) {
			m_normals.value()->setNormal(0, normals[triIndex][0]);
			m_normals.value()->setNormal(1, normals[triIndex][1]);
			m_normals.value()->setNormal(2, normals[triIndex][2]);
		}
		++triIndex;
	}
	OWL_CORE_INFO("MeshLoader::loadObj: Loaded mesh {} with {} vertices and {} triangles.", iFilePath.string(),
				  mesh->getVertexCount(), mesh->getTriangleCount())
	OWL_CORE_INFO("MeshLoader::loadObj: Mesh has {} UV coordinates and {} normals.",
				  !hasUv || mesh->isExtraDataDefinedOnAllTriangles<TriangleUVCoordinate>() ? "complete" : "incomplete",
				  !hasNormals || mesh->isExtraDataDefinedOnAllTriangles<TriangleNormals>() ? "complete" : "incomplete")
	OWL_CORE_INFO("MeshLoader::loadObj: All other mesh data are ignored.")
	return mesh;
}

auto MeshLoader::loadFbx([[maybe_unused]] const std::filesystem::path& iFilePath) -> shared<StaticMesh> {
	OWL_CORE_WARN("MeshLoader::loadFbx: FBX loading not implemented yet!")
	shared<StaticMesh> mesh = mkShared<StaticMesh>();

	constexpr ufbx_load_opts opts = {};
	ufbx_error error;
	ufbx_scene* scene = ufbx_load_file(iFilePath.string().c_str(), &opts, &error);
	if (scene == nullptr) {
		OWL_CORE_ERROR("MeshLoader::loadFbx: Failed to load {}: {}", iFilePath.string(), error.description.data)
		return mesh;
	}

	std::vector<std::array<vec2, 3>> uvCoords;
	std::vector<std::array<vec3, 3>> normals;
	bool hasUv = false;
	bool hasNormals = false;

	// Reserve space based on total counts across all meshes
	size_t totalVertices = 0;
	size_t totalFaces = 0;
	for (size_t i = 0; i < scene->meshes.count; ++i) {
		totalVertices += scene->meshes.data[i]->num_vertices;
		totalFaces += scene->meshes.data[i]->num_faces;
	}
	mesh->reserveVertices(totalVertices);
	mesh->reserveTriangles(totalFaces);
	uvCoords.reserve(totalFaces);
	normals.reserve(totalFaces);

	for (size_t meshIdx = 0; meshIdx < scene->meshes.count; ++meshIdx) {
		const ufbx_mesh* fbxMesh = scene->meshes.data[meshIdx];

		// Add vertices
		const size_t vertexOffset = mesh->getVertexCount();
		for (size_t i = 0; i < fbxMesh->num_vertices; ++i) {
			const ufbx_vec3& pos = fbxMesh->vertices.data[i];
			mesh->addVertex(vec3(static_cast<float>(pos.x), static_cast<float>(pos.y), static_cast<float>(pos.z)));
		}

		// Check for UVs and normals
		const bool meshHasUv = fbxMesh->uv_sets.count > 0;
		const bool meshHasNormals = fbxMesh->vertex_normal.exists && !fbxMesh->generated_normals;
		hasUv = hasUv || meshHasUv;
		hasNormals = hasNormals || meshHasNormals;

		// Process faces
		for (size_t faceIdx = 0; faceIdx < fbxMesh->num_faces; ++faceIdx) {
			const auto& [index_begin, num_indices] = fbxMesh->faces.data[faceIdx];
			if (num_indices != 3) {
				OWL_CORE_WARN(
						"MeshLoader::loadFbx: Face with {} vertices found, only triangles are supported, skipping.",
						num_indices)
				continue;
			}

			std::array<uint32_t, 3> vertexIndices{};
			std::array<vec2, 3> faceUv{};
			std::array<vec3, 3> faceNormals{};

			for (size_t cornerIdx = 0; cornerIdx < 3; ++cornerIdx) {
				const size_t index = index_begin + cornerIdx;
				vertexIndices[cornerIdx] = static_cast<uint32_t>(vertexOffset + fbxMesh->vertex_indices.data[index]);

				// UVs
				if (meshHasUv) {
					const ufbx_vertex_vec2& uvSet = fbxMesh->uv_sets.data[0].vertex_uv;
					const auto uvIndex = static_cast<size_t>(uvSet.indices.data[index]);
					const ufbx_vec2& uv = uvSet.values.data[uvIndex];
					faceUv[cornerIdx] = vec2(static_cast<float>(uv.x), static_cast<float>(uv.y));
				} else {
					faceUv[cornerIdx] = vec2(0.0f, 0.0f);
				}

				// Normals
				if (meshHasNormals) {
					const size_t normalIndex = fbxMesh->vertex_normal.indices.data[index];
					const ufbx_vec3& normal = fbxMesh->vertex_normal.values.data[normalIndex];
					faceNormals[cornerIdx] = vec3(static_cast<float>(normal.x), static_cast<float>(normal.y),
												  static_cast<float>(normal.z));
				} else {
					faceNormals[cornerIdx] = vec3(0.0f, 0.0f, 0.0f);
				}
			}

			if (hasUv)
				uvCoords.push_back(faceUv);
			if (hasNormals)
				normals.push_back(faceNormals);

			mesh->addTriangle(vertexIndices);
		}
	}

	ufbx_free_scene(scene);

	if (hasUv)
		mesh->addTriangleExtraData<TriangleUVCoordinate>();
	if (hasNormals)
		mesh->addTriangleExtraData<TriangleNormals>();

	// Fill UVs and normals
	size_t triIndex = 0;
	for (auto& [uv, m_normals]: MeshTriangleRange(*mesh, component::WriteUvCoords, component::WriteTriangleNormals)) {
		if (uv.hasValue()) {
			uv.value()->setUvCoord(0, uvCoords[triIndex][0]);
			uv.value()->setUvCoord(1, uvCoords[triIndex][1]);
			uv.value()->setUvCoord(2, uvCoords[triIndex][2]);
		}
		if (m_normals.hasValue()) {
			m_normals.value()->setNormal(0, normals[triIndex][0]);
			m_normals.value()->setNormal(1, normals[triIndex][1]);
			m_normals.value()->setNormal(2, normals[triIndex][2]);
		}
		++triIndex;
	}

	OWL_CORE_INFO("MeshLoader::loadFbx: Loaded mesh {} with {} vertices and {} triangles.", iFilePath.string(),
				  mesh->getVertexCount(), mesh->getTriangleCount())
	OWL_CORE_INFO("MeshLoader::loadFbx: Mesh has {} UV coordinates and {} normals.",
				  !hasUv || mesh->isExtraDataDefinedOnAllTriangles<TriangleUVCoordinate>() ? "complete" : "incomplete",
				  !hasNormals || mesh->isExtraDataDefinedOnAllTriangles<TriangleNormals>() ? "complete" : "incomplete")
	OWL_CORE_INFO("MeshLoader::loadFbx: All other mesh data are ignored.")


	return mesh;
}

auto MeshLoader::loadGltf([[maybe_unused]] const std::filesystem::path& iFilePath) -> shared<StaticMesh> {
	shared<StaticMesh> mesh = mkShared<StaticMesh>();

	using size_type = std::vector<tinygltf::Accessor>::size_type;
	tinygltf::Model gltfModel;
	tinygltf::TinyGLTF gltfLoader;
	std::string warn;
	std::string err;
	const bool isBinary = iFilePath.extension() == ".glb";
	bool loaded = false;
	if (isBinary)
		loaded = gltfLoader.LoadBinaryFromFile(&gltfModel, &err, &warn, iFilePath.string());
	else
		loaded = gltfLoader.LoadASCIIFromFile(&gltfModel, &err, &warn, iFilePath.string());
	if (!loaded) {
		OWL_CORE_ERROR("MeshLoader::loadGltf: Failed to load {}: {}", iFilePath.string(), err)
		return mesh;
	}
	if (!warn.empty()) {
		OWL_CORE_WARN("MeshLoader::loadGltf: {}", warn)
	}
	if (gltfModel.meshes.empty()) {
		OWL_CORE_ERROR("MeshLoader::loadGltf: No mesh found in {}", iFilePath.string())
		return mesh;
	}
	std::vector<std::array<vec2, 3>> uvCoords;
	std::vector<std::array<vec3, 3>> normals;
	bool hasUv = false;
	bool hasNormals = false;

	// Reserve space based on primitive counts
	{
		size_t totalVertices = 0;
		size_t totalIndices = 0;
		for (const auto& primitive: gltfModel.meshes.at(0).primitives) {
			if (const auto posIt = primitive.attributes.find("POSITION"); posIt != primitive.attributes.end())
				totalVertices += gltfModel.accessors.at(static_cast<size_type>(posIt->second)).count;
			if (primitive.indices >= 0)
				totalIndices += gltfModel.accessors.at(static_cast<size_type>(primitive.indices)).count;
		}
		mesh->reserveVertices(totalVertices);
		mesh->reserveTriangles(totalIndices / 3);
		uvCoords.reserve(totalIndices / 3);
		normals.reserve(totalIndices / 3);
	}

	for (const tinygltf::Mesh& gltfMesh = gltfModel.meshes.at(0); const auto& primitive: gltfMesh.primitives) {
		// Positions
		const auto posIt = primitive.attributes.find("POSITION");
		if (posIt == primitive.attributes.end()) {
			OWL_CORE_ERROR("MeshLoader::loadGltf: No POSITION attribute in mesh")
			continue;
		}
		const auto posAccessorIdx = static_cast<size_type>(posIt->second);
		const tinygltf::Accessor& posAccessor = gltfModel.accessors.at(posAccessorIdx);
		const tinygltf::BufferView& posBufferView =
				gltfModel.bufferViews.at(static_cast<size_type>(posAccessor.bufferView));
		const tinygltf::Buffer& posBuffer = gltfModel.buffers.at(static_cast<size_type>(posBufferView.buffer));
		const auto* positions =
				reinterpret_cast<const float*>(&posBuffer.data.at(posBufferView.byteOffset + posAccessor.byteOffset));
		for (size_t i = 0; i < posAccessor.count; ++i) {
			const vec3 position{positions[3 * i + 0], positions[3 * i + 1], positions[3 * i + 2]};
			mesh->addVertex(position);
		}

		// UVs
		const auto uvIt = primitive.attributes.find("TEXCOORD_0");
		const float* uvs = nullptr;
		size_t uvCount = 0;
		if (uvIt != primitive.attributes.end()) {
			hasUv = true;
			const auto uvAccessorIdx = static_cast<size_type>(uvIt->second);
			const tinygltf::Accessor& uvAccessor = gltfModel.accessors.at(uvAccessorIdx);
			const tinygltf::BufferView& uvBufferView =
					gltfModel.bufferViews.at(static_cast<size_type>(uvAccessor.bufferView));
			const tinygltf::Buffer& uvBuffer = gltfModel.buffers.at(static_cast<size_type>(uvBufferView.buffer));
			uvs = reinterpret_cast<const float*>(&uvBuffer.data.at(uvBufferView.byteOffset + uvAccessor.byteOffset));
			uvCount = uvAccessor.count;
		}

		// Normals
		const auto normalIt = primitive.attributes.find("NORMAL");
		const float* normalData = nullptr;
		size_t normalCount = 0;
		if (normalIt != primitive.attributes.end()) {
			hasNormals = true;
			const auto normalAccessorIdx = static_cast<size_type>(normalIt->second);
			const tinygltf::Accessor& normalAccessor = gltfModel.accessors.at(normalAccessorIdx);
			const tinygltf::BufferView& normalBufferView =
					gltfModel.bufferViews.at(static_cast<size_type>(normalAccessor.bufferView));
			const tinygltf::Buffer& normalBuffer =
					gltfModel.buffers.at(static_cast<size_type>(normalBufferView.buffer));
			normalData = reinterpret_cast<const float*>(
					&normalBuffer.data.at(normalBufferView.byteOffset + normalAccessor.byteOffset));
			normalCount = normalAccessor.count;
		}

		// Indices
		if (primitive.indices < 0) {
			OWL_CORE_WARN("MeshLoader::loadGltf: No indices found, skipping primitive")
			continue;
		}
		const tinygltf::Accessor& idxAccessor = gltfModel.accessors.at(static_cast<size_type>(primitive.indices));
		const tinygltf::BufferView& idxBufferView =
				gltfModel.bufferViews.at(static_cast<size_type>(idxAccessor.bufferView));
		const tinygltf::Buffer& idxBuffer = gltfModel.buffers.at(static_cast<size_type>(idxBufferView.buffer));
		const void* indicesPtr = &idxBuffer.data.at(idxBufferView.byteOffset + idxAccessor.byteOffset);
		for (size_t i = 0; i < idxAccessor.count; i += 3) {
			std::array<uint32_t, 3> vertexIndices{};
			for (size_t j = 0; j < 3; ++j) {
				uint32_t idx = 0;
				switch (idxAccessor.componentType) {
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
						idx = static_cast<const uint16_t*>(indicesPtr)[i + j];
						break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
						idx = static_cast<const uint32_t*>(indicesPtr)[i + j];
						break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
						idx = static_cast<const uint8_t*>(indicesPtr)[i + j];
						break;
					default:
						OWL_CORE_ERROR("MeshLoader::loadGltf: Unsupported index component type")
						break;
				}
				vertexIndices.at(j) = idx;
			}
			std::array<vec2, 3> faceUv{};
			std::array<vec3, 3> faceNormals{};
			for (size_t j = 0; j < 3; ++j) {
				const uint32_t vertIdx = vertexIndices[j];
				if (uvs != nullptr && vertIdx < uvCount) {
					faceUv[j].x() = uvs[2 * vertIdx + 0];
					faceUv[j].y() = uvs[2 * vertIdx + 1];
				} else {
					faceUv[j] = vec2(0.0f, 0.0f);
				}
				if (normalData != nullptr && vertIdx < normalCount) {
					faceNormals[j].x() = normalData[3 * vertIdx + 0];
					faceNormals[j].y() = normalData[3 * vertIdx + 1];
					faceNormals[j].z() = normalData[3 * vertIdx + 2];
				} else {
					faceNormals[j] = vec3(0.0f, 0.0f, 0.0f);
				}
			}

			if (hasUv)
				uvCoords.push_back(faceUv);
			if (hasNormals)
				normals.push_back(faceNormals);
			mesh->addTriangle(vertexIndices);
		}
	}
	if (hasUv)
		mesh->addTriangleExtraData<TriangleUVCoordinate>();
	if (hasNormals)
		mesh->addTriangleExtraData<TriangleNormals>();

	// Fill UVs and normals
	size_t triIndex = 0;
	for (auto& [uv, m_normals]: MeshTriangleRange(*mesh, component::WriteUvCoords, component::WriteTriangleNormals)) {
		if (uv.hasValue()) {
			uv.value()->setUvCoord(0, uvCoords[triIndex][0]);
			uv.value()->setUvCoord(1, uvCoords[triIndex][1]);
			uv.value()->setUvCoord(2, uvCoords[triIndex][2]);
		}
		if (m_normals.hasValue()) {
			m_normals.value()->setNormal(0, normals[triIndex][0]);
			m_normals.value()->setNormal(1, normals[triIndex][1]);
			m_normals.value()->setNormal(2, normals[triIndex][2]);
		}
		++triIndex;
	}

	OWL_CORE_INFO("MeshLoader::loadGltf: Loaded mesh {} with {} vertices and {} triangles.", iFilePath.string(),
				  mesh->getVertexCount(), mesh->getTriangleCount())
	OWL_CORE_INFO("MeshLoader::loadGltf: Mesh has {} UV coordinates and {} normals.",
				  !hasUv || mesh->isExtraDataDefinedOnAllTriangles<TriangleUVCoordinate>() ? "complete" : "incomplete",
				  !hasNormals || mesh->isExtraDataDefinedOnAllTriangles<TriangleNormals>() ? "complete" : "incomplete")
	OWL_CORE_INFO("MeshLoader::loadGltf: All other mesh data are ignored.")

	return mesh;
}

}// namespace owl::data
