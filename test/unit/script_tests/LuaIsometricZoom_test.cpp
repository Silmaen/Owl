/**
 * @file LuaIsometricZoom_test.cpp
 * @author Silmaen
 * @date 24/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <app/Application.h>
#include <core/Log.h>
#include <renderer/RenderStack.h>
#include <renderer/Renderer.h>
#include <renderer/RendererIsometric.h>
#include <renderer/RendererIsometricLayer.h>
#include <renderer/gpu/RenderAPI.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/ScenePicking.h>
#include <script/ScriptInstance.h>

#include <cstdint>
#include <format>
#include <string>
#include <vector>

using namespace owl;
using namespace owl::script;

namespace {

/// A render stack whose isometric layer starts at the zoom level 1, and a script running `iBody` on creation.
class LuaIsometricZoom : public testing::Test {
protected:
	static void SetUpTestSuite() {
		core::Log::init(core::Log::Level::Off);
		s_app = mkShared<app::Application>(app::AppParams{.args = nullptr,
														  .frameLogFrequency = 0,
														  .name = "luaIsometricZoom",
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
		m_scene = mkShared<scene::Scene>();
		m_entity = m_scene->createEntity("Game");
		installStack(true);
	}

	void TearDown() override { renderer::Renderer::setRenderStack(renderer::RenderStack{}); }

	static void installStack(const bool iIsometric) {
		renderer::RendererStackConfig config;
		config.entries.push_back({.typeKey = "Renderer2D", .name = "world", .defaultConfig = {}});
		if (iIsometric)
			config.entries.push_back({.typeKey = "RendererIsometric", .name = "iso", .defaultConfig = "Zoom: 1"});
		renderer::Renderer::setRenderStack(
				renderer::RenderStack::buildFromConfig(config, renderer::EnabledRenderersConfig{}));
	}

	/// Run a script on creation and return the instance holding its globals.
	auto run(const std::string& iBody) -> shared<ScriptInstance> {
		const std::string text = std::format("function on_create()\n{}\nend\n", iBody);
		const std::vector<uint8_t> data(text.begin(), text.end());
		auto instance = mkShared<ScriptInstance>();
		instance->setScene(m_scene.get());
		EXPECT_TRUE(instance->createFromBuffer(data, "zoom_test", static_cast<uint64_t>(m_entity.getUUID())));
		instance->onCreate();
		return instance;
	}

	inline static shared<app::Application> s_app;
	shared<scene::Scene> m_scene;
	scene::Entity m_entity;
};

}// namespace

TEST_F(LuaIsometricZoom, theZoomOfTheSceneIsRead) {
	EXPECT_EQ(run("zoom = isometric.get_zoom()")->getPropertyInt("zoom").value_or(-9), 1);
}

TEST_F(LuaIsometricZoom, setZoomChangesTheLayerAndThePicking) {
	const auto instance = run("applied = isometric.set_zoom(-2)\nzoom = isometric.get_zoom()");
	EXPECT_EQ(instance->getPropertyInt("applied").value_or(-9), -2);
	EXPECT_EQ(instance->getPropertyInt("zoom").value_or(-9), -2);
	EXPECT_EQ(scene::makeRuntimeIsometricCamera(*m_scene).getZoomLevel(), -2);
}

TEST_F(LuaIsometricZoom, setZoomIsClampedToTheRangeOfTheLayer) {
	const auto instance = run("high = isometric.set_zoom(99)\nlow = isometric.set_zoom(-99)");
	EXPECT_EQ(instance->getPropertyInt("high").value_or(-9), renderer::IsometricCamera::maxZoomLevel);
	EXPECT_EQ(instance->getPropertyInt("low").value_or(-9), renderer::IsometricCamera::minZoomLevel);
}

TEST_F(LuaIsometricZoom, aGameWithoutIsometricLayerGetsNothing) {
	installStack(false);
	const auto instance = run("none = isometric.set_zoom(2) == nil and isometric.get_zoom() == nil");
	EXPECT_TRUE(instance->getPropertyBool("none").value_or(false));
}

TEST_F(LuaIsometricZoom, theLayerClampsItsZoomLevel) {
	renderer::RendererIsometricLayer layer{"iso"};
	layer.setZoomLevel(3);
	EXPECT_EQ(layer.getZoomLevel(), 3);
	layer.setZoomLevel(40);
	EXPECT_EQ(layer.getZoomLevel(), renderer::IsometricCamera::maxZoomLevel);
}
