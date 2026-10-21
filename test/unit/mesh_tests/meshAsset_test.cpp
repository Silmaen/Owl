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
	const auto& material = loaded.material;
	ASSERT_TRUE(material.hasBaseColorTexture());
	EXPECT_EQ(material.baseColorSize, (math::vec2ui{8, 8}));
	ASSERT_EQ(material.baseColorPixels.size(), 8u * 8u * 4u);
	EXPECT_EQ(material.baseColorName, "quadrants");
	EXPECT_EQ(material.baseColorFactor, (math::vec4{1.f, 1.f, 1.f, 1.f}));
	// The image is blue at its bottom-left and red at its top-left.
	const auto pixel = [&material](const size_t iX, const size_t iY) -> std::vector<uint8_t> {
		const size_t offset = (iY * 8u + iX) * 4u;
		return {material.baseColorPixels.begin() + static_cast<std::ptrdiff_t>(offset),
				material.baseColorPixels.begin() + static_cast<std::ptrdiff_t>(offset + 4)};
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
	EXPECT_TRUE(loaded.material.hasBaseColorTexture());

	const auto obj = readBytes(test::getTestAssetsPath("mesh_tests") / "multiple_triangle_uvnormal.obj");
	const auto fromObj = MeshLoader::loadMeshFromMemory(obj, ".obj", "multiple_triangle_uvnormal.obj");
	ASSERT_NE(fromObj.mesh, nullptr);
	EXPECT_EQ(fromObj.mesh->getTriangleCount(), 2u);
	EXPECT_TRUE(fromObj.mesh->isExtraDataDefinedOnAllTriangles<extradata::TriangleUVCoordinate>());
	EXPECT_FALSE(fromObj.material.hasBaseColorTexture());
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
