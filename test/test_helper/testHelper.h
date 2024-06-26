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

#include <entt/entt.hpp>
