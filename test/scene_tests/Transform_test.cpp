
#include "testHelper.h"

#include <scene/component/Transform.h>

using namespace owl::scene::component;

TEST(Scene, Transform) {
	const Transform tr;
	auto mat = tr.getTransform();
	EXPECT_EQ(mat(0, 0), 1.0);
	EXPECT_EQ(mat(1, 1), 1.0);
	EXPECT_EQ(mat(2, 2), 1.0);
	EXPECT_EQ(mat(3, 3), 1.0);
	EXPECT_EQ(mat(0, 1), 0.0);
}
