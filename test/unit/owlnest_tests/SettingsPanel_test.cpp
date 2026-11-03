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

#include <string>
#include <vector>

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

TEST(SettingsPanel, EditsADraftUntilApply) {
	core::Log::init(core::Log::Level::Off);
	SettingsPanel panel;
	const ActionRegistry registry;
	EditorSettings settings;
	settings.recentProjects = {"a"};
	panel.beginEdit(settings, registry);
	ASSERT_TRUE(panel.getDraft().has_value());
	EXPECT_FALSE(panel.hasPendingChanges(settings));
	panel.getDraft()->showStats = !settings.showStats;
	panel.getDraft()->snapStep = 0.25f;
	panel.getDraft()->recentProjects = {"b"};
	EXPECT_TRUE(panel.hasPendingChanges(settings));
	EXPECT_NE(settings.snapStep, 0.25f);
	panel.apply(settings, registry);
	EXPECT_FALSE(panel.hasPendingChanges(settings));
	EXPECT_EQ(settings.snapStep, 0.25f);
	EXPECT_EQ(settings.showStats, !EditorSettings{}.showStats);
	EXPECT_EQ(settings.recentProjects, std::vector<std::string>{"a"});
	core::Log::invalidate();
}

TEST(SettingsPanel, CancelForgetsTheDraftAndTheShortcuts) {
	core::Log::init(core::Log::Level::Off);
	SettingsPanel panel;
	ActionRegistry registry;
	registry.registerAction("test.save", "Save", Shortcut{.key = input::key::S, .modifiers = Modifiers::Ctrl},
							[]() -> void {});
	const EditorSettings settings;
	panel.open();
	panel.beginEdit(settings, registry);
	panel.getDraft()->idleWaitMs = 7;
	registry.rebind("test.save", Shortcut{.key = input::key::D, .modifiers = Modifiers::Ctrl});
	panel.cancel(registry);
	EXPECT_FALSE(panel.isOpen());
	EXPECT_FALSE(panel.getDraft().has_value());
	EXPECT_NE(settings.idleWaitMs, 7);
	EXPECT_EQ(registry.getShortcutString("test.save"),
			  (Shortcut{.key = input::key::S, .modifiers = Modifiers::Ctrl}).toString());
	core::Log::invalidate();
}
