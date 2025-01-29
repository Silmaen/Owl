
#include "testHelper.h"

#include <core/Application.h>
#include <renderer/RenderCommand.h>
#include <renderer/Renderer.h>

using namespace owl::renderer;
using namespace owl::core;

TEST(TextureLibrary, creation) {
	Log::init(owl::core::Log::Level::Off);
	RenderCommand::create(RenderAPI::Type::Null);
	const AppParams params{.name = "super boby", .renderer = RenderAPI::Type::Null, .hasGui = false, .isDummy = true};
	auto app = owl::mkShared<Application>(params);
	auto lib = Renderer::TextureLibrary();
	{
		EXPECT_FALSE(lib.exists("bob"));
		lib.load("CheckerBoard");
		lib.load("CheckerBoard");
		EXPECT_TRUE(lib.exists("CheckerBoard"));
		EXPECT_EQ(lib.get("bob"), nullptr);
		EXPECT_NE(lib.get("CheckerBoard"), nullptr);
	}
	{
		auto bob = Texture2D::create(Texture2D::Specification{{1, 1}, ImageFormat::RGB8});
		lib.add("checkerboard", bob);
		lib.add("superbob", bob);
		EXPECT_TRUE(lib.exists("superbob"));
	}
	Application::invalidate();
	app.reset();
	RenderCommand::invalidate();
	Log::invalidate();
}
