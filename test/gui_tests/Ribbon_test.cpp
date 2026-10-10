/**
 * @file Ribbon_test.cpp
 * @author Silmaen
 * @date 11/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <gui/UiLayer.h>
#include <gui/widgets/Ribbon.h>

#include <imgui.h>

#include <tuple>

using namespace owl;
using namespace owl::gui;
using namespace owl::gui::widgets;

namespace {

/**
 * @brief
 *  Headless frames drawing a ribbon.
 */
class RibbonTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_layer.disableApp();
		m_layer.onAttach();
		ImGui::GetIO().IniFilename = nullptr;
		const auto home = m_ribbon.addTab("Home");
		const auto group = m_ribbon.addGroup(home, "Edit");
		m_ribbon.addButton(home, group,
						   {.iconName = "undo",
							.label = "Undo",
							.tooltip = "Undo the last change.",
							.shortcut = "Ctrl+Z",
							.onClick = [this]() -> void { ++m_clicks; }});
		m_ribbon.addButton(
				home, group,
				{.iconName = "save", .label = "Save", .tooltip = "Save.", .size = Ribbon::ButtonSize::Large});
		const auto scene = m_ribbon.addTab("Scene");
		m_ribbon.setTabHighlighted(scene, true);
		std::ignore = m_ribbon.addGroup(scene, "Document");
		m_ribbon.setBackstage("File", [this]() -> void { ++m_backstage; });
		m_ribbon.addQuickAccess({.iconName = "save", .label = "Save", .tooltip = "Save."});
	}

	void TearDown() override {
		m_layer.onDetach();
		core::Log::invalidate();
	}

	void frame() {
		m_layer.begin();
		ImGui::SetNextWindowPos({0.f, 0.f});
		ImGui::SetNextWindowSize({800.f, 300.f});
		ImGui::Begin("Host");
		m_ribbon.onRender();
		ImGui::End();
		m_layer.end();
	}

	gui::UiLayer m_layer;
	Ribbon m_ribbon;
	int m_clicks = 0;
	int m_backstage = 0;
};

}// namespace

TEST_F(RibbonTest, TabsSelectAndKeepTheirLabel) {
	EXPECT_EQ(m_ribbon.getTabCount(), 2u);
	EXPECT_EQ(m_ribbon.getTabLabel(1), "Scene");
	EXPECT_EQ(m_ribbon.getTabLabel(5), "");
	frame();
	EXPECT_EQ(m_ribbon.getSelectedTab(), "Home");
	m_ribbon.selectTab("Scene");
	frame();
	EXPECT_EQ(m_ribbon.getSelectedTab(), "Scene");
	m_ribbon.selectTab("Missing");
	EXPECT_EQ(m_ribbon.getSelectedTab(), "Scene");
	EXPECT_EQ(m_clicks, 0);
	EXPECT_EQ(m_backstage, 0);
}

TEST_F(RibbonTest, CollapsingKeepsOnlyTheTabRow) {
	m_layer.begin();
	const float expanded = m_ribbon.getHeight();
	m_ribbon.setCollapsed(true);
	const float collapsed = m_ribbon.getHeight();
	m_layer.end();
	EXPECT_TRUE(m_ribbon.isCollapsed());
	EXPECT_LT(collapsed, expanded);
	frame();
	m_ribbon.setCollapsed(false);
	frame();
	EXPECT_FALSE(m_ribbon.isCollapsed());
}

TEST_F(RibbonTest, ClearRemovesTabsAndQuickAccess) {
	m_ribbon.clear();
	EXPECT_EQ(m_ribbon.getTabCount(), 0u);
	frame();
	EXPECT_EQ(m_ribbon.getSelectedTab(), "");
}
