#include "testHelper.h"

#include <data/geometry/extradata/TriangleUVCoordinate.h>

#include <data/geometry/StaticMesh.h>
#include <data/geometry/extradata/TriangleNormals.h>
#include <data/geometry/extradata/VertexNormal.h>

using namespace owl::core;
using namespace owl::math;
using namespace owl::data::geometry;

TEST(BasicStaticMesh, Creation) {
	Log::init(Log::Level::Off);

	const StaticMesh mesh;
	EXPECT_EQ(mesh.getVertexCount(), 0);
	EXPECT_EQ(mesh.getTriangleCount(), 0);
	EXPECT_EQ(mesh.getVertices().size(), 0);
	EXPECT_EQ(mesh.getTriangles().size(), 0);

	StaticMesh mesh2{mesh};
	EXPECT_EQ(mesh2.getVertexCount(), 0);
	StaticMesh mesh3{};
	mesh3 = mesh;
	EXPECT_EQ(mesh3.getVertexCount(), 0);
	StaticMesh mesh4 = std::move(mesh2);
	EXPECT_EQ(mesh4.getVertexCount(), 0);
	mesh4 = std::move(mesh3);
	EXPECT_EQ(mesh4.getVertexCount(), 0);

	EXPECT_TRUE(mesh.isEmpty());
	Log::invalidate();
}

TEST(BasicStaticMesh, CreateTrianglesAndVertices) {
	Log::init(Log::Level::Off);

	StaticMesh mesh;
	mesh.addVertex(vec3(0.0f, 0.0f, 0.0f));
	mesh.addVertex(vec3(1.0f, 0.0f, 0.0f));
	mesh.addVertex(vec3(0.0f, 1.0f, 0.0f));
	EXPECT_EQ(mesh.getVertexCount(), 3);

	mesh.addTriangle({0, 1, 2});
	EXPECT_EQ(mesh.getTriangleCount(), 1);

	const auto& triangle = mesh.getTriangles().at(0);
	EXPECT_EQ(triangle.getIndex(), 0);
	EXPECT_EQ(triangle.getVertex(0)->getPosition(), vec3(0.0f, 0.0f, 0.0f));
	EXPECT_EQ(triangle.getVertex(1)->getPosition(), vec3(1.0f, 0.0f, 0.0f));
	EXPECT_EQ(triangle.getVertex(2)->getPosition(), vec3(0.0f, 1.0f, 0.0f));

	EXPECT_FALSE(mesh.isEmpty());
	const StaticMesh cloneMesh = mesh.clone();

	mesh.clear();
	EXPECT_EQ(mesh.getVertexCount(), 0);
	EXPECT_EQ(mesh.getTriangleCount(), 0);
	EXPECT_TRUE(mesh.isEmpty());
	EXPECT_FALSE(cloneMesh.isEmpty());
	EXPECT_EQ(cloneMesh.getVertexCount(), 3);
	EXPECT_EQ(cloneMesh.getTriangleCount(), 1);

	Log::invalidate();
}

TEST(BasicStaticMesh, MoveTrianglesAndVertices) {
	Log::init(Log::Level::Off);

	StaticMesh mesh;
	mesh.addVertex(vec3(0.0f, 0.0f, 0.0f));
	mesh.addVertex(vec3(1.0f, 0.0f, 0.0f));
	mesh.addVertex(vec3(0.0f, 1.0f, 0.0f));
	EXPECT_EQ(mesh.getVertexCount(), 3);

	mesh.addTriangle({0, 1, 2});
	EXPECT_EQ(mesh.getTriangleCount(), 1);

	const StaticMesh movedMesh = std::move(mesh);
	EXPECT_EQ(movedMesh.getVertexCount(), 3);
	EXPECT_EQ(movedMesh.getTriangleCount(), 1);

	Log::invalidate();
}

TEST(BasicStaticMesh, ExtraData) {
	Log::init(Log::Level::Off);

	StaticMesh mesh;
	mesh.addVertex(vec3(0.0f, 0.0f, 0.0f));
	mesh.addVertex(vec3(1.0f, 0.0f, 0.0f));
	mesh.addVertex(vec3(0.0f, 1.0f, 0.0f));
	EXPECT_EQ(mesh.getVertexCount(), 3);

	mesh.addTriangle({0, 1, 2});
	EXPECT_EQ(mesh.getTriangleCount(), 1);

	mesh.addTriangleExtraData<extradata::TriangleUVCoordinate>();

	ASSERT_TRUE(mesh.isExtraDataDefinedOnAllTriangles<extradata::TriangleUVCoordinate>());

	const auto eds = mesh.createTriangleExtraDataRange<extradata::TriangleUVCoordinate>();
	ASSERT_FALSE(eds.empty());
	const auto& ed = eds.front();
	ASSERT_NE(nullptr, ed);
	ed->setUvCoord(0, vec2(0.0f, 0.0f));
	ed->setUvCoord(1, vec2(1.0f, 0.0f));
	ed->setUvCoord(2, vec2(0.0f, 1.0f));

	EXPECT_EQ(ed->getUvCoord(0), vec2(0.0f, 0.0f));
	EXPECT_EQ(ed->getUvCoord(1), vec2(1.0f, 0.0f));
	EXPECT_EQ(ed->getUvCoord(2), vec2(0.0f, 1.0f));
	EXPECT_TRUE(mesh.isExtraDataDefinedOnAllTriangles<extradata::TriangleUVCoordinate>());
	mesh.deleteTriangleExtraData<extradata::TriangleUVCoordinate>();
	EXPECT_FALSE(mesh.isExtraDataDefinedOnAllTriangles<extradata::TriangleUVCoordinate>());
}

TEST(BasicStaticMesh, MultipleExtraData) {

	Log::init(Log::Level::Off);

	StaticMesh mesh;
	mesh.addVertex(vec3(0.0f, 0.0f, 0.0f));
	mesh.addVertex(vec3(1.0f, 0.0f, 0.0f));
	mesh.addVertex(vec3(0.0f, 1.0f, 0.0f));
	EXPECT_EQ(mesh.getVertexCount(), 3);

	mesh.addTriangle({0, 1, 2});
	EXPECT_EQ(mesh.getTriangleCount(), 1);

	mesh.addTriangleExtraData<extradata::TriangleUVCoordinate>();
	mesh.addTriangleExtraData<extradata::TriangleNormals>();
	mesh.addVertexExtraData<extradata::VertexNormal>();

	ASSERT_TRUE(mesh.isExtraDataDefinedOnAllTriangles<extradata::TriangleUVCoordinate>());
	ASSERT_TRUE(mesh.isExtraDataDefinedOnAllTriangles<extradata::TriangleNormals>());
	ASSERT_TRUE(mesh.isExtraDataDefinedOnAllVertices<extradata::VertexNormal>());
}


TEST(BasicStaticMesh, MultipleExtraDataCopy) {

	Log::init(Log::Level::Off);

	StaticMesh mesh;
	mesh.addVertex(vec3(0.0f, 0.0f, 0.0f));
	mesh.addVertex(vec3(1.0f, 0.0f, 0.0f));
	mesh.addVertex(vec3(0.0f, 1.0f, 0.0f));
	EXPECT_EQ(mesh.getVertexCount(), 3);

	mesh.addTriangle({0, 1, 2});
	EXPECT_EQ(mesh.getTriangleCount(), 1);

	mesh.addTriangleExtraData<extradata::TriangleUVCoordinate>();
	mesh.addTriangleExtraData<extradata::TriangleNormals>();
	mesh.addVertexExtraData<extradata::VertexNormal>();

	{
		const StaticMesh meshCopy = mesh;

		EXPECT_TRUE(meshCopy.isExtraDataDefinedOnAllTriangles<extradata::TriangleUVCoordinate>());
		EXPECT_TRUE(meshCopy.isExtraDataDefinedOnAllTriangles<extradata::TriangleNormals>());
		EXPECT_TRUE(meshCopy.isExtraDataDefinedOnAllVertices<extradata::VertexNormal>());
	}
	{
		const StaticMesh meshCopy{mesh};

		EXPECT_TRUE(meshCopy.isExtraDataDefinedOnAllTriangles<extradata::TriangleUVCoordinate>());
		EXPECT_TRUE(meshCopy.isExtraDataDefinedOnAllTriangles<extradata::TriangleNormals>());
		EXPECT_TRUE(meshCopy.isExtraDataDefinedOnAllVertices<extradata::VertexNormal>());
	}
	{
		const StaticMesh meshCopy = std::move(mesh);

		EXPECT_TRUE(meshCopy.isExtraDataDefinedOnAllTriangles<extradata::TriangleUVCoordinate>());
		EXPECT_TRUE(meshCopy.isExtraDataDefinedOnAllTriangles<extradata::TriangleNormals>());
		EXPECT_TRUE(meshCopy.isExtraDataDefinedOnAllVertices<extradata::VertexNormal>());
		mesh = std::move(meshCopy);
	}
	{
		const StaticMesh meshCopy{std::move(mesh)};

		EXPECT_TRUE(meshCopy.isExtraDataDefinedOnAllTriangles<extradata::TriangleUVCoordinate>());
		EXPECT_TRUE(meshCopy.isExtraDataDefinedOnAllTriangles<extradata::TriangleNormals>());
		EXPECT_TRUE(meshCopy.isExtraDataDefinedOnAllVertices<extradata::VertexNormal>());
	}
}
