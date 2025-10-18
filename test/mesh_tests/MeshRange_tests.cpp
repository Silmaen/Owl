#include "testHelper.h"

#include <data/component/MeshComponentExtraData.h>
#include <data/geometry/MeshRange.h>
#include <data/geometry/StaticMesh.h>
#include <data/geometry/extradata/TriangleNormals.h>
#include <data/geometry/extradata/TriangleUVCoordinate.h>
#include <data/geometry/extradata/VertexNormal.h>

using namespace owl::core;
using namespace owl::math;
using namespace owl::data::geometry;
using namespace owl::data::component;

namespace {
auto createTestMesh() -> StaticMesh {
	StaticMesh mesh;
	mesh.addVertex(vec3(0.0f, 0.0f, 0.0f));
	mesh.addVertex(vec3(1.0f, 0.0f, 0.0f));
	mesh.addVertex(vec3(0.0f, 1.0f, 0.0f));
	mesh.addVertex(vec3(1.0f, 1.0f, 0.0f));
	mesh.addTriangle({0, 1, 2});
	mesh.addTriangle({1, 2, 3});
	return mesh;
}
}// namespace

TEST(MeshRange, VertexIteration) {
	Log::init(Log::Level::Off);

	StaticMesh mesh = createTestMesh();
	EXPECT_EQ(mesh.getVertexCount(), 4);
	EXPECT_EQ(mesh.getTriangleCount(), 2);

	for (auto coord: MeshVertexRange(mesh, Coordinates)) {
		EXPECT_TRUE(coord.x() >= 0.0f && coord.x() <= 1.0f);
		EXPECT_TRUE(coord.y() >= 0.0f && coord.y() <= 1.0f);
		EXPECT_EQ(coord.z(), 0.0f);
	}

	// Modify all vertex coordinates
	for (auto& coord: MeshVertexRange(mesh, EditCoordinates)) {

		coord.setValue(coord.value() + vec3(0.0f, 0.0f, 1.0f));
	}
	for (auto coord: MeshVertexRange(mesh, Coordinates)) {
		EXPECT_TRUE(coord.x() >= 0.0f && coord.x() <= 1.0f);
		EXPECT_TRUE(coord.y() >= 0.0f && coord.y() <= 1.0f);
		EXPECT_EQ(coord.z(), 1.0f);
	}

	Log::invalidate();
}

TEST(MeshRange, ExtraDataIteration) {
	Log::init(Log::Level::Off);

	StaticMesh mesh = createTestMesh();
	EXPECT_EQ(mesh.getVertexCount(), 4);
	EXPECT_EQ(mesh.getTriangleCount(), 2);

	mesh.addTriangleExtraData<extradata::TriangleUVCoordinate>();
	mesh.addTriangleExtraData<extradata::TriangleNormals>();
	mesh.addVertexExtraData<extradata::VertexNormal>();

	for (auto&[uvCoord, normal]: MeshTriangleRange(mesh, WriteUvCoords, WriteTriangleNormals)) {
		uvCoord.value()->setUvCoord(0, vec2(0.0f, 0.0f));
		uvCoord.value()->setUvCoord(1, vec2(1.0f, 0.0f));
		uvCoord.value()->setUvCoord(2, vec2(0.0f, 1.0f));
		normal.value()->setNormal(0, vec3(0.0f, 0.0f, 1.0f));
		normal.value()->setNormal(1, vec3(0.0f, 0.0f, 1.0f));
		normal.value()->setNormal(2, vec3(0.0f, 0.0f, 1.0f));
	}
	for (auto& normal: MeshVertexRange(mesh, WriteVertexNormal)) { normal.value()->setNormal(vec3(0.0f, 0.0f, 1.0f)); }

	for (const auto& [uvCoord, normal]: MeshTriangleRange(mesh, UvCoords, TriangleNormals)) {
		EXPECT_EQ(uvCoord.value()->getUvCoord(0), vec2(0.0f, 0.0f));
		EXPECT_EQ(uvCoord.value()->getUvCoord(1), vec2(1.0f, 0.0f));
		EXPECT_EQ(uvCoord.value()->getUvCoord(2), vec2(0.0f, 1.0f));
		EXPECT_EQ(normal.value()->getNormal(0), vec3(0.0f, 0.0f, 1.0f));
		EXPECT_EQ(normal.value()->getNormal(1), vec3(0.0f, 0.0f, 1.0f));
		EXPECT_EQ(normal.value()->getNormal(2), vec3(0.0f, 0.0f, 1.0f));
	}
	for (const auto& normal: MeshVertexRange(mesh, VertexNormal)) {
		EXPECT_EQ(normal.value()->getNormal(), vec3(0.0f, 0.0f, 1.0f));
	}
	Log::invalidate();
}
