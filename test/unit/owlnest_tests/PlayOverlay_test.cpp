/**
 * @file PlayOverlay_test.cpp
 * @author Silmaen
 * @date 10/16/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "panel/PlayOverlay.h"

#include <gtest/gtest.h>

#include <array>

using namespace owl::nest::panel;

TEST(PlayBanner, HiddenInEdit) {
	PlayBanner banner;
	banner.update(PlayPhase::Edit, 0.1f);
	EXPECT_FALSE(banner.visible());
}

TEST(PlayBanner, ShowsFadesAndDisappears) {
	PlayBanner banner;
	banner.update(PlayPhase::Edit, 0.f);
	banner.update(PlayPhase::Play, 0.f);
	EXPECT_FLOAT_EQ(banner.alpha(), 1.f);
	banner.update(PlayPhase::Play, 1.f);
	EXPECT_FLOAT_EQ(banner.alpha(), 1.f);
	banner.update(PlayPhase::Play, PlayBanner::holdSeconds - 1.f + PlayBanner::fadeSeconds * 0.5f);
	EXPECT_NEAR(banner.alpha(), 0.5f, 1e-4f);
	banner.update(PlayPhase::Play, PlayBanner::fadeSeconds);
	EXPECT_FALSE(banner.visible());
	banner.update(PlayPhase::Play, 100.f);
	EXPECT_FALSE(banner.visible());
}

TEST(PlayBanner, PauseRestartsAndStopHides) {
	PlayBanner banner;
	banner.update(PlayPhase::Play, 0.f);
	banner.update(PlayPhase::Play, 10.f);
	EXPECT_FALSE(banner.visible());
	banner.update(PlayPhase::Pause, 0.f);
	EXPECT_TRUE(banner.visible());
	banner.update(PlayPhase::Edit, 0.f);
	EXPECT_FALSE(banner.visible());
}

TEST(PlayInputGate, PressOnGameReachesItUntilRelease) {
	PlayInputGate gate;
	EXPECT_FALSE(gate.filter(false, true, false));
	EXPECT_TRUE(gate.filter(true, true, false));
	EXPECT_TRUE(gate.filter(true, false, true));
	EXPECT_FALSE(gate.filter(false, false, false));
}

TEST(PlayInputGate, PressOnOverlayOrOutsideNeverReachesTheGame) {
	PlayInputGate gate;
	EXPECT_FALSE(gate.filter(true, true, true));
	EXPECT_FALSE(gate.filter(true, true, false));
	EXPECT_FALSE(gate.filter(false, true, false));
	EXPECT_FALSE(gate.filter(true, false, false));
	EXPECT_FALSE(gate.filter(false, false, false));
	EXPECT_TRUE(gate.filter(true, true, false));
}

TEST(PlayOverlay, RectContains) {
	constexpr std::array rects{OverlayRect{0.f, 0.f, 10.f, 10.f}, OverlayRect{20.f, 20.f, 30.f, 30.f}};
	EXPECT_TRUE(anyContains(rects, 2, 5.f, 5.f));
	EXPECT_TRUE(anyContains(rects, 2, 25.f, 25.f));
	EXPECT_FALSE(anyContains(rects, 1, 25.f, 25.f));
	EXPECT_FALSE(anyContains(rects, 2, 15.f, 15.f));
	EXPECT_FALSE(anyContains(rects, 2, 10.f, 5.f));
}
