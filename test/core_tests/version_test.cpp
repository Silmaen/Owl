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
