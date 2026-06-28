/**
 * @file RendererIsometric_test.cpp
 * @author Silmaen
 * @date 27/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <renderer/RenderLayerFactory.h>
#include <renderer/Renderer.h>
#include <renderer/RendererIsometric.h>

#include "renderer/RendererIsometricLayer.h"

using namespace owl;
using owl::renderer::dimetricViewProjection;
using owl::renderer::IsometricConfig;
using owl::renderer::Renderer;
using owl::renderer::RendererIsometricLayer;
using owl::renderer::RenderLayerFactory;
using owl::renderer::screenToWorld;
using owl::renderer::worldToScreen;

namespace {
auto bootRendererStack() -> void {
	core::Log::init(core::Log::Level::Off);
	renderer::gpu::RenderCommand::create(renderer::gpu::RenderAPI::Type::Null);
	Renderer::init();
}

auto teardownRendererStack() -> void {
	Renderer::shutdown();
	renderer::gpu::RenderCommand::invalidate();
	core::Log::invalidate();
}
}// namespace

TEST(RendererIsometric, factoryRegistersTypeKey) {
	bootRendererStack();
	EXPECT_TRUE(RenderLayerFactory::hasType("RendererIsometric"));
	const auto layer = RenderLayerFactory::create("RendererIsometric", "iso");
	ASSERT_NE(layer, nullptr);
	EXPECT_STREQ(layer->getTypeKey(), "RendererIsometric");
	EXPECT_EQ(layer->getName(), "iso");
	teardownRendererStack();
}

TEST(RendererIsometric, defaultConfigIsTwoToOne) {
	const IsometricConfig cfg;
	EXPECT_EQ(cfg.tileSize.x(), 64u);
	EXPECT_EQ(cfg.tileSize.y(), 32u);
}

TEST(RendererIsometric, applyConfigParsesYaml) {
	bootRendererStack();
	auto layerBase = RenderLayerFactory::create("RendererIsometric", "iso");
	ASSERT_NE(layerBase, nullptr);
	auto* layer = dynamic_cast<RendererIsometricLayer*>(layerBase.get());
	ASSERT_NE(layer, nullptr);

	YAML::Node cfg;
	cfg["TileWidth"] = 128u;
	cfg["TileHeight"] = 64u;
	cfg["ZStep"] = 24.f;
	cfg["Origin"].push_back(10.f);
	cfg["Origin"].push_back(20.f);
	layer->applyConfig(cfg);

	EXPECT_EQ(layer->getConfig().tileSize.x(), 128u);
	EXPECT_EQ(layer->getConfig().tileSize.y(), 64u);
	EXPECT_FLOAT_EQ(layer->getConfig().zStep, 24.f);
	EXPECT_FLOAT_EQ(layer->getConfig().origin.x(), 10.f);
	EXPECT_FLOAT_EQ(layer->getConfig().origin.y(), 20.f);
	teardownRendererStack();
}

TEST(RendererIsometric, applyConfigKeepsDefaultsOnEmpty) {
	bootRendererStack();
	auto layerBase = RenderLayerFactory::create("RendererIsometric", "iso");
	auto* layer = dynamic_cast<RendererIsometricLayer*>(layerBase.get());
	ASSERT_NE(layer, nullptr);

	const YAML::Node empty;
	layer->applyConfig(empty);

	EXPECT_EQ(layer->getConfig().tileSize.x(), 64u);
	EXPECT_EQ(layer->getConfig().tileSize.y(), 32u);
	teardownRendererStack();
}

TEST(RendererIsometric, worldScreenRoundTrip) {
	IsometricConfig cfg;
	cfg.origin = math::vec2{400.f, 50.f};
	for (const auto& world: {math::vec3{0.f, 0.f, 0.f}, math::vec3{3.f, 2.f, 1.f}, math::vec3{-4.f, 7.f, -2.f},
							 math::vec3{12.5f, -3.5f, 4.f}}) {
		const math::vec2 screen = worldToScreen(world, cfg);
		const math::vec3 back = screenToWorld(screen, world.z(), cfg);
		EXPECT_NEAR(back.x(), world.x(), 1e-3f);
		EXPECT_NEAR(back.y(), world.y(), 1e-3f);
		EXPECT_NEAR(back.z(), world.z(), 1e-3f);
	}
}

TEST(RendererIsometric, projectionFormulaMatchesRoadmap) {
	const IsometricConfig cfg;
	const math::vec3 world{3.f, 1.f, 2.f};
	const math::vec2 screen = worldToScreen(world, cfg);
	EXPECT_FLOAT_EQ(screen.x(), (3.f - 1.f) * 32.f);
	EXPECT_FLOAT_EQ(screen.y(), (3.f + 1.f) * 16.f - 2.f * cfg.zStep);
}

TEST(RendererIsometric, dimetricMatrixMatchesWorldToScreen) {
	const math::vec2ui viewport{800u, 600u};
	IsometricConfig cfg;
	cfg.origin = math::vec2{120.f, 40.f};
	const math::mat4 mat = dimetricViewProjection(viewport, cfg);

	for (const auto& world: {math::vec3{0.f, 0.f, 0.f}, math::vec3{5.f, 2.f, 1.f}, math::vec3{-3.f, 6.f, 3.f}}) {
		const math::vec2 screen = worldToScreen(world, cfg);
		const float expectedNdcX = 2.f * screen.x() / 800.f - 1.f;
		const float expectedNdcY = 1.f - 2.f * screen.y() / 600.f;
		const float ndcX =
				mat(0, 0) * world.x() + mat(0, 1) * world.y() + mat(0, 2) * world.z() + mat(0, 3);
		const float ndcY =
				mat(1, 0) * world.x() + mat(1, 1) * world.y() + mat(1, 2) * world.z() + mat(1, 3);
		EXPECT_NEAR(ndcX, expectedNdcX, 1e-4f);
		EXPECT_NEAR(ndcY, expectedNdcY, 1e-4f);
	}
}
