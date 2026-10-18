/**
 * @file NavigationToolState_test.cpp
 * @author Silmaen
 * @date 10/17/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "panel/NavigationToolState.h"

#include <gtest/gtest.h>

using owl::nest::panel::NavigationToolState;

TEST(NavigationToolState, SelectingTurnsTheOverlayOn) {
	NavigationToolState tool;
	bool overlay = false;
	tool.toggle(overlay);
	EXPECT_TRUE(tool.active);
	EXPECT_TRUE(overlay);
}

TEST(NavigationToolState, LeavingRestoresAnOverlayThatWasOff) {
	NavigationToolState tool;
	bool overlay = false;
	tool.toggle(overlay);
	tool.toggle(overlay);
	EXPECT_FALSE(tool.active);
	EXPECT_FALSE(overlay);
}

TEST(NavigationToolState, LeavingKeepsAnOverlayThatWasOn) {
	NavigationToolState tool;
	bool overlay = true;
	tool.toggle(overlay);
	tool.toggle(overlay);
	EXPECT_FALSE(tool.active);
	EXPECT_TRUE(overlay);
}

TEST(NavigationToolState, SwitchingTheOverlayOffLeavesTheTool) {
	NavigationToolState tool;
	bool overlay = true;
	tool.toggle(overlay);
	overlay = false;
	tool.sync(overlay, true);
	EXPECT_FALSE(tool.active);
	EXPECT_FALSE(overlay);
}

TEST(NavigationToolState, ToolStaysWhileTheOverlayStaysOn) {
	NavigationToolState tool;
	bool overlay = false;
	tool.toggle(overlay);
	tool.sync(overlay, true);
	EXPECT_TRUE(tool.active);
	EXPECT_TRUE(overlay);
}

TEST(NavigationToolState, PlayingLeavesTheToolAndRestoresTheOverlay) {
	NavigationToolState tool;
	bool overlay = false;
	tool.toggle(overlay);
	tool.sync(overlay, false);
	EXPECT_FALSE(tool.active);
	EXPECT_FALSE(overlay);
}

TEST(NavigationToolState, OverlayChosenInPlayIsKept) {
	NavigationToolState tool;
	bool overlay = false;
	tool.sync(overlay, false);
	overlay = true;
	tool.sync(overlay, false);
	EXPECT_FALSE(tool.active);
	EXPECT_TRUE(overlay);
}
