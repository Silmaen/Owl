/**
 * @file HudMarker_test.cpp
 * @author Silmaen
 * @date 31/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <app/Application.h>
#include <core/Log.h>
#include <renderer/CameraOrtho.h>
#include <renderer/RenderLayer.h>
#include <renderer/Renderer.h>
#include <renderer/Renderer2D.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/component/components.h>
#include <scene/ui/HudMarkerPlacement.h>

#include <cstdint>
#include <numbers>

using namespace owl;
using scene::component::HudMarker;

TEST(HudMarkerProjection, aMatrixMapsClipSpaceToYDownPixels) {
	const auto identity = math::identity<float, 4>();
	const math::vec2ui viewport{800, 600};
	const auto centre = renderer::projectToViewport(identity, viewport, {0.f, 0.f, 0.f});
	EXPECT_TRUE(centre.ahead);
	EXPECT_NEAR(centre.position.x(), 400.f, 1e-4f);
	EXPECT_NEAR(centre.position.y(), 300.f, 1e-4f);
	const auto corner = renderer::projectToViewport(identity, viewport, {1.f, 1.f, 0.f});
	EXPECT_NEAR(corner.position.x(), 800.f, 1e-4f);
	EXPECT_NEAR(corner.position.y(), 0.f, 1e-4f);
}

TEST(HudMarkerProjection, aPointBehindAPerspectiveCameraStaysOnItsSide) {
	// w = -z, as a perspective projection looking down -Z.
	auto projection = math::identity<float, 4>();
	projection(3, 2) = -1.f;
	projection(3, 3) = 0.f;
	const auto behind = renderer::projectToViewport(projection, {800, 600}, {0.5f, 0.f, 1.f});
	EXPECT_FALSE(behind.ahead);
	EXPECT_GT(behind.position.x(), 400.f);
	const auto ahead = renderer::projectToViewport(projection, {800, 600}, {0.5f, 0.f, -1.f});
	EXPECT_TRUE(ahead.ahead);
	EXPECT_NEAR(ahead.position.x(), 600.f, 1e-4f);
}

TEST(HudMarkerPlacement, aPointInViewStaysWhereItIs) {
	const auto placement = scene::ui::placeHudMarker({.position = {100.f, 50.f}, .ahead = true}, {800.f, 600.f}, 10.f);
	EXPECT_TRUE(placement.onScreen);
	EXPECT_NEAR(placement.position.x(), 100.f, 1e-4f);
	EXPECT_NEAR(placement.position.y(), 50.f, 1e-4f);
}

TEST(HudMarkerPlacement, anOffScreenPointSlidesToTheEdgeTowardsIt) {
	const auto right = scene::ui::placeHudMarker({.position = {2000.f, 300.f}, .ahead = true}, {800.f, 600.f}, 10.f);
	EXPECT_FALSE(right.onScreen);
	EXPECT_NEAR(right.position.x(), 790.f, 1e-3f);
	EXPECT_NEAR(right.position.y(), 300.f, 1e-3f);
	EXPECT_NEAR(right.angle, 0.f, 1e-5f);
	// Along the diagonal the nearer border wins: the bottom one, 290 pixels under the centre.
	const auto corner = scene::ui::placeHudMarker({.position = {1400.f, 1300.f}, .ahead = true}, {800.f, 600.f}, 10.f);
	EXPECT_NEAR(corner.position.y(), 590.f, 1e-3f);
	EXPECT_NEAR(corner.position.x(), 690.f, 1e-3f);
	EXPECT_NEAR(corner.angle, std::numbers::pi_v<float> / 4.f, 1e-5f);
}

TEST(HudMarkerPlacement, aPointBehindGoesToTheEdgeEvenInsideTheRectangle) {
	const auto behind = scene::ui::placeHudMarker({.position = {500.f, 300.f}, .ahead = false}, {800.f, 600.f}, 0.f);
	EXPECT_FALSE(behind.onScreen);
	EXPECT_NEAR(behind.position.x(), 800.f, 1e-3f);
	const auto straight = scene::ui::placeHudMarker({.position = {400.f, 300.f}, .ahead = false}, {800.f, 600.f}, 0.f);
	EXPECT_NEAR(straight.position.y(), 600.f, 1e-3f);
}

TEST(HudMarkerScene, theViewerIsThePrimaryPlayerOutsideAFrame) {
	core::Log::init(core::Log::Level::Off);
	scene::Scene scn;
	EXPECT_FALSE(scn.getViewerPosition().has_value());
	auto marker = scn.createEntity("Waypoint");
	EXPECT_FALSE(scn.projectToViewport(marker, {0.f, 0.f, 0.f}, {800, 600}).has_value());
	auto player = scn.createEntity("Player");
	player.addComponent<scene::component::Player>();
	player.getComponent<scene::component::Transform>().transform.translation() = {3.f, 4.f, 0.f};
	const auto viewer = scn.getViewerPosition();
	ASSERT_TRUE(viewer.has_value());
	EXPECT_NEAR(viewer->x(), 3.f, 1e-5f);
	EXPECT_NEAR(viewer->y(), 4.f, 1e-5f);
	core::Log::invalidate();
}

namespace {

class HudMarkerPassTest : public testing::Test {
protected:
	static void SetUpTestSuite() {
		core::Log::init(core::Log::Level::Off);
		s_app = mkShared<app::Application>(app::AppParams{.args = nullptr,
														  .frameLogFrequency = 0,
														  .name = "hudMarkerPass",
														  .assetsPattern = "",
														  .icon = "",
														  .width = 0,
														  .height = 0,
														  .argCount = 0,
														  .renderer = renderer::gpu::RenderAPI::Type::Null,
														  .hasGui = false,
														  .useDebugging = false,
														  .isDummy = true});
	}

	static void TearDownTestSuite() {
		app::Application::invalidate();
		s_app.reset();
		core::Log::invalidate();
	}

	void SetUp() override {
		renderer::RendererStackConfig config;
		config.entries.push_back({.typeKey = "Renderer2D", .name = "world", .defaultConfig = {}});
		renderer::Renderer::setRenderStack(
				renderer::RenderStack::buildFromConfig(config, renderer::EnabledRenderersConfig{}));
		m_scene = mkShared<scene::Scene>();
		m_scene->onViewportResize({640, 360});
		m_marker = m_scene->createEntity("Waypoint");
		m_marker.addComponent<HudMarker>();
	}

	void TearDown() override {
		m_scene.reset();
		renderer::Renderer::setRenderStack(renderer::RenderStack{});
	}

	// Quads drawn by one editor frame through a camera showing the world square [0, 10]².
	[[nodiscard]] auto drawnQuads() const -> uint32_t {
		renderer::Renderer2D::resetStats();
		const renderer::CameraOrtho camera{0.f, 10.f, 0.f, 10.f};
		m_scene->onUpdateEditor(core::Timestep{}, camera);
		return renderer::Renderer2D::getStats().quadCount;
	}

	void placeMarker(const math::vec3& iPosition) const {
		m_marker.getComponent<scene::component::Transform>().transform.translation() = iPosition;
	}

	inline static shared<app::Application> s_app;
	shared<scene::Scene> m_scene;
	scene::Entity m_marker;
};

}// namespace

TEST_F(HudMarkerPassTest, aMarkerInViewDrawsItsIcon) {
	placeMarker({5.f, 5.f, 0.f});
	EXPECT_EQ(drawnQuads(), 1u);
}

TEST_F(HudMarkerPassTest, anOffScreenMarkerAddsItsEdgeArrow) {
	placeMarker({50.f, 5.f, 0.f});
	EXPECT_EQ(drawnQuads(), 3u);
	m_marker.getComponent<HudMarker>().offscreenIndicator = false;
	EXPECT_EQ(drawnQuads(), 0u);
}

TEST_F(HudMarkerPassTest, aDisabledOrFarMarkerDrawsNothing) {
	placeMarker({5.f, 5.f, 0.f});
	auto& marker = m_marker.getComponent<HudMarker>();
	marker.enabled = false;
	EXPECT_EQ(drawnQuads(), 0u);
	marker.enabled = true;
	auto player = m_scene->createEntity("Player");
	player.addComponent<scene::component::Player>();
	marker.maxDistance = 2.f;
	EXPECT_EQ(drawnQuads(), 0u);
	marker.maxDistance = 10.f;
	EXPECT_EQ(drawnQuads(), 1u);
}

TEST_F(HudMarkerPassTest, aHiddenEntityHidesItsMarker) {
	placeMarker({5.f, 5.f, 0.f});
	m_marker.addOrReplaceComponent<scene::component::Visibility>().editorVisible = false;
	EXPECT_EQ(drawnQuads(), 0u);
}
