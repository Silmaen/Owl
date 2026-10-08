/**
 * @file FileWatcher_test.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <platform/FileWatcher.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

using namespace owl;
using namespace std::chrono_literals;

namespace {

auto makeDir(const std::string& iName) -> std::filesystem::path {
	const auto dir = std::filesystem::temp_directory_path() / iName;
	std::filesystem::remove_all(dir);
	std::filesystem::create_directories(dir / "sub");
	return dir;
}

void writeFile(const std::filesystem::path& iPath, const std::string& iContent) {
	std::ofstream file(iPath, std::ios::binary | std::ios::trunc);
	file << iContent;
}

}// namespace

TEST(FileWatcher, ReportsAChangeOnceItSettled) {
	const auto dir = makeDir("owl_filewatcher_settle");
	const auto file = dir / "sub" / "a.txt";
	writeFile(file, "one");
	platform::FileWatcher watcher;
	watcher.addDirectory(dir);
	watcher.scan();
	EXPECT_FALSE(watcher.hasChanges());

	writeFile(file, "two, longer");
	watcher.scan();
	EXPECT_FALSE(watcher.hasChanges());
	watcher.scan();
	ASSERT_TRUE(watcher.hasChanges());
	const auto changes = watcher.takeChanges();
	ASSERT_EQ(changes.size(), 1u);
	EXPECT_TRUE(platform::isSameFile(changes.front(), file));
	EXPECT_FALSE(watcher.hasChanges());

	watcher.scan();
	EXPECT_FALSE(watcher.hasChanges());
	std::filesystem::remove_all(dir);
}

TEST(FileWatcher, ReportsCreatedFilesAndForgetsRemovedDirectories) {
	const auto dir = makeDir("owl_filewatcher_create");
	platform::FileWatcher watcher;
	watcher.addDirectory(dir);
	EXPECT_EQ(watcher.getDirectories().size(), 1u);
	watcher.addDirectory(dir);
	EXPECT_EQ(watcher.getDirectories().size(), 1u);

	writeFile(dir / "new.txt", "fresh");
	watcher.scan();
	watcher.scan();
	EXPECT_EQ(watcher.takeChanges().size(), 1u);

	watcher.removeDirectory(dir);
	EXPECT_TRUE(watcher.getDirectories().empty());
	writeFile(dir / "new.txt", "changed again");
	watcher.scan();
	watcher.scan();
	EXPECT_FALSE(watcher.hasChanges());
	std::filesystem::remove_all(dir);
}

TEST(FileWatcher, BackgroundThreadFindsTheChange) {
	const auto dir = makeDir("owl_filewatcher_thread");
	const auto file = dir / "b.txt";
	writeFile(file, "x");
	platform::FileWatcher watcher(10ms);
	watcher.addDirectory(dir);
	watcher.start();
	EXPECT_TRUE(watcher.isRunning());
	watcher.start();
	writeFile(file, "a different content");
	const auto deadline = std::chrono::steady_clock::now() + 5s;
	while (!watcher.hasChanges() && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(5ms);
	watcher.stop();
	EXPECT_FALSE(watcher.isRunning());
	ASSERT_TRUE(watcher.hasChanges());
	EXPECT_EQ(watcher.takeChanges().size(), 1u);
	watcher.stop();
	std::filesystem::remove_all(dir);
}

TEST(FileWatcher, SameFile) {
	const auto dir = makeDir("owl_filewatcher_same");
	writeFile(dir / "c.txt", "c");
	EXPECT_TRUE(platform::isSameFile(dir / "c.txt", dir / "sub" / ".." / "c.txt"));
	EXPECT_FALSE(platform::isSameFile(dir / "c.txt", dir / "sub"));
	EXPECT_TRUE(platform::isSameFile(dir / "missing.txt", dir / "sub" / ".." / "missing.txt"));
	EXPECT_FALSE(platform::isSameFile(dir / "missing.txt", dir / "other.txt"));
	std::filesystem::remove_all(dir);
}
