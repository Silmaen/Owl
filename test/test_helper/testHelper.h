/**
 * @file testHelper.h
 * @author Silmaen
 * @date 03/08/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <gtest/gtest.h>

// This macro is used to generate a disabled test case.
#define TEST_DISABLED(test_case_name, test_name)                                                                       \
	GTEST_TEST_(test_case_name, DISABLED_UNIT##_##test_name, ::testing::Test, ::testing::internal::GetTestTypeId())

template<typename T>
class TestWithParam : public ::testing::Test, public ::testing::WithParamInterface<T> {};

#include <filesystem>
#include <fstream>
#include <list>
#include <queue>

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

inline auto getTestFilesDir() -> std::filesystem::path {
	const std::filesystem::path rootPath = getRootPath();
	std::filesystem::path testFilesDir = rootPath / "test" / "test_helper" / "files";
	if (std::filesystem::exists(testFilesDir) && std::filesystem::is_directory(testFilesDir)) {
		return testFilesDir;
	}
	throw std::runtime_error(std::format("Test files directory not found at {}.", testFilesDir.string()));
}
}// namespace owl::test
