
#include "testHelper.h"

#include <core/Core.h>

using namespace owl;

TEST(Core_Version, base) {
	EXPECT_STREQ("0.1.0", getVersionString().c_str());
	EXPECT_EQ(0x00010000, getVersionCode());
}
