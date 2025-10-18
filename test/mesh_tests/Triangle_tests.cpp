#include "testHelper.h"

#include <data/geometry/primitive/Triangle.h>

using namespace owl::core;
using namespace owl::math;
using namespace owl::data::geometry::primitive;

TEST(Triangle, BasicOperations) {
	Log::init(Log::Level::Off);

	Triangle triangle;
	EXPECT_EQ(triangle.getIndex(), std::numeric_limits<uint32_t>::max());

	triangle.setIndex(3);
	EXPECT_EQ(triangle.getIndex(), 3);

	MeshVertex v0, v1, v2;
	v0.setIndex(0);
	v1.setIndex(1);
	v2.setIndex(2);

	triangle.getVertices()[0] = &v0;
	triangle.getVertices()[1] = &v1;
	triangle.getVertices()[2] = &v2;

	EXPECT_EQ(triangle.getVertex(0)->getIndex(), 0);
	EXPECT_EQ(triangle.getVertex(1)->getIndex(), 1);
	EXPECT_EQ(triangle.getVertex(2)->getIndex(), 2);

	Log::invalidate();
}


TEST(Triangle, SimpleOperations) {
	Log::init(Log::Level::Off);

	Triangle triangle;
	EXPECT_EQ(triangle.getIndex(), std::numeric_limits<uint32_t>::max());

	triangle.setIndex(7);
	EXPECT_EQ(triangle.getIndex(), 7);

	MeshVertex v0{{0, 0, 0}};
	MeshVertex v1{{1, 0, 0}};
	MeshVertex v2{{0, 1, 0}};
	v0.setIndex(10);
	v1.setIndex(11);
	v2.setIndex(12);

	triangle.getVertices()[0] = &v0;
	triangle.getVertices()[1] = &v1;
	triangle.getVertices()[2] = &v2;

	EXPECT_EQ(triangle.getNormal(), vec3(0.0f, 0.0f, 1.0f));

	Log::invalidate();
}
