/**
 * @file meshAsset_test.cpp
 * @author Silmaen
 * @date 21/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <data/geometry/MeshLoader.h>
#include <data/geometry/extradata/TriangleNormals.h>
#include <data/geometry/extradata/TriangleUVCoordinate.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

using namespace owl;
using namespace owl::data::geometry;

namespace {

auto cubePath() -> std::filesystem::path {
	return test::getTestAssetsPath("render_tests") / "meshes" / "textured_cube.glb";
}

auto readBytes(const std::filesystem::path& iPath) -> std::vector<uint8_t> {
	std::ifstream file(iPath, std::ios::binary);
	return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

class MeshAssetTest : public testing::Test {
protected:
	void SetUp() override { core::Log::init(core::Log::Level::Off); }

	void TearDown() override { core::Log::invalidate(); }
};

}// namespace

TEST_F(MeshAssetTest, onlyObjAndGltfAreMeshAssets) {
	EXPECT_TRUE(MeshLoader::isMeshAsset("crate.glb"));
	EXPECT_TRUE(MeshLoader::isMeshAsset("meshes/crate.gltf"));
	EXPECT_TRUE(MeshLoader::isMeshAsset("crate.obj"));
	EXPECT_FALSE(MeshLoader::isMeshAsset("crate.fbx"));
	EXPECT_FALSE(MeshLoader::isMeshAsset("crate.png"));
	EXPECT_EQ(MeshLoader::getMeshAssetExtensions().size(), 3u);
}

TEST_F(MeshAssetTest, glbWithTwoPrimitivesOffsetsTheSecondOne) {
	const auto loaded = MeshLoader::loadMesh(cubePath());
	ASSERT_NE(loaded.mesh, nullptr);
	EXPECT_EQ(loaded.mesh->getVertexCount(), 24u);
	EXPECT_EQ(loaded.mesh->getTriangleCount(), 12u);
	EXPECT_TRUE(loaded.mesh->isExtraDataDefinedOnAllTriangles<extradata::TriangleUVCoordinate>());
	EXPECT_TRUE(loaded.mesh->isExtraDataDefinedOnAllTriangles<extradata::TriangleNormals>());
	// The second primitive (faces 4 to 6) uses vertices 12 to 23.
	const auto& last = loaded.mesh->getTriangles().back();
	for (uint8_t corner = 0; corner < 3; ++corner) EXPECT_GE(last.getVertex(corner)->getIndex(), 12u);
}

TEST_F(MeshAssetTest, glbTexCoordsFollowTheEngineConvention) {
	const auto loaded = MeshLoader::loadMesh(cubePath());
	ASSERT_NE(loaded.mesh, nullptr);
	const auto uvs = loaded.mesh->createTriangleExtraDataRange<extradata::TriangleUVCoordinate>();
	ASSERT_FALSE(uvs.empty());
	ASSERT_NE(uvs.front(), nullptr);
	// The file gives (0, 1) to the first corner (bottom-left in glTF terms, v down): v = 0 in the engine.
	EXPECT_EQ(uvs.front()->getUvCoord(0), (math::vec2{0.f, 0.f}));
	EXPECT_EQ(uvs.front()->getUvCoord(2), (math::vec2{1.f, 1.f}));
}

TEST_F(MeshAssetTest, glbMaterialGivesTheBaseColourImageBottomRowFirst) {
	const auto loaded = MeshLoader::loadMesh(cubePath());
	ASSERT_FALSE(loaded.materials.empty());
	const auto& material = loaded.materials.front();
	ASSERT_TRUE(material.hasBaseColorTexture());
	EXPECT_EQ(material.baseColor.size, (math::vec2ui{8, 8}));
	ASSERT_EQ(material.baseColor.pixels.size(), 8u * 8u * 4u);
	EXPECT_EQ(material.baseColor.name, "quadrants");
	EXPECT_EQ(material.baseColorFactor, (math::vec4{1.f, 1.f, 1.f, 1.f}));
	// The image is blue at its bottom-left and red at its top-left.
	const auto pixel = [&material](const size_t iX, const size_t iY) -> std::vector<uint8_t> {
		const size_t offset = (iY * 8u + iX) * 4u;
		return {material.baseColor.pixels.begin() + static_cast<std::ptrdiff_t>(offset),
				material.baseColor.pixels.begin() + static_cast<std::ptrdiff_t>(offset + 4)};
	};
	EXPECT_EQ(pixel(0, 0), (std::vector<uint8_t>{40, 80, 230, 255}));
	EXPECT_EQ(pixel(0, 7), (std::vector<uint8_t>{230, 40, 40, 255}));
}

TEST_F(MeshAssetTest, memoryLoadMatchesTheFileLoad) {
	const auto bytes = readBytes(cubePath());
	ASSERT_FALSE(bytes.empty());
	const auto loaded = MeshLoader::loadMeshFromMemory(bytes, ".glb", "textured_cube.glb");
	ASSERT_NE(loaded.mesh, nullptr);
	EXPECT_EQ(loaded.mesh->getTriangleCount(), 12u);
	EXPECT_TRUE(loaded.materials.front().hasBaseColorTexture());

	const auto obj = readBytes(test::getTestAssetsPath("mesh_tests") / "multiple_triangle_uvnormal.obj");
	const auto fromObj = MeshLoader::loadMeshFromMemory(obj, ".obj", "multiple_triangle_uvnormal.obj");
	ASSERT_NE(fromObj.mesh, nullptr);
	EXPECT_EQ(fromObj.mesh->getTriangleCount(), 2u);
	EXPECT_TRUE(fromObj.mesh->isExtraDataDefinedOnAllTriangles<extradata::TriangleUVCoordinate>());
	EXPECT_FALSE(fromObj.materials.front().hasBaseColorTexture());
}

TEST_F(MeshAssetTest, unreadableInputsGiveNoMesh) {
	EXPECT_EQ(MeshLoader::loadMesh(test::getTestAssetsPath("mesh_tests") / "multiple_triangle_normal.fbx").mesh,
			  nullptr);
	EXPECT_EQ(MeshLoader::loadMesh("/nowhere/crate.glb").mesh, nullptr);
	const std::vector<uint8_t> garbage{1, 2, 3, 4};
	EXPECT_EQ(MeshLoader::loadMeshFromMemory(garbage, ".glb", "garbage.glb").mesh, nullptr);
	EXPECT_EQ(MeshLoader::loadMeshFromMemory(garbage, ".fbx", "garbage.fbx").mesh, nullptr);
}

TEST_F(MeshAssetTest, objCornersKeepTheirThirdTexCoord) {
	const auto mesh =
			MeshLoader::loadStaticMesh(test::getTestAssetsPath("mesh_tests") / "multiple_triangle_uvnormal.obj");
	ASSERT_NE(mesh, nullptr);
	const auto uvs = mesh->createTriangleExtraDataRange<extradata::TriangleUVCoordinate>();
	ASSERT_EQ(uvs.size(), 2u);
	ASSERT_NE(uvs[1], nullptr);
	// Second face: `f 2/2/1 4/4/1 3/3/1`.
	EXPECT_EQ(uvs[1]->getUvCoord(0), (math::vec2{1.f, 0.f}));
	EXPECT_EQ(uvs[1]->getUvCoord(1), (math::vec2{1.f, 1.f}));
	EXPECT_EQ(uvs[1]->getUvCoord(2), (math::vec2{0.f, 1.f}));
}

TEST_F(MeshAssetTest, gltfReadsItsBuffersThroughTheGivenReader) {
	const auto folder = test::getTestAssetsPath("mesh_tests");
	const auto gltf = readBytes(folder / "multiple_triangle_uv.gltf");
	std::vector<std::string> requested;
	const MeshFileReader reader = [&](const std::string& iPath) -> std::optional<std::vector<uint8_t>> {
		requested.push_back(iPath);
		if (iPath != "packed/multiple_triangle_uv.bin")
			return std::nullopt;
		return readBytes(folder / "multiple_triangle_uv.bin");
	};
	const auto loaded =
			MeshLoader::loadMeshFromMemory(gltf, ".gltf", "packed/multiple_triangle_uv.gltf", "packed", reader);
	ASSERT_NE(loaded.mesh, nullptr);
	EXPECT_EQ(loaded.mesh->getTriangleCount(), 2u);
	EXPECT_NE(std::ranges::find(requested, "packed/multiple_triangle_uv.bin"), requested.end());
	const MeshFileReader empty = [](const std::string&) -> std::optional<std::vector<uint8_t>> { return std::nullopt; };
	EXPECT_EQ(MeshLoader::loadMeshFromMemory(gltf, ".gltf", "packed/multiple_triangle_uv.gltf", "packed", empty).mesh,
			  nullptr);
}

TEST_F(MeshAssetTest, gltfListsItsExternalFiles) {
	const auto files =
			MeshLoader::getExternalFiles(test::getTestAssetsPath("mesh_tests") / "multiple_triangle_uv.gltf");
	ASSERT_EQ(files.size(), 1u);
	EXPECT_EQ(files.front(), "multiple_triangle_uv.bin");
	EXPECT_TRUE(MeshLoader::getExternalFiles(cubePath()).empty());
	EXPECT_TRUE(MeshLoader::getExternalFiles("/nowhere/crate.gltf").empty());
}

namespace {

auto twoMaterialsPath() -> std::filesystem::path { return test::getTestAssetsPath("mesh_tests") / "two_materials.glb"; }

}// namespace

TEST_F(MeshAssetTest, gltfPrimitivesBecomeSubmeshesWithTheirMaterial) {
	const auto loaded = MeshLoader::loadMesh(twoMaterialsPath());
	ASSERT_NE(loaded.mesh, nullptr);
	EXPECT_EQ(loaded.mesh->getTriangleCount(), 2u);
	ASSERT_EQ(loaded.materials.size(), 2u);
	ASSERT_EQ(loaded.submeshes.size(), 2u);
	EXPECT_EQ(loaded.submeshes[0].firstIndex, 0u);
	EXPECT_EQ(loaded.submeshes[0].indexCount, 3u);
	EXPECT_EQ(loaded.submeshes[0].materialIndex, 0u);
	EXPECT_EQ(loaded.submeshes[1].firstIndex, 3u);
	EXPECT_EQ(loaded.submeshes[1].indexCount, 3u);
	EXPECT_EQ(loaded.submeshes[1].materialIndex, 1u);
}

TEST_F(MeshAssetTest, gltfPlainMaterialReadsItsFactors) {
	const auto loaded = MeshLoader::loadMesh(twoMaterialsPath());
	ASSERT_EQ(loaded.materials.size(), 2u);
	const auto& red = loaded.materials[0];
	EXPECT_EQ(red.name, "Red");
	EXPECT_EQ(red.baseColorFactor, (math::vec4{1.f, 0.f, 0.f, 1.f}));
	EXPECT_FLOAT_EQ(red.metallicFactor, 0.25f);
	EXPECT_FLOAT_EQ(red.roughnessFactor, 0.75f);
	EXPECT_FLOAT_EQ(red.emissiveFactor.x(), 0.1f);
	EXPECT_FLOAT_EQ(red.emissiveFactor.y(), 0.2f);
	EXPECT_FLOAT_EQ(red.emissiveFactor.z(), 0.3f);
	EXPECT_FLOAT_EQ(red.emissiveStrength, 2.f);
	EXPECT_TRUE(red.doubleSided);
	EXPECT_EQ(red.alphaMode, MeshMaterial::AlphaMode::Opaque);
	EXPECT_FLOAT_EQ(red.normalScale, 1.f);
	EXPECT_FLOAT_EQ(red.occlusionStrength, 1.f);
	EXPECT_FLOAT_EQ(red.clearcoat, 0.f);
	EXPECT_TRUE(red.normal.isEmpty());
	EXPECT_TRUE(red.metallicRoughness.isEmpty());
	EXPECT_FALSE(red.hasBaseColorTexture());
}

TEST_F(MeshAssetTest, gltfLayeredMaterialReadsExtensionsAndImages) {
	const auto loaded = MeshLoader::loadMesh(twoMaterialsPath());
	ASSERT_EQ(loaded.materials.size(), 2u);
	const auto& green = loaded.materials[1];
	EXPECT_EQ(green.name, "Green");
	EXPECT_EQ(green.baseColorFactor, (math::vec4{0.f, 1.f, 0.f, 0.5f}));
	EXPECT_FLOAT_EQ(green.metallicFactor, 1.f);
	EXPECT_FLOAT_EQ(green.roughnessFactor, 0.1f);
	EXPECT_EQ(green.alphaMode, MeshMaterial::AlphaMode::Mask);
	EXPECT_FLOAT_EQ(green.alphaCutoff, 0.4f);
	EXPECT_FALSE(green.doubleSided);
	EXPECT_FLOAT_EQ(green.normalScale, 0.5f);
	EXPECT_FLOAT_EQ(green.occlusionStrength, 0.7f);
	EXPECT_FLOAT_EQ(green.clearcoat, 0.6f);
	EXPECT_FLOAT_EQ(green.clearcoatRoughness, 0.2f);
	EXPECT_FLOAT_EQ(green.sheenColor.x(), 0.1f);
	EXPECT_FLOAT_EQ(green.sheenColor.y(), 0.2f);
	EXPECT_FLOAT_EQ(green.sheenColor.z(), 0.3f);
	EXPECT_FLOAT_EQ(green.sheenRoughness, 0.4f);
	EXPECT_FLOAT_EQ(green.anisotropy, 0.3f);
	EXPECT_FLOAT_EQ(green.anisotropyRotation, 1.5f);
	EXPECT_FLOAT_EQ(green.emissiveStrength, 1.f);
	ASSERT_FALSE(green.normal.isEmpty());
	EXPECT_EQ(green.normal.size, (math::vec2ui{2, 2}));
	EXPECT_EQ(green.normal.pixels.size(), 2u * 2u * 4u);
	EXPECT_EQ(green.normal.name, "normal_map");
	// Bottom row first: the blue texel of the lower-left corner comes first.
	EXPECT_EQ(std::vector<uint8_t>(green.normal.pixels.begin(), green.normal.pixels.begin() + 4),
			  (std::vector<uint8_t>{0, 0, 255, 255}));
	ASSERT_FALSE(green.metallicRoughness.isEmpty());
	EXPECT_EQ(green.metallicRoughness.size, (math::vec2ui{4, 2}));
	ASSERT_FALSE(green.occlusion.isEmpty());
	EXPECT_EQ(green.occlusion.size, (math::vec2ui{4, 2}));
	EXPECT_TRUE(green.emissive.isEmpty());
	EXPECT_TRUE(green.baseColor.isEmpty());
}

TEST_F(MeshAssetTest, fileWithoutMaterialGivesOneDefaultMaterialAndOneSubmesh) {
	const auto loaded = MeshLoader::loadMesh(test::getTestAssetsPath("mesh_tests") / "simple_triangle.glb");
	ASSERT_NE(loaded.mesh, nullptr);
	ASSERT_EQ(loaded.materials.size(), 1u);
	EXPECT_EQ(loaded.materials.front().baseColorFactor, (math::vec4{1.f, 1.f, 1.f, 1.f}));
	EXPECT_FLOAT_EQ(loaded.materials.front().metallicFactor, 1.f);
	EXPECT_FLOAT_EQ(loaded.materials.front().roughnessFactor, 1.f);
	ASSERT_EQ(loaded.submeshes.size(), 1u);
	EXPECT_EQ(loaded.submeshes.front().firstIndex, 0u);
	EXPECT_EQ(loaded.submeshes.front().indexCount, static_cast<uint32_t>(loaded.mesh->getTriangleCount() * 3));
	EXPECT_EQ(loaded.submeshes.front().materialIndex, 0u);
}

TEST_F(MeshAssetTest, unreadableMeshKeepsADefaultMaterial) {
	const auto loaded = MeshLoader::loadMesh("/nowhere/crate.glb");
	EXPECT_EQ(loaded.mesh, nullptr);
	EXPECT_EQ(loaded.materials.size(), 1u);
	EXPECT_TRUE(loaded.submeshes.empty());
}

TEST_F(MeshAssetTest, objMaterialGroupsBecomeContiguousSubmeshes) {
	const auto loaded = MeshLoader::loadMesh(test::getTestAssetsPath("mesh_tests") / "two_materials.obj");
	ASSERT_NE(loaded.mesh, nullptr);
	EXPECT_EQ(loaded.mesh->getTriangleCount(), 3u);
	ASSERT_EQ(loaded.materials.size(), 2u);
	ASSERT_EQ(loaded.submeshes.size(), 2u);
	// Blue owns the first and third faces: they are grouped before the gold one.
	EXPECT_EQ(loaded.materials[loaded.submeshes[0].materialIndex].name, "Blue");
	EXPECT_EQ(loaded.submeshes[0].firstIndex, 0u);
	EXPECT_EQ(loaded.submeshes[0].indexCount, 6u);
	EXPECT_EQ(loaded.materials[loaded.submeshes[1].materialIndex].name, "Gold");
	EXPECT_EQ(loaded.submeshes[1].firstIndex, 6u);
	EXPECT_EQ(loaded.submeshes[1].indexCount, 3u);
}

TEST_F(MeshAssetTest, objMaterialsMapToPbrFactors) {
	const auto bytes = readBytes(test::getTestAssetsPath("mesh_tests") / "two_materials.obj");
	const auto loaded =
			MeshLoader::loadMeshFromMemory(bytes, ".obj", "two_materials.obj", test::getTestAssetsPath("mesh_tests"));
	ASSERT_EQ(loaded.materials.size(), 2u);
	const auto find = [&loaded](const std::string& iName) -> const MeshMaterial& {
		return *std::ranges::find(loaded.materials, iName, &MeshMaterial::name);
	};
	const auto& blue = find("Blue");
	EXPECT_FLOAT_EQ(blue.baseColorFactor.x(), 0.1f);
	EXPECT_FLOAT_EQ(blue.baseColorFactor.w(), 1.f);
	EXPECT_FLOAT_EQ(blue.metallicFactor, 0.f);
	EXPECT_NEAR(blue.roughnessFactor, std::sqrt(2.f / 32.f), 1e-5f);
	EXPECT_EQ(blue.alphaMode, MeshMaterial::AlphaMode::Opaque);
	const auto& gold = find("Gold");
	EXPECT_FLOAT_EQ(gold.baseColorFactor.w(), 0.5f);
	EXPECT_EQ(gold.alphaMode, MeshMaterial::AlphaMode::Blend);
	EXPECT_FLOAT_EQ(gold.roughnessFactor, 0.5f);
	EXPECT_FLOAT_EQ(gold.emissiveFactor.x(), 0.5f);
	EXPECT_FLOAT_EQ(gold.emissiveFactor.y(), 0.25f);
}

TEST_F(MeshAssetTest, objWithoutMaterialFileGivesOneDefaultMaterial) {
	const auto bytes = readBytes(test::getTestAssetsPath("mesh_tests") / "two_materials.obj");
	const auto loaded = MeshLoader::loadMeshFromMemory(bytes, ".obj", "two_materials.obj");
	ASSERT_NE(loaded.mesh, nullptr);
	EXPECT_EQ(loaded.materials.size(), 1u);
	ASSERT_EQ(loaded.submeshes.size(), 1u);
	EXPECT_EQ(loaded.submeshes.front().indexCount, 9u);
}

TEST_F(MeshAssetTest, gltfPlacesEveryNodeMeshWithItsTransform) {
	// One triangle (0,0,0) (1,0,0) (0,1,0), placed by a translated node and by a child of a scaled node.
	const std::string gltf = R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0,1]}],)"
							 R"("nodes":[{"mesh":0,"translation":[10,0,0]},{"scale":[2,2,2],"children":[2]},)"
							 R"({"mesh":0,"translation":[0,5,0]}],)"
							 R"("meshes":[{"primitives":[{"attributes":{"POSITION":0}}]}],)"
							 R"("accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3",)"
							 R"("min":[0,0,0],"max":[1,1,0]}],"bufferViews":[{"buffer":0,"byteLength":36}],)"
							 R"("buffers":[{"byteLength":36,"uri":"data:application/octet-stream;base64,)"
							 R"(AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAA"}]})";
	const std::vector<uint8_t> bytes(gltf.begin(), gltf.end());
	const auto loaded = MeshLoader::loadMeshFromMemory(bytes, ".gltf", "nodes.gltf");
	ASSERT_NE(loaded.mesh, nullptr);
	ASSERT_EQ(loaded.mesh->getTriangleCount(), 2u);
	const auto& translated = loaded.mesh->getTriangles().front();
	EXPECT_EQ(translated.getVertex(1)->getPosition(), (math::vec3{11.f, 0.f, 0.f}));
	const auto& scaled = loaded.mesh->getTriangles().back();
	EXPECT_EQ(scaled.getVertex(0)->getPosition(), (math::vec3{0.f, 10.f, 0.f}));
	EXPECT_EQ(scaled.getVertex(2)->getPosition(), (math::vec3{0.f, 12.f, 0.f}));
	EXPECT_EQ(loaded.submeshes.size(), 2u);
}
