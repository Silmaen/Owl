/**
 * @file ViewNavigation_test.cpp
 * @author Silmaen
 * @date 01/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "panel/ViewNavigation.h"
#include "EditorSettings.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <tuple>

using owl::nest::panel::ViewNavigation;
using owl::nest::panel::ViewNavigationMode;
using LeftDrag = owl::renderer::CameraEditor::LeftDrag;

TEST(ViewNavigation, EveryModeHasItsLeftDrag) {
	EXPECT_EQ(getLeftDrag(ViewNavigationMode::Select), LeftDrag::None);
	EXPECT_EQ(getLeftDrag(ViewNavigationMode::Orbit), LeftDrag::Orbit);
	EXPECT_EQ(getLeftDrag(ViewNavigationMode::Fly), LeftDrag::Look);
	EXPECT_EQ(getLeftDrag(ViewNavigationMode::Pan), LeftDrag::Pan);
}

TEST(ViewNavigation, NamesRoundTrip) {
	for (const auto mode: owl::nest::panel::g_viewNavigationModes) {
		EXPECT_EQ(owl::nest::panel::parseViewNavigationMode(getName(mode)), mode);
		EXPECT_NE(std::string{getHelp(mode)}, "");
		EXPECT_TRUE(std::string{getActionId(mode)}.starts_with("view.navigate"));
	}
	EXPECT_EQ(owl::nest::panel::parseViewNavigationMode("orbit"), ViewNavigationMode::Orbit);
	EXPECT_FALSE(owl::nest::panel::parseViewNavigationMode("walk").has_value());
}

TEST(ViewNavigation, SelectModePicksOnPress) {
	ViewNavigation navigation;
	EXPECT_TRUE(navigation.pressLeft(true));
	EXPECT_EQ(navigation.getActiveLeftDrag(), LeftDrag::None);
	EXPECT_FALSE(navigation.releaseLeft(0.f));
}

TEST(ViewNavigation, NavigationModesPickOnAClickOnly) {
	ViewNavigation navigation{.mode = ViewNavigationMode::Orbit};
	EXPECT_FALSE(navigation.pressLeft(true));
	EXPECT_EQ(navigation.getActiveLeftDrag(), LeftDrag::Orbit);
	EXPECT_TRUE(navigation.releaseLeft(1.f));
	EXPECT_EQ(navigation.getActiveLeftDrag(), LeftDrag::None);

	// A drag moved the view: its release selects nothing.
	EXPECT_FALSE(navigation.pressLeft(true));
	EXPECT_FALSE(navigation.releaseLeft(ViewNavigation::clickTravel + 1.f));
}

TEST(ViewNavigation, PressOnTheGizmoNeverMovesTheView) {
	ViewNavigation navigation{.mode = ViewNavigationMode::Pan};
	EXPECT_FALSE(navigation.pressLeft(false));
	EXPECT_EQ(navigation.getActiveLeftDrag(), LeftDrag::None);
	EXPECT_FALSE(navigation.releaseLeft(0.f));
}

TEST(ViewNavigation, SwitchingModeDuringADragKeepsTheDragGoing) {
	ViewNavigation navigation{.mode = ViewNavigationMode::Orbit};
	std::ignore = navigation.pressLeft(true);
	navigation.mode = ViewNavigationMode::Fly;
	EXPECT_EQ(navigation.getActiveLeftDrag(), LeftDrag::Look);
	navigation.mode = ViewNavigationMode::Select;
	EXPECT_EQ(navigation.getActiveLeftDrag(), LeftDrag::None);
}

TEST(ViewNavigation, SettingsKeepTheModeAndTheFlySpeed) {
	const auto file = std::filesystem::temp_directory_path() / "view_navigation_settings.yml";
	owl::nest::EditorSettings saved;
	saved.viewNavigation = ViewNavigationMode::Fly;
	saved.cameraFlySpeed = 12.5f;
	saved.saveToFile(file);
	owl::nest::EditorSettings loaded;
	loaded.loadFromFile(file);
	EXPECT_EQ(loaded.viewNavigation, ViewNavigationMode::Fly);
	EXPECT_FLOAT_EQ(loaded.cameraFlySpeed, 12.5f);
	std::filesystem::remove(file);
}
