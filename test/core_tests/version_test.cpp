/**
 * @file version_test.cpp
 * @author Silmaen
 * @date 16/01/2025
 * Copyright (c) 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Core.h>

#include <cstdint>
#include <format>

using namespace owl;

TEST(Core_Version, base) {
	// OWL_MAJOR/MINOR/PATCH come from project(VERSION) in CMakeLists.txt, the single source of the version.
	EXPECT_EQ(std::format("{}.{}.{}", OWL_MAJOR, OWL_MINOR, OWL_PATCH), getVersionString());
	EXPECT_EQ(static_cast<uint32_t>(OWL_MAJOR << 24 | OWL_MINOR << 16 | OWL_PATCH << 8), getVersionCode());
	EXPECT_EQ(OWL_MAJOR, getVersionMajor());
	EXPECT_EQ(OWL_MINOR, getVersionMinor());
	EXPECT_EQ(OWL_PATCH, getVersionPatch());
}
