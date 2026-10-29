/**
 * @file UserDirectory_test.cpp
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Environment.h>
#include <platform/UserDirectory.h>

#include <filesystem>
#include <string>

using namespace owl::platform;

namespace {

class UserDirectoryTest : public ::testing::Test {
protected:
	void SetUp() override {
		m_dir = std::filesystem::temp_directory_path() / "owl_user_directory_test";
		std::filesystem::remove_all(m_dir);
		std::filesystem::create_directories(m_dir);
		m_previous = getUserDirectory();
	}

	void TearDown() override {
		setUserDirectory(m_previous);
		std::error_code error;
		std::filesystem::permissions(m_dir / "app", std::filesystem::perms::owner_all,
									 std::filesystem::perm_options::add, error);
		std::filesystem::remove_all(m_dir, error);
	}

	/// Scratch folder of the test.
	std::filesystem::path m_dir;
	/// User directory to restore.
	std::filesystem::path m_previous;
};

}// namespace

TEST_F(UserDirectoryTest, writableApplicationFolder) {
	const auto app = m_dir / "app";
	std::filesystem::create_directories(app);
	EXPECT_EQ(resolveUserDirectory(app, "OwlTest"), app / "user");
	EXPECT_TRUE(std::filesystem::is_directory(app / "user"));
	EXPECT_FALSE(std::filesystem::exists(app / "user" / ".owl_write_probe"));
}

#ifdef __linux__
TEST_F(UserDirectoryTest, readOnlyApplicationFolderFallsBackToProfile) {
	const auto app = m_dir / "app";
	std::filesystem::create_directories(app);
	std::filesystem::permissions(app, std::filesystem::perms::owner_read | std::filesystem::perms::owner_exec);
	if (std::error_code error; std::filesystem::create_directories(app / "probe", error))
		GTEST_SKIP() << "Permissions are not enforced for this user.";
	const auto previous = owl::core::getEnv("XDG_DATA_HOME");
	owl::core::setEnv("XDG_DATA_HOME", (m_dir / "profile").string());
	const auto chosen = resolveUserDirectory(app, "OwlTest");
	owl::core::setEnv("XDG_DATA_HOME", previous);
	EXPECT_EQ(chosen, m_dir / "profile" / "OwlTest");
	EXPECT_TRUE(std::filesystem::is_directory(chosen));
}
#endif

TEST_F(UserDirectoryTest, forcedDirectory) {
	setUserDirectory(m_dir / "forced");
	EXPECT_EQ(getUserDirectory(), m_dir / "forced");
	EXPECT_TRUE(std::filesystem::is_directory(m_dir / "forced"));
	EXPECT_EQ(getGameDirectory("Space Trader"), m_dir / "forced" / "games" / "Space Trader");
	EXPECT_EQ(getGameDirectory(""), m_dir / "forced" / "games" / "OwlGame");
}

TEST_F(UserDirectoryTest, testsRunInTheirOwnDirectory) {
	setUserDirectory({});
	EXPECT_TRUE(getUserDirectory().string().starts_with(std::filesystem::temp_directory_path().string()));
}

TEST(UserDirectory, executableDirectory) {
	const auto directory = getExecutableDirectory();
	EXPECT_TRUE(std::filesystem::is_directory(directory));
	EXPECT_TRUE(directory.is_absolute());
}
