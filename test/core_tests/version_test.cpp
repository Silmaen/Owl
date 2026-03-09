
#include "testHelper.h"

#include <core/Core.h>

using namespace owl;

TEST(Core_Version, base) {
	EXPECT_STREQ("0.0.3", getVersionString().c_str());
	EXPECT_EQ(0x00000300, getVersionCode());
}
