
#include "testHelper.h"

#include <gui/Theme.h>
#include <yaml-cpp/yaml.h>

using namespace owl::gui;
using namespace owl::core;

namespace {
void touchFile(const char* iPath) {
	std::ofstream ifs(iPath, std::ios::out);
	ifs.close();
}

void badYmlFile(const char* iPath) {
	YAML::Emitter out;
	out << YAML::BeginMap;
	out << YAML::Key << "Theme" << YAML::Value << YAML::BeginMap;
	out << YAML::Key << "File" << YAML::Value << iPath;
	out << YAML::EndMap;
	out << YAML::EndMap;
	std::ofstream ifs(iPath, std::ios::out);
	ifs << out.c_str();
	ifs.close();
}
}// namespace

TEST(Theme, Serialization) {
	Log::init(Log::Level::Off);
	constexpr Theme theme;
	const std::filesystem::path filePath("theme_blablabla.yml");
	theme.saveToFile(filePath);
	Theme themeCopy;
	themeCopy.controlsRounding = 150.f;

	themeCopy.loadFromFile("bobybob");// not existing
	EXPECT_EQ(themeCopy.controlsRounding, 150.f);

	std::filesystem::create_directory("onedir");
	themeCopy.loadFromFile("onedir");// a directory
	EXPECT_EQ(themeCopy.controlsRounding, 150.f);
	std::filesystem::remove_all("onedir");

	touchFile("onefile");
	themeCopy.loadFromFile("onefile");// not a yml file.
	EXPECT_EQ(themeCopy.controlsRounding, 150.f);
	remove(std::filesystem::path("onefile"));

	touchFile("onefile.yml");
	themeCopy.loadFromFile("onefile.yml");// yml file but empty.
	EXPECT_EQ(themeCopy.controlsRounding, 150.f);
	remove(std::filesystem::path("onefile.yml"));

	badYmlFile("onefile.yml");
	themeCopy.loadFromFile("onefile.yml");// yml file with bad theme info.
	EXPECT_EQ(themeCopy.controlsRounding, 150.f);
	remove(std::filesystem::path("onefile.yml"));

	themeCopy.loadFromFile(filePath);
	EXPECT_EQ(theme.controlsRounding, themeCopy.controlsRounding);
	remove(filePath);
	Log::invalidate();
}
