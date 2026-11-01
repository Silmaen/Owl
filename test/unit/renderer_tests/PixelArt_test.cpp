/**
 * @file PixelArt_test.cpp
 * @author Silmaen
 * @date 01/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <renderer/PixelArt.h>

using namespace owl;
using namespace owl::renderer;

TEST(PixelArt, inactiveWithoutAHeight) {
	const PixelArtSettings settings;
	EXPECT_FALSE(settings.isActive());
	EXPECT_TRUE(PixelArtSettings{.height = 180}.isActive());
}

TEST(PixelArt, theTargetKeepsTheAspectOfTheFrame) {
	const PixelArtSettings settings{.height = 180};
	EXPECT_EQ(settings.getTargetSize({1280, 720}), (math::vec2ui{320, 180}));
	EXPECT_EQ(settings.getTargetSize({1600, 960}), (math::vec2ui{300, 180}));
	EXPECT_EQ(settings.getTargetSize({100, 50}), (math::vec2ui{100, 50}));
	EXPECT_EQ(settings.getTargetSize({0, 0}), (math::vec2ui{1, 1}));
}

TEST(PixelArt, readFromTheConfigurationOfALayer) {
	const PixelArtSettings fallback{.height = 120, .colorLevels = 4, .outline = {1.f, 0.f, 0.f, 1.f}};
	const auto settings = PixelArtSettings::fromConfig("PixelHeight: 180\nPixelOutline: [0.1, 0.2, 0.3, 1]", fallback);
	EXPECT_EQ(settings.height, 180u);
	EXPECT_EQ(settings.colorLevels, 4u);
	EXPECT_FLOAT_EQ(settings.outline.g(), 0.2f);
	EXPECT_EQ(PixelArtSettings::fromConfig("not: [a map", fallback).height, 120u);
	EXPECT_EQ(PixelArtSettings::fromConfig("PixelHeight: 100000", fallback).height, 4096u);
}
