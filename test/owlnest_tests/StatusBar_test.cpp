/**
 * @file StatusBar_test.cpp
 * @author Silmaen
 * @date 10/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "panel/StatusBar.h"
#include "testHelper.h"

#include <core/Log.h>
#include <gui/UiLayer.h>
#include <gui/widgets/Toast.h>

#include <imgui.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <utility>

using namespace owl;
using namespace owl::nest::panel;

TEST(StatusBar, FormatsElapsedTime) {
	EXPECT_EQ(StatusBar::formatElapsed(-3.f), "just now");
	EXPECT_EQ(StatusBar::formatElapsed(4.9f), "just now");
	EXPECT_EQ(StatusBar::formatElapsed(12.4f), "12 s ago");
	EXPECT_EQ(StatusBar::formatElapsed(59.9f), "59 s ago");
	EXPECT_EQ(StatusBar::formatElapsed(185.f), "3 min ago");
	EXPECT_EQ(StatusBar::formatElapsed(7300.f), "2 h ago");
}

TEST(StatusBar, LabelsEveryMode) {
	EXPECT_EQ(StatusBar::getModeLabel(StatusInfo::Mode::Edit), "EDIT");
	EXPECT_EQ(StatusBar::getModeLabel(StatusInfo::Mode::Play), "PLAYING");
	EXPECT_EQ(StatusBar::getModeLabel(StatusInfo::Mode::Pause), "PAUSED");
}

TEST(StatusBar, CountsOnlyUnseenWarningsAndErrors) {
	StatusBar bar;
	EXPECT_EQ(bar.getUnseenWarnings(3), 3u);
	EXPECT_EQ(bar.getUnseenErrors(1), 1u);
	bar.acknowledge(3, 1);
	EXPECT_EQ(bar.getUnseenWarnings(3), 0u);
	EXPECT_EQ(bar.getUnseenErrors(1), 0u);
	EXPECT_EQ(bar.getUnseenWarnings(5), 2u);
	EXPECT_EQ(bar.getUnseenErrors(0), 0u);
	EXPECT_FALSE(bar.consumeOpenLogRequest());
}

TEST(StatusBar, RendersEveryModeWithoutRequest) {
	core::Log::init(core::Log::Level::Off);
	gui::UiLayer layer;
	layer.disableApp();
	layer.onAttach();
	ImGui::GetIO().IniFilename = nullptr;
	StatusBar bar;
	gui::widgets::ToastQueue toasts;
	toasts.push(gui::widgets::ToastKind::Success, "Saved main_menu.owl");
	for (const auto mode: {StatusInfo::Mode::Edit, StatusInfo::Mode::Play, StatusInfo::Mode::Pause}) {
		layer.begin();
		ImGui::Begin("Host");
		bar.onImGuiRender({.mode = mode,
						   .document = "main_menu.owl",
						   .entityCount = 18,
						   .secondsSinceAutosave = 12.f,
						   .backend = "Vulkan",
						   .device = "llvmpipe",
						   .previews = std::pair<size_t, size_t>{3, 8},
						   .notifications = &toasts,
						   .warningCount = 2,
						   .errorCount = 0});
		ImGui::End();
		layer.end();
	}
	EXPECT_FALSE(bar.consumeOpenLogRequest());
	EXPECT_EQ(bar.getUnseenWarnings(2), 2u);
	layer.onDetach();
	core::Log::invalidate();
}
