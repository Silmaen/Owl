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

#include <filesystem>
#include <fstream>

using namespace owl;
using namespace owl::nest::test;

using HelpPanelTest = NestTest;

TEST_F(HelpPanelTest, FindsTheBundleOfTheBuildTree) {
	const auto root = nest::panel::HelpPanel::resolveHelpRoot();
	ASSERT_FALSE(root.empty());
	EXPECT_EQ(root, std::filesystem::path{OWL_HELP_BUILD_DIR});
	EXPECT_TRUE(std::filesystem::exists(root / "index.yml"));
}

TEST_F(HelpPanelTest, FindsTheHelpOfAnExtractedPackage) {
	// The layout of an extracted OwlNest archive: `assets/help/index.yml` beside the binaries, started from there.
	const auto packageBin = std::filesystem::temp_directory_path() / "owl_help_panel_test" / "bin";
	std::filesystem::create_directories(packageBin / "assets" / "help");
	std::ofstream(packageBin / "assets" / "help" / "index.yml") << "Help:\n  Version: 1\n  Pages: []\n";
	const auto previousDir = std::filesystem::current_path();
	std::filesystem::current_path(packageBin);
	auto app = mkShared<app::Application>(app::AppParams{.args = nullptr,
														 .frameLogFrequency = 0,
														 .name = "helpPanelTest",
														 .assetsPattern = "",
														 .icon = "",
														 .width = 0,
														 .height = 0,
														 .argCount = 0,
														 .renderer = renderer::gpu::RenderAPI::Type::Null,
														 .hasGui = false,
														 .useDebugging = false,
														 .isDummy = true});

	EXPECT_EQ(nest::panel::HelpPanel::resolveHelpRoot(), packageBin / "assets" / "help");

	app::Application::invalidate();
	std::filesystem::current_path(previousDir);
	std::filesystem::remove_all(packageBin.parent_path());
}
