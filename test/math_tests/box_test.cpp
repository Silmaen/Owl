
#include "testHelper.h"

#include <math/box.h>

using namespace owl::math;

TEST(Box, Simple) {
	box2f box;
	EXPECT_TRUE(box.isEmpty());
	box2f box2{vec2{1, 2}, vec2{2, 3}};
	EXPECT_FALSE(box2.isEmpty());
	EXPECT_FALSE(box2.intersect(box));
	EXPECT_TRUE(box2.contains(vec2(1.5, 2.5)));
	box.update(box2);
	EXPECT_TRUE(box2.intersect(box));
	EXPECT_TRUE(box.contains(vec2(1.5, 2.5)));
	box2.update(vec2{45, 66});
	box2.update(vec2{-45, -66});
	EXPECT_TRUE(box2.contains(vec2(0, 0)));
	const box2f box3{vec2{2, 3}, vec2{1, 2}};
	EXPECT_NEAR(box3.diagonal().normSq(), 2, 1e-4);
}

TEST(Box, changes) {
	box2f boxE;
	boxE.scale(vec2{1, 2});
	EXPECT_TRUE(boxE.isEmpty());
	EXPECT_NEAR(boxE.min().x(), 0.f, 0.001);
	box2f box{vec2{-1, -2}, vec2{2, 3}};
	EXPECT_TRUE(box.contains(vec2(0, 0)));
	box.translate(vec2{1.1f, 2.1f});
	EXPECT_FALSE(box.contains(vec2(4.5f, 7.5f)));
	box.scale(vec2{1.5f, 1.5f});
	EXPECT_TRUE(box.contains(vec2(4.5f, 7.5f)));
}

TEST(Box, updates) {
	{
		box2f box;
		box.update(vec2{1, 2});
		box.update(vec2{-1, -2});
		EXPECT_NEAR(box.diagonal().normSq(), 20, 1e-4);
	}
	{
		box2f box;
		box.update(box2f{vec2{1, 2}, vec2{0, 0}});
		box.update(box2f{vec2{-1, -2}, vec2{0, 0}});
		EXPECT_NEAR(box.diagonal().normSq(), 20, 1e-4);
	}
}
