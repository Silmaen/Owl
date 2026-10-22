/**
 * @file SpatialCulling_test.cpp
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <app/Application.h>
#include <renderer/RenderStack.h>
#include <renderer/Renderer.h>
#include <renderer/Renderer2D.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/component/components.h>

#include <string>

using namespace owl;

namespace {

class SpatialCullingTest : public testing::Test {
protected:
	static void SetUpTestSuite() {
		core::Log::init(core::Log::Level::Off);
		s_app = mkShared<app::Application>(app::AppParams{.args = nullptr,
														  .frameLogFrequency = 0,
														  .name = "spatialCulling",
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
	}

	void TearDown() override { renderer::Renderer::setRenderStack(renderer::RenderStack{}); }

	inline static shared<app::Application> s_app;
};

auto addSprite(scene::Scene& ioScene, const std::string& iName, const math::vec3& iPosition) -> scene::Entity {
	auto entity = ioScene.createEntity(iName);
	entity.getComponent<scene::component::Transform>().transform.translation() = iPosition;
	entity.addComponent<scene::component::SpriteRenderer>();
	return entity;
}

// An orthographic camera of size 10 at the origin, 640x360: x in [-8.9, 8.9], y in [-5, 5].
auto makeScene() -> shared<scene::Scene> {
	auto sc = mkShared<scene::Scene>();
	auto cam = sc->createEntity("Camera");
	cam.getComponent<scene::component::Transform>().transform.translation() = math::vec3{0.f, 0.f, 5.f};
	auto& camera = cam.addComponent<scene::component::Camera>();
	camera.primary = true;
	camera.camera.setOrthographicFarClip(10.f);
	sc->onViewportResize({640, 360});
	sc->status = scene::Scene::Status::Playing;
	return sc;
}

}// namespace

TEST_F(SpatialCullingTest, spritesOutsideTheCameraAreNotDrawn) {
	const auto sc = makeScene();
	addSprite(*sc, "In", {1.f, 1.f, 0.f});
	addSprite(*sc, "Edge", {9.f, 0.f, 0.f});
	addSprite(*sc, "Out", {40.f, 0.f, 0.f});
	addSprite(*sc, "Above", {0.f, 30.f, 0.f});
	sc->onRenderRuntime();
	const auto& stats = sc->getCullingStatistics();
	EXPECT_EQ(stats.spritesIndexed, 4u);
	EXPECT_EQ(stats.spritesInFrustum, 2u);
	EXPECT_EQ(renderer::Renderer2D::getStats().quadCount, 2u);
}

TEST_F(SpatialCullingTest, aMovedSpriteIsRefiled) {
	const auto sc = makeScene();
	auto sprite = addSprite(*sc, "Moving", {40.f, 0.f, 0.f});
	sc->onRenderRuntime();
	EXPECT_EQ(sc->getCullingStatistics().spritesInFrustum, 0u);
	sprite.getComponent<scene::component::Transform>().transform.translation() = math::vec3{0.f, 0.f, 0.f};
	sc->onRenderRuntime();
	EXPECT_EQ(sc->getCullingStatistics().spritesInFrustum, 1u);
	EXPECT_EQ(renderer::Renderer2D::getStats().quadCount, 1u);
}

TEST_F(SpatialCullingTest, aChildFollowsItsParent) {
	const auto sc = makeScene();
	auto parent = sc->createEntity("Parent");
	parent.getComponent<scene::component::Transform>().transform.translation() = math::vec3{40.f, 0.f, 0.f};
	auto child = addSprite(*sc, "Child", {0.f, 0.f, 0.f});
	sc->setParent(child, parent);
	// `setParent` keeps the world position: the local offset is set afterwards.
	child.getComponent<scene::component::Transform>().transform.translation() = math::vec3{1.f, 0.f, 0.f};
	sc->onRenderRuntime();
	EXPECT_EQ(sc->getCullingStatistics().spritesInFrustum, 0u);
	parent.getComponent<scene::component::Transform>().transform.translation() = math::vec3{0.f, 0.f, 0.f};
	sc->onRenderRuntime();
	EXPECT_EQ(sc->getCullingStatistics().spritesInFrustum, 1u);
}

TEST_F(SpatialCullingTest, removedSpritesLeaveTheIndex) {
	const auto sc = makeScene();
	auto first = addSprite(*sc, "First", {0.f, 0.f, 0.f});
	auto second = addSprite(*sc, "Second", {1.f, 0.f, 0.f});
	sc->onRenderRuntime();
	EXPECT_EQ(sc->getSpriteSpatialIndex().size(), 2u);
	second.removeComponent<scene::component::SpriteRenderer>();
	sc->destroyEntity(first);
	sc->onRenderRuntime();
	EXPECT_EQ(sc->getSpriteSpatialIndex().size(), 0u);
	EXPECT_EQ(renderer::Renderer2D::getStats().quadCount, 0u);
}
