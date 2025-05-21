
#include "testHelper.h"

#include <math/math.h>

using namespace owl::math;

TEST(math, baseAngles) {
	EXPECT_NEAR(degrees(radians(45.f)), 45.f, 0.001);
	EXPECT_NEAR(radians(degrees(1.2f)), 1.2f, 0.001);
	constexpr vec3 rot{15, 25, 35};
	vec3 trot = degrees(radians(rot));
	EXPECT_NEAR(rot.x(), trot.x(), 0.001);
	EXPECT_NEAR(rot.y(), trot.y(), 0.001);
	EXPECT_NEAR(rot.z(), trot.z(), 0.001);
}

TEST(math, epsilon) {
	EXPECT_NEAR(epsilon<float>(), 1.1920929e-7, 1e-7);
	EXPECT_TRUE(epsilonEqual(0.1f, 0.1f, epsilon<float>()));
	EXPECT_FALSE(epsilonEqual(0.1f, 0.100001f, epsilon<float>()));
	EXPECT_FALSE(epsilonNotEqual(0.1f, 0.1f, epsilon<float>()));
	EXPECT_TRUE(epsilonNotEqual(0.1f, 0.100001f, epsilon<float>()));
}
