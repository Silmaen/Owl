
#include "testHelper.h"

#include <renderer/Renderer.h>
#include <renderer/Renderer2D.h>

using namespace owl::renderer;

TEST(Renderer2D, fakeEmptyScene) {
	owl::core::Log::init(spdlog::level::off);
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
	EXPECT_NEAR(Renderer2D::getLineWidth(), 2, 0.01);
	Renderer2D::setLineWidth(0.24f);
	EXPECT_NEAR(Renderer2D::getLineWidth(), 0.24, 0.01);

	RenderCommand::invalidate();
	owl::core::Log::invalidate();
}

TEST(Renderer2D, fakeLineScene) {
	owl::core::Log::init(spdlog::level::off);
	RenderCommand::create(RenderAPI::Type::Null);
	Renderer::init();
	CameraEditor cam;
	Renderer2D::resetStats();
	Renderer2D::beginScene(cam);
	Renderer2D::drawLine({.point1 = {0, 0, 0},
						  .point2 = {1.f, 1.f, 0.f}});
	PolyLineData data{.transform = utils::Transform2D{owl::math::identity<float,4>()},
					  .points = std::vector<owl::math::vec3>{}};
	Renderer2D::drawPolyLine(data);
	data.points.emplace_back(owl::math::vec3{1.f, 1.f, 1.f});
	data.points.emplace_back(owl::math::vec3{1.f, 0.f, 1.f});
	data.points.emplace_back(owl::math::vec3{0.f, 1.f, 1.f});
	data.closed = true;
	Renderer2D::drawPolyLine(data);
	Renderer2D::endScene();
	auto st = Renderer2D::getStats();
	EXPECT_EQ(st.drawCalls, 5);
	EXPECT_EQ(st.quadCount, 0);
	EXPECT_EQ(st.getTotalIndexCount(), 0);
	EXPECT_EQ(st.getTotalVertexCount(), 0);

	Renderer2D::shutdown();
	Renderer2D::shutdown();
	RenderCommand::invalidate();
	owl::core::Log::invalidate();
}

TEST(Renderer2D, fakeCircleRectScene) {
	owl::core::Log::init(spdlog::level::off);
	RenderCommand::create(RenderAPI::Type::Null);
	Renderer::init();
	const CameraEditor cam;
	Renderer2D::resetStats();
	Renderer2D::beginScene(cam, owl::math::identity<float,4>());
	Renderer2D::drawRect({.transform = utils::Transform2D{owl::math::identity<float,4>()}});
	Renderer2D::nextBatch();
	Renderer2D::drawCircle({.transform = utils::Transform2D{owl::math::identity<float,4>()}});
	Renderer2D::endScene();
	const auto st = Renderer2D::getStats();
	EXPECT_EQ(st.drawCalls, 6);
	EXPECT_EQ(st.quadCount, 1);
	EXPECT_EQ(st.getTotalIndexCount(), 6);
	EXPECT_EQ(st.getTotalVertexCount(), 4);

	RenderCommand::invalidate();
	owl::core::Log::invalidate();
}


TEST(Renderer2D, fakeQuadSpriteScene) {
	owl::core::Log::init(spdlog::level::off);
	RenderCommand::create(RenderAPI::Type::Null);
	Renderer::init();
	CameraEditor cam;
	Renderer2D::resetStats();
	Renderer2D::beginScene(cam);
	utils::PRS tr{.position = {0.f, 1.f, 5.f},
				  .rotation = 12,
				  .size = {1.f, 2.f}};
	owl::scene::component::SpriteRenderer spr;
	spr.texture = Texture2D::create(2, 2);
	Renderer2D::drawSprite(toTransform(tr), spr, 1);
	Renderer2D::drawSprite(toTransform(tr), spr, 2);
	Renderer2D::drawQuad({toTransform(tr)});
	Renderer2D::endScene();
	auto st = Renderer2D::getStats();
	EXPECT_EQ(st.drawCalls, 1);
	EXPECT_EQ(st.quadCount, 3);
	EXPECT_EQ(st.getTotalIndexCount(), 18);
	EXPECT_EQ(st.getTotalVertexCount(), 12);

	RenderCommand::invalidate();
	owl::core::Log::invalidate();
}
