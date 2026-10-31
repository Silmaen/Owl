/**
 * @file MeshRenderer_test.cpp
 * @author Silmaen
 * @date 21/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <app/Application.h>
#include <app/EngineContext.h>
#include <core/Log.h>
#include <core/Timestep.h>
#include <data/geometry/MeshLoader.h>
#include <renderer/CameraEditor.h>
#include <renderer/MeshCache.h>
#include <renderer/RenderStack.h>
#include <renderer/Renderer.h>
#include <renderer/RendererMesh.h>
#include <scene/ComponentRegistry.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/component/components.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

using namespace owl;

namespace {

auto cubePath() -> std::filesystem::path {
	return test::getTestAssetsPath("render_tests") / "meshes" / "textured_cube.glb";
}

// One draw per submesh of the cube file: the counts of a scene scale with it.
auto cubeParts() -> uint32_t {
	return static_cast<uint32_t>(data::geometry::MeshLoader::loadMesh(cubePath()).submeshes.size());
}

auto meshCache() -> renderer::MeshCache& { return app::Application::get().getEngineContext().getMeshCache(); }

auto makeStep(const int iMs) -> core::Timestep {
	core::Timestep ts;
	ts.forceUpdate(std::chrono::milliseconds(iMs));
	return ts;
}

class MeshRendererTest : public testing::Test {
protected:
	static void SetUpTestSuite() {
		core::Log::init(core::Log::Level::Off);
		s_app = mkShared<app::Application>(app::AppParams{.args = nullptr,
														  .frameLogFrequency = 0,
														  .name = "meshRenderer",
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

	void SetUp() override { meshCache().clear(); }

	void TearDown() override {
		meshCache().clear();
		renderer::Renderer::setRenderStack(renderer::RenderStack{});
	}

	// A 2D world layer first, then the mesh layer, as the Engine Demo stack.
	static void useStack(const bool iMeshFirst, const std::string& iMeshConfig = {}) {
		renderer::RendererStackConfig config;
		if (!iMeshFirst)
			config.entries.push_back({.typeKey = "Renderer2D", .name = "world", .defaultConfig = {}});
		config.entries.push_back({.typeKey = "RendererMesh", .name = "mesh_world", .defaultConfig = iMeshConfig});
		renderer::Renderer::setRenderStack(
				renderer::RenderStack::buildFromConfig(config, renderer::EnabledRenderersConfig{}));
	}

	// A perspective camera looking at three cubes: one opaque, two blended.
	static auto makeScene(const bool iTagged) -> shared<scene::Scene> {
		auto sc = mkShared<scene::Scene>();
		auto cam = sc->createEntity("Camera");
		cam.getComponent<scene::component::Transform>().transform.translation() = math::vec3{0.f, 0.f, 10.f};
		auto& camera = cam.addComponent<scene::component::Camera>();
		camera.primary = true;
		camera.camera.setPerspective(0.785f, 0.1f, 1000.f);
		for (const auto& [name, transparent]:
			 {std::pair{"Opaque", false}, std::pair{"GlassA", true}, std::pair{"GlassB", true}}) {
			auto crate = sc->createEntity(name);
			if (iTagged)
				crate.addComponent<scene::component::RendererTag>().rendererName = "mesh_world";
			auto& mesh = crate.addComponent<scene::component::MeshRenderer>();
			mesh.setMeshPath(cubePath());
			mesh.transparent = transparent;
		}
		sc->onViewportResize({320, 180});
		return sc;
	}

	inline static shared<app::Application> s_app;
};

}// namespace

TEST_F(MeshRendererTest, serializationRoundTripsEveryField) {
	const auto sc = mkShared<scene::Scene>();
	auto crate = sc->createEntity("Crate");
	auto& mesh = crate.addComponent<scene::component::MeshRenderer>();
	mesh.setMeshPath("meshes/crate.glb");
	mesh.color = {0.25f, 0.5f, 0.75f, 0.5f};
	mesh.transparent = true;
	const auto yaml = scene::SceneSerializer::serializeEntityToString(crate);
	EXPECT_NE(yaml.find("MeshRenderer"), std::string::npos);
	EXPECT_NE(yaml.find("meshes/crate.glb"), std::string::npos);

	const auto copy = mkShared<scene::Scene>();
	ASSERT_TRUE(scene::SceneSerializer::deserializeEntityFromString(copy, yaml));
	const auto restored = copy->findEntityByUUID(crate.getUUID());
	ASSERT_TRUE(restored);
	ASSERT_TRUE(restored.hasComponent<scene::component::MeshRenderer>());
	const auto& read = restored.getComponent<scene::component::MeshRenderer>();
	EXPECT_EQ(read.meshPath.generic_string(), "meshes/crate.glb");
	EXPECT_EQ(read.color, (math::vec4{0.25f, 0.5f, 0.75f, 0.5f}));
	EXPECT_TRUE(read.transparent);
	EXPECT_EQ(read.texture, nullptr);
	EXPECT_EQ(read.mesh, nullptr);
}

TEST_F(MeshRendererTest, isAnOptionalComponentOfTheRegistry) {
	const auto* desc = scene::ComponentRegistry::find("MeshRenderer");
	ASSERT_NE(desc, nullptr);
	EXPECT_TRUE(desc->optional);
	EXPECT_EQ(scene::ComponentRegistry::find("Mesh Renderer"), desc);
}

TEST_F(MeshRendererTest, framesDrawOpaqueThenBlendedMeshesSharingOneResource) {
	useStack(false);
	const auto sc = makeScene(true);
	sc->onStartRuntime();
	sc->onUpdateRuntime(makeStep(16));
	EXPECT_EQ(meshCache().size(), 1u);
	const auto resource = meshCache().find(cubePath());
	ASSERT_NE(resource, nullptr);
	for (const auto entity: sc->registry.view<scene::component::MeshRenderer>())
		EXPECT_EQ(sc->registry.get<scene::component::MeshRenderer>(entity).mesh, resource);
	const auto stats = renderer::RendererMesh::getStatistics();
	EXPECT_EQ(stats.opaqueDrawCount, cubeParts());
	EXPECT_EQ(stats.transparentDrawCount, 2 * cubeParts());
	sc->onEndRuntime();
}

TEST_F(MeshRendererTest, untaggedMeshesBelongToAFirstMeshLayer) {
	useStack(true);
	const auto sc = makeScene(false);
	sc->onRenderRuntime();
	EXPECT_EQ(renderer::RendererMesh::getStatistics().opaqueDrawCount, cubeParts());
	EXPECT_EQ(renderer::RendererMesh::getStatistics().transparentDrawCount, 2 * cubeParts());
}

TEST_F(MeshRendererTest, editorFramesDrawTheMeshLayerToo) {
	useStack(false);
	const auto sc = makeScene(true);
	renderer::CameraEditor camera{45.f, 1.778f, 0.1f, 1000.f};
	camera.setViewportSize({320, 180});
	sc->onUpdateEditor(makeStep(16), camera);
	EXPECT_EQ(renderer::RendererMesh::getStatistics().opaqueDrawCount, cubeParts());
	EXPECT_EQ(renderer::RendererMesh::getStatistics().transparentDrawCount, 2 * cubeParts());
}

TEST_F(MeshRendererTest, aSecondEditorViewDrawsTheSameMeshes) {
	useStack(false);
	const auto sc = makeScene(true);
	renderer::CameraEditor camera{45.f, 1.778f, 0.1f, 1000.f};
	camera.setViewportSize({320, 180});
	sc->renderEditorView(camera);
	EXPECT_EQ(renderer::RendererMesh::getStatistics().opaqueDrawCount, cubeParts());
	EXPECT_EQ(renderer::RendererMesh::getStatistics().transparentDrawCount, 2 * cubeParts());
}

TEST_F(MeshRendererTest, anEditorViewRunsTheWorldThroughTheFrameGraph) {
	useStack(false);
	const auto sc = makeScene(true);
	renderer::CameraEditor camera{45.f, 1.778f, 0.1f, 1000.f};
	camera.setViewportSize({320, 180});
	sc->onViewportResize({320, 180});
	sc->renderEditorView(camera);
	const auto passes = renderer::Renderer::getRenderGraph().getCompiledPasses();
	ASSERT_GE(passes.size(), 4u);
	EXPECT_EQ(passes.front(), "scene clear");
	EXPECT_EQ(passes[passes.size() - 3], "colour encode");
	EXPECT_EQ(passes[passes.size() - 2], "entity ids and depth");
	EXPECT_EQ(passes.back(), "overlays and HUD");
	EXPECT_GE(renderer::Renderer::getRenderGraph().getPooledTargetCount(), 1u);
}

TEST_F(MeshRendererTest, aColourVisionFilterIsTheLastPassOfTheFrame) {
	useStack(false);
	const auto sc = makeScene(true);
	renderer::CameraEditor camera{45.f, 1.778f, 0.1f, 1000.f};
	camera.setViewportSize({320, 180});
	sc->onViewportResize({320, 180});
	sc->setColorVisionPreview(
			{.deficiency = renderer::ColorDeficiency::Tritanopia, .mode = renderer::ColorVisionMode::Simulate});
	sc->renderEditorView(camera);
	const auto passes = renderer::Renderer::getRenderGraph().getCompiledPasses();
	ASSERT_GE(passes.size(), 5u);
	EXPECT_EQ(passes[passes.size() - 2], "overlays and HUD");
	EXPECT_EQ(passes.back(), "colour vision");
	sc->setColorVisionPreview({});
	sc->renderEditorView(camera);
	EXPECT_EQ(renderer::Renderer::getRenderGraph().getCompiledPasses().back(), "overlays and HUD");
}

TEST_F(MeshRendererTest, occlusionCullingDrawsTheOccludersBeforeTheMeshLayer) {
	const auto passesOf = [](const std::string& iConfig) -> std::vector<std::string> {
		useStack(false, iConfig);
		const auto sc = makeScene(true);
		sc->onRenderRuntime();
		return renderer::Renderer::getRenderGraph().getCompiledPasses();
	};
	const auto plain = passesOf({});
	EXPECT_EQ(std::ranges::find(plain, "occluder depth"), plain.end());
	const auto passes = passesOf("OcclusionCulling: true\nOcclusionScale: 0.5");
	const auto depth = std::ranges::find(passes, "occluder depth");
	const auto pyramid = std::ranges::find(passes, "occlusion pyramid");
	const auto meshes = std::ranges::find(passes, "mesh_world");
	ASSERT_NE(depth, passes.end());
	ASSERT_NE(pyramid, passes.end());
	ASSERT_NE(meshes, passes.end());
	EXPECT_LT(depth, pyramid);
	EXPECT_LT(pyramid, meshes);
	// The Null backend has no compute: the meshes are still drawn, culled by frustum only.
	EXPECT_EQ(renderer::RendererMesh::getStatistics().opaqueDrawCount, cubeParts());
	EXPECT_EQ(renderer::RendererMesh::getStatistics().occlusionLevelCount, 0u);
}

TEST_F(MeshRendererTest, aHotReloadedFileIsFetchedAgain) {
	useStack(false);
	const auto sc = makeScene(true);
	sc->onRenderRuntime();
	const auto before = meshCache().find(cubePath());
	ASSERT_NE(before, nullptr);
	const auto report = app::Application::get().getHotReload().reloadFile(cubePath());
	EXPECT_EQ(report.reloaded, 1u);
	EXPECT_TRUE(before->isStale());
	sc->onRenderRuntime();
	const auto after = meshCache().find(cubePath());
	ASSERT_NE(after, nullptr);
	EXPECT_NE(after, before);
	for (const auto entity: sc->registry.view<scene::component::MeshRenderer>())
		EXPECT_EQ(sc->registry.get<scene::component::MeshRenderer>(entity).mesh, after);
}

TEST_F(MeshRendererTest, aMissingFileDrawsNothing) {
	useStack(false);
	const auto sc = makeScene(true);
	for (const auto entity: sc->registry.view<scene::component::MeshRenderer>())
		sc->registry.get<scene::component::MeshRenderer>(entity).setMeshPath("meshes/missing.glb");
	sc->onRenderRuntime();
	for (const auto entity: sc->registry.view<scene::component::MeshRenderer>())
		EXPECT_EQ(sc->registry.get<scene::component::MeshRenderer>(entity).mesh, nullptr);
	EXPECT_EQ(renderer::RendererMesh::getStatistics().opaqueDrawCount, 0u);
}

TEST_F(MeshRendererTest, theMotionBlurFollowsTheCameraInPlayOnly) {
	useStack(true);
	const auto sc = makeScene(false);
	for (const auto handle: sc->registry.view<scene::component::Camera>()) {
		auto& camera = sc->registry.get<scene::component::Camera>(handle).camera;
		auto post = camera.getPostProcess();
		post.motionBlur = true;
		post.depthOfField = true;
		camera.setPostProcess(post);
	}
	const auto passCount = [](const std::string& iName) -> size_t {
		const auto passes = renderer::Renderer::getRenderGraph().getCompiledPasses();
		return static_cast<size_t>(std::ranges::count(passes, iName));
	};
	sc->onRenderRuntime();
	EXPECT_EQ(passCount("depth of field"), 1u);
	EXPECT_EQ(passCount("motion blur"), 0u);
	sc->onStartRuntime();
	sc->onRenderRuntime();
	EXPECT_EQ(passCount("motion blur"), 0u);
	sc->onRenderRuntime();
	EXPECT_EQ(passCount("motion blur"), 1u);
	EXPECT_EQ(passCount("post depth"), 1u);
	sc->onEndRuntime();
}
