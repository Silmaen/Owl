/**
 * @file Renderer_test.cpp
 * @author Silmaen
 * @date 03/08/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <renderer/ColorEncoder.h>
#include <renderer/Renderer.h>

using namespace owl::renderer;
using namespace owl::renderer::gpu;

TEST(Renderer, creation) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	// The test checks init() without a render API, so start from none whatever ran before.
	RenderCommand::invalidate();
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
	EXPECT_EQ(Renderer::getState(), Renderer::State::Running);
	const CameraOrtho cam(0, 0, 800, 600);
	EXPECT_NO_THROW(Renderer::beginScene(cam));
	EXPECT_NO_THROW(Renderer::endScene());
	Renderer::shutdown();
	RenderCommand::invalidate();
	owl::core::Log::invalidate();
}

TEST(Renderer, encodesTheSceneColourWithEachToneMapping) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	RenderCommand::create(RenderAPI::Type::Null);
	Renderer::init();
	const auto texture = Texture2D::create(Texture2D::Specification{.size = {1, 1}, .format = ImageFormat::Rgba8});
	for (const auto tone: {ToneMapping::None, ToneMapping::PbrNeutral, ToneMapping::Aces})
		EXPECT_NO_THROW(ColorEncoder::draw(texture, tone));
	EXPECT_NO_THROW(ColorEncoder::draw(nullptr, ToneMapping::None));
	EXPECT_EQ(Renderer::getRenderGraph().getPooledTargetCount(), 0);
	Renderer::shutdown();
	RenderCommand::invalidate();
	owl::core::Log::invalidate();
}

TEST(Renderer, cameraToneMappingDefaultsToNone) {
	CameraOrtho camera(0, 1, 0, 1);
	EXPECT_EQ(camera.getToneMapping(), ToneMapping::None);
	camera.setToneMapping(ToneMapping::Aces);
	EXPECT_EQ(camera.getToneMapping(), ToneMapping::Aces);
}
