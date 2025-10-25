#include "testHelper.h"

#include <data/geometry/primitive/MeshVertex.h>

using namespace owl::core;
using namespace owl::math;
using namespace owl::data::geometry::primitive;

TEST(MeshVertex, BaseicOperations) {
	Log::init(Log::Level::Off);

	MeshVertex vertex;
	EXPECT_EQ(vertex.getIndex(), std::numeric_limits<uint32_t>::max());
	vertex.setIndex(5);

	// Reset from GPointDouble
	constexpr vec3 newCoords(4.0, 5.0, 6.0);
	vertex.setPosition(newCoords);
	EXPECT_EQ(vertex.getIndex(), 5);
	EXPECT_EQ(vertex.getPosition(), newCoords);

	// Reset from VertexBase
	MeshVertex sourceVertex;
	sourceVertex.setIndex(10);
	sourceVertex.setPosition(vec3(7.0, 8.0, 9.0));


	vertex = sourceVertex;
	EXPECT_EQ(vertex.getIndex(), 10);
	EXPECT_EQ(vertex.getPosition(), vec3(7.0, 8.0, 9.0));
	EXPECT_EQ(vertex, sourceVertex);

	Log::invalidate();
}
