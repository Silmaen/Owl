/**
 * @file ParallelPreparation_test.cpp
 * @author Silmaen
 * @date 23/10/2026
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

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

using namespace owl;

namespace {

constexpr uint32_t g_count = 6000;

class ParallelPreparationTest : public testing::Test {
protected:
	static void SetUpTestSuite() {
		core::Log::init(core::Log::Level::Off);
		s_app = mkShared<app::Application>(app::AppParams{.args = nullptr,
														  .frameLogFrequency = 0,
														  .name = "parallelPreparation",
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

// The same scene built twice: one prepares on the scheduler whatever its size, the other never does.
struct TwinScenes {
	shared<scene::Scene> parallel;
	shared<scene::Scene> serial;
	std::vector<scene::Entity> parallelEntities;
	std::vector<scene::Entity> serialEntities;
};

auto makeScene(const bool iParallel, std::vector<scene::Entity>& oEntities) -> shared<scene::Scene> {
	auto sc = mkShared<scene::Scene>();
	sc->setParallelMinEntities(iParallel ? 0 : std::numeric_limits<size_t>::max());
	auto cam = sc->createEntity("Camera");
	cam.getComponent<scene::component::Transform>().transform.translation() = math::vec3{0.f, 0.f, 5.f};
	auto& camera = cam.addComponent<scene::component::Camera>();
	camera.primary = true;
	camera.camera.setOrthographicFarClip(10.f);
	sc->onViewportResize({640, 360});
	sc->status = scene::Scene::Status::Playing;
	uint32_t seed = 12345;
	const auto next = [&seed]() -> uint32_t {
		seed = seed * 1664525U + 1013904223U;
		return seed >> 8U;
	};
	for (uint32_t i = 0; i < g_count; ++i) {
		auto ent = sc->createEntity("e");
		auto& transform = ent.getComponent<scene::component::Transform>().transform;
		transform.translation() = math::vec3{static_cast<float>(next() % 400) / 10.f - 20.f,
											 static_cast<float>(next() % 200) / 10.f - 10.f, 0.f};
		ent.addComponent<scene::component::SpriteRenderer>();
		oEntities.push_back(ent);
	}
	// Roots, small trees, chains and a wide node, in an order where a child can precede its parent in the pools.
	for (uint32_t i = 1; i < g_count; ++i) {
		const uint32_t kind = i % 7;
		if (kind == 0 || kind == 3)
			continue;
		uint32_t parent = i < 100 ? 0 : i - 7;
		if (kind == 1)
			parent = i - 1;
		else if (kind == 2)
			parent = i - i % 7;
		sc->setParent(oEntities[i], oEntities[parent]);
	}
	for (uint32_t i = 0; i < g_count; i += 211) {
		oEntities[i].getComponent<scene::component::Visibility>().gameVisible = (i % 422) != 0;
	}
	return sc;
}

auto makeTwins() -> TwinScenes {
	TwinScenes twins;
	twins.parallel = makeScene(true, twins.parallelEntities);
	twins.serial = makeScene(false, twins.serialEntities);
	return twins;
}

void expectSameFrame(const TwinScenes& iTwins) {
	iTwins.serial->onRenderRuntime();
	const auto serialQuads = renderer::Renderer2D::getStats().quadCount;
	const auto serialStats = iTwins.serial->getCullingStatistics();
	iTwins.parallel->onRenderRuntime();
	EXPECT_EQ(renderer::Renderer2D::getStats().quadCount, serialQuads);
	const auto parallelStats = iTwins.parallel->getCullingStatistics();
	EXPECT_EQ(parallelStats.spritesIndexed, serialStats.spritesIndexed);
	EXPECT_EQ(parallelStats.spritesInFrustum, serialStats.spritesInFrustum);
	const auto serialWorlds = iTwins.serial->getWorldMatrices();
	const auto parallelWorlds = iTwins.parallel->getWorldMatrices();
	ASSERT_EQ(parallelWorlds.size(), serialWorlds.size());
	for (size_t i = 0; i < iTwins.serialEntities.size(); ++i) {
		const auto& serialEntity = iTwins.serialEntities[i];
		const auto& parallelEntity = iTwins.parallelEntities[i];
		if (!serialEntity || !parallelEntity) {
			EXPECT_EQ(static_cast<bool>(serialEntity), static_cast<bool>(parallelEntity));
			continue;
		}
		const uint32_t slot = iTwins.serial->getWorldIndex(serialEntity);
		ASSERT_EQ(iTwins.parallel->getWorldIndex(parallelEntity), slot) << "entity " << i;
		if (slot < serialWorlds.size())
			ASSERT_EQ(parallelWorlds[slot], serialWorlds[slot]) << "entity " << i;
	}
	for (size_t slot = 0; slot < serialWorlds.size(); ++slot) ASSERT_EQ(parallelWorlds[slot], serialWorlds[slot]);
}

template<typename F>
void onBoth(TwinScenes& ioTwins, F&& iAction) {
	const auto& iTwins = ioTwins;
	iAction(*iTwins.serial, ioTwins.serialEntities);
	iAction(*iTwins.parallel, ioTwins.parallelEntities);
}

}// namespace

TEST_F(ParallelPreparationTest, matchesTheSerialPath) {
	const auto twins = makeTwins();
	expectSameFrame(twins);
	expectSameFrame(twins);
}

TEST_F(ParallelPreparationTest, parentsComeBeforeChildren) {
	const auto twins = makeTwins();
	twins.parallel->prepareWorldTransforms();
	for (const auto& entity: twins.parallelEntities) {
		const core::UUID parentId = entity.getComponent<scene::component::Hierarchy>().parentId;
		if (parentId == core::UUID{0})
			continue;
		EXPECT_LT(twins.parallel->getWorldIndex(twins.parallel->findEntityByUUID(parentId)),
				  twins.parallel->getWorldIndex(entity));
	}
}

TEST_F(ParallelPreparationTest, followsMovesAndReparenting) {
	auto twins = makeTwins();
	expectSameFrame(twins);
	onBoth(twins, [](scene::Scene&, std::vector<scene::Entity>& iEntities) -> void {
		for (size_t i = 0; i < iEntities.size(); i += 5)
			iEntities[i].getComponent<scene::component::Transform>().transform.translation().x() += 3.f;
	});
	expectSameFrame(twins);
	onBoth(twins, [](scene::Scene& ioScene, std::vector<scene::Entity>& iEntities) -> void {
		for (size_t i = 10; i + 40 < iEntities.size(); i += 97) ioScene.setParent(iEntities[i], iEntities[i + 40]);
		ioScene.unparent(iEntities[7]);
	});
	expectSameFrame(twins);
	expectSameFrame(twins);
}

TEST_F(ParallelPreparationTest, followsCreationsAndDestructions) {
	auto twins = makeTwins();
	expectSameFrame(twins);
	onBoth(twins, [](scene::Scene& ioScene, std::vector<scene::Entity>& ioEntities) -> void {
		for (size_t i = 3; i < ioEntities.size(); i += 53) {
			if (!ioScene.registry.valid(static_cast<entt::entity>(ioEntities[i])))
				continue;
			if (i % 2 == 0)
				ioScene.destroyEntity(ioEntities[i]);
			else
				ioScene.destroyEntityWithChildren(ioEntities[i]);
		}
		auto fresh = ioScene.createEntity("fresh");
		fresh.addComponent<scene::component::SpriteRenderer>();
		ioEntities.push_back(fresh);
	});
	expectSameFrame(twins);
	expectSameFrame(twins);
}

TEST_F(ParallelPreparationTest, followsVisibilityChanges) {
	auto twins = makeTwins();
	expectSameFrame(twins);
	onBoth(twins, [](scene::Scene&, std::vector<scene::Entity>& iEntities) -> void {
		for (size_t i = 0; i < iEntities.size(); i += 301) {
			auto& visibility = iEntities[i].getComponent<scene::component::Visibility>();
			visibility.gameVisible = !visibility.gameVisible;
		}
	});
	expectSameFrame(twins);
}

TEST_F(ParallelPreparationTest, staleChildListsFallBackToTheSerialWalk) {
	auto twins = makeTwins();
	expectSameFrame(twins);
	onBoth(twins, [](scene::Scene&, std::vector<scene::Entity>& iEntities) -> void {
		for (size_t i = 0; i < iEntities.size(); i += 13)
			iEntities[i].getComponent<scene::component::Hierarchy>().childrenIds.clear();
	});
	expectSameFrame(twins);
	expectSameFrame(twins);
}

TEST_F(ParallelPreparationTest, smallScenesStayOnTheCallingThread) {
	scene::Scene sc;
	EXPECT_GT(sc.getParallelMinEntities(), 1000u);
	const auto a = sc.createEntity("a");
	sc.prepareWorldTransforms();
	EXPECT_EQ(sc.getWorldIndex(a), 0u);
}
