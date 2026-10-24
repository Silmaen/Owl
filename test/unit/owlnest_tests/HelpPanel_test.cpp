/**
 * @file HelpPanel_test.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "panel/HelpPanel.h"
#include "nestTestHelper.h"
#include "testHelper.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace owl;
using namespace owl::nest::test;

using HelpPanelTest = NestTest;

namespace {

auto readText(const std::filesystem::path& iPath) -> std::string {
	const std::ifstream stream(iPath, std::ios::binary);
	std::stringstream buffer;
	buffer << stream.rdbuf();
	return buffer.str();
}

void writeIndex(const std::filesystem::path& iHelpDir) {
	std::filesystem::create_directories(iHelpDir);
	std::ofstream(iHelpDir / "index.yml") << "Help:\n  Version: 2\n  Pages: []\n";
}

auto bundlePages() -> std::vector<nest::panel::HelpPanel::HelpPage> {
	return nest::panel::HelpPanel::readIndex(std::filesystem::path{OWL_HELP_BUILD_DIR} / "index.yml");
}

}// namespace

TEST_F(HelpPanelTest, FindsTheBundleOfTheBuildTree) {
	const auto root = nest::panel::HelpPanel::resolveHelpRoot();
	ASSERT_FALSE(root.empty());
	EXPECT_EQ(root, std::filesystem::path{OWL_HELP_BUILD_DIR});
	EXPECT_TRUE(std::filesystem::exists(root / "index.yml"));
}

TEST_F(HelpPanelTest, BuildBundleWinsOverAStaleAssetHelp) {
	// A help folder left in an asset directory (an old `engine_assets/help`) never hides the pages of the build.
	const auto base = std::filesystem::temp_directory_path() / "owl_help_panel_stale";
	writeIndex(base / "engine_assets" / "help");
	writeIndex(base / "build" / "help_bundle");

	EXPECT_EQ(nest::panel::HelpPanel::resolveHelpRoot(base / "build" / "help_bundle", {base / "engine_assets"}),
			  base / "build" / "help_bundle");

	std::filesystem::remove_all(base);
}

TEST_F(HelpPanelTest, FindsTheHelpOfAnExtractedPackage) {
	// An extracted OwlNest archive: `assets/help/index.yml` beside the binaries, no build tree on this machine.
	const auto base = std::filesystem::temp_directory_path() / "owl_help_panel_package";
	writeIndex(base / "bin" / "assets" / "help");

	EXPECT_EQ(nest::panel::HelpPanel::resolveHelpRoot(base / "missing_build", {base / "bin" / "assets"}),
			  base / "bin" / "assets" / "help");
	EXPECT_EQ(nest::panel::HelpPanel::resolveHelpRoot({}, {base / "nothing", base / "bin" / "assets"}),
			  base / "bin" / "assets" / "help");
	EXPECT_TRUE(nest::panel::HelpPanel::resolveHelpRoot({}, {base / "nothing"}).empty());

	std::filesystem::remove_all(base);
}

TEST_F(HelpPanelTest, ReadIndexKeepsTheFileOrder) {
	const auto dir = std::filesystem::temp_directory_path() / "owl_help_panel_index";
	std::filesystem::create_directories(dir);
	std::ofstream(dir / "index.yml") << "Help:\n  Version: 2\n  Pages:\n"
										"    - {id: zeta, title: Zeta, group: First, path: zeta.md}\n"
										"    - {id: alpha, title: Alpha, group: First, path: alpha.md}\n"
										"    - {id: beta, title: Beta, group: Second, path: beta.md}\n"
										"    - {id: broken, title: No path, group: Second}\n";

	const auto pages = nest::panel::HelpPanel::readIndex(dir / "index.yml");
	ASSERT_EQ(pages.size(), 3u);
	EXPECT_EQ(pages[0].id, "zeta");
	EXPECT_EQ(pages[1].id, "alpha");
	EXPECT_EQ(pages[2].group, "Second");
	EXPECT_TRUE(nest::panel::HelpPanel::readIndex(dir / "missing.yml").empty());

	std::filesystem::remove_all(dir);
}

TEST_F(HelpPanelTest, BundleStartsOnTheEditorHelpWithItsGroupsInOrder) {
	const auto pages = bundlePages();
	ASSERT_FALSE(pages.empty());
	EXPECT_EQ(pages.front().id, "nest_help");
	std::vector<std::string> groups;
	for (const auto& page: pages) {
		if (groups.empty() || groups.back() != page.group) {
			EXPECT_EQ(std::ranges::find(groups, page.group), groups.end()) << "group split: " << page.group;
			groups.push_back(page.group);
		}
	}
	const std::vector<std::string> expected{
			"Getting started",           "The editor",   "Making games",       "Scripting",
			"Engine for C++ developers", "Contributing", "Roadmap and changes"};
	EXPECT_EQ(groups, expected);
}

TEST_F(HelpPanelTest, BundleHoldsNoPlanningPageNorStaleSyntax) {
	const auto pages = bundlePages();
	ASSERT_FALSE(pages.empty());
	const auto designDir = owl::test::getRootPath() / "doc" / "pages" / "design";
	ASSERT_TRUE(std::filesystem::is_directory(designDir));
	for (const auto& entry: std::filesystem::directory_iterator(designDir)) {
		const auto stem = entry.path().stem().string();
		EXPECT_EQ(std::ranges::find(pages, stem, &nest::panel::HelpPanel::HelpPage::id), pages.end())
				<< "planning page in the help: " << stem;
	}
	const std::filesystem::path root{OWL_HELP_BUILD_DIR};
	for (const auto& page: pages) {
		const auto text = readText(root / page.path);
		ASSERT_FALSE(text.empty()) << page.path;
		EXPECT_EQ(text.find("```mermaid"), std::string::npos) << page.path;
		EXPECT_EQ(text.find("https://img.shields.io"), std::string::npos) << page.path;
		EXPECT_EQ(text.find("\n[TOC]\n"), std::string::npos) << page.path;
		EXPECT_EQ(text.find("](design/"), std::string::npos) << page.path;
	}
}
