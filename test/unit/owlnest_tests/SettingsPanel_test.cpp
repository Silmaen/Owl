/**
 * @file SettingsPanel_test.cpp
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "panel/SettingsPanel.h"
#include "testHelper.h"

#include <core/Log.h>
#include <gui/UiLayer.h>

#include <imgui.h>

#include <gtest/gtest.h>

using namespace owl;
using namespace owl::nest;
using namespace owl::nest::panel;

TEST(SettingsPanel, RendersTheMcpTabWithoutChangingAnything) {
	core::Log::init(core::Log::Level::Off);
	gui::UiLayer layer;
	layer.disableApp();
	layer.onAttach();
	ImGui::GetIO().IniFilename = nullptr;
	SettingsPanel panel;
	panel.setMcpInfo({.status = "MCP listening",
					  .endpoint = "http://127.0.0.1:47800/mcp",
					  .tokenFile = "/run/user/1000/owlnest/mcp_token"});
	EditorSettings settings;
	for (int frame = 0; frame < 2; ++frame) {
		layer.begin();
		ImGui::Begin("Host");
		panel.renderMcpSection(settings);
		ImGui::End();
		layer.end();
	}
	EXPECT_FALSE(panel.consumeRegenerateTokenRequest());
	EXPECT_FALSE(settings.mcpEnabled);
	EXPECT_EQ(settings.mcpPort, EditorSettings{}.mcpPort);
	layer.onDetach();
	core::Log::invalidate();
}
