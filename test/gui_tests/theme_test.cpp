
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
	Log::init(owl::core::Log::Level::Off);
	constexpr Theme theme;
	std::filesystem::path filePath("theme_blablabla.yml");
	theme.saveToFile(filePath);
	Theme theme_copy;
	theme_copy.controlsRounding = 150.f;

	theme_copy.loadFromFile("bobybob");// not existing
	EXPECT_EQ(theme_copy.controlsRounding, 150.f);

	std::filesystem::create_directory("onedir");
	theme_copy.loadFromFile("onedir");// a directory
	EXPECT_EQ(theme_copy.controlsRounding, 150.f);
	std::filesystem::remove_all("onedir");

	touchFile("onefile");
	theme_copy.loadFromFile("onefile");// not a yml file.
	EXPECT_EQ(theme_copy.controlsRounding, 150.f);
	remove(std::filesystem::path("onefile"));

	touchFile("onefile.yml");
	theme_copy.loadFromFile("onefile.yml");// yml file but empty.
	EXPECT_EQ(theme_copy.controlsRounding, 150.f);
	remove(std::filesystem::path("onefile.yml"));

	badYmlFile("onefile.yml");
	theme_copy.loadFromFile("onefile.yml");// yml file with bad theme info.
	EXPECT_EQ(theme_copy.controlsRounding, 150.f);
	remove(std::filesystem::path("onefile.yml"));

	theme_copy.loadFromFile(filePath);
	EXPECT_EQ(theme.controlsRounding, theme_copy.controlsRounding);
	remove(filePath);
	Log::invalidate();
}
