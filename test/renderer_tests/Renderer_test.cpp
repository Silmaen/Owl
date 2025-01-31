
#include "testHelper.h"

#include <renderer/Renderer.h>

using namespace owl::renderer;

TEST(Renderer, creation) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	Renderer::reset();
	EXPECT_EQ(Renderer::getState(), Renderer::State::Created);
	Renderer::init();
	EXPECT_EQ(Renderer::getState(), Renderer::State::Error);
	RenderCommand::create(RenderAPI::Type::Null);
	Renderer::init();
	EXPECT_EQ(Renderer::getState(), Renderer::State::Running);
	Renderer::onWindowResized(800, 600);
	Renderer::shutdown();
	EXPECT_EQ(Renderer::getState(), Renderer::State::Stopped);

	RenderCommand::invalidate();
	owl::core::Log::invalidate();
}

TEST(Renderer, fakeScene) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	RenderCommand::create(RenderAPI::Type::Null);
	Renderer::init();
	const CameraOrtho cam(0, 0, 800, 600);
	Renderer::beginScene(cam);
	Renderer::endScene();
	RenderCommand::invalidate();
	owl::core::Log::invalidate();
}
