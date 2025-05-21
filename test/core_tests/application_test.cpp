
#include "testHelper.h"

#include <core/Application.h>
#include <core/utils/FileUtils.h>
#include <event/KeyEvent.h>
#include <fstream>
#include <renderer/Renderer.h>

using namespace owl::core;

TEST(Core, ApplicationDummy) {
	Log::init(Log::Level::Off);
	AppParams params;
	params.isDummy = true;
	params.renderer = owl::renderer::RenderAPI::Type::Null;
	params.hasGui = false;
	params.useDebugging = true;
	auto app = owl::mkShared<Application>(params);
	EXPECT_EQ(app->getState(), Application::State::Running);
	Application::invalidate();
	app.reset();
	Log::invalidate();
}

TEST(Core, ApplicationBasic) {
	Log::init(Log::Level::Off);
	const AppParams params{.args = nullptr,
						   .frameLogFrequency = 0,
						   .name = "test",
						   .assetsPattern = "",
						   .icon = "",
						   .width = 0,
						   .height = 0,
						   .argCount = 0,
						   .renderer = owl::renderer::RenderAPI::Type::Null,
						   .hasGui = false,
						   .useDebugging = false,
						   .isDummy = true};
	auto app = owl::mkShared<Application>(params);
	ASSERT_FALSE(app->getAssetDirectories().empty());
	EXPECT_EQ(app->getAssetDirectories().front().assetsPath.filename(), "engine_assets");
	EXPECT_FALSE(app->getWorkingDirectory().filename().string().empty());
	EXPECT_EQ(app->getState(), Application::State::Running);
	app->enableDocking();
	app->disableDocking();
	owl::event::WindowResizeEvent resEv({800, 600});
	app->onEvent(resEv);
	owl::event::KeyPressedEvent keyEv(owl::input::key::A, 1);
	app->onEvent(keyEv);
	owl::event::WindowCloseEvent closEv;
	app->onEvent(closEv);
	app->close();
	EXPECT_EQ(app->getState(), Application::State::Stopped);
	Application::invalidate();
	app.reset();
	Log::invalidate();
}

TEST(Core, App) {
	Log::init(Log::Level::Off);
	const AppParams params{.name = "super boby",
						   .renderer = owl::renderer::RenderAPI::Type::Null,
						   .hasGui = false,
						   .isDummy = true};
	auto app = owl::mkShared<Application>(params);

	EXPECT_EQ(app->getInitParams().hasGui, params.hasGui);
	EXPECT_GE(app->getTimeStep().getMilliseconds(), 0);
	EXPECT_EQ(Application::get().getImGuiLayer(), nullptr);
	EXPECT_EQ(Application::get().getWindow().getGraphContext(), nullptr);
	Application::invalidate();
	app.reset();
	Log::invalidate();
}

TEST(Core, AppParamsSerialize) {
	AppParams paramsIni;
	paramsIni.width = 785;
	paramsIni.height = 7415;
	paramsIni.renderer = owl::renderer::RenderAPI::Type::OpenGL;
	auto tmpFile = std::filesystem::temp_directory_path() / "param.yml";
	paramsIni.saveToFile(tmpFile);

	AppParams paramsLoad;
	EXPECT_NE(paramsIni.width, paramsLoad.width);
	EXPECT_NE(paramsIni.height, paramsLoad.height);
	EXPECT_NE(paramsIni.renderer, paramsLoad.renderer);
	paramsLoad.loadFromFile(tmpFile);
	EXPECT_EQ(paramsIni.width, paramsLoad.width);
	EXPECT_EQ(paramsIni.height, paramsLoad.height);
	EXPECT_EQ(paramsIni.renderer, paramsLoad.renderer);
	remove(tmpFile);
}

TEST(Core, fileToString) {
	Log::init(Log::Level::Off);
	const std::string superStr = "yoho\nI'm fine!\n";
	const auto tmpFile = std::filesystem::temp_directory_path() / "testFile.b";
	{
		std::ofstream out(tmpFile, std::ios::out);
		out << superStr;
		out.close();
	}
	std::string read = utils::fileToString(tmpFile);
	EXPECT_STREQ(superStr.c_str(), read.c_str());
	read = utils::fileToString("");
	EXPECT_STREQ("", read.c_str());
	remove(tmpFile);
	Log::invalidate();
}
