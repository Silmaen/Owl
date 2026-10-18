/**
 * @file AnimationDocument_test.cpp
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "document/AnimationDocument.h"
#include "testHelper.h"

#include <gtest/gtest.h>

#include <tuple>

using namespace owl;
using namespace owl::nest;

TEST(AnimationDocument, FramesRunFromTheTopLeftOfTheSheet) {
	AnimationDocument animation;
	auto& clip = animation.clip();
	clip.columns = 2;
	clip.rows = 2;
	clip.firstFrame = 0;
	clip.lastFrame = 3;
	animation.stop();
	auto [topLeft, bottomRight] = animation.currentFrameRect();
	EXPECT_EQ(topLeft, (math::vec2{0.f, 0.f}));
	EXPECT_EQ(bottomRight, (math::vec2{0.5f, 0.5f}));
	auto [uv0, uv1] = animation.currentFrameUv();
	EXPECT_EQ(uv0, (math::vec2{0.f, 1.f}));
	EXPECT_EQ(uv1, (math::vec2{0.5f, 0.5f}));

	animation.stepNext();
	std::tie(topLeft, bottomRight) = animation.currentFrameRect();
	EXPECT_EQ(topLeft, (math::vec2{0.5f, 0.f}));
	animation.stepNext();
	std::tie(topLeft, bottomRight) = animation.currentFrameRect();
	EXPECT_EQ(topLeft, (math::vec2{0.f, 0.5f}));
	EXPECT_EQ(bottomRight, (math::vec2{0.5f, 1.f}));
	std::tie(uv0, uv1) = animation.currentFrameUv();
	EXPECT_EQ(uv0, (math::vec2{0.f, 0.5f}));
	EXPECT_EQ(uv1, (math::vec2{0.5f, 0.f}));
}

TEST(AnimationDocument, FrameOutlineIsOnByDefault) {
	AnimationDocument animation;
	EXPECT_TRUE(animation.isFrameOutlined());
	animation.setFrameOutlined(false);
	EXPECT_FALSE(animation.isFrameOutlined());
}
