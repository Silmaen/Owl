/**
 * @file ProjectVersion_test.cpp
 * @author Silmaen
 * @date 20/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "Project.h"
#include "testHelper.h"

#include <core/Core.h>
#include <core/Log.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

using namespace owl;
using namespace owl::nest;

TEST(ProjectVersion, WithoutVersionTheGameFollowsTheEngine) {
	core::Log::init(core::Log::Level::Off);
	const auto dir = std::filesystem::temp_directory_path() / "owl_project_version_test";
	std::filesystem::remove_all(dir);
	std::filesystem::create_directories(dir);
	{
		std::ofstream file(dir / "owl_project.yml");
		file << "OwlProject:\n  name: Demo\n  firstScene: scenes/main.owl\n";
	}

	Project project;
	ASSERT_TRUE(project.loadFromFile(dir / "owl_project.yml"));
	EXPECT_TRUE(project.version.empty());
	EXPECT_EQ(project.getGameVersion(), getVersionString());
	EXPECT_EQ(project.makeExportSettings(dir / "out", dir / "bin").version, getVersionString());

	ASSERT_TRUE(project.saveToFile(dir / "owl_project.yml"));
	Project saved;
	ASSERT_TRUE(saved.loadFromFile(dir / "owl_project.yml"));
	EXPECT_TRUE(saved.version.empty());

	std::filesystem::remove_all(dir);
	core::Log::invalidate();
}

TEST(ProjectVersion, OwnVersionWins) {
	Project project;
	project.version = "1.2.3";
	EXPECT_EQ(project.getGameVersion(), "1.2.3");
	EXPECT_EQ(project.makeExportSettings("out", "bin").version, "1.2.3");
}

TEST(ProjectVersion, ShippedProjectsFollowTheEngine) {
	const auto root = test::getRootPath();
	for (const auto& file:
		 {root / "engine_demo" / "owl_project.yml", root / "samples" / "breakout" / "owl_project.yml",
		  root / "samples" / "sokoban" / "owl_project.yml", root / "samples" / "wolfenstein" / "owl_project.yml"}) {
		Project project;
		ASSERT_TRUE(project.loadFromFile(file)) << file.string();
		EXPECT_TRUE(project.version.empty()) << file.string() << " writes its own version";
	}
}
