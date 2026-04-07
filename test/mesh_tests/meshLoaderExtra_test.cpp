/**
 * @file meshLoaderExtra_test.cpp
 * @author Silmaen
 * @date 06/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <data/geometry/MeshLoader.h>

using namespace owl::core;
using namespace owl::data;

TEST(MeshLoader, SupportedExtensions) {
	const auto extensions = MeshLoader::getSupportedExtensions();
	EXPECT_EQ(extensions.size(), 4u);
	bool hasObj = false;
	bool hasFbx = false;
	bool hasGltf = false;
	bool hasGlb = false;
	for (const auto& ext: extensions) {
		if (ext == ".obj")
			hasObj = true;
		if (ext == ".fbx")
			hasFbx = true;
		if (ext == ".gltf")
			hasGltf = true;
		if (ext == ".glb")
			hasGlb = true;
	}
	EXPECT_TRUE(hasObj);
	EXPECT_TRUE(hasFbx);
	EXPECT_TRUE(hasGltf);
	EXPECT_TRUE(hasGlb);
}

TEST(MeshLoader, NonExistentFile) {
	Log::init(Log::Level::Off);
	const auto mesh = MeshLoader::loadStaticMesh("nonexistent_file.obj");
	EXPECT_EQ(mesh, nullptr);
	Log::invalidate();
}

TEST(MeshLoader, UnsupportedExtension) {
	Log::init(Log::Level::Off);
	// Create a temp file with unsupported extension.
	const std::filesystem::path tmpFile("temp_mesh_test.xyz");
	{
		std::ofstream ofs(tmpFile);
		ofs << "dummy";
	}
	const auto mesh = MeshLoader::loadStaticMesh(tmpFile);
	EXPECT_EQ(mesh, nullptr);
	std::filesystem::remove(tmpFile);
	Log::invalidate();
}

TEST(MeshLoader, ObjVertexPositions) {
	Log::init(Log::Level::Off);
	const auto mesh = MeshLoader::loadStaticMesh(owl::test::getTestFilesDir() / "simple_triangle.obj");
	ASSERT_NE(mesh, nullptr);
	EXPECT_EQ(mesh->getVertexCount(), 3u);
	const auto& vertices = mesh->getVertices();
	EXPECT_EQ(vertices.size(), 3u);
	Log::invalidate();
}

TEST(MeshLoader, MeshIsNotEmpty) {
	Log::init(Log::Level::Off);
	const auto mesh = MeshLoader::loadStaticMesh(owl::test::getTestFilesDir() / "multiple_triangle.obj");
	ASSERT_NE(mesh, nullptr);
	EXPECT_FALSE(mesh->isEmpty());
	Log::invalidate();
}
