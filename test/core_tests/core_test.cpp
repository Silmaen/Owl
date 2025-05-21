
#include "testHelper.h"

#include <core/Core.h>

using namespace owl;

TEST(Core, ptr) {
	const auto totoUniq = mkUniq<int32_t>(3);
	const auto totoShared = mkShared<int32_t>(3);
	EXPECT_EQ(*totoUniq, *totoShared);
}
