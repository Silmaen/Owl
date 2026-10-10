/**
 * @file CommandPalette_test.cpp
 * @author Silmaen
 * @date 11/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "panel/CommandPalette.h"
#include "testHelper.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <format>
#include <string>
#include <vector>

using namespace owl::nest::panel;

namespace {

auto makeItems() -> std::vector<PaletteItem> {
	return {{.category = "Scene", .label = "Save Document", .shortcut = "Ctrl+S", .run = {}},
			{.category = "Scene", .label = "Play/Resume", .shortcut = "F5", .run = {}},
			{.category = "View", .label = "Layout: Default", .shortcut = "Alt+1", .run = {}},
			{.category = "Select", .label = "MainCamera", .shortcut = {}, .run = {}},
			{.category = "Select", .label = "Player", .shortcut = {}, .run = {}}};
}

}// namespace

TEST(CommandPalette, ScoresSubsequencesOnly) {
	EXPECT_LT(CommandPalette::score("Save Document", "xyz"), 0);
	EXPECT_LT(CommandPalette::score("Save", "eva"), 0);
	EXPECT_GE(CommandPalette::score("Save Document", "svdoc"), 0);
	EXPECT_GE(CommandPalette::score("Save Document", "SAVE"), 0);
	EXPECT_EQ(CommandPalette::score("Anything", ""), 0);
}

TEST(CommandPalette, RanksWordStartsAndContiguousRunsFirst) {
	EXPECT_GT(CommandPalette::score("Play/Resume", "pl"), CommandPalette::score("Player", "ay"));
	EXPECT_GT(CommandPalette::score("MainCamera", "cam"), CommandPalette::score("Scam", "cam"));
	EXPECT_GT(CommandPalette::score("Play", "play"), CommandPalette::score("Play from here", "play"));
}

TEST(CommandPalette, FiltersOnCategoryAndLabel) {
	const auto items = makeItems();
	EXPECT_EQ(CommandPalette::filter(items, "").size(), items.size());
	const auto views = CommandPalette::filter(items, "view lay");
	ASSERT_FALSE(views.empty());
	EXPECT_EQ(items[views.front()].label, "Layout: Default");
	const auto players = CommandPalette::filter(items, "play");
	ASSERT_GE(players.size(), 2u);
	EXPECT_EQ(items[players.front()].label, "Player");
	EXPECT_TRUE(CommandPalette::filter(items, "qqq").empty());
}

TEST(CommandPalette, CapsTheResults) {
	std::vector<PaletteItem> items;
	for (size_t i = 0; i < CommandPalette::maxResults * 2; ++i)
		items.push_back({.category = "Select", .label = std::format("Entity {}", i), .shortcut = {}, .run = {}});
	EXPECT_EQ(CommandPalette::filter(items, "ent").size(), CommandPalette::maxResults);
}

TEST(CommandPalette, NamesTheCategoryOfAnAction) {
	EXPECT_EQ(CommandPalette::categoryOf("scene.save"), "Scene");
	EXPECT_EQ(CommandPalette::categoryOf("guizmo.all"), "Gizmo");
	EXPECT_EQ(CommandPalette::categoryOf("view.layoutDefault"), "View");
	EXPECT_EQ(CommandPalette::categoryOf("unknown"), "Command");
}

TEST(CommandPalette, OpensAndCloses) {
	CommandPalette palette;
	EXPECT_FALSE(palette.isOpen());
	palette.open();
	EXPECT_TRUE(palette.isOpen());
	palette.close();
	EXPECT_FALSE(palette.isOpen());
}
