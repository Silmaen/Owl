/**
 * @file SettingsImport_test.cpp
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "SettingsImport.h"
#include "testHelper.h"

#include <core/Log.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

using namespace owl;
using namespace owl::nest::settings_import;

namespace {

void writeText(const std::filesystem::path& iPath, const std::string& iText) {
	std::filesystem::create_directories(iPath.parent_path());
	std::ofstream out(iPath, std::ios::binary);
	out << iText;
}

auto readText(const std::filesystem::path& iPath) -> std::string {
	std::ifstream in(iPath, std::ios::binary);
	return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

class SettingsImportTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_dir = std::filesystem::temp_directory_path() / "owl_settings_import_test";
		std::filesystem::remove_all(m_dir);
		m_old = m_dir / "OwlNest-0.5" / "user";
		writeText(m_old / "config.yml", "renderer: Vulkan\n");
		writeText(m_old / "OwlNest_settings.yml", "themePreset: Light\n");
		writeText(m_old / "OwlNest_imgui.ini", "[Window]\n");
		writeText(m_old / "games" / "Trader" / "saves" / "save_1.owl_save", "slot 1\n");
		writeText(m_old / "logs" / "Owl.log", "log\n");
		writeText(m_old / "cache" / "font" / "a.owlatlas", "atlas\n");
		m_user = m_dir / "OwlNest-0.6" / "user";
		writeText(m_user / "OwlNest_settings.yml", "themePreset: Dark\n");
	}

	void TearDown() override {
		std::filesystem::remove_all(m_dir);
		core::Log::invalidate();
	}

	/// Scratch folder of the test.
	std::filesystem::path m_dir;
	/// User directory of the previous installation.
	std::filesystem::path m_old;
	/// User directory of the current installation.
	std::filesystem::path m_user;
};

}// namespace

TEST_F(SettingsImportTest, findsTheUserFolderOfAnInstallation) {
	EXPECT_EQ(findSettingsSource(m_dir / "OwlNest-0.5"), m_old);
	EXPECT_EQ(findSettingsSource(m_old), m_old);
	EXPECT_FALSE(findSettingsSource(m_dir).has_value());
}

TEST_F(SettingsImportTest, findsSettingsBesideAnOlderExecutable) {
	const auto flat = m_dir / "OwlNest-0.4";
	writeText(flat / "OwlNest_settings.yml", "themePreset: Light\n");
	EXPECT_EQ(findSettingsSource(flat), flat);
}

TEST_F(SettingsImportTest, stagesSettingsUntilTheNextStart) {
	const auto staged = stageImport(m_dir / "OwlNest-0.5", m_user);
	ASSERT_TRUE(staged.has_value());
	EXPECT_EQ(*staged, 4u);
	EXPECT_EQ(readText(m_user / "OwlNest_settings.yml"), "themePreset: Dark\n");

	EXPECT_EQ(applyPendingImport(m_user), 4u);
	EXPECT_EQ(readText(m_user / "OwlNest_settings.yml"), "themePreset: Light\n");
	EXPECT_EQ(readText(m_user / "config.yml"), "renderer: Vulkan\n");
	EXPECT_TRUE(std::filesystem::exists(m_user / "OwlNest_imgui.ini"));
	EXPECT_TRUE(std::filesystem::exists(m_user / "games" / "Trader" / "saves" / "save_1.owl_save"));
	EXPECT_FALSE(std::filesystem::exists(m_user / "logs"));
	EXPECT_FALSE(std::filesystem::exists(m_user / "cache"));
	EXPECT_FALSE(std::filesystem::exists(m_user / "import_pending"));
	EXPECT_EQ(applyPendingImport(m_user), 0u);
}

TEST_F(SettingsImportTest, refusesAFolderWithoutSettings) {
	const auto staged = stageImport(m_dir, m_user);
	ASSERT_FALSE(staged.has_value());
	EXPECT_EQ(staged.error(), ImportError::NoSettings);
}

TEST_F(SettingsImportTest, refusesTheCurrentUserDirectory) {
	const auto staged = stageImport(m_user, m_user);
	ASSERT_FALSE(staged.has_value());
	EXPECT_EQ(staged.error(), ImportError::SameDirectory);
}
