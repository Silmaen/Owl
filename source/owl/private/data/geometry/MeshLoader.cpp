/**
 * @file MeshLoader.cpp
 * @author Silmaen
 * @date 26/10/2025
 * Copyright (c) 2025 All rights reserved.
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

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

using namespace owl::core;
using namespace owl::math;
using namespace owl::data::geometry;
using namespace owl::data::geometry::extradata;

namespace owl::data::geometry {

namespace {

using CornerUvs = std::vector<std::array<vec2, 3>>;
using CornerNormals = std::vector<std::array<vec3, 3>>;
using size_type = std::vector<tinygltf::Accessor>::size_type;

// Attach the per-corner UVs and normals (one entry per triangle, or empty when the file has none) to the mesh.
void attachCornerData(StaticMesh& ioMesh, const CornerUvs& iUvs, const CornerNormals& iNormals) {
	if (!iUvs.empty())
		ioMesh.addTriangleExtraData<TriangleUVCoordinate>();
	if (!iNormals.empty())
		ioMesh.addTriangleExtraData<TriangleNormals>();
	size_t triIndex = 0;
	for (auto& [uv, normal]: MeshTriangleRange(ioMesh, meshrange::WriteUvCoords, meshrange::WriteTriangleNormals)) {
		for (uint8_t corner = 0; corner < 3; ++corner) {
			if (uv.hasValue() && triIndex < iUvs.size())
				uv.value()->setUvCoord(corner, iUvs[triIndex][corner]);
			if (normal.hasValue() && triIndex < iNormals.size())
				normal.value()->setNormal(corner, iNormals[triIndex][corner]);
		}
		++triIndex;
	}
}

void logCompleteness(const std::string& iLoader, const std::string& iName, const StaticMesh& iMesh, const bool iHasUv,
					 const bool iHasNormals) {
	OWL_CORE_INFO("MeshLoader::{}: Loaded mesh {} with {} vertices and {} triangles.", iLoader, iName,
				  iMesh.getVertexCount(), iMesh.getTriangleCount())
	OWL_CORE_INFO("MeshLoader::{}: Mesh has {} UV coordinates and {} normals.", iLoader,
				  !iHasUv || iMesh.isExtraDataDefinedOnAllTriangles<TriangleUVCoordinate>() ? "complete" : "incomplete",
				  !iHasNormals || iMesh.isExtraDataDefinedOnAllTriangles<TriangleNormals>() ? "complete" : "incomplete")
}

auto buildObj(const tinyobj::attrib_t& iAttrib, const std::vector<tinyobj::shape_t>& iShapes, const std::string& iName)
		-> shared<StaticMesh> {
	auto mesh = mkShared<StaticMesh>();
	mesh->reserveVertices(iAttrib.vertices.size() / 3);
	for (size_t i = 0; i + 2 < iAttrib.vertices.size(); i += 3)
		mesh->addVertex(vec3{iAttrib.vertices[i], iAttrib.vertices[i + 1], iAttrib.vertices[i + 2]});
	const bool hasUv = !iAttrib.texcoords.empty();
	const bool hasNormals = !iAttrib.normals.empty();
	size_t estimatedTriangles = 0;
	for (const auto& shape: iShapes) estimatedTriangles += shape.mesh.num_face_vertices.size();
	mesh->reserveTriangles(estimatedTriangles);
	CornerUvs uvCoords;
	CornerNormals normals;
	uvCoords.reserve(hasUv ? estimatedTriangles : 0);
	normals.reserve(hasNormals ? estimatedTriangles : 0);
	for (const auto& shape: iShapes) {
		size_t indexOffset = 0;
		for (const size_t fv: shape.mesh.num_face_vertices) {
			if (fv != 3) {
				OWL_CORE_WARN(
						"MeshLoader::loadObj: Face with {} vertices found, only triangles are supported, skipping.", fv)
				indexOffset += fv;
				continue;
			}
			std::array<uint32_t, 3> vertexIndices{};
			std::array<vec2, 3> faceUv{};
			std::array<vec3, 3> faceNormals{};
			for (size_t v = 0; v < fv; v++) {
				const tinyobj::index_t idx = shape.mesh.indices[indexOffset + v];
				vertexIndices[v] = static_cast<uint32_t>(idx.vertex_index);
				if (idx.texcoord_index >= 0) {
					const auto uvIndex = 2u * static_cast<size_t>(idx.texcoord_index);
					faceUv[v] = vec2{iAttrib.texcoords[uvIndex], iAttrib.texcoords[uvIndex + 1]};
				}
				if (idx.normal_index >= 0) {
					const auto normalIndex = 3u * static_cast<size_t>(idx.normal_index);
					faceNormals[v] = vec3{iAttrib.normals[normalIndex], iAttrib.normals[normalIndex + 1],
										  iAttrib.normals[normalIndex + 2]};
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
	attachCornerData(*mesh, uvCoords, normals);
	logCompleteness("loadObj", iName, *mesh, hasUv, hasNormals);
	return mesh;
}

// Decode a glTF image to RGBA8, bottom row first like every engine texture.
auto decodeGltfImage(tinygltf::Image* oImage, [[maybe_unused]] const int iImageIndex, std::string* oError,
					 [[maybe_unused]] std::string* oWarning, [[maybe_unused]] const int iRequestedWidth,
					 [[maybe_unused]] const int iRequestedHeight, const unsigned char* iBytes, const int iSize,
					 [[maybe_unused]] void* iUserData) -> bool {
	int width = 0;
	int height = 0;
	int channels = 0;
	stbi_set_flip_vertically_on_load_thread(1);
	stbi_uc* pixels = stbi_load_from_memory(iBytes, iSize, &width, &height, &channels, 4);
	stbi_set_flip_vertically_on_load_thread(0);
	if (pixels == nullptr) {
		if (oError != nullptr)
			*oError += std::format("cannot decode image {}: {}", oImage->name, stbi_failure_reason());
		return false;
	}
	const auto byteCount = static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;
	oImage->image.assign(pixels, pixels + byteCount);
	stbi_image_free(pixels);
	oImage->width = width;
	oImage->height = height;
	oImage->component = 4;
	oImage->bits = 8;
	oImage->pixel_type = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
	return true;
}

// Float elements of an accessor, its byte stride honoured; empty when it is missing or not made of floats.
template<size_t N>
auto readFloats(const tinygltf::Model& iModel, const int iAccessor) -> std::vector<std::array<float, N>> {
	if (iAccessor < 0)
		return {};
	const auto& accessor = iModel.accessors.at(static_cast<size_type>(iAccessor));
	if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || accessor.bufferView < 0) {
		OWL_CORE_WARN("MeshLoader::loadGltf: Accessor {} is not made of floats, ignored.", iAccessor)
		return {};
	}
	const auto& view = iModel.bufferViews.at(static_cast<size_type>(accessor.bufferView));
	const auto& buffer = iModel.buffers.at(static_cast<size_type>(view.buffer));
	const int stride = accessor.ByteStride(view);
	const size_t start = view.byteOffset + accessor.byteOffset;
	if (stride <= 0 ||
		start + static_cast<size_t>(stride) * (accessor.count - 1) + N * sizeof(float) > buffer.data.size()) {
		OWL_CORE_WARN("MeshLoader::loadGltf: Accessor {} overflows its buffer, ignored.", iAccessor)
		return {};
	}
	std::vector<std::array<float, N>> values(accessor.count);
	for (size_t i = 0; i < accessor.count; ++i)
		std::memcpy(values[i].data(), &buffer.data[start + i * static_cast<size_t>(stride)], N * sizeof(float));
	return values;
}

// Indices of a triangle primitive: read from its accessor, or 0..n-1 for a non-indexed one.
auto readIndices(const tinygltf::Model& iModel, const tinygltf::Primitive& iPrimitive, const size_t iVertexCount)
		-> std::optional<std::vector<uint32_t>> {
	std::vector<uint32_t> indices;
	if (iPrimitive.indices < 0) {
		indices.resize(iVertexCount);
		for (size_t i = 0; i < iVertexCount; ++i) indices[i] = static_cast<uint32_t>(i);
		return indices;
	}
	const auto& accessor = iModel.accessors.at(static_cast<size_type>(iPrimitive.indices));
	if (accessor.bufferView < 0)
		return std::nullopt;
	const auto& view = iModel.bufferViews.at(static_cast<size_type>(accessor.bufferView));
	const auto& buffer = iModel.buffers.at(static_cast<size_type>(view.buffer));
	const size_t start = view.byteOffset + accessor.byteOffset;
	const auto componentSize =
			static_cast<size_t>(tinygltf::GetComponentSizeInBytes(static_cast<uint32_t>(accessor.componentType)));
	if (start + componentSize * accessor.count > buffer.data.size())
		return std::nullopt;
	indices.resize(accessor.count);
	const unsigned char* data = &buffer.data[start];
	for (size_t i = 0; i < accessor.count; ++i) {
		switch (accessor.componentType) {
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
				indices[i] = data[i];
				break;
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
				{
					uint16_t value = 0;
					std::memcpy(&value, data + i * sizeof(uint16_t), sizeof(uint16_t));
					indices[i] = value;
					break;
				}
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
				std::memcpy(&indices[i], data + i * sizeof(uint32_t), sizeof(uint32_t));
				break;
			default:
				return std::nullopt;
		}
	}
	return indices;
}

auto buildGltfMesh(const tinygltf::Model& iModel, const std::string& iName) -> shared<StaticMesh> {
	auto mesh = mkShared<StaticMesh>();
	if (iModel.meshes.empty()) {
		OWL_CORE_ERROR("MeshLoader::loadGltf: No mesh found in {}.", iName)
		return mesh;
	}
	// Triangles point at their vertices: every vertex is reserved before the first triangle is added.
	size_t vertexCount = 0;
	for (const auto& primitive: iModel.meshes.front().primitives) {
		if (const auto found = primitive.attributes.find("POSITION"); found != primitive.attributes.end())
			vertexCount += iModel.accessors.at(static_cast<size_type>(found->second)).count;
	}
	mesh->reserveVertices(vertexCount);
	CornerUvs uvCoords;
	CornerNormals normals;
	bool hasUv = false;
	bool hasNormals = false;
	for (const auto& primitive: iModel.meshes.front().primitives) {
		if (primitive.mode != TINYGLTF_MODE_TRIANGLES && primitive.mode != -1) {
			OWL_CORE_WARN("MeshLoader::loadGltf: Primitive of mode {} in {} is not made of triangles, skipped.",
						  primitive.mode, iName)
			continue;
		}
		const auto attribute = [&primitive](const char* iKey) -> int {
			const auto found = primitive.attributes.find(iKey);
			return found == primitive.attributes.end() ? -1 : found->second;
		};
		const auto positions = readFloats<3>(iModel, attribute("POSITION"));
		if (positions.empty()) {
			OWL_CORE_ERROR("MeshLoader::loadGltf: No POSITION attribute in a primitive of {}.", iName)
			continue;
		}
		const auto uvs = readFloats<2>(iModel, attribute("TEXCOORD_0"));
		const auto vertexNormals = readFloats<3>(iModel, attribute("NORMAL"));
		const auto indices = readIndices(iModel, primitive, positions.size());
		if (!indices) {
			OWL_CORE_WARN("MeshLoader::loadGltf: Unreadable indices in a primitive of {}, skipped.", iName)
			continue;
		}
		hasUv = hasUv || !uvs.empty();
		hasNormals = hasNormals || !vertexNormals.empty();
		// Several primitives share the mesh: their indices start after the vertices of the previous ones.
		const auto base = static_cast<uint32_t>(mesh->getVertexCount());
		for (const auto& position: positions) mesh->addVertex(vec3{position[0], position[1], position[2]});
		for (size_t i = 0; i + 2 < indices->size(); i += 3) {
			std::array<uint32_t, 3> corners{};
			std::array<vec2, 3> faceUv{};
			std::array<vec3, 3> faceNormals{};
			bool valid = true;
			for (size_t j = 0; j < 3; ++j) {
				const uint32_t index = (*indices)[i + j];
				valid = valid && index < positions.size();
				corners[j] = base + index;
				// glTF puts v = 0 at the top of the image, the engine at the bottom.
				if (index < uvs.size())
					faceUv[j] = vec2{uvs[index][0], 1.f - uvs[index][1]};
				if (index < vertexNormals.size())
					faceNormals[j] = vec3{vertexNormals[index][0], vertexNormals[index][1], vertexNormals[index][2]};
			}
			if (!valid) {
				OWL_CORE_WARN("MeshLoader::loadGltf: Triangle with an index out of range in {}, skipped.", iName)
				continue;
			}
			mesh->addTriangle(corners);
			uvCoords.push_back(faceUv);
			normals.push_back(faceNormals);
		}
	}
	attachCornerData(*mesh, hasUv ? uvCoords : CornerUvs{}, hasNormals ? normals : CornerNormals{});
	logCompleteness("loadGltf", iName, *mesh, hasUv, hasNormals);
	return mesh;
}

auto readGltfMaterial(const tinygltf::Model& iModel) -> MeshMaterial {
	MeshMaterial material;
	if (iModel.meshes.empty())
		return material;
	const auto& primitives = iModel.meshes.front().primitives;
	const auto withMaterial =
			std::ranges::find_if(primitives, [&iModel](const tinygltf::Primitive& iPrimitive) -> bool {
				return iPrimitive.material >= 0 && static_cast<size_t>(iPrimitive.material) < iModel.materials.size();
			});
	if (withMaterial == primitives.end())
		return material;
	const auto& pbr = iModel.materials[static_cast<size_type>(withMaterial->material)].pbrMetallicRoughness;
	if (pbr.baseColorFactor.size() == 4)
		material.baseColorFactor =
				vec4{static_cast<float>(pbr.baseColorFactor[0]), static_cast<float>(pbr.baseColorFactor[1]),
					 static_cast<float>(pbr.baseColorFactor[2]), static_cast<float>(pbr.baseColorFactor[3])};
	const int textureIndex = pbr.baseColorTexture.index;
	if (textureIndex < 0 || static_cast<size_t>(textureIndex) >= iModel.textures.size())
		return material;
	const int source = iModel.textures[static_cast<size_type>(textureIndex)].source;
	if (source < 0 || static_cast<size_t>(source) >= iModel.images.size())
		return material;
	const auto& image = iModel.images[static_cast<size_type>(source)];
	material.baseColorName = image.uri.empty() ? image.name : image.uri;
	if (image.width <= 0 || image.height <= 0 || image.component != 4 || image.bits != 8 ||
		image.image.size() != static_cast<size_t>(image.width) * static_cast<size_t>(image.height) * 4u) {
		OWL_CORE_WARN("MeshLoader::loadGltf: Base colour image {} could not be decoded, ignored.",
					  material.baseColorName)
		return material;
	}
	material.baseColorSize = {static_cast<uint32_t>(image.width), static_cast<uint32_t>(image.height)};
	material.baseColorPixels = image.image;
	return material;
}

// File callbacks of tinygltf reading through a mesh file reader (an asset pack) instead of the disk.
auto makeFsCallbacks(const MeshFileReader& iReadFile) -> tinygltf::FsCallbacks {
	return {.FileExists = [&iReadFile](const std::string& iPath, void*) -> bool {
				return iReadFile(iPath).has_value();
			},
			.ExpandFilePath = [](const std::string& iPath, void*) -> std::string { return iPath; },
			.ReadWholeFile = [&iReadFile](std::vector<unsigned char>* oData, std::string* oError,
										  const std::string& iPath, void*) -> bool {
				auto data = iReadFile(iPath);
				if (!data) {
					if (oError != nullptr)
						*oError += std::format("file {} not found", iPath);
					return false;
				}
				*oData = std::move(*data);
				return true;
			},
			.WriteWholeFile = [](std::string*, const std::string&, const std::vector<unsigned char>&, void*) -> bool {
				return false;
			},
			.GetFileSizeInBytes = [&iReadFile](size_t* oSize, std::string*, const std::string& iPath, void*) -> bool {
				const auto data = iReadFile(iPath);
				if (!data)
					return false;
				*oSize = data->size();
				return true;
			},
			.user_data = nullptr};
}

auto parseGltf(const std::span<const uint8_t> iData, const bool iBinary, const std::string& iName,
			   const std::filesystem::path& iBaseDirectory, const MeshFileReader& iReadFile = {})
		-> std::optional<tinygltf::Model> {
	tinygltf::Model model;
	tinygltf::TinyGLTF loader;
	loader.SetImageLoader(decodeGltfImage, nullptr);
	if (iReadFile)
		std::ignore = loader.SetFsCallbacks(makeFsCallbacks(iReadFile));
	std::string warn;
	std::string err;
	const auto size = static_cast<unsigned int>(iData.size());
	const auto baseDirectory = iBaseDirectory.generic_string();
	const bool loaded =
			iBinary ? loader.LoadBinaryFromMemory(&model, &err, &warn, iData.data(), size, baseDirectory)
					: loader.LoadASCIIFromString(&model, &err, &warn, reinterpret_cast<const char*>(iData.data()), size,
												 baseDirectory);
	if (!warn.empty())
		OWL_CORE_WARN("MeshLoader::loadGltf: {}.", warn)
	if (!loaded) {
		OWL_CORE_ERROR("MeshLoader::loadGltf: Failed to load {}: {}.", iName, err)
		return std::nullopt;
	}
	return model;
}

auto readFileBytes(const std::filesystem::path& iFilePath) -> std::optional<std::vector<uint8_t>> {
	std::ifstream file(iFilePath, std::ios::binary);
	if (!file) {
		OWL_CORE_ERROR("MeshLoader: Cannot open {}.", iFilePath.string())
		return std::nullopt;
	}
	return std::vector<uint8_t>{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

}// namespace

auto MeshLoader::isMeshAsset(const std::filesystem::path& iFilePath) -> bool {
	const auto extension = iFilePath.extension().string();
	return extension == ".obj" || extension == ".gltf" || extension == ".glb";
}

auto MeshLoader::loadMesh(const std::filesystem::path& iFilePath) -> LoadedMesh {
	if (!isMeshAsset(iFilePath)) {
		OWL_CORE_ERROR("MeshLoader::loadMesh: Unsupported mesh file {}.", iFilePath.string())
		return {};
	}
	const auto bytes = readFileBytes(iFilePath);
	if (!bytes)
		return {};
	return loadMeshFromMemory(*bytes, iFilePath.extension().string(), iFilePath.string(), iFilePath.parent_path());
}

auto MeshLoader::loadMeshFromMemory(const std::span<const uint8_t> iData, const std::string& iExtension,
									const std::string& iName, const std::filesystem::path& iBaseDirectory,
									const MeshFileReader& iReadFile) -> LoadedMesh {
	if (iExtension == ".gltf" || iExtension == ".glb") {
		const auto model = parseGltf(iData, iExtension == ".glb", iName, iBaseDirectory, iReadFile);
		if (!model)
			return {};
		return {.mesh = buildGltfMesh(*model, iName), .material = readGltfMaterial(*model)};
	}
	if (iExtension == ".obj") {
		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		std::string warn;
		std::string err;
		std::istringstream stream{std::string{reinterpret_cast<const char*>(iData.data()), iData.size()}};
		if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, &stream)) {
			OWL_CORE_ERROR("MeshLoader::loadObj: Failed to load {}: {}.", iName, err)
			return {};
		}
		if (!warn.empty())
			OWL_CORE_WARN("MeshLoader::loadObj: {}.", warn)
		return {.mesh = buildObj(attrib, shapes, iName), .material = {}};
	}
	OWL_CORE_ERROR("MeshLoader::loadMesh: Unsupported mesh format {} for {}.", iExtension, iName)
	return {};
}

auto MeshLoader::getExternalFiles(const std::filesystem::path& iFilePath) -> std::vector<std::string> {
	if (iFilePath.extension() != ".gltf")
		return {};
	std::vector<std::string> files;
	try {
		const auto document = nlohmann::json::parse(std::ifstream{iFilePath});
		for (const char* section: {"buffers", "images"}) {
			const auto found = document.find(section);
			if (found == document.end() || !found->is_array())
				continue;
			for (const auto& item: *found) {
				const auto uri = item.find("uri");
				if (uri == item.end() || !uri->is_string())
					continue;
				if (auto value = uri->get<std::string>(); !value.starts_with("data:"))
					files.push_back(std::move(value));
			}
		}
	} catch (const nlohmann::json::exception& iError) {
		OWL_CORE_WARN("MeshLoader: Cannot list the files of {}: {}.", iFilePath.string(), iError.what())
		return {};
	}
	return files;
}

auto MeshLoader::loadObj(const std::filesystem::path& iFilePath) -> shared<StaticMesh> {
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warn;
	std::string err;
	if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, iFilePath.string().c_str())) {
		OWL_CORE_ERROR("MeshLoader::loadObj: Failed to load {}: {}.", iFilePath.string(), err)
		return mkShared<StaticMesh>();
	}
	if (!warn.empty())
		OWL_CORE_WARN("MeshLoader::loadObj: {}.", warn)
	return buildObj(attrib, shapes, iFilePath.string());
}

auto MeshLoader::loadFbx([[maybe_unused]] const std::filesystem::path& iFilePath) -> shared<StaticMesh> {
	OWL_CORE_WARN("MeshLoader::loadFbx: FBX loading not implemented yet!")
	shared<StaticMesh> mesh = mkShared<StaticMesh>();

	constexpr ufbx_load_opts opts = {};
	ufbx_error error;
	ufbx_scene* scene = ufbx_load_file(iFilePath.string().c_str(), &opts, &error);
	if (scene == nullptr) {
		OWL_CORE_ERROR("MeshLoader::loadFbx: Failed to load {}: {}.", iFilePath.string(), error.description.data)
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

	attachCornerData(*mesh, hasUv ? uvCoords : std::vector<std::array<vec2, 3>>{},
					 hasNormals ? normals : std::vector<std::array<vec3, 3>>{});

	OWL_CORE_INFO("MeshLoader::loadFbx: Loaded mesh {} with {} vertices and {} triangles.", iFilePath.string(),
				  mesh->getVertexCount(), mesh->getTriangleCount())
	OWL_CORE_INFO("MeshLoader::loadFbx: Mesh has {} UV coordinates and {} normals.",
				  !hasUv || mesh->isExtraDataDefinedOnAllTriangles<TriangleUVCoordinate>() ? "complete" : "incomplete",
				  !hasNormals || mesh->isExtraDataDefinedOnAllTriangles<TriangleNormals>() ? "complete" : "incomplete")
	OWL_CORE_INFO("MeshLoader::loadFbx: All other mesh data are ignored.")
	return mesh;
}

auto MeshLoader::loadGltf(const std::filesystem::path& iFilePath) -> shared<StaticMesh> {
	const auto bytes = readFileBytes(iFilePath);
	if (!bytes)
		return mkShared<StaticMesh>();
	const auto model = parseGltf(*bytes, iFilePath.extension() == ".glb", iFilePath.string(), iFilePath.parent_path());
	if (!model)
		return mkShared<StaticMesh>();
	return buildGltfMesh(*model, iFilePath.string());
}

}// namespace owl::data::geometry
