/**
 * @file DockLayout_test.cpp
 * @author Silmaen
 * @date 10/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "DockLayout.h"
#include "EditorSettings.h"
#include "testHelper.h"

#include <core/Log.h>
#include <core/Macros.h>
#include <gui/UiLayer.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
#include <imgui.h>
#include <imgui_internal.h>
OWL_DIAG_POP

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

using namespace owl;
using namespace owl::nest;

namespace {

/**
 * @brief
 *  Headless docked ImGui frames with the dock layout callback of the editor.
 */
class DockLayoutTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_layer.disableApp();
		m_layer.enableDocking();
		m_layer.onAttach();
		ImGui::GetIO().IniFilename = nullptr;
		m_layer.setDockLayoutCallback([this](const uint32_t iId) -> void {
			m_dockSpaceId = iId;
			m_rebuilt = m_layout.onDockSpace(iId, m_savedVersion) || m_rebuilt;
		});
	}

	void TearDown() override {
		m_layer.onDetach();
		core::Log::invalidate();
	}

	// One frame submitting the given panels, then documents asking for the central node like the editor's do.
	void frame(const std::vector<std::string>& iWindows = {}, const std::vector<std::string>& iDocuments = {}) {
		m_layer.begin();
		for (const auto& name: iWindows) {
			ImGui::Begin(name.c_str());
			ImGui::End();
		}
		for (const auto& name: iDocuments) {
			if (const auto* central = ImGui::DockBuilderGetCentralNode(gui::UiLayer::getDockSpaceId()))
				ImGui::SetNextWindowDockID(central->ID, ImGuiCond_FirstUseEver);
			ImGui::Begin(name.c_str());
			ImGui::End();
		}
		m_layer.end();
	}

	// Dock node holding a window, from its live window or its settings.
	static auto nodeOf(const char* iName) -> ImGuiDockNode* {
		const ImGuiID id = ImHashStr(iName);
		ImGuiID dockId = 0;
		if (const auto* window = ImGui::FindWindowByID(id); window != nullptr)
			dockId = window->DockId;
		else if (const auto* settings = ImGui::FindWindowSettingsByID(id); settings != nullptr)
			dockId = settings->DockId;
		return dockId == 0 ? nullptr : ImGui::DockBuilderGetNode(dockId);
	}

	gui::UiLayer m_layer;
	DockLayout m_layout;
	int m_savedVersion = 0;
	uint32_t m_dockSpaceId = 0;
	bool m_rebuilt = false;
};

}// namespace

TEST(DockLayoutPreset, NamesRoundTrip) {
	for (const auto preset: {LayoutPreset::Default, LayoutPreset::HierarchyLeft})
		EXPECT_EQ(DockLayout::presetFromName(DockLayout::getPresetName(preset)), preset);
	EXPECT_EQ(DockLayout::presetFromName("Unknown"), LayoutPreset::Default);
	EXPECT_EQ(DockLayout::presetFromName(""), LayoutPreset::Default);
}

TEST(DockLayoutPreset, RequestSchedulesARebuild) {
	DockLayout layout;
	EXPECT_FALSE(layout.isRebuildPending());
	layout.request(LayoutPreset::HierarchyLeft);
	EXPECT_TRUE(layout.isRebuildPending());
	EXPECT_EQ(layout.getPreset(), LayoutPreset::HierarchyLeft);
	layout.setPreset(LayoutPreset::Default);
	EXPECT_EQ(layout.getPreset(), LayoutPreset::Default);
}

TEST(DockLayoutPreset, SettingsKeepThePresetAndTheVersion) {
	const auto dir = std::filesystem::temp_directory_path() / "owl_dock_layout_settings_test";
	std::filesystem::remove_all(dir);
	std::filesystem::create_directories(dir);
	EditorSettings settings;
	EXPECT_EQ(settings.layoutPreset, "Default");
	EXPECT_EQ(settings.layoutVersion, 0);
	settings.layoutPreset = DockLayout::getPresetName(LayoutPreset::HierarchyLeft);
	settings.layoutVersion = DockLayout::version;
	settings.saveToFile(dir / "settings.yml");

	EditorSettings loaded;
	loaded.loadFromFile(dir / "settings.yml");
	EXPECT_EQ(DockLayout::presetFromName(loaded.layoutPreset), LayoutPreset::HierarchyLeft);
	EXPECT_EQ(loaded.layoutVersion, DockLayout::version);
	std::filesystem::remove_all(dir);
}

TEST_F(DockLayoutTest, FirstFrameBuildsTheDefaultLayout) {
	frame({panelWindow::hierarchy, panelWindow::properties, panelWindow::contentBrowser, panelWindow::log,
		   panelWindow::help});
	EXPECT_TRUE(m_rebuilt);
	EXPECT_EQ(m_savedVersion, DockLayout::version);
	EXPECT_EQ(m_dockSpaceId, gui::UiLayer::getDockSpaceId());
	const auto* root = ImGui::DockBuilderGetNode(m_dockSpaceId);
	ASSERT_NE(root, nullptr);
	EXPECT_TRUE(root->IsSplitNode());

	const auto* central = ImGui::DockBuilderGetCentralNode(m_dockSpaceId);
	const auto* hierarchy = nodeOf(panelWindow::hierarchy);
	const auto* properties = nodeOf(panelWindow::properties);
	const auto* content = nodeOf(panelWindow::contentBrowser);
	ASSERT_NE(central, nullptr);
	ASSERT_NE(hierarchy, nullptr);
	ASSERT_NE(properties, nullptr);
	ASSERT_NE(content, nullptr);
	// Hierarchy above Properties, both right of the documents; Content Browser under them, sharing with the Log.
	EXPECT_NE(hierarchy, properties);
	EXPECT_GT(hierarchy->Pos.x, central->Pos.x);
	EXPECT_LT(hierarchy->Pos.y, properties->Pos.y);
	EXPECT_GT(content->Pos.y, central->Pos.y);
	EXPECT_EQ(nodeOf(panelWindow::log), content);
	EXPECT_EQ(nodeOf(panelWindow::tilePalette), properties);
	EXPECT_EQ(nodeOf(panelWindow::help), central);
}

TEST_F(DockLayoutTest, HierarchyLeftGetsItsOwnColumn) {
	frame();
	m_layout.request(LayoutPreset::HierarchyLeft);
	m_rebuilt = false;
	frame({panelWindow::hierarchy, panelWindow::properties});
	EXPECT_TRUE(m_rebuilt);
	// Node geometry settles on the frame after a rebuild.
	frame({panelWindow::hierarchy, panelWindow::properties});
	EXPECT_FALSE(m_layout.isRebuildPending());
	const auto* central = ImGui::DockBuilderGetCentralNode(m_dockSpaceId);
	const auto* hierarchy = nodeOf(panelWindow::hierarchy);
	const auto* properties = nodeOf(panelWindow::properties);
	ASSERT_NE(central, nullptr);
	ASSERT_NE(hierarchy, nullptr);
	ASSERT_NE(properties, nullptr);
	EXPECT_LT(hierarchy->Pos.x, central->Pos.x);
	EXPECT_GT(properties->Pos.x, central->Pos.x);
}

TEST_F(DockLayoutTest, RebuildKeepsDocumentsInTheCentralNode) {
	frame({panelWindow::hierarchy}, {"main_menu.owl###doc1"});
	frame({panelWindow::hierarchy}, {"main_menu.owl###doc1"});
	ASSERT_NE(nodeOf("main_menu.owl###doc1"), nullptr);
	m_layout.request(LayoutPreset::Default);
	frame({panelWindow::hierarchy}, {"main_menu.owl###doc1"});
	const auto* central = ImGui::DockBuilderGetCentralNode(m_dockSpaceId);
	ASSERT_NE(central, nullptr);
	const auto documents = DockLayout::collectDocumentWindows(m_dockSpaceId);
	EXPECT_EQ(nodeOf("main_menu.owl###doc1"), central);
	EXPECT_TRUE(std::ranges::contains(documents, std::string{"main_menu.owl###doc1"}));
	EXPECT_FALSE(std::ranges::contains(documents, std::string{panelWindow::hierarchy}));
}

TEST_F(DockLayoutTest, UpToDateSplitLayoutIsKept) {
	frame({panelWindow::hierarchy});
	ASSERT_TRUE(m_rebuilt);
	m_rebuilt = false;
	frame({panelWindow::hierarchy});
	EXPECT_FALSE(m_rebuilt);
}

TEST_F(DockLayoutTest, BottomGroupShowsTheContentBrowser) {
	// Submitted in the reverse order: the tab order and the shown tab must not depend on it.
	const std::vector<std::string> panels{panelWindow::stats, panelWindow::log, panelWindow::contentBrowser};
	frame(panels);
	frame(panels);
	const auto* bottom = nodeOf(panelWindow::contentBrowser);
	ASSERT_NE(bottom, nullptr);
	ASSERT_NE(bottom->TabBar, nullptr);
	const auto* content = ImGui::FindWindowByName(panelWindow::contentBrowser);
	ASSERT_NE(content, nullptr);
	EXPECT_EQ(bottom->TabBar->SelectedTabId, content->TabId);
	ASSERT_EQ(bottom->TabBar->Tabs.Size, 3);
	EXPECT_EQ(bottom->TabBar->Tabs[0].Window, content);
	EXPECT_EQ(bottom->TabBar->Tabs[2].Window, ImGui::FindWindowByName(panelWindow::stats));
}
