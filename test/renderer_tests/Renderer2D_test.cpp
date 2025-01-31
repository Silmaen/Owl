
#include "testHelper.h"

#include <core/Application.h>
#include <renderer/Renderer.h>
#include <renderer/Renderer2D.h>

using namespace owl::renderer;
using namespace owl::core;
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
	EXPECT_EQ(st.drawCalls, 5);
	EXPECT_EQ(st.quadCount, 0);
	EXPECT_EQ(st.getTotalIndexCount(), 0);
	EXPECT_EQ(st.getTotalVertexCount(), 0);

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
	EXPECT_EQ(st.drawCalls, 6);
	EXPECT_EQ(st.quadCount, 1);
	EXPECT_EQ(st.getTotalIndexCount(), 6);
	EXPECT_EQ(st.getTotalVertexCount(), 4);

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
