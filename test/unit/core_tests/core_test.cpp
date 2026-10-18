/**
 * @file core_test.cpp
 * @author Silmaen
 * @date 03/08/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Core.h>

#include <cstdint>

using namespace owl;

TEST(Core, ptr) {
	const auto totoUniq = mkUniq<int32_t>(3);
	const auto totoShared = mkShared<int32_t>(3);
	EXPECT_EQ(*totoUniq, *totoShared);
}
