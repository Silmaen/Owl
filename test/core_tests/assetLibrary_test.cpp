
#include "testHelper.h"

#include <core/Application.h>
#include <renderer/Renderer.h>

using namespace owl::core;
using namespace owl::renderer;

TEST(AssetLibrary, DummyTexture) {
	Log::init(owl::core::Log::Level::Off);
	const AppParams params{.name = "super boby", .renderer = RenderAPI::Type::Null, .hasGui = false, .isDummy = true};
	auto app = owl::mkShared<Application>(params);
	{
		auto lib = Renderer::TextureLibrary();
		auto lists = lib.list();
		EXPECT_GT(lists.size(), 0);
	}
	Application::invalidate();
	app.reset();
	Log::invalidate();
}
