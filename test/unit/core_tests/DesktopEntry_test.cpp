/**
 * @file DesktopEntry_test.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Environment.h>
#include <platform/DesktopEntry.h>
#include <platform/FileUtils.h>

#include <filesystem>
#include <string>

using namespace owl::platform;

namespace {
auto sampleEntry() -> DesktopEntry {
	return {.appId = "owl-test-app",
			.name = "Owl Test App",
			.executable = "/opt/owl/bin/OwlRunner",
			.icon = "/opt/owl/assets/icons/logo.png"};
}
}// namespace

TEST(DesktopEntry, text) {
	const auto text = desktopEntryText(sampleEntry());
	EXPECT_TRUE(text.starts_with("[Desktop Entry]\n"));
	EXPECT_NE(text.find("Name=Owl Test App\n"), std::string::npos);
	EXPECT_NE(text.find("Exec=\"/opt/owl/bin/OwlRunner\"\n"), std::string::npos);
	EXPECT_NE(text.find("Icon=/opt/owl/assets/icons/logo.png\n"), std::string::npos);
	EXPECT_NE(text.find("StartupWMClass=owl-test-app\n"), std::string::npos);
	EXPECT_NE(text.find("NoDisplay=true\n"), std::string::npos);
}

#ifdef __linux__
TEST(DesktopEntry, install) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	const auto savedDataHome = owl::core::getEnv("XDG_DATA_HOME");
	const auto dataHome = std::filesystem::temp_directory_path() / "owl_desktop_entry_test";
	std::filesystem::remove_all(dataHome);
	owl::core::setEnv("XDG_DATA_HOME", dataHome.string());

	const auto path = desktopEntryPath("owl-test-app");
	ASSERT_TRUE(path.has_value());
	EXPECT_EQ(*path, dataHome / "applications" / "owl-test-app.desktop");
	EXPECT_TRUE(installDesktopEntry(sampleEntry()));
	ASSERT_TRUE(exists(*path));
	EXPECT_EQ(fileToString(*path), desktopEntryText(sampleEntry()));
	const auto firstWrite = std::filesystem::last_write_time(*path);
	EXPECT_TRUE(installDesktopEntry(sampleEntry()));
	EXPECT_EQ(std::filesystem::last_write_time(*path), firstWrite);

	auto noIcon = sampleEntry();
	noIcon.icon.clear();
	EXPECT_FALSE(installDesktopEntry(noIcon));

	owl::core::setEnv("XDG_DATA_HOME", savedDataHome);
	std::filesystem::remove_all(dataHome);
	owl::core::Log::invalidate();
}
#endif
