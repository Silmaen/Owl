/**
 * @file ProjectGamepad_test.cpp
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "Project.h"
#include "testHelper.h"

#include <core/Log.h>
#include <input/Input.h>
#include <window/Window.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

using namespace owl;
using namespace owl::nest;

TEST(ProjectGamepad, MappingFileRoundTripsAndLoads) {
	core::Log::init(core::Log::Level::Off);
	input::Input::init(window::Type::Null);
	const auto dir = std::filesystem::temp_directory_path() / "owl_project_gamepad_test";
	std::filesystem::remove_all(dir);
	std::filesystem::create_directories(dir / "input");

	Project project;
	project.name = "Pads";
	project.projectDirectory = dir;
	EXPECT_TRUE(project.loadGamepadMappings());
	project.gamepadMappings = "input/gamecontrollerdb.txt";
	EXPECT_FALSE(project.loadGamepadMappings());
	{
		std::ofstream file(dir / project.gamepadMappings);
		file << "030000005e0400008e02000010010000,Xbox 360 Controller,a:b0,b:b1,platform:Linux,\n";
	}
	EXPECT_TRUE(project.loadGamepadMappings());
	EXPECT_EQ(project.makeExportSettings(dir / "out", dir / "bin").gamepadMappings, project.gamepadMappings);

	ASSERT_TRUE(project.saveToFile(dir / "owl_project.yml"));
	Project loaded;
	ASSERT_TRUE(loaded.loadFromFile(dir / "owl_project.yml"));
	EXPECT_EQ(loaded.gamepadMappings, "input/gamecontrollerdb.txt");

	project.gamepadMappings.clear();
	ASSERT_TRUE(project.saveToFile(dir / "owl_project.yml"));
	Project cleared;
	ASSERT_TRUE(cleared.loadFromFile(dir / "owl_project.yml"));
	EXPECT_TRUE(cleared.gamepadMappings.empty());

	std::filesystem::remove_all(dir);
	input::Input::invalidate();
	core::Log::invalidate();
}
