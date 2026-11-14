/**
 * @file EnvironmentShare_test.cpp
 * @author Silmaen
 * @date 13/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <app/Application.h>
#include <core/Log.h>
#include <core/Timestep.h>
#include <renderer/CameraEditor.h>
#include <renderer/RenderStack.h>
#include <renderer/Renderer.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/component/components.h>

#include <chrono>
#include <filesystem>
#include <string>

using namespace owl;

namespace {

auto makeStep(const int iMs) -> core::Timestep {
	core::Timestep ts;
	ts.forceUpdate(std::chrono::milliseconds(iMs));
	return ts;
}

class EnvironmentShareTest : public testing::Test {
protected:
	static void SetUpTestSuite() {
		core::Log::init(core::Log::Level::Off);
		s_app = mkShared<app::Application>(app::AppParams{.args = nullptr,
														  .frameLogFrequency = 0,
														  .name = "environmentShare",
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
		// A file of its own: the environments the process keeps are keyed by the file.
		m_hdri = std::filesystem::temp_directory_path() /
				 (std::string{testing::UnitTest::GetInstance()->current_test_info()->name()} + ".hdr");
		std::filesystem::copy_file(test::getTestAssetsPath("render_tests") / "hdri" / "sky_gradient.hdr", m_hdri,
								   std::filesystem::copy_options::overwrite_existing);
	}

	void TearDown() override {
		renderer::Renderer::setRenderStack(renderer::RenderStack{});
		app::Application::get().getTaskScheduler().waitEmptyQueue();
		std::error_code error;
		std::filesystem::remove(m_hdri, error);
	}

	// A scene lit by the HDRI, or by a procedural sky.
	[[nodiscard]] auto makeScene(const bool iProcedural = false) const -> shared<scene::Scene> {
		auto sc = mkShared<scene::Scene>();
		auto sky = sc->createEntity("Sky");
		auto& environment = sky.addComponent<scene::component::Environment>();
		if (iProcedural)
			sky.addComponent<scene::component::ProceduralSky>();
		else
			environment.hdri = m_hdri;
		sc->onViewportResize({320, 180});
		return sc;
	}

	static void render(const shared<scene::Scene>& iScene) {
		renderer::CameraEditor camera{45.f, 1.778f, 0.1f, 1000.f};
		camera.setViewportSize({320, 180});
		iScene->onUpdateEditor(makeStep(16), camera);
	}

	static auto hasPendingTasks() -> bool { return app::Application::get().getTaskScheduler().hasPendingTasks(); }

	std::filesystem::path m_hdri;
	inline static shared<app::Application> s_app;
};

}// namespace

TEST_F(EnvironmentShareTest, aSecondSceneReusesTheBuiltEnvironment) {
	const auto first = makeScene();
	render(first);
	EXPECT_TRUE(hasPendingTasks());
	app::Application::get().getTaskScheduler().waitEmptyQueue();
	render(first);
	const auto second = makeScene();
	render(second);
	EXPECT_FALSE(hasPendingTasks());
}

TEST_F(EnvironmentShareTest, aSceneJoinsTheBuildInFlight) {
	const auto first = makeScene();
	const auto second = makeScene();
	render(first);
	render(second);
	app::Application::get().getTaskScheduler().waitEmptyQueue();
	render(first);
	render(second);
	const auto third = makeScene();
	render(third);
	EXPECT_FALSE(hasPendingTasks());
}

TEST_F(EnvironmentShareTest, anEditedFileIsBuiltAgain) {
	const auto first = makeScene();
	render(first);
	app::Application::get().getTaskScheduler().waitEmptyQueue();
	render(first);
	std::filesystem::last_write_time(m_hdri, std::filesystem::last_write_time(m_hdri) + std::chrono::seconds{5});
	const auto second = makeScene();
	render(second);
	EXPECT_TRUE(hasPendingTasks());
}

TEST_F(EnvironmentShareTest, aLeftSceneStopsItsBuild) {
	{
		const auto left = makeScene();
		render(left);
		EXPECT_TRUE(hasPendingTasks());
	}
	app::Application::get().getTaskScheduler().waitEmptyQueue();
	// Nobody kept the cancelled build: the next scene builds it again.
	const auto next = makeScene();
	render(next);
	EXPECT_TRUE(hasPendingTasks());
}

TEST_F(EnvironmentShareTest, proceduralSkiesAreShared) {
	const auto first = makeScene(true);
	render(first);
	app::Application::get().getTaskScheduler().waitEmptyQueue();
	render(first);
	const auto second = makeScene(true);
	render(second);
	EXPECT_FALSE(hasPendingTasks());
}

TEST_F(EnvironmentShareTest, aMissingFileIsNeverKept) {
	const auto sc = mkShared<scene::Scene>();
	sc->createEntity("Sky").addComponent<scene::component::Environment>().hdri = m_hdri.parent_path() / "missing.hdr";
	sc->onViewportResize({320, 180});
	render(sc);
	app::Application::get().getTaskScheduler().waitEmptyQueue();
	render(sc);
	const auto again = mkShared<scene::Scene>();
	again->createEntity("Sky").addComponent<scene::component::Environment>().hdri =
			m_hdri.parent_path() / "missing.hdr";
	again->onViewportResize({320, 180});
	render(again);
	EXPECT_TRUE(hasPendingTasks());
}
