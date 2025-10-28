#include "testHelper.h"

#include <data/geometry/MeshLoader.h>
#include <data/geometry/extradata/TriangleNormals.h>
#include <data/geometry/extradata/TriangleUVCoordinate.h>

using namespace owl::core;
using namespace owl::math;
using namespace owl::data;

TEST(LoadOBJMesh, SimpleTriangle) {
	Log::init(Log::Level::Off);

	const auto mesh = MeshLoader::loadStaticMesh(owl::test::getTestFilesDir() / "simple_triangle.obj");

	ASSERT_NE(mesh, nullptr);
	EXPECT_EQ(mesh->getVertexCount(), 3u);
	EXPECT_EQ(mesh->getTriangleCount(), 1u);

	EXPECT_FALSE(mesh->isExtraDataDefinedOnAllTriangles<owl::data::geometry::extradata::TriangleNormals>());
	EXPECT_FALSE(mesh->isExtraDataDefinedOnAllTriangles<owl::data::geometry::extradata::TriangleUVCoordinate>());

	Log::invalidate();
}

TEST(LoadOBJMesh, MultipleTriangles) {
	Log::init(Log::Level::Off);

	const auto mesh = MeshLoader::loadStaticMesh(owl::test::getTestFilesDir() / "multiple_triangle.obj");
	ASSERT_NE(mesh, nullptr);
	EXPECT_EQ(mesh->getVertexCount(), 4u);
	EXPECT_EQ(mesh->getTriangleCount(), 2u);

	EXPECT_FALSE(mesh->isExtraDataDefinedOnAllTriangles<owl::data::geometry::extradata::TriangleNormals>());
	EXPECT_FALSE(mesh->isExtraDataDefinedOnAllTriangles<owl::data::geometry::extradata::TriangleUVCoordinate>());

	Log::invalidate();
}

TEST(LoadOBJMesh, TriangleWithUVs) {
	Log::init(Log::Level::Off);

	const auto mesh = MeshLoader::loadStaticMesh(owl::test::getTestFilesDir() / "multiple_triangle_uv.obj");
	ASSERT_NE(mesh, nullptr);
	EXPECT_EQ(mesh->getVertexCount(), 4u);
	EXPECT_EQ(mesh->getTriangleCount(), 2u);

	EXPECT_FALSE(mesh->isExtraDataDefinedOnAllTriangles<owl::data::geometry::extradata::TriangleNormals>());
	EXPECT_TRUE(mesh->isExtraDataDefinedOnAllTriangles<owl::data::geometry::extradata::TriangleUVCoordinate>());

	Log::invalidate();
}

TEST(LoadOBJMesh, TriangleWithNormals) {
	Log::init(Log::Level::Off);

	const auto mesh = MeshLoader::loadStaticMesh(owl::test::getTestFilesDir() / "multiple_triangle_normal.obj");
	ASSERT_NE(mesh, nullptr);
	EXPECT_EQ(mesh->getVertexCount(), 4u);
	EXPECT_EQ(mesh->getTriangleCount(), 2u);

	EXPECT_TRUE(mesh->isExtraDataDefinedOnAllTriangles<owl::data::geometry::extradata::TriangleNormals>());
	EXPECT_FALSE(mesh->isExtraDataDefinedOnAllTriangles<owl::data::geometry::extradata::TriangleUVCoordinate>());

	Log::invalidate();
}

TEST(LoadOBJMesh, TriangleWithUVsAndNormals) {
	Log::init(Log::Level::Off);

	const auto mesh = MeshLoader::loadStaticMesh(owl::test::getTestFilesDir() / "multiple_triangle_uvnormal.obj");
	ASSERT_NE(mesh, nullptr);
	EXPECT_EQ(mesh->getVertexCount(), 4u);
	EXPECT_EQ(mesh->getTriangleCount(), 2u);

	EXPECT_TRUE(mesh->isExtraDataDefinedOnAllTriangles<owl::data::geometry::extradata::TriangleNormals>());
	EXPECT_TRUE(mesh->isExtraDataDefinedOnAllTriangles<owl::data::geometry::extradata::TriangleUVCoordinate>());

	Log::invalidate();
}


TEST(LoadGLTFMesh, SimpleTriangle) {
	Log::init(Log::Level::Off);

	const auto mesh = MeshLoader::loadStaticMesh(owl::test::getTestFilesDir() / "simple_triangle.gltf");

	ASSERT_NE(mesh, nullptr);
	EXPECT_EQ(mesh->getVertexCount(), 3u);
	EXPECT_EQ(mesh->getTriangleCount(), 1u);

	EXPECT_FALSE(mesh->isExtraDataDefinedOnAllTriangles<owl::data::geometry::extradata::TriangleNormals>());
	EXPECT_FALSE(mesh->isExtraDataDefinedOnAllTriangles<owl::data::geometry::extradata::TriangleUVCoordinate>());

	Log::invalidate();
}

TEST(LoadGLTFMesh, MultipleTriangles) {
	Log::init(Log::Level::Off);

	const auto mesh = MeshLoader::loadStaticMesh(owl::test::getTestFilesDir() / "multiple_triangle.gltf");
	ASSERT_NE(mesh, nullptr);
	EXPECT_EQ(mesh->getVertexCount(), 4u);
	EXPECT_EQ(mesh->getTriangleCount(), 2u);

	EXPECT_FALSE(mesh->isExtraDataDefinedOnAllTriangles<owl::data::geometry::extradata::TriangleNormals>());
	EXPECT_FALSE(mesh->isExtraDataDefinedOnAllTriangles<owl::data::geometry::extradata::TriangleUVCoordinate>());

	Log::invalidate();
}

TEST(LoadGLTFMesh, TriangleWithUVs) {
	Log::init(Log::Level::Off);

	const auto mesh = MeshLoader::loadStaticMesh(owl::test::getTestFilesDir() / "multiple_triangle_uv.gltf");
	ASSERT_NE(mesh, nullptr);
	EXPECT_EQ(mesh->getVertexCount(), 4u);
	EXPECT_EQ(mesh->getTriangleCount(), 2u);

	EXPECT_FALSE(mesh->isExtraDataDefinedOnAllTriangles<owl::data::geometry::extradata::TriangleNormals>());
	EXPECT_TRUE(mesh->isExtraDataDefinedOnAllTriangles<owl::data::geometry::extradata::TriangleUVCoordinate>());

	Log::invalidate();
}

TEST(LoadGLTFMesh, TriangleWithNormals) {
	Log::init(Log::Level::Off);

	const auto mesh = MeshLoader::loadStaticMesh(owl::test::getTestFilesDir() / "multiple_triangle_normal.gltf");
	ASSERT_NE(mesh, nullptr);
	EXPECT_EQ(mesh->getVertexCount(), 4u);
	EXPECT_EQ(mesh->getTriangleCount(), 2u);

	EXPECT_TRUE(mesh->isExtraDataDefinedOnAllTriangles<owl::data::geometry::extradata::TriangleNormals>());
	EXPECT_FALSE(mesh->isExtraDataDefinedOnAllTriangles<owl::data::geometry::extradata::TriangleUVCoordinate>());

	Log::invalidate();
}

TEST(LoadGLTFMesh, TriangleWithUVsAndNormals) {
	Log::init(Log::Level::Off);

	const auto mesh = MeshLoader::loadStaticMesh(owl::test::getTestFilesDir() / "multiple_triangle_uvnormal.gltf");
	ASSERT_NE(mesh, nullptr);
	EXPECT_EQ(mesh->getVertexCount(), 4u);
	EXPECT_EQ(mesh->getTriangleCount(), 2u);

	EXPECT_TRUE(mesh->isExtraDataDefinedOnAllTriangles<owl::data::geometry::extradata::TriangleNormals>());
	EXPECT_TRUE(mesh->isExtraDataDefinedOnAllTriangles<owl::data::geometry::extradata::TriangleUVCoordinate>());

	Log::invalidate();
}


TEST(LoadGLBMesh, SimpleTriangle) {
	Log::init(Log::Level::Off);

	const auto mesh = MeshLoader::loadStaticMesh(owl::test::getTestFilesDir() / "simple_triangle.glb");

	ASSERT_NE(mesh, nullptr);
	EXPECT_EQ(mesh->getVertexCount(), 3u);
	EXPECT_EQ(mesh->getTriangleCount(), 1u);

	EXPECT_FALSE(mesh->isExtraDataDefinedOnAllTriangles<owl::data::geometry::extradata::TriangleNormals>());
	EXPECT_FALSE(mesh->isExtraDataDefinedOnAllTriangles<owl::data::geometry::extradata::TriangleUVCoordinate>());

	Log::invalidate();
}

TEST(LoadGLBMesh, MultipleTriangles) {
	Log::init(Log::Level::Off);

	const auto mesh = MeshLoader::loadStaticMesh(owl::test::getTestFilesDir() / "multiple_triangle.glb");
	ASSERT_NE(mesh, nullptr);
	EXPECT_EQ(mesh->getVertexCount(), 4u);
	EXPECT_EQ(mesh->getTriangleCount(), 2u);

	EXPECT_FALSE(mesh->isExtraDataDefinedOnAllTriangles<owl::data::geometry::extradata::TriangleNormals>());
	EXPECT_FALSE(mesh->isExtraDataDefinedOnAllTriangles<owl::data::geometry::extradata::TriangleUVCoordinate>());

	Log::invalidate();
}

TEST(LoadGLBMesh, TriangleWithUVs) {
	Log::init(Log::Level::Off);

	const auto mesh = MeshLoader::loadStaticMesh(owl::test::getTestFilesDir() / "multiple_triangle_uv.glb");
	ASSERT_NE(mesh, nullptr);
	EXPECT_EQ(mesh->getVertexCount(), 4u);
	EXPECT_EQ(mesh->getTriangleCount(), 2u);

	EXPECT_FALSE(mesh->isExtraDataDefinedOnAllTriangles<owl::data::geometry::extradata::TriangleNormals>());
	EXPECT_TRUE(mesh->isExtraDataDefinedOnAllTriangles<owl::data::geometry::extradata::TriangleUVCoordinate>());

	Log::invalidate();
}

TEST(LoadGLBMesh, TriangleWithNormals) {
	Log::init(Log::Level::Off);

	const auto mesh = MeshLoader::loadStaticMesh(owl::test::getTestFilesDir() / "multiple_triangle_normal.glb");
	ASSERT_NE(mesh, nullptr);
	EXPECT_EQ(mesh->getVertexCount(), 4u);
	EXPECT_EQ(mesh->getTriangleCount(), 2u);

	EXPECT_TRUE(mesh->isExtraDataDefinedOnAllTriangles<owl::data::geometry::extradata::TriangleNormals>());
	EXPECT_FALSE(mesh->isExtraDataDefinedOnAllTriangles<owl::data::geometry::extradata::TriangleUVCoordinate>());

	Log::invalidate();
}

TEST(LoadGLBMesh, TriangleWithUVsAndNormals) {
	Log::init(Log::Level::Off);

	const auto mesh = MeshLoader::loadStaticMesh(owl::test::getTestFilesDir() / "multiple_triangle_uvnormal.glb");
	ASSERT_NE(mesh, nullptr);
	EXPECT_EQ(mesh->getVertexCount(), 4u);
	EXPECT_EQ(mesh->getTriangleCount(), 2u);

	EXPECT_TRUE(mesh->isExtraDataDefinedOnAllTriangles<owl::data::geometry::extradata::TriangleNormals>());
	EXPECT_TRUE(mesh->isExtraDataDefinedOnAllTriangles<owl::data::geometry::extradata::TriangleUVCoordinate>());

	Log::invalidate();
}

TEST(LoadFBXMesh, TriangleWithNormals) {
	Log::init(Log::Level::Off);

	const auto mesh = MeshLoader::loadStaticMesh(owl::test::getTestFilesDir() / "multiple_triangle_normal.fbx");
	ASSERT_NE(mesh, nullptr);
	EXPECT_EQ(mesh->getVertexCount(), 4u);
	EXPECT_EQ(mesh->getTriangleCount(), 2u);

	EXPECT_TRUE(mesh->isExtraDataDefinedOnAllTriangles<owl::data::geometry::extradata::TriangleNormals>());
	EXPECT_FALSE(mesh->isExtraDataDefinedOnAllTriangles<owl::data::geometry::extradata::TriangleUVCoordinate>());

	Log::invalidate();
}

TEST(LoadFBXMesh, TriangleWithUVsAndNormals) {
	Log::init(Log::Level::Off);

	const auto mesh = MeshLoader::loadStaticMesh(owl::test::getTestFilesDir() / "multiple_triangle_uvnormal.fbx");
	ASSERT_NE(mesh, nullptr);
	EXPECT_EQ(mesh->getVertexCount(), 4u);
	EXPECT_EQ(mesh->getTriangleCount(), 2u);

	EXPECT_TRUE(mesh->isExtraDataDefinedOnAllTriangles<owl::data::geometry::extradata::TriangleNormals>());
	EXPECT_TRUE(mesh->isExtraDataDefinedOnAllTriangles<owl::data::geometry::extradata::TriangleUVCoordinate>());

	Log::invalidate();
}
