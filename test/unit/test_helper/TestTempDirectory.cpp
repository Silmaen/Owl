/**
 * @file TestTempDirectory.cpp
 * @author Silmaen
 * @date 20/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 *
 * Compiled into every test executable: the process, and the processes it starts, get a temporary directory of their
 * own. `std::filesystem::temp_directory_path()` reads `TMPDIR` (POSIX) and `TMP` / `TEMP` (Windows), so every fixed
 * `temp_directory_path() / "name"` of the tests and of the engine is private to the process: two test processes (the
 * gtest shards of one binary, two build trees) never write the same file. `OWL_USER_DIR` points the engine's user
 * directory (settings, saves, logs, caches) inside it too.
 */

#include <core/Environment.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <system_error>

#ifdef OWL_PLATFORM_WINDOWS
#include <process.h>
#else
#include <unistd.h>
#endif

namespace {

std::filesystem::path g_processTempDirectory;

auto getProcessId() -> long {
#ifdef OWL_PLATFORM_WINDOWS
	return static_cast<long>(_getpid());
#else
	return static_cast<long>(getpid());
#endif
}

auto isolateTempDirectory() -> bool {
	const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
	g_processTempDirectory =
			std::filesystem::temp_directory_path() / std::format("owl_test_{}_{:x}", getProcessId(), stamp);
	std::error_code error;
	std::filesystem::remove_all(g_processTempDirectory, error);
	if (!std::filesystem::create_directories(g_processTempDirectory, error))
		return false;
	for (const auto* variable: {"TMPDIR", "TMP", "TEMP"}) owl::core::setEnv(variable, g_processTempDirectory.string());
	owl::core::setEnv("OWL_USER_DIR", (g_processTempDirectory / "user").string());
	std::atexit([]() -> void {
		std::error_code ignored;
		std::filesystem::remove_all(g_processTempDirectory, ignored);
	});
	return true;
}

// Static initialisation of the test executable: before any test runs.
[[maybe_unused]] const bool g_tempDirectoryIsolated = isolateTempDirectory();

}// namespace
