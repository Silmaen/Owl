/**
 * @file testHelper.h
 * @author Silmaen
 * @date 03/08/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <gtest/gtest.h>

// This macro is used to generate a disabled test case.
#define TEST_DISABLED(test_case_name, test_name)                                                                       \
	GTEST_TEST_(test_case_name, DISABLED_UNIT##_##test_name, ::testing::Test, ::testing::internal::GetTestTypeId())

// Skip the current test (or every test of a fixture, from SetUp) when the engine module it needs is not built:
// OWL_REQUIRE_MODULE(PHYSICS) skips under OWL_MODULE_PHYSICS=OFF.
#define OWL_REQUIRE_MODULE(module)                                                                                     \
	if constexpr (!OWL_WITH_##module)                                                                                  \
	GTEST_SKIP() << "OWL_MODULE_" #module " is OFF"

template<typename T>
class TestWithParam : public ::testing::Test, public ::testing::WithParamInterface<T> {};

#include <filesystem>
#include <format>
#include <fstream>
#include <list>
#include <queue>
#include <stdexcept>
#include <string_view>

#include <entt/entt.hpp>

namespace owl::test {

inline auto getExecutableDir() -> std::filesystem::path { return std::filesystem::current_path(); }

inline auto getRootPath() -> std::filesystem::path {
	auto rootPath = getExecutableDir();
	while (rootPath.has_parent_path()) {
		rootPath = rootPath.parent_path();
		if (std::filesystem::exists(rootPath / "CMakeLists.txt") && std::filesystem::exists(rootPath / "source") &&
			std::filesystem::exists(rootPath / "test")) {
			return rootPath;
		}
	}
	throw std::runtime_error("Unable to find source root path.");
}

// Every fixture file of the tests lives in `test/unit/test_helper/assets/<category>/` (category = test folder name).
inline auto getTestAssetsPath(const std::string_view iCategory = {}) -> std::filesystem::path {
	std::filesystem::path assetsDir = getRootPath() / "test" / "unit" / "test_helper" / "assets";
	if (!iCategory.empty()) {
		assetsDir /= iCategory;
	}
	if (std::filesystem::is_directory(assetsDir)) {
		return assetsDir;
	}
	throw std::runtime_error(std::format("Test assets directory not found at {}.", assetsDir.string()));
}
}// namespace owl::test
