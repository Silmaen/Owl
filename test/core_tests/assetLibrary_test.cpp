
#include "testHelper.h"

#include <core/Application.h>
#include <renderer/Renderer.h>

using namespace owl::core;
using namespace owl::renderer;

TEST(AssetLibrary, DummyTexture) {
	Log::init(Log::Level::Off);
	const AppParams params{.name = "super boby", .renderer = RenderAPI::Type::Null, .hasGui = false, .isDummy = true};
	auto app = owl::mkShared<Application>(params);
	{
		const auto lib = Renderer::TextureLibrary();
		const auto lists = lib.list();
		EXPECT_GT(lists.size(), 0);
	}
	Application::invalidate();
	app.reset();
	Log::invalidate();
}
