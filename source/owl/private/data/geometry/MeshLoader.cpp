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
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <map>
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

// Decode an encoded image to RGBA8, bottom row first like every engine texture.
auto decodeImageBytes(const std::span<const uint8_t> iBytes, const std::string& iName) -> std::optional<MeshImage> {
	int width = 0;
	int height = 0;
	int channels = 0;
	stbi_set_flip_vertically_on_load_thread(1);
	stbi_uc* pixels =
			stbi_load_from_memory(iBytes.data(), static_cast<int>(iBytes.size()), &width, &height, &channels, 4);
	stbi_set_flip_vertically_on_load_thread(0);
	if (pixels == nullptr) {
		OWL_CORE_WARN("MeshLoader: Cannot decode image {}: {}.", iName, stbi_failure_reason())
		return std::nullopt;
	}
	const auto byteCount = static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;
	MeshImage image;
	image.size = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
	image.pixels.assign(pixels, pixels + byteCount);
	image.name = iName;
	stbi_image_free(pixels);
	return image;
}

// Bytes of a file next to a mesh file, through the reader (an asset pack) or from the disk; nothing when missing.
auto readSiblingFile(const std::filesystem::path& iBaseDirectory, const MeshFileReader& iReadFile,
					 const std::string& iRelative) -> std::optional<std::vector<uint8_t>> {
	std::string relative = iRelative;
	std::ranges::replace(relative, '\\', '/');
	const auto path = (iBaseDirectory / relative).generic_string();
	if (iReadFile)
		return iReadFile(path);
	std::ifstream file(path, std::ios::binary);
	if (!file)
		return std::nullopt;
	return std::vector<uint8_t>{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

// Reads the `.mtl` files of an OBJ next to it, from the disk or through a mesh file reader.
class ObjMaterialReader final : public tinyobj::MaterialReader {
public:
	ObjMaterialReader(std::filesystem::path iBaseDirectory, MeshFileReader iReadFile)
		: m_baseDirectory{std::move(iBaseDirectory)}, m_readFile{std::move(iReadFile)} {}

	auto operator()(const std::string& iMaterialFile, std::vector<tinyobj::material_t>* oMaterials,
					std::map<std::string, int>* oMaterialMap, std::string* oWarning, std::string* oError)
			-> bool override {
		const auto bytes = readSiblingFile(m_baseDirectory, m_readFile, iMaterialFile);
		if (!bytes) {
			OWL_CORE_WARN("MeshLoader::loadObj: Material file {} not found.", iMaterialFile)
			return false;
		}
		std::istringstream stream{std::string{reinterpret_cast<const char*>(bytes->data()), bytes->size()}};
		tinyobj::LoadMtl(oMaterialMap, oMaterials, &stream, oWarning, oError);
		return true;
	}

private:
	// Folder the material files are looked up in.
	std::filesystem::path m_baseDirectory;
	// Reader of the material files, empty to read the disk.
	MeshFileReader m_readFile;
};

auto convertObjMaterial(const tinyobj::material_t& iSource, const std::filesystem::path& iBaseDirectory,
						const MeshFileReader& iReadFile) -> MeshMaterial {
	MeshMaterial material;
	material.name = iSource.name;
	material.baseColorFactor = vec4{iSource.diffuse[0], iSource.diffuse[1], iSource.diffuse[2], iSource.dissolve};
	if (iSource.dissolve < 1.f)
		material.alphaMode = MeshMaterial::AlphaMode::Blend;
	material.metallicFactor = 0.f;
	// tinyobj reports a shininess of 1 for a material that gives none.
	material.roughnessFactor = iSource.shininess > 1.f ? std::sqrt(2.f / (iSource.shininess + 2.f)) : 0.5f;
	material.emissiveFactor = vec3{iSource.emission[0], iSource.emission[1], iSource.emission[2]};
	if (!iSource.diffuse_texname.empty()) {
		if (const auto bytes = readSiblingFile(iBaseDirectory, iReadFile, iSource.diffuse_texname)) {
			if (auto image = decodeImageBytes(*bytes, iSource.diffuse_texname))
				material.baseColor = std::move(*image);
		} else {
			OWL_CORE_WARN("MeshLoader::loadObj: Texture {} of material {} not found.", iSource.diffuse_texname,
						  iSource.name)
		}
	}
	return material;
}

// A triangle of an OBJ shape and the material slot it uses.
struct ObjFace {
	// Index of the shape.
	size_t shape = 0;
	// Position of the first corner in the index list of the shape.
	size_t offset = 0;
	// Position of the material in the converted list.
	uint32_t slot = 0;
};

auto buildObj(const tinyobj::attrib_t& iAttrib, const std::vector<tinyobj::shape_t>& iShapes,
			  const std::vector<tinyobj::material_t>& iMaterials, const std::string& iName,
			  const std::filesystem::path& iBaseDirectory, const MeshFileReader& iReadFile) -> LoadedMesh {
	LoadedMesh loaded;
	loaded.mesh = mkShared<StaticMesh>();
	auto& mesh = *loaded.mesh;
	mesh.reserveVertices(iAttrib.vertices.size() / 3);
	for (size_t i = 0; i + 2 < iAttrib.vertices.size(); i += 3)
		mesh.addVertex(vec3{iAttrib.vertices[i], iAttrib.vertices[i + 1], iAttrib.vertices[i + 2]});
	const bool hasUv = !iAttrib.texcoords.empty();
	const bool hasNormals = !iAttrib.normals.empty();
	// Faces are grouped by material so that every submesh covers a contiguous run of indices.
	const auto defaultSlot = static_cast<uint32_t>(iMaterials.size());
	bool usesDefault = iMaterials.empty();
	std::vector<ObjFace> faces;
	for (size_t shapeIndex = 0; shapeIndex < iShapes.size(); ++shapeIndex) {
		const auto& shape = iShapes[shapeIndex];
		size_t indexOffset = 0;
		for (size_t faceIndex = 0; faceIndex < shape.mesh.num_face_vertices.size(); ++faceIndex) {
			const size_t fv = shape.mesh.num_face_vertices[faceIndex];
			if (fv != 3) {
				OWL_CORE_WARN(
						"MeshLoader::loadObj: Face with {} vertices found, only triangles are supported, skipping.", fv)
				indexOffset += fv;
				continue;
			}
			const int materialId = faceIndex < shape.mesh.material_ids.size() ? shape.mesh.material_ids[faceIndex] : -1;
			const bool known = materialId >= 0 && static_cast<size_t>(materialId) < iMaterials.size();
			usesDefault = usesDefault || !known;
			faces.push_back({.shape = shapeIndex,
							 .offset = indexOffset,
							 .slot = known ? static_cast<uint32_t>(materialId) : defaultSlot});
			indexOffset += fv;
		}
	}
	std::ranges::stable_sort(
			faces, [](const ObjFace& iLeft, const ObjFace& iRight) -> bool { return iLeft.slot < iRight.slot; });
	mesh.reserveTriangles(faces.size());
	CornerUvs uvCoords;
	CornerNormals normals;
	uvCoords.reserve(hasUv ? faces.size() : 0);
	normals.reserve(hasNormals ? faces.size() : 0);
	for (const auto& [shapeIndex, offset, slot]: faces) {
		const auto& shape = iShapes[shapeIndex];
		std::array<uint32_t, 3> vertexIndices{};
		std::array<vec2, 3> faceUv{};
		std::array<vec3, 3> faceNormals{};
		for (size_t v = 0; v < 3; v++) {
			const tinyobj::index_t idx = shape.mesh.indices[offset + v];
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
		if (loaded.submeshes.empty() || loaded.submeshes.back().materialIndex != slot)
			loaded.submeshes.push_back({.firstIndex = static_cast<uint32_t>(mesh.getTriangleCount() * 3),
										.indexCount = 0,
										.materialIndex = slot});
		loaded.submeshes.back().indexCount += 3;
		mesh.addTriangle(vertexIndices);
	}
	attachCornerData(mesh, uvCoords, normals);
	logCompleteness("loadObj", iName, mesh, hasUv, hasNormals);
	loaded.materials.clear();
	for (const auto& source: iMaterials)
		loaded.materials.push_back(convertObjMaterial(source, iBaseDirectory, iReadFile));
	if (usesDefault)
		loaded.materials.emplace_back();
	return loaded;
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

// Image a glTF texture refers to, decoded; empty when the texture is absent or could not be decoded.
auto readGltfImage(const tinygltf::Model& iModel, const int iTextureIndex) -> MeshImage {
	if (iTextureIndex < 0 || static_cast<size_t>(iTextureIndex) >= iModel.textures.size())
		return {};
	const int source = iModel.textures[static_cast<size_type>(iTextureIndex)].source;
	if (source < 0 || static_cast<size_t>(source) >= iModel.images.size())
		return {};
	const auto& image = iModel.images[static_cast<size_type>(source)];
	MeshImage result;
	result.name = image.uri.empty() ? image.name : image.uri;
	if (image.width <= 0 || image.height <= 0 || image.component != 4 || image.bits != 8 ||
		image.image.size() != static_cast<size_t>(image.width) * static_cast<size_t>(image.height) * 4u) {
		OWL_CORE_WARN("MeshLoader::loadGltf: Image {} could not be decoded, ignored.", result.name)
		return result;
	}
	result.size = {static_cast<uint32_t>(image.width), static_cast<uint32_t>(image.height)};
	result.pixels = image.image;
	return result;
}

auto extensionNumber(const tinygltf::Value& iExtension, const char* iKey, const float iDefault) -> float {
	if (!iExtension.Has(iKey))
		return iDefault;
	const auto& value = iExtension.Get(iKey);
	return value.IsNumber() ? static_cast<float>(value.GetNumberAsDouble()) : iDefault;
}

auto extensionColor(const tinygltf::Value& iExtension, const char* iKey) -> vec3 {
	vec3 color{0.f, 0.f, 0.f};
	if (!iExtension.Has(iKey))
		return color;
	const auto& value = iExtension.Get(iKey);
	if (!value.IsArray() || value.ArrayLen() < 3)
		return color;
	for (size_t i = 0; i < 3; ++i) {
		const auto& component = value.Get(i);
		color[i] = component.IsNumber() ? static_cast<float>(component.GetNumberAsDouble()) : 0.f;
	}
	return color;
}

auto convertGltfMaterial(const tinygltf::Model& iModel, const tinygltf::Material& iSource) -> MeshMaterial {
	MeshMaterial material;
	material.name = iSource.name;
	const auto& pbr = iSource.pbrMetallicRoughness;
	if (pbr.baseColorFactor.size() == 4)
		material.baseColorFactor =
				vec4{static_cast<float>(pbr.baseColorFactor[0]), static_cast<float>(pbr.baseColorFactor[1]),
					 static_cast<float>(pbr.baseColorFactor[2]), static_cast<float>(pbr.baseColorFactor[3])};
	material.metallicFactor = static_cast<float>(pbr.metallicFactor);
	material.roughnessFactor = static_cast<float>(pbr.roughnessFactor);
	if (iSource.emissiveFactor.size() == 3)
		material.emissiveFactor =
				vec3{static_cast<float>(iSource.emissiveFactor[0]), static_cast<float>(iSource.emissiveFactor[1]),
					 static_cast<float>(iSource.emissiveFactor[2])};
	material.normalScale = static_cast<float>(iSource.normalTexture.scale);
	material.occlusionStrength = static_cast<float>(iSource.occlusionTexture.strength);
	if (iSource.alphaMode == "MASK")
		material.alphaMode = MeshMaterial::AlphaMode::Mask;
	else if (iSource.alphaMode == "BLEND")
		material.alphaMode = MeshMaterial::AlphaMode::Blend;
	material.alphaCutoff = static_cast<float>(iSource.alphaCutoff);
	material.doubleSided = iSource.doubleSided;
	if (const auto found = iSource.extensions.find("KHR_materials_emissive_strength");
		found != iSource.extensions.end())
		material.emissiveStrength = extensionNumber(found->second, "emissiveStrength", 1.f);
	if (const auto found = iSource.extensions.find("KHR_materials_clearcoat"); found != iSource.extensions.end()) {
		material.clearcoat = extensionNumber(found->second, "clearcoatFactor", 0.f);
		material.clearcoatRoughness = extensionNumber(found->second, "clearcoatRoughnessFactor", 0.f);
	}
	if (const auto found = iSource.extensions.find("KHR_materials_sheen"); found != iSource.extensions.end()) {
		material.sheenColor = extensionColor(found->second, "sheenColorFactor");
		material.sheenRoughness = extensionNumber(found->second, "sheenRoughnessFactor", 0.f);
	}
	if (const auto found = iSource.extensions.find("KHR_materials_anisotropy"); found != iSource.extensions.end()) {
		material.anisotropy = extensionNumber(found->second, "anisotropyStrength", 0.f);
		material.anisotropyRotation = extensionNumber(found->second, "anisotropyRotation", 0.f);
	}
	material.baseColor = readGltfImage(iModel, pbr.baseColorTexture.index);
	material.metallicRoughness = readGltfImage(iModel, pbr.metallicRoughnessTexture.index);
	material.normal = readGltfImage(iModel, iSource.normalTexture.index);
	material.occlusion = readGltfImage(iModel, iSource.occlusionTexture.index);
	material.emissive = readGltfImage(iModel, iSource.emissiveTexture.index);
	return material;
}

// Column-major 4x4 transform of a glTF node, as the file stores its `matrix`.
using NodeMatrix = std::array<double, 16>;

constexpr NodeMatrix g_identityMatrix{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

auto multiply(const NodeMatrix& iA, const NodeMatrix& iB) -> NodeMatrix {
	NodeMatrix result{};
	for (size_t col = 0; col < 4; ++col)
		for (size_t row = 0; row < 4; ++row)
			for (size_t k = 0; k < 4; ++k) result[col * 4 + row] += iA[k * 4 + row] * iB[col * 4 + k];
	return result;
}

// Local transform of a node: its matrix, or translation * rotation * scale.
auto localMatrix(const tinygltf::Node& iNode) -> NodeMatrix {
	if (iNode.matrix.size() == 16) {
		NodeMatrix matrix{};
		std::ranges::copy(iNode.matrix, matrix.begin());
		return matrix;
	}
	const auto& t = iNode.translation;
	const auto& r = iNode.rotation;
	const auto& sc = iNode.scale;
	const double x = r.size() == 4 ? r[0] : 0.;
	const double y = r.size() == 4 ? r[1] : 0.;
	const double z = r.size() == 4 ? r[2] : 0.;
	const double w = r.size() == 4 ? r[3] : 1.;
	const double sx = sc.size() == 3 ? sc[0] : 1.;
	const double sy = sc.size() == 3 ? sc[1] : 1.;
	const double sz = sc.size() == 3 ? sc[2] : 1.;
	return {(1. - 2. * (y * y + z * z)) * sx, 2. * (x * y + z * w) * sx,        2. * (x * z - y * w) * sx,        0.,
			2. * (x * y - z * w) * sy,        (1. - 2. * (x * x + z * z)) * sy, 2. * (y * z + x * w) * sy,        0.,
			2. * (x * z + y * w) * sz,        2. * (y * z - x * w) * sz,        (1. - 2. * (x * x + y * y)) * sz, 0.,
			t.size() == 3 ? t[0] : 0.,        t.size() == 3 ? t[1] : 0.,        t.size() == 3 ? t[2] : 0.,        1.};
}

// A mesh of the file placed by the node chain leading to it.
struct MeshInstance {
	size_t mesh = 0;
	NodeMatrix transform = g_identityMatrix;
};

// NOLINTNEXTLINE(misc-no-recursion): the depth is bounded by the node count of the file.
void collectInstances(const tinygltf::Model& iModel, const int iNode, const NodeMatrix& iParent, const size_t iDepth,
					  std::vector<MeshInstance>& ioInstances) {
	if (iNode < 0 || static_cast<size_t>(iNode) >= iModel.nodes.size() || iDepth > iModel.nodes.size())
		return;
	const auto& node = iModel.nodes[static_cast<size_t>(iNode)];
	const NodeMatrix transform = multiply(iParent, localMatrix(node));
	if (node.mesh >= 0 && static_cast<size_t>(node.mesh) < iModel.meshes.size())
		ioInstances.push_back({.mesh = static_cast<size_t>(node.mesh), .transform = transform});
	for (const int child: node.children) collectInstances(iModel, child, transform, iDepth + 1, ioInstances);
}

// Every mesh the default scene places, with its world transform; every mesh once without a scene.
auto meshInstances(const tinygltf::Model& iModel) -> std::vector<MeshInstance> {
	std::vector<MeshInstance> instances;
	if (!iModel.scenes.empty()) {
		const auto sceneIndex =
				iModel.defaultScene >= 0 && static_cast<size_t>(iModel.defaultScene) < iModel.scenes.size()
						? static_cast<size_t>(iModel.defaultScene)
						: size_t{0};
		for (const int root: iModel.scenes[sceneIndex].nodes)
			collectInstances(iModel, root, g_identityMatrix, 0, instances);
	}
	if (instances.empty()) {
		for (size_t mesh = 0; mesh < iModel.meshes.size(); ++mesh) instances.push_back({.mesh = mesh});
	}
	return instances;
}

auto transformPoint(const NodeMatrix& iM, const std::array<float, 3>& iP) -> vec3 {
	const auto at = [&iM, &iP](const size_t iRow) -> float {
		return static_cast<float>(iM[iRow] * static_cast<double>(iP[0]) + iM[4 + iRow] * static_cast<double>(iP[1]) +
								  iM[8 + iRow] * static_cast<double>(iP[2]) + iM[12 + iRow]);
	};
	return vec3{at(0), at(1), at(2)};
}

// Cofactor matrix of the upper 3x3 (the inverse transpose times the determinant): it carries the normals.
auto normalMatrix(const NodeMatrix& iM) -> std::array<double, 9> {
	const auto m = [&iM](const size_t iRow, const size_t iCol) -> double { return iM[iCol * 4 + iRow]; };
	std::array<double, 9> cofactor{};
	for (size_t row = 0; row < 3; ++row) {
		for (size_t col = 0; col < 3; ++col) {
			const size_t r0 = (row + 1) % 3;
			const size_t r1 = (row + 2) % 3;
			const size_t c0 = (col + 1) % 3;
			const size_t c1 = (col + 2) % 3;
			cofactor[row * 3 + col] = m(r0, c0) * m(r1, c1) - m(r0, c1) * m(r1, c0);
		}
	}
	return cofactor;
}

auto determinant(const NodeMatrix& iM) -> double {
	return iM[0] * (iM[5] * iM[10] - iM[9] * iM[6]) - iM[4] * (iM[1] * iM[10] - iM[9] * iM[2]) +
		   iM[8] * (iM[1] * iM[6] - iM[5] * iM[2]);
}

auto transformNormal(const std::array<double, 9>& iCofactor, const std::array<float, 3>& iN) -> vec3 {
	const auto at = [&iCofactor, &iN](const size_t iRow) -> double {
		return iCofactor[iRow * 3] * static_cast<double>(iN[0]) + iCofactor[iRow * 3 + 1] * static_cast<double>(iN[1]) +
			   iCofactor[iRow * 3 + 2] * static_cast<double>(iN[2]);
	};
	const double x = at(0);
	const double y = at(1);
	const double z = at(2);
	const double length = std::sqrt(x * x + y * y + z * z);
	if (length <= 0.)
		return vec3{0.f, 0.f, 0.f};
	return vec3{static_cast<float>(x / length), static_cast<float>(y / length), static_cast<float>(z / length)};
}

// State of a glTF file being turned into one mesh.
struct GltfBuild {
	LoadedMesh& loaded;
	const tinygltf::Model& model;
	const std::string& name;
	CornerUvs uvCoords;
	CornerNormals normals;
	bool hasUv = false;
	bool hasNormals = false;
	std::optional<uint32_t> defaultSlot;
};

auto materialSlot(GltfBuild& ioBuild, const tinygltf::Primitive& iPrimitive) -> uint32_t {
	if (iPrimitive.material >= 0 && static_cast<size_t>(iPrimitive.material) < ioBuild.model.materials.size())
		return static_cast<uint32_t>(iPrimitive.material);
	if (!ioBuild.defaultSlot) {
		ioBuild.defaultSlot = static_cast<uint32_t>(ioBuild.loaded.materials.size());
		ioBuild.loaded.materials.emplace_back();
	}
	return *ioBuild.defaultSlot;
}

void appendPrimitive(GltfBuild& ioBuild, const tinygltf::Primitive& iPrimitive, const NodeMatrix& iTransform) {
	const auto& model = ioBuild.model;
	if (iPrimitive.mode != TINYGLTF_MODE_TRIANGLES && iPrimitive.mode != -1) {
		OWL_CORE_WARN("MeshLoader::loadGltf: Primitive of mode {} in {} is not made of triangles, skipped.",
					  iPrimitive.mode, ioBuild.name)
		return;
	}
	const auto attribute = [&iPrimitive](const char* iKey) -> int {
		const auto found = iPrimitive.attributes.find(iKey);
		return found == iPrimitive.attributes.end() ? -1 : found->second;
	};
	const auto positions = readFloats<3>(model, attribute("POSITION"));
	if (positions.empty()) {
		OWL_CORE_ERROR("MeshLoader::loadGltf: No POSITION attribute in a primitive of {}.", ioBuild.name)
		return;
	}
	const auto uvs = readFloats<2>(model, attribute("TEXCOORD_0"));
	const auto vertexNormals = readFloats<3>(model, attribute("NORMAL"));
	const auto indices = readIndices(model, iPrimitive, positions.size());
	if (!indices) {
		OWL_CORE_WARN("MeshLoader::loadGltf: Unreadable indices in a primitive of {}, skipped.", ioBuild.name)
		return;
	}
	ioBuild.hasUv = ioBuild.hasUv || !uvs.empty();
	ioBuild.hasNormals = ioBuild.hasNormals || !vertexNormals.empty();
	auto& mesh = *ioBuild.loaded.mesh;
	const size_t firstTriangle = mesh.getTriangleCount();
	const uint32_t slot = materialSlot(ioBuild, iPrimitive);
	const auto cofactor = normalMatrix(iTransform);
	// A mirroring transform turns the faces inside out: their corners are taken the other way round.
	const bool mirrored = determinant(iTransform) < 0.;
	const double normalSign = mirrored ? -1. : 1.;
	// Several primitives share the mesh: their indices start after the vertices of the previous ones.
	const auto base = static_cast<uint32_t>(mesh.getVertexCount());
	for (const auto& position: positions) mesh.addVertex(transformPoint(iTransform, position));
	for (size_t i = 0; i + 2 < indices->size(); i += 3) {
		std::array<uint32_t, 3> corners{};
		std::array<vec2, 3> faceUv{};
		std::array<vec3, 3> faceNormals{};
		bool valid = true;
		for (size_t j = 0; j < 3; ++j) {
			const uint32_t index = (*indices)[i + (mirrored ? 2 - j : j)];
			valid = valid && index < positions.size();
			corners[j] = base + index;
			// glTF puts v = 0 at the top of the image, the engine at the bottom.
			if (index < uvs.size())
				faceUv[j] = vec2{uvs[index][0], 1.f - uvs[index][1]};
			if (index < vertexNormals.size())
				faceNormals[j] = transformNormal(cofactor, vertexNormals[index]) * static_cast<float>(normalSign);
		}
		if (!valid) {
			OWL_CORE_WARN("MeshLoader::loadGltf: Triangle with an index out of range in {}, skipped.", ioBuild.name)
			continue;
		}
		mesh.addTriangle(corners);
		ioBuild.uvCoords.push_back(faceUv);
		ioBuild.normals.push_back(faceNormals);
	}
	if (const auto added = mesh.getTriangleCount() - firstTriangle; added > 0)
		ioBuild.loaded.submeshes.push_back({.firstIndex = static_cast<uint32_t>(firstTriangle * 3),
											.indexCount = static_cast<uint32_t>(added * 3),
											.materialIndex = slot});
}

auto buildGltf(const tinygltf::Model& iModel, const std::string& iName) -> LoadedMesh {
	LoadedMesh loaded;
	loaded.mesh = mkShared<StaticMesh>();
	if (iModel.meshes.empty()) {
		OWL_CORE_ERROR("MeshLoader::loadGltf: No mesh found in {}.", iName)
		return loaded;
	}
	loaded.materials.clear();
	for (const auto& source: iModel.materials) loaded.materials.push_back(convertGltfMaterial(iModel, source));
	const auto instances = meshInstances(iModel);
	// Triangles point at their vertices: every vertex is reserved before the first triangle is added.
	size_t vertexCount = 0;
	for (const auto& instance: instances) {
		for (const auto& primitive: iModel.meshes[instance.mesh].primitives) {
			if (const auto found = primitive.attributes.find("POSITION"); found != primitive.attributes.end())
				vertexCount += iModel.accessors.at(static_cast<size_type>(found->second)).count;
		}
	}
	loaded.mesh->reserveVertices(vertexCount);
	GltfBuild build{.loaded = loaded,
					.model = iModel,
					.name = iName,
					.uvCoords = {},
					.normals = {},
					.hasUv = false,
					.hasNormals = false,
					.defaultSlot = std::nullopt};
	for (const auto& instance: instances) {
		for (const auto& primitive: iModel.meshes[instance.mesh].primitives)
			appendPrimitive(build, primitive, instance.transform);
	}
	attachCornerData(*loaded.mesh, build.hasUv ? build.uvCoords : CornerUvs{},
					 build.hasNormals ? build.normals : CornerNormals{});
	logCompleteness("loadGltf", iName, *loaded.mesh, build.hasUv, build.hasNormals);
	if (loaded.materials.empty())
		loaded.materials.emplace_back();
	return loaded;
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
		return buildGltf(*model, iName);
	}
	if (iExtension == ".obj") {
		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		std::string warn;
		std::string err;
		std::istringstream stream{std::string{reinterpret_cast<const char*>(iData.data()), iData.size()}};
		ObjMaterialReader materialReader{iBaseDirectory, iReadFile};
		if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, &stream, &materialReader)) {
			OWL_CORE_ERROR("MeshLoader::loadObj: Failed to load {}: {}.", iName, err)
			return {};
		}
		if (!warn.empty())
			OWL_CORE_WARN("MeshLoader::loadObj: {}.", warn)
		return buildObj(attrib, shapes, materials, iName, iBaseDirectory, iReadFile);
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
	return buildObj(attrib, shapes, materials, iFilePath.string(), iFilePath.parent_path(), {}).mesh;
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
	return buildGltf(*model, iFilePath.string()).mesh;
}

}// namespace owl::data::geometry
