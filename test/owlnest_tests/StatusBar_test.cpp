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
#include <string>
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

TEST(StatusBar, LabelsTheMcpIndicator) {
	EXPECT_EQ(StatusBar::getMcpLabel(StatusInfo::Mcp::Off), "MCP off");
	EXPECT_EQ(StatusBar::getMcpLabel(StatusInfo::Mcp::Listening), "MCP listening");
	EXPECT_EQ(StatusBar::getMcpLabel(StatusInfo::Mcp::ClientActive), "MCP client active");
	StatusInfo info;
	EXPECT_NE(StatusBar::getMcpTooltip(info).find("off"), std::string::npos);
	info.mcp = StatusInfo::Mcp::Listening;
	info.mcpEndpoint = "http://127.0.0.1:47800/mcp";
	info.mcpTokenFile = "/run/user/1000/owlnest/mcp_token";
	const auto tooltip = StatusBar::getMcpTooltip(info);
	EXPECT_NE(tooltip.find(info.mcpEndpoint), std::string::npos);
	EXPECT_NE(tooltip.find(info.mcpTokenFile), std::string::npos);
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
	EXPECT_FALSE(bar.consumeOpenMcpSettingsRequest());
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
	for (const auto& [mode, mcp]: {std::pair{StatusInfo::Mode::Edit, StatusInfo::Mcp::Off},
								   std::pair{StatusInfo::Mode::Play, StatusInfo::Mcp::Listening},
								   std::pair{StatusInfo::Mode::Pause, StatusInfo::Mcp::ClientActive}}) {
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
						   .errorCount = 0,
						   .mcp = mcp,
						   .mcpEndpoint = "http://127.0.0.1:47800/mcp",
						   .mcpTokenFile = "/run/user/1000/owlnest/mcp_token"});
		ImGui::End();
		layer.end();
	}
	EXPECT_FALSE(bar.consumeOpenLogRequest());
	EXPECT_FALSE(bar.consumeOpenMcpSettingsRequest());
	EXPECT_EQ(bar.getUnseenWarnings(2), 2u);
	layer.onDetach();
	core::Log::invalidate();
}
