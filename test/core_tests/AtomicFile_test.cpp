/**
 * @file AtomicFile_test.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <platform/AtomicFile.h>
#include <platform/AtomicFileFault.h>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>

using namespace owl;
using namespace owl::platform;

namespace {

class AtomicFileTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_dir = std::filesystem::temp_directory_path() / "owl_atomic_file_test";
		std::filesystem::remove_all(m_dir);
		std::filesystem::create_directories(m_dir);
	}

	void TearDown() override {
		setAtomicWriteFault(std::nullopt);
		std::filesystem::remove_all(m_dir);
		core::Log::invalidate();
	}

	[[nodiscard]] static auto read(const std::filesystem::path& iPath) -> std::string {
		const std::ifstream in(iPath, std::ios::binary);
		std::stringstream buffer;
		buffer << in.rdbuf();
		return buffer.str();
	}

	[[nodiscard]] auto fileCount() const -> size_t {
		return static_cast<size_t>(std::distance(std::filesystem::directory_iterator(m_dir), {}));
	}

	std::filesystem::path m_dir;
};

}// namespace

TEST_F(AtomicFileTest, CreatesThenReplacesTheFile) {
	const auto path = m_dir / "doc.yml";
	ASSERT_TRUE(writeFileAtomic(path, "first: 1\n"));
	EXPECT_EQ(read(path), "first: 1\n");
	ASSERT_TRUE(writeFileAtomic(path, "second: 2\r\n"));
	EXPECT_EQ(read(path), "second: 2\r\n");
	EXPECT_EQ(fileCount(), 1u);
}

TEST_F(AtomicFileTest, EveryFailedStepKeepsThePreviousFile) {
	const auto path = m_dir / "doc.yml";
	const std::string original = "original: content\n";
	const std::string replacement(4096, 'x');
	for (const auto fault:
		 {WriteError::CreateFailed, WriteError::WriteFailed, WriteError::FlushFailed, WriteError::RenameFailed}) {
		ASSERT_TRUE(writeFileAtomic(path, original));
		setAtomicWriteFault(fault);
		const auto written = writeFileAtomic(path, replacement);
		setAtomicWriteFault(std::nullopt);
		ASSERT_FALSE(written) << describe(fault);
		EXPECT_EQ(written.error(), fault);
		EXPECT_EQ(read(path), original) << describe(fault);
		EXPECT_EQ(fileCount(), 1u) << "temporary file left behind after " << describe(fault);
	}
}

TEST_F(AtomicFileTest, InterruptedFirstWriteLeavesNothing) {
	const auto path = m_dir / "new.yml";
	setAtomicWriteFault(WriteError::WriteFailed);
	EXPECT_FALSE(writeFileAtomic(path, "never: written\n"));
	setAtomicWriteFault(std::nullopt);
	EXPECT_FALSE(std::filesystem::exists(path));
	EXPECT_EQ(fileCount(), 0u);
}

TEST_F(AtomicFileTest, MissingDirectoryFailsCleanly) {
	const auto written = writeFileAtomic(m_dir / "missing" / "doc.yml", "a: 1\n");
	ASSERT_FALSE(written);
	EXPECT_EQ(written.error(), WriteError::CreateFailed);
}

#ifndef OWL_PLATFORM_WINDOWS
TEST_F(AtomicFileTest, KeepsThePermissionsOfTheReplacedFile) {
	const auto path = m_dir / "doc.yml";
	ASSERT_TRUE(writeFileAtomic(path, "a: 1\n"));
	std::filesystem::permissions(path, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
	ASSERT_TRUE(writeFileAtomic(path, "a: 2\n"));
	EXPECT_EQ(std::filesystem::status(path).permissions(),
			  std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
}
#endif
