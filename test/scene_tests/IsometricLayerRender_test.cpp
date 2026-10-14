/**
 * @file IsometricLayerRender_test.cpp
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <app/Application.h>
#include <core/Log.h>
#include <core/Timestep.h>
#include <renderer/RenderStack.h>
#include <renderer/Renderer.h>
#include <renderer/Renderer2D.h>
#include <renderer/RendererIsometric.h>
#include <renderer/RendererIsometricLayer.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/component/components.h>

#include <chrono>
#include <string>

using namespace owl;

namespace {

class IsometricLayerRenderTest : public testing::Test {
protected:
	static void SetUpTestSuite() {
		core::Log::init(core::Log::Level::Off);
		s_app = mkShared<app::Application>(app::AppParams{.args = nullptr,
														  .frameLogFrequency = 0,
														  .name = "isometricLayerRender",
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
		config.entries.push_back({.typeKey = "RendererIsometric", .name = "iso", .defaultConfig = "Zoom: 1"});
		renderer::Renderer::setRenderStack(
				renderer::RenderStack::buildFromConfig(config, renderer::EnabledRenderersConfig{}));
	}

	void TearDown() override { renderer::Renderer::setRenderStack(renderer::RenderStack{}); }

	inline static shared<app::Application> s_app;
};

auto makeStep() -> core::Timestep {
	core::Timestep ts;
	ts.forceUpdate(std::chrono::milliseconds(16));
	return ts;
}

void addSprite(scene::Scene& ioScene, const std::string& iName, const math::vec3& iPosition, const float iAlpha,
			   const std::string& iLayer) {
	auto entity = ioScene.createEntity(iName);
	entity.getComponent<scene::component::Transform>().transform.translation() = iPosition;
	entity.addComponent<scene::component::SpriteRenderer>().color = math::vec4{1.f, 1.f, 1.f, iAlpha};
	if (!iLayer.empty())
		entity.addComponent<scene::component::RendererTag>().rendererName = iLayer;
}

auto makeScene() -> shared<scene::Scene> {
	auto sc = mkShared<scene::Scene>();
	auto cam = sc->createEntity("Camera");
	cam.getComponent<scene::component::Transform>().transform.translation() = math::vec3{4.f, 4.f, 5.f};
	auto& camera = cam.addComponent<scene::component::Camera>();
	camera.primary = true;
	addSprite(*sc, "Flat", {0.f, 0.f, 0.f}, 1.f, "");
	addSprite(*sc, "House", {4.f, 4.f, 0.f}, 1.f, "iso");
	addSprite(*sc, "Smoke", {5.f, 4.f, 1.f}, 0.5f, "iso");
	sc->onViewportResize({640, 360});
	return sc;
}

}// namespace

TEST_F(IsometricLayerRenderTest, selfDrawnLayerNeedsContent) {
	const auto sc = makeScene();
	const auto layer = sc->getSelfDrawnLayer();
	ASSERT_NE(layer, nullptr);
	EXPECT_STREQ(layer->getTypeKey(), "RendererIsometric");

	scene::Scene flat;
	addSprite(flat, "Flat", {0.f, 0.f, 0.f}, 1.f, "");
	EXPECT_EQ(flat.getSelfDrawnLayer(), nullptr);
}

TEST_F(IsometricLayerRenderTest, editorDrawsTheStackLayerByLayer) {
	const auto sc = makeScene();
	renderer::IsometricCamera camera;
	camera.setViewportSize({640, 360});
	camera.setTarget({4.f, 4.f, 0.f});
	sc->onUpdateEditor(makeStep(), camera);
	const auto stats = renderer::RendererIsometric::getStatistics();
	EXPECT_EQ(stats.opaqueCount, 1u);
	EXPECT_EQ(stats.translucentCount, 1u);
	// The 2D pass drew only the untagged sprite: the isometric ones are the layer's own.
	EXPECT_EQ(renderer::Renderer2D::getStats().quadCount, 1u);
}

TEST_F(IsometricLayerRenderTest, runtimeDrawsTheIsometricLayerWithTheSceneCamera) {
	const auto sc = makeScene();
	sc->status = scene::Scene::Status::Playing;
	sc->onRenderRuntime();
	const auto stats = renderer::RendererIsometric::getStatistics();
	EXPECT_EQ(stats.opaqueCount, 1u);
	EXPECT_EQ(stats.translucentCount, 1u);
	EXPECT_EQ(renderer::Renderer2D::getStats().quadCount, 1u);
}

TEST_F(IsometricLayerRenderTest, hiddenSpritesAreSkipped) {
	const auto sc = makeScene();
	for (const auto view = sc->registry.view<scene::component::Tag>(); const auto entity: view) {
		if (view.get<scene::component::Tag>(entity).tag == "Smoke")
			scene::Entity{entity, sc.get()}.addOrReplaceComponent<scene::component::Visibility>().gameVisible = false;
	}
	sc->status = scene::Scene::Status::Playing;
	sc->onRenderRuntime();
	const auto stats = renderer::RendererIsometric::getStatistics();
	EXPECT_EQ(stats.opaqueCount, 1u);
	EXPECT_EQ(stats.translucentCount, 0u);
}
