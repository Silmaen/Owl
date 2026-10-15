/**
 * @file Renderer2D_test.cpp
 * @author Silmaen
 * @date 03/08/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <app/Application.h>
#include <data/fonts/TextLayout.h>
#include <renderer/Renderer.h>
#include <renderer/Renderer2D.h>
#include <scene/component/SpriteRenderer.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

using namespace owl::renderer;
using namespace owl::renderer::gpu;
using namespace owl::renderer;
using namespace owl::core;
using namespace owl::app;
using namespace owl::math;

TEST(Renderer2D, fakeEmptyScene) {
	Log::init(owl::core::Log::Level::Off);
	RenderCommand::create(RenderAPI::Type::Null);
	Renderer::init();
	const CameraOrtho cam(0, 0, 800, 600);
	Renderer2D::beginScene(cam);
	Renderer2D::endScene();
	const auto st = Renderer2D::getStats();
	EXPECT_EQ(st.drawCalls, 0);
	EXPECT_EQ(st.quadCount, 0);
	EXPECT_EQ(st.getTotalIndexCount(), 0);
	EXPECT_EQ(st.getTotalVertexCount(), 0);

	RenderCommand::invalidate();
	Log::invalidate();
}

TEST(Renderer2D, fakeLineScene) {
	Log::init(owl::core::Log::Level::Off);
	RenderCommand::create(RenderAPI::Type::Null);
	Renderer::init();
	const CameraEditor cam;
	Renderer2D::resetStats();
	Renderer2D::beginScene(cam);
	Renderer2D::drawLine({.point1 = {0, 0, 0}, .point2 = {1.f, 1.f, 0.f}});
	PolyLineData data{.transform = owl::math::Transform{owl::math::identity<float, 4>()},
					  .points = std::vector<owl::math::vec3>{}};
	Renderer2D::drawPolyLine(data);
	data.points.emplace_back(1.f, 1.f, 1.f);
	data.points.emplace_back(1.f, 0.f, 1.f);
	data.points.emplace_back(0.f, 1.f, 1.f);
	data.closed = true;
	Renderer2D::drawPolyLine(data);
	Renderer2D::endScene();
	const auto st = Renderer2D::getStats();
	EXPECT_EQ(st.drawCalls, 1u);
	EXPECT_EQ(st.quadCount, 0u);
	EXPECT_EQ(st.lineCount, 4u);
	EXPECT_EQ(st.getTotalIndexCount(), 8u);
	EXPECT_EQ(st.getTotalVertexCount(), 8u);

	Renderer2D::shutdown();
	Renderer2D::shutdown();
	RenderCommand::invalidate();
	Log::invalidate();
}

TEST(Renderer2D, fakeCircleRectScene) {
	Log::init(owl::core::Log::Level::Off);
	RenderCommand::create(RenderAPI::Type::Null);
	Renderer::init();
	const CameraEditor cam;
	Renderer2D::resetStats();
	Renderer2D::beginScene(cam);
	Renderer2D::drawRect({.transform = Transform{owl::math::identity<float, 4>()}});
	Renderer2D::nextBatch();
	Renderer2D::drawCircle({.transform = Transform{owl::math::identity<float, 4>()}});
	Renderer2D::endScene();
	const auto st = Renderer2D::getStats();
	EXPECT_EQ(st.drawCalls, 2u);
	EXPECT_EQ(st.quadCount, 1u);
	EXPECT_EQ(st.lineCount, 4u);
	EXPECT_EQ(st.getTotalIndexCount(), 14u);
	EXPECT_EQ(st.getTotalVertexCount(), 12u);

	RenderCommand::invalidate();
	Log::invalidate();
}


TEST(Renderer2D, fakeQuadSpriteScene) {
	Log::init(owl::core::Log::Level::Off);
	RenderCommand::create(RenderAPI::Type::Null);
	Renderer::init();
	const CameraEditor cam;
	Renderer2D::resetStats();
	Renderer2D::beginScene(cam);
	const Transform tr{{0.f, 1.f, 5.f}, {0, 0, 12.f}, {1.f, 2.f, 0}};
	owl::scene::component::SpriteRenderer spr;
	spr.texture = Texture2D::create(Texture2D::Specification{.size = {2, 2}});
	Renderer2D::drawQuad({.transform = tr,
						  .color = spr.color,
						  .texture = spr.texture,
						  .tilingFactor = spr.tilingFactor,
						  .entityId = 1});
	Renderer2D::drawQuad({.transform = tr,
						  .color = spr.color,
						  .texture = spr.texture,
						  .tilingFactor = spr.tilingFactor,
						  .entityId = 2});
	Renderer2D::drawQuad({.transform = tr});
	Renderer2D::endScene();
	const auto st = Renderer2D::getStats();
	EXPECT_EQ(st.drawCalls, 1);
	EXPECT_EQ(st.quadCount, 3);
	EXPECT_EQ(st.getTotalIndexCount(), 18);
	EXPECT_EQ(st.getTotalVertexCount(), 12);

	RenderCommand::invalidate();
	Log::invalidate();
}

TEST(Renderer2D, sceneWorldsUploadedAndGrownAtFlush) {
	Log::init(owl::core::Log::Level::Off);
	RenderCommand::create(RenderAPI::Type::Null);
	Renderer::init();
	const CameraEditor cam;
	for (const size_t count: {size_t{64}, size_t{1000}, size_t{0}}) {
		std::vector<mat4> worlds(count, identity<float, 4>());
		Renderer2D::setSceneWorlds(worlds);
		Renderer2D::resetStats();
		Renderer2D::beginScene(cam);
		Renderer2D::drawQuad(
				{.transform = Transform{}, .worldIndex = count == 0 ? -1 : static_cast<int32_t>(count - 1)});
		Renderer2D::endScene();
		EXPECT_EQ(Renderer2D::getStats().quadCount, 1u);
	}

	RenderCommand::invalidate();
	Log::invalidate();
}

TEST(Renderer2D, instancedQuadBatchRollover) {
	Log::init(owl::core::Log::Level::Off);
	RenderCommand::create(RenderAPI::Type::Null);
	Renderer::init();
	const CameraEditor cam;
	Renderer2D::resetStats();
	Renderer2D::beginScene(cam);
	const Transform tr{{0.f, 0.f, 0.f}, {0, 0, 0}, {1.f, 1.f, 1.f}};
	constexpr uint32_t kQuadCount = 20'001u;
	for (uint32_t i = 0; i < kQuadCount; ++i) Renderer2D::drawQuad({.transform = tr, .entityId = static_cast<int>(i)});
	Renderer2D::endScene();
	const auto st = Renderer2D::getStats();
	EXPECT_EQ(st.quadCount, kQuadCount);
	EXPECT_GE(st.drawCalls, 2u);

	RenderCommand::invalidate();
	Log::invalidate();
}

TEST(Renderer2D, instancedLineBatch) {
	Log::init(owl::core::Log::Level::Off);
	RenderCommand::create(RenderAPI::Type::Null);
	Renderer::init();
	const CameraEditor cam;
	Renderer2D::resetStats();
	Renderer2D::beginScene(cam);
	for (int i = 0; i < 100; ++i) Renderer2D::drawLine({.point1 = {0, 0, 0}, .point2 = {1.f, 1.f, 0.f}, .entityId = i});
	Renderer2D::endScene();
	const auto st = Renderer2D::getStats();
	EXPECT_EQ(st.lineCount, 100u);
	EXPECT_EQ(st.quadCount, 0u);
	EXPECT_EQ(st.drawCalls, 1u);

	RenderCommand::invalidate();
	Log::invalidate();
}

TEST(Renderer2D, fakeTextScene) {
	Log::init(owl::core::Log::Level::Off);
	auto app = owl::mkShared<Application>(
			AppParams{.renderer = RenderAPI::Type::Null, .hasGui = false, .useDebugging = false, .isDummy = true});
	const auto font = app->getFontLibrary().getDefaultFont();

	RenderCommand::create(RenderAPI::Type::Null);
	Renderer::init();
	const CameraEditor cam;
	Renderer2D::resetStats();
	Renderer2D::beginScene(cam);
	Renderer2D::drawString({.transform = Transform{owl::math::identity<float, 4>()}, .text = "", .font = nullptr});
	Renderer2D::nextBatch();
	Renderer2D::drawString({.transform = Transform{owl::math::identity<float, 4>()}, .text = "bob\r\n", .font = font});
	Renderer2D::endScene();
	const auto st = Renderer2D::getStats();
	EXPECT_EQ(st.drawCalls, 1);
	EXPECT_EQ(st.quadCount, 3);
	EXPECT_EQ(st.getTotalIndexCount(), 18);
	EXPECT_EQ(st.getTotalVertexCount(), 12);

	RenderCommand::invalidate();
	Application::invalidate();
	app.reset();
	Log::invalidate();
}

TEST(Renderer2D, utf8TextBeyondLatin1StaysOneDrawCall) {
	Log::init(owl::core::Log::Level::Off);
	auto app = owl::mkShared<Application>(
			AppParams{.renderer = RenderAPI::Type::Null, .hasGui = false, .useDebugging = false, .isDummy = true});
	const auto font = app->getFontLibrary().getDefaultFont();
	ASSERT_NE(font, nullptr);

	RenderCommand::create(RenderAPI::Type::Null);
	Renderer::init();
	const CameraEditor cam;
	// 10 code points, half of them beyond U+00FF (œ Œ € ’ …), 1000 times; the space draws no quad.
	const std::string line = "\u0153uvre \u0152\u20AC\u2019\u2026";
	std::string text;
	for (int i = 0; i < 1000; ++i) text += line;
	Renderer2D::resetStats();
	Renderer2D::beginScene(cam);
	Renderer2D::drawString({.transform = Transform{owl::math::identity<float, 4>()}, .text = text, .font = font});
	Renderer2D::endScene();
	const auto st = Renderer2D::getStats();
	EXPECT_EQ(st.quadCount, 9000u);
	EXPECT_EQ(st.drawCalls, 1u);

	RenderCommand::invalidate();
	Application::invalidate();
	app.reset();
	Log::invalidate();
}

TEST(Renderer2D, laidOutTextStaysOneDrawCall) {
	Log::init(owl::core::Log::Level::Off);
	auto app = owl::mkShared<Application>(
			AppParams{.renderer = RenderAPI::Type::Null, .hasGui = false, .useDebugging = false, .isDummy = true});
	const auto font = app->getFontLibrary().getDefaultFont();
	ASSERT_NE(font, nullptr);

	RenderCommand::create(RenderAPI::Type::Null);
	Renderer::init();
	const CameraEditor cam;
	std::string text;
	for (int i = 0; i < 1000; ++i) text += "abcd efghi ";
	const owl::data::fonts::TextLayout layout{.fontSize = 2.f,
											  .boxSize = {40.f, 20.f},
											  .horizontal = owl::data::fonts::HorizontalAlignment::Right,
											  .vertical = owl::data::fonts::VerticalAlignment::Top,
											  .wrap = true,
											  .fit = owl::data::fonts::TextFit::Shrink};
	Renderer2D::resetStats();
	Renderer2D::beginScene(cam);
	Renderer2D::drawString(
			{.transform = Transform{owl::math::identity<float, 4>()}, .text = text, .font = font, .layout = layout});
	Renderer2D::endScene();
	const auto st = Renderer2D::getStats();
	EXPECT_EQ(st.quadCount, 9000u);
	EXPECT_EQ(st.drawCalls, 1u);

	RenderCommand::invalidate();
	Application::invalidate();
	app.reset();
	Log::invalidate();
}

TEST(Renderer2D, transientQuadsFlushEveryShapeTransientBatch) {
	Log::init(owl::core::Log::Level::Off);
	RenderCommand::create(RenderAPI::Type::Null);
	Renderer::init();
	const CameraEditor cam;
	const Transform tr{{1.f, 2.f, 0.f}, {0, 0, 0}, {1.f, 1.f, 1.f}};
	for (const auto& [count, expectedCalls]: {std::pair{4096u, 1u}, std::pair{4097u, 2u}, std::pair{20'000u, 5u}}) {
		Renderer2D::resetStats();
		Renderer2D::beginScene(cam);
		for (uint32_t i = 0; i < count; ++i) Renderer2D::drawQuad({.transform = tr});
		Renderer2D::endScene();
		EXPECT_EQ(Renderer2D::getStats().quadCount, count);
		EXPECT_EQ(Renderer2D::getStats().drawCalls, expectedCalls);
	}

	Renderer2D::shutdown();
	RenderCommand::invalidate();
	Log::invalidate();
}

TEST(Renderer2D, texturesKeepTheirSlotAcrossBatches) {
	Log::init(owl::core::Log::Level::Off);
	RenderCommand::create(RenderAPI::Type::Null);
	Renderer::init();
	const CameraEditor cam;
	const owl::shared<Texture> texture = Texture2D::create(Texture2D::Specification{.size = {2, 2}});
	const owl::shared<Texture> other = Texture2D::create(Texture2D::Specification{.size = {2, 2}});
	Renderer2D::resetStats();
	Renderer2D::beginScene(cam);
	for (uint32_t i = 0; i < 100; ++i) {
		Renderer2D::drawQuad({.transform = Transform{}, .texture = (i % 2) == 0 ? texture : other});
		if (i == 50)
			Renderer2D::nextBatch();
	}
	Renderer2D::drawRect({.transform = Transform{}});
	Renderer2D::endScene();
	const auto st = Renderer2D::getStats();
	EXPECT_EQ(st.quadCount, 100u);
	EXPECT_EQ(st.lineCount, 4u);
	EXPECT_EQ(st.drawCalls, 3u);

	Renderer2D::shutdown();
	RenderCommand::invalidate();
	Log::invalidate();
}

TEST(Renderer2D, aScissorSplitsTheBatchAndIsLiftedByTheScene) {
	Log::init(owl::core::Log::Level::Off);
	RenderCommand::create(RenderAPI::Type::Null);
	Renderer::init();
	const CameraOrtho cam(0, 800, 0, 600);
	Renderer2D::resetStats();
	Renderer2D::beginScene(cam);
	Renderer2D::drawQuad({.transform = Transform{}});
	Renderer2D::setScissor({.position = {10, 20}, .size = {100, 50}, .targetHeight = 600});
	Renderer2D::drawQuad({.transform = Transform{}});
	Renderer2D::clearScissor();
	Renderer2D::drawQuad({.transform = Transform{}});
	Renderer2D::endScene();
	const auto st = Renderer2D::getStats();
	EXPECT_EQ(st.quadCount, 3u);
	// One batch before the rectangle, one inside it, one after.
	EXPECT_EQ(st.drawCalls, 3u);

	// A rectangle left set when the scene ends does not leak into the next one.
	Renderer2D::resetStats();
	Renderer2D::beginScene(cam);
	Renderer2D::setScissor({.position = {0, 0}, .size = {1, 1}, .targetHeight = 600});
	Renderer2D::drawQuad({.transform = Transform{}});
	Renderer2D::endScene();
	Renderer2D::beginScene(cam);
	Renderer2D::drawQuad({.transform = Transform{}});
	Renderer2D::endScene();
	EXPECT_EQ(Renderer2D::getStats().quadCount, 2u);

	Renderer2D::shutdown();
	RenderCommand::invalidate();
	Log::invalidate();
}
