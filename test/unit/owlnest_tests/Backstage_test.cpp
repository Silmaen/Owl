/**
 * @file Backstage_test.cpp
 * @author Silmaen
 * @date 11/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "panel/Backstage.h"
#include "testHelper.h"

#include <core/Log.h>
#include <gui/UiLayer.h>

#include <imgui.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

using namespace owl;
using namespace owl::nest::panel;

namespace {

/**
 * @brief
 *  Headless frames of the File screen, counting the actions it runs.
 */
class BackstageTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_layer.disableApp();
		m_layer.onAttach();
		ImGui::GetIO().IniFilename = nullptr;
		m_context.recentProjects = {"/games/demo", "/games/other"};
		const auto count = [this]() -> void { ++m_actions; };
		m_context.newProject = count;
		m_context.openProject = count;
		m_context.openRecent = [this](const std::filesystem::path& iPath) -> void {
			++m_actions;
			m_opened = iPath;
		};
		m_context.forgetRecent = [this](const std::filesystem::path&) -> void { ++m_actions; };
		m_context.saveProject = count;
		m_context.saveProjectAs = count;
		m_context.packGame = count;
		m_context.editorSettings = count;
		m_context.projectSettings = count;
		m_context.engineSettings = count;
		m_context.help = [this](const std::string&) -> void { ++m_actions; };
		m_context.closeProject = count;
		m_context.exit = count;
	}

	void TearDown() override {
		m_layer.onDetach();
		core::Log::invalidate();
	}

	void frame() {
		m_layer.begin();
		m_backstage.onImGuiRender(m_context);
		m_layer.end();
	}

	gui::UiLayer m_layer;
	Backstage m_backstage;
	BackstageContext m_context;
	int m_actions = 0;
	std::filesystem::path m_opened;
};

}// namespace

TEST_F(BackstageTest, ClosedScreenDrawsNothing) {
	EXPECT_FALSE(m_backstage.isOpen());
	frame();
	EXPECT_EQ(m_actions, 0);
}

TEST_F(BackstageTest, OpenScreenRunsNothingWithoutInput) {
	m_backstage.open();
	for (int i = 0; i < 6; ++i) frame();
	EXPECT_EQ(m_actions, 0);
	EXPECT_TRUE(m_backstage.isOpen());
	m_context.projectName = "Demo";
	m_context.projectDirectory = "/games/demo";
	for (int i = 0; i < 3; ++i) frame();
	EXPECT_EQ(m_actions, 0);
	m_backstage.close();
	EXPECT_FALSE(m_backstage.isOpen());
}

TEST_F(BackstageTest, EscapeClosesTheScreen) {
	m_backstage.open();
	frame();
	ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, true);
	frame();
	ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, false);
	frame();
	EXPECT_FALSE(m_backstage.isOpen());
	EXPECT_EQ(m_actions, 0);
}

TEST(Backstage, FindsTheSampleProjectAboveTheAssets) {
	const auto root = std::filesystem::temp_directory_path() / "owl_backstage_sample_test";
	std::filesystem::remove_all(root);
	const auto assets = root / "bin" / "linux_x64" / "assets";
	std::filesystem::create_directories(assets);
	// Nothing to find yet.
	EXPECT_FALSE(findSampleProject({assets}).has_value());
	std::filesystem::create_directories(root / "engine_demo");
	std::ofstream{root / "engine_demo" / "owl_project.yml"} << "name: Sample\n";
	const auto found = findSampleProject({root / "missing", assets});
	ASSERT_TRUE(found.has_value());
	EXPECT_EQ(*found, (root / "engine_demo").lexically_normal());
	// Too deep for the depth allowed.
	EXPECT_FALSE(findSampleProject({assets}, 1).has_value());
	std::filesystem::remove_all(root);
}

TEST_F(BackstageTest, SampleGamesAreListedWithoutRunningAnything) {
	m_context.sampleProject = "/games/engine_demo";
	m_context.sampleGames = {"/games/samples/breakout", "/games/samples/sokoban"};
	m_backstage.open();
	for (int i = 0; i < 4; ++i) frame();
	EXPECT_EQ(m_actions, 0);
	EXPECT_TRUE(m_backstage.isOpen());
}

TEST(Backstage, FindsTheSampleGamesBesideTheEngineDemo) {
	const auto root = std::filesystem::temp_directory_path() / "owl_backstage_games_test";
	std::filesystem::remove_all(root);
	std::filesystem::create_directories(root / "engine_demo");
	EXPECT_TRUE(findSampleGames({}).empty());
	EXPECT_TRUE(findSampleGames(root / "engine_demo").empty());
	for (const auto* game: {"zeta", "alpha", "not_a_game"})
		std::filesystem::create_directories(root / "samples" / game);
	std::ofstream{root / "samples" / "zeta" / "owl_project.yml"} << "name: Zeta\n";
	std::ofstream{root / "samples" / "alpha" / "owl_project.yml"} << "name: Alpha\n";
	std::ofstream{root / "samples" / "loose_file.yml"} << "name: Loose\n";
	const auto games = findSampleGames(root / "engine_demo");
	ASSERT_EQ(games.size(), 2u);
	EXPECT_EQ(games[0], (root / "samples" / "alpha").lexically_normal());
	EXPECT_EQ(games[1], (root / "samples" / "zeta").lexically_normal());
	std::filesystem::remove_all(root);
}
