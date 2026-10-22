/**
 * @file ClipSpace_test.cpp
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <math/linAlgebra.h>
#include <math/matrixCreation.h>
#include <math/trigonometry.h>
#include <renderer/ClipSpace.h>

#include <cstddef>

using namespace owl;
using namespace owl::renderer;

TEST(ClipSpace, VulkanNegatesTheYRowOnly) {
	const auto projection = math::perspectiveReversedZ(math::radians(60.f), 1.5f, 0.1f, 100.f);
	const auto openGl = toClipSpace(projection, ClipSpace::OpenGl);
	const auto vulkan = toClipSpace(projection, ClipSpace::Vulkan);
	EXPECT_EQ(openGl, projection);
	for (size_t column = 0; column < 4; ++column) {
		EXPECT_FLOAT_EQ(vulkan(1, column), -projection(1, column));
		EXPECT_FLOAT_EQ(vulkan(2, column), projection(2, column));
	}
}

TEST(ClipSpace, CameraRelativeViewKeepsTheRotationAndDropsTheEye) {
	const math::vec3 eye{1.e5f, 20.f, -3.e4f};
	const auto world = math::translate(math::identity<float, 4>(), eye) *
					   math::rotate(math::identity<float, 4>(), 0.7f, math::vec3{0.f, 1.f, 0.f});
	const auto view = math::inverse(world);
	const auto relative = cameraRelativeView(view);
	for (size_t row = 0; row < 3; ++row) {
		EXPECT_FLOAT_EQ(relative(row, 3), 0.f);
		for (size_t column = 0; column < 3; ++column) EXPECT_FLOAT_EQ(relative(row, column), view(row, column));
	}
	// A point near the far-away eye, given relative to it, lands where the world view puts it.
	const math::vec3 offset{0.5f, -0.25f, -2.f};
	const math::vec4 fromRelative = relative * math::vec4{offset.x(), offset.y(), offset.z(), 1.f};
	const math::vec4 expected = math::rotate(math::identity<float, 4>(), -0.7f, math::vec3{0.f, 1.f, 0.f}) *
								math::vec4{offset.x(), offset.y(), offset.z(), 1.f};
	EXPECT_NEAR(fromRelative.x(), expected.x(), 1e-5f);
	EXPECT_NEAR(fromRelative.z(), expected.z(), 1e-5f);
}
