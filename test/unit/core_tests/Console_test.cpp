/**
 * @file Console_test.cpp
 * @author Silmaen
 * @date 19/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <platform/Console.h>

#include <gtest/gtest.h>

using namespace owl;

// CTest redirects both streams (pipe or file): they must stay where they are, on every platform.
TEST(Console, KeepsRedirectedOutput) { EXPECT_FALSE(platform::attachParentConsole()); }
