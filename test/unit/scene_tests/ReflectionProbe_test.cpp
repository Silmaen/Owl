/**
 * @file ReflectionProbe_test.cpp
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <app/Application.h>
#include <core/Log.h>
#include <core/Timestep.h>
#include <renderer/CameraEditor.h>
#include <renderer/LocalShadows.h>
#include <renderer/ReflectionProbes.h>
#include <renderer/RenderStack.h>
#include <renderer/Renderer.h>
#include <scene/ComponentRegistry.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/component/components.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

using namespace owl;
using scene::component::ReflectionProbe;

namespace {

auto makeStep(const int iMs) -> core::Timestep {
	core::Timestep ts;
	ts.forceUpdate(std::chrono::milliseconds(iMs));
	return ts;
}

class ReflectionProbeTest : public testing::Test {
protected:
	static void SetUpTestSuite() {
		core::Log::init(core::Log::Level::Off);
		s_app = mkShared<app::Application>(app::AppParams{.args = nullptr,
														  .frameLogFrequency = 0,
														  .name = "reflectionProbe",
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
		config.entries.push_back({.typeKey = "RendererMesh", .name = "mesh_world", .defaultConfig = {}});
		renderer::Renderer::setRenderStack(
				renderer::RenderStack::buildFromConfig(config, renderer::EnabledRenderersConfig{}));
	}

	void TearDown() override { renderer::Renderer::setRenderStack(renderer::RenderStack{}); }

	// A scene with a crate and probes at the given x positions, drawn by an editor camera at the origin.
	static auto makeScene(const std::vector<float>& iPositions) -> shared<scene::Scene> {
		auto sc = mkShared<scene::Scene>();
		sc->createEntity("Crate").addComponent<scene::component::MeshRenderer>().setMeshPath(
				test::getTestAssetsPath("render_tests") / "meshes" / "textured_cube.glb");
		for (const float x: iPositions) {
			auto entity = sc->createEntity("Probe");
			entity.getComponent<scene::component::Transform>().transform.translation() = {x, 0.f, 0.f};
			entity.addComponent<ReflectionProbe>().resolution = 32;
		}
		sc->onViewportResize({320, 180});
		return sc;
	}

	static void render(const shared<scene::Scene>& iScene) {
		renderer::CameraEditor camera{45.f, 1.778f, 0.1f, 1000.f};
		camera.setViewportSize({320, 180});
		iScene->onUpdateEditor(makeStep(16), camera);
	}

	static auto passCount(const std::string& iName) -> size_t {
		const auto passes = renderer::Renderer::getRenderGraph().getCompiledPasses();
		return static_cast<size_t>(std::ranges::count(passes, iName));
	}

	inline static shared<app::Application> s_app;
};

}// namespace

TEST_F(ReflectionProbeTest, serializationRoundTripsEveryField) {
	const auto sc = mkShared<scene::Scene>();
	auto entity = sc->createEntity("Probe");
	auto& probe = entity.addComponent<ReflectionProbe>();
	probe.shape = ReflectionProbe::Shape::Sphere;
	probe.extents = {1.f, 2.f, 3.f};
	probe.radius = 4.5f;
	probe.blendDistance = 0.25f;
	probe.parallax = false;
	probe.intensity = 2.f;
	probe.resolution = 64;
	probe.realtime = true;
	const auto yaml = scene::SceneSerializer::serializeEntityToString(entity);
	EXPECT_NE(yaml.find("ReflectionProbe"), std::string::npos);

	const auto copy = mkShared<scene::Scene>();
	ASSERT_TRUE(scene::SceneSerializer::deserializeEntityFromString(copy, yaml));
	const auto restored = copy->findEntityByUUID(entity.getUUID());
	ASSERT_TRUE(restored);
	ASSERT_TRUE(restored.hasComponent<ReflectionProbe>());
	const auto& read = restored.getComponent<ReflectionProbe>();
	EXPECT_EQ(read.shape, ReflectionProbe::Shape::Sphere);
	EXPECT_EQ(read.extents, (math::vec3{1.f, 2.f, 3.f}));
	EXPECT_FLOAT_EQ(read.radius, 4.5f);
	EXPECT_FLOAT_EQ(read.blendDistance, 0.25f);
	EXPECT_FALSE(read.parallax);
	EXPECT_FLOAT_EQ(read.intensity, 2.f);
	EXPECT_EQ(read.resolution, 64u);
	EXPECT_TRUE(read.realtime);
}

TEST_F(ReflectionProbeTest, deserializationClampsTheSizes) {
	const auto sc = mkShared<scene::Scene>();
	auto entity = sc->createEntity("Probe");
	entity.addComponent<ReflectionProbe>();
	auto yaml = scene::SceneSerializer::serializeEntityToString(entity);
	const auto replace = [&yaml](const std::string& iFrom, const std::string& iTo) -> void {
		const auto at = yaml.find(iFrom);
		ASSERT_NE(at, std::string::npos) << iFrom;
		yaml.replace(at, iFrom.size(), iTo);
	};
	replace("resolution: 128", "resolution: 100");
	replace("radius: 3", "radius: -1");
	replace("blendDistance: 0.5", "blendDistance: -2");
	const auto copy = mkShared<scene::Scene>();
	ASSERT_TRUE(scene::SceneSerializer::deserializeEntityFromString(copy, yaml));
	const auto& read = copy->findEntityByUUID(entity.getUUID()).getComponent<ReflectionProbe>();
	EXPECT_EQ(read.resolution, 64u);
	EXPECT_GT(read.radius, 0.f);
	EXPECT_FLOAT_EQ(read.blendDistance, 0.f);
}

TEST_F(ReflectionProbeTest, isAnOptionalComponentOfTheRegistry) {
	const auto* desc = scene::ComponentRegistry::find("ReflectionProbe");
	ASSERT_NE(desc, nullptr);
	EXPECT_TRUE(desc->optional);
}

TEST(ReflectionProbeLevels, stopAtFacesOfEightTexels) {
	EXPECT_EQ(renderer::getProbeLevelCount(8), 1u);
	EXPECT_EQ(renderer::getProbeLevelCount(16), 2u);
	EXPECT_EQ(renderer::getProbeLevelCount(128), 5u);
	EXPECT_EQ(renderer::getProbeLevelCount(512), renderer::g_maxProbeLevels);
}

TEST(ReflectionProbeLevels, roughnessGrowsToOneOnTheLastLevel) {
	EXPECT_FLOAT_EQ(renderer::getProbeFilterAlpha(0, 5), 0.f);
	EXPECT_FLOAT_EQ(renderer::getProbeFilterAlpha(2, 5), 0.25f);
	EXPECT_FLOAT_EQ(renderer::getProbeFilterAlpha(4, 5), 1.f);
	EXPECT_FLOAT_EQ(renderer::getProbeFilterAlpha(3, 1), 0.f);
}

TEST_F(ReflectionProbeTest, aProbeIsCapturedOnceThenOnRequest) {
	const auto sc = makeScene({0.f});
	render(sc);
	EXPECT_EQ(sc->getReflectionProbeCaptureCount(), 1u);
	ASSERT_EQ(sc->getReflectionProbes().size(), 1u);
	EXPECT_NE(sc->getReflectionProbes().front().cube, nullptr);
	EXPECT_EQ(passCount("probe capture"), 1u);
	EXPECT_EQ(passCount("probe chain"), renderer::getProbeLevelCount(32) - 1);
	EXPECT_EQ(passCount("probe filter"), renderer::getProbeLevelCount(32) - 1);

	render(sc);
	EXPECT_EQ(sc->getReflectionProbeCaptureCount(), 1u);
	EXPECT_EQ(passCount("probe capture"), 0u);
	EXPECT_EQ(sc->getReflectionProbes().size(), 1u);

	sc->requestReflectionProbeCapture();
	render(sc);
	EXPECT_EQ(sc->getReflectionProbeCaptureCount(), 2u);

	for (const auto handle: sc->registry.view<ReflectionProbe>())
		++sc->registry.get<ReflectionProbe>(handle).captureRequest;
	render(sc);
	EXPECT_EQ(sc->getReflectionProbeCaptureCount(), 3u);
}

TEST_F(ReflectionProbeTest, movingAProbeCapturesItAgain) {
	const auto sc = makeScene({0.f});
	render(sc);
	for (const auto handle: sc->registry.view<scene::component::Transform, ReflectionProbe>())
		sc->registry.get<scene::component::Transform>(handle).transform.translation() = {1.f, 0.f, 0.f};
	render(sc);
	EXPECT_EQ(sc->getReflectionProbeCaptureCount(), 2u);
	EXPECT_EQ(sc->getReflectionProbes().front().position, (math::vec3{1.f, 0.f, 0.f}));
}

TEST_F(ReflectionProbeTest, realtimeProbesAreCapturedInTurn) {
	const auto sc = makeScene({0.f, 5.f});
	for (const auto handle: sc->registry.view<ReflectionProbe>())
		sc->registry.get<ReflectionProbe>(handle).realtime = true;
	render(sc);
	EXPECT_EQ(sc->getReflectionProbeCaptureCount(), 2u);
	render(sc);
	EXPECT_EQ(sc->getReflectionProbeCaptureCount(), 3u);
	EXPECT_EQ(passCount("probe capture"), 1u);
	render(sc);
	EXPECT_EQ(sc->getReflectionProbeCaptureCount(), 4u);
}

TEST_F(ReflectionProbeTest, theNearestProbesFitTheBudget) {
	std::vector<float> positions;
	for (uint32_t index = 0; index < 20; ++index) positions.push_back(static_cast<float>(index) * 50.f);
	const auto sc = makeScene(positions);
	render(sc);
	const auto probes = sc->getReflectionProbes();
	ASSERT_EQ(probes.size(), size_t{renderer::getLocalShadowBudget().point});
	EXPECT_EQ(probes.front().position, (math::vec3{0.f, 0.f, 0.f}));
}

TEST_F(ReflectionProbeTest, nothingIsCapturedWithoutGeometry) {
	auto sc = mkShared<scene::Scene>();
	sc->createEntity("Probe").addComponent<ReflectionProbe>();
	sc->onViewportResize({320, 180});
	render(sc);
	EXPECT_EQ(sc->getReflectionProbeCaptureCount(), 0u);
	EXPECT_TRUE(sc->getReflectionProbes().empty());
}

TEST_F(ReflectionProbeTest, aSphereProbeGivesItsRadius) {
	const auto sc = makeScene({0.f});
	for (const auto handle: sc->registry.view<ReflectionProbe>()) {
		auto& probe = sc->registry.get<ReflectionProbe>(handle);
		probe.shape = ReflectionProbe::Shape::Sphere;
		probe.radius = 2.5f;
	}
	render(sc);
	ASSERT_EQ(sc->getReflectionProbes().size(), 1u);
	EXPECT_FLOAT_EQ(sc->getReflectionProbes().front().sphereRadius, 2.5f);
}
