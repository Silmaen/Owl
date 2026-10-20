/**
 * @file HudLayout_test.cpp
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <app/Application.h>
#include <core/Log.h>
#include <renderer/CameraOrtho.h>
#include <renderer/RenderStack.h>
#include <renderer/Renderer.h>
#include <renderer/Renderer2D.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/UiInputSystem.h>
#include <scene/component/components.h>
#include <scene/ui/HudLayout.h>
#include <scene/ui/HudPass.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

using namespace owl;
using scene::component::Canvas;
using scene::component::UiRect;

namespace {

class HudLayoutTest : public ::testing::Test {
protected:
	void SetUp() override { core::Log::init(core::Log::Level::Off); }
	void TearDown() override { core::Log::invalidate(); }
};

auto makeCanvas(scene::Scene& ioScene, const Canvas::ScaleMode iMode, const int32_t iSortOrder = 0) -> scene::Entity {
	auto canvas = ioScene.createEntity("Canvas");
	auto& component = canvas.addComponent<Canvas>();
	component.scaleMode = iMode;
	component.sortOrder = iSortOrder;
	return canvas;
}

auto makeWidget(scene::Scene& ioScene, const scene::Entity& iParent, const UiRect::Anchor iAnchor,
				const math::vec2& iPivot, const math::vec2& iSize, const math::vec2& iOffset) -> scene::Entity {
	auto widget = ioScene.createEntity("Widget");
	auto& rect = widget.addComponent<UiRect>();
	rect.setAnchor(iAnchor);
	rect.pivot = iPivot;
	rect.size = iSize;
	rect.anchorOffset = iOffset;
	ioScene.setParent(widget, iParent);
	return widget;
}

auto boundsOf(const scene::ui::HudLayout& iLayout, const scene::Entity& iEntity) -> UiRect::Bounds {
	const auto* node = iLayout.findNode(static_cast<entt::entity>(iEntity));
	return node == nullptr ? UiRect::Bounds{.position = {-1.f, -1.f}, .size = {-1.f, -1.f}} : node->bounds;
}

void expectBounds(const UiRect::Bounds& iBounds, const math::vec2& iPosition, const math::vec2& iSize) {
	EXPECT_NEAR(iBounds.position.x(), iPosition.x(), 1e-3f);
	EXPECT_NEAR(iBounds.position.y(), iPosition.y(), 1e-3f);
	EXPECT_NEAR(iBounds.size.x(), iSize.x(), 1e-3f);
	EXPECT_NEAR(iBounds.size.y(), iSize.y(), 1e-3f);
}

}// namespace

TEST_F(HudLayoutTest, nestedRectsLiveInTheirParent) {
	scene::Scene scn;
	const auto canvas = makeCanvas(scn, Canvas::ScaleMode::ConstantPixels);
	const auto panel = makeWidget(scn, canvas, UiRect::Anchor::TopLeft, {0.f, 0.f}, {300.f, 200.f}, {100.f, 50.f});
	const auto corner = makeWidget(scn, panel, UiRect::Anchor::BottomRight, {1.f, 1.f}, {50.f, 20.f}, {-10.f, -10.f});
	auto& layout = scn.getHudLayout();
	layout.compute(scn, {800u, 600u}, false);
	ASSERT_EQ(layout.getNodes().size(), 2u);
	expectBounds(boundsOf(layout, panel), {100.f, 50.f}, {300.f, 200.f});
	expectBounds(boundsOf(layout, corner), {340.f, 220.f}, {50.f, 20.f});
	EXPECT_EQ(layout.getNodes()[1].parent, size_t{0});
	EXPECT_EQ(layout.getNodes()[0].parent, size_t{scene::ui::HudNode::noParent});
}

TEST_F(HudLayoutTest, stretchedAnchorsFillTheParent) {
	scene::Scene scn;
	const auto canvas = makeCanvas(scn, Canvas::ScaleMode::ConstantPixels);
	const auto panel = makeWidget(scn, canvas, UiRect::Anchor::Center, {0.5f, 0.5f}, {400.f, 300.f}, {0.f, 0.f});
	auto inset = makeWidget(scn, panel, UiRect::Anchor::TopLeft, {0.5f, 0.5f}, {-20.f, -20.f}, {0.f, 0.f});
	auto& rect = inset.getComponent<UiRect>();
	rect.anchorMin = {0.f, 0.f};
	rect.anchorMax = {1.f, 1.f};
	auto& layout = scn.getHudLayout();
	layout.compute(scn, {800u, 600u}, false);
	expectBounds(boundsOf(layout, panel), {200.f, 150.f}, {400.f, 300.f});
	expectBounds(boundsOf(layout, inset), {210.f, 160.f}, {380.f, 280.f});
}

TEST_F(HudLayoutTest, scaleModesMapTheReferenceResolution) {
	scene::Scene scn;
	const auto canvas = makeCanvas(scn, Canvas::ScaleMode::ScaleWithHeight);
	const auto bar = makeWidget(scn, canvas, UiRect::Anchor::TopRight, {1.f, 0.f}, {200.f, 50.f}, {-20.f, 20.f});
	auto& layout = scn.getHudLayout();
	layout.compute(scn, {640u, 360u}, false);
	expectBounds(boundsOf(layout, bar), {530.f, 10.f}, {100.f, 25.f});
	EXPECT_NEAR(layout.getNodes()[0].scale, 0.5f, 1e-5f);

	canvas.getComponent<Canvas>().scaleMode = Canvas::ScaleMode::MatchSmallerSide;
	layout.compute(scn, {1280u, 360u}, false);
	expectBounds(boundsOf(layout, bar), {1170.f, 10.f}, {100.f, 25.f});

	canvas.getComponent<Canvas>().scaleMode = Canvas::ScaleMode::ConstantPixels;
	layout.compute(scn, {1280u, 360u}, false);
	expectBounds(boundsOf(layout, bar), {1060.f, 20.f}, {200.f, 50.f});
}

TEST_F(HudLayoutTest, entityWithoutRectPassesItsParentOn) {
	scene::Scene scn;
	const auto canvas = makeCanvas(scn, Canvas::ScaleMode::ConstantPixels);
	const auto panel = makeWidget(scn, canvas, UiRect::Anchor::TopLeft, {0.f, 0.f}, {100.f, 100.f}, {50.f, 50.f});
	auto group = scn.createEntity("Group");
	scn.setParent(group, panel);
	const auto child = makeWidget(scn, group, UiRect::Anchor::Center, {0.5f, 0.5f}, {10.f, 10.f}, {0.f, 0.f});
	auto& layout = scn.getHudLayout();
	layout.compute(scn, {800u, 600u}, false);
	ASSERT_EQ(layout.getNodes().size(), 2u);
	expectBounds(boundsOf(layout, child), {95.f, 95.f}, {10.f, 10.f});
	EXPECT_EQ(layout.findNode(static_cast<entt::entity>(group)), nullptr);
}

TEST_F(HudLayoutTest, hiddenEntitiesHideTheirSubtree) {
	scene::Scene scn;
	const auto canvas = makeCanvas(scn, Canvas::ScaleMode::ConstantPixels);
	auto panel = makeWidget(scn, canvas, UiRect::Anchor::TopLeft, {0.f, 0.f}, {100.f, 100.f}, {0.f, 0.f});
	makeWidget(scn, panel, UiRect::Anchor::Center, {0.5f, 0.5f}, {10.f, 10.f}, {0.f, 0.f});
	panel.getComponent<scene::component::Visibility>().gameVisible = false;
	auto& layout = scn.getHudLayout();
	layout.compute(scn, {800u, 600u}, false);
	EXPECT_TRUE(layout.getNodes().empty());
	layout.compute(scn, {800u, 600u}, true);
	EXPECT_EQ(layout.getNodes().size(), 2u);
	layout.compute(scn, {0u, 600u}, false);
	EXPECT_TRUE(layout.getNodes().empty());
}

TEST_F(HudLayoutTest, canvasSortOrderIsTheDrawOrder) {
	scene::Scene scn;
	const auto front = makeCanvas(scn, Canvas::ScaleMode::ConstantPixels, 5);
	const auto back = makeCanvas(scn, Canvas::ScaleMode::ConstantPixels, -1);
	const auto top = makeWidget(scn, front, UiRect::Anchor::Center, {0.5f, 0.5f}, {100.f, 100.f}, {0.f, 0.f});
	const auto bottom = makeWidget(scn, back, UiRect::Anchor::Center, {0.5f, 0.5f}, {100.f, 100.f}, {0.f, 0.f});
	auto& layout = scn.getHudLayout();
	layout.compute(scn, {800u, 600u}, false);
	ASSERT_EQ(layout.getNodes().size(), 2u);
	EXPECT_EQ(layout.getNodes()[0].entity, static_cast<entt::entity>(bottom));
	EXPECT_EQ(layout.getNodes()[1].entity, static_cast<entt::entity>(top));
	EXPECT_EQ(layout.getNodes()[1].canvas, static_cast<entt::entity>(front));
	const auto* hit = layout.hitTest({400.f, 300.f});
	ASSERT_NE(hit, nullptr);
	EXPECT_EQ(hit->entity, static_cast<entt::entity>(top));
	EXPECT_EQ(layout.hitTest({10.f, 10.f}), nullptr);
}

TEST_F(HudLayoutTest, invalidateForgetsTheViewport) {
	scene::Scene scn;
	auto& layout = scn.getHudLayout();
	EXPECT_FALSE(layout.isComputedFor({800u, 600u}));
	layout.compute(scn, {800u, 600u}, false);
	EXPECT_TRUE(layout.isComputedFor({800u, 600u}));
	EXPECT_FALSE(layout.isComputedFor({640u, 480u}));
	layout.invalidate();
	EXPECT_FALSE(layout.isComputedFor({800u, 600u}));
}

namespace {

class HudPassTest : public testing::Test {
protected:
	static void SetUpTestSuite() {
		core::Log::init(core::Log::Level::Off);
		s_app = mkShared<app::Application>(app::AppParams{.args = nullptr,
														  .frameLogFrequency = 0,
														  .name = "hudPass",
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
		config.entries.push_back({.typeKey = "Renderer2D", .name = "ui", .defaultConfig = {}});
		renderer::Renderer::setRenderStack(
				renderer::RenderStack::buildFromConfig(config, renderer::EnabledRenderersConfig{}));
	}

	void TearDown() override { renderer::Renderer::setRenderStack(renderer::RenderStack{}); }

	inline static shared<app::Application> s_app;
};

auto makeHudScene() -> shared<scene::Scene> {
	auto sc = mkShared<scene::Scene>();
	auto cam = sc->createEntity("Camera");
	cam.addComponent<scene::component::Camera>().primary = true;
	auto canvas = makeCanvas(*sc, Canvas::ScaleMode::ScaleWithHeight);
	// A `ui` routing tag does not matter: the HUD pass draws every canvas.
	canvas.addComponent<scene::component::RendererTag>().rendererName = "ui";
	auto panel = makeWidget(*sc, canvas, UiRect::Anchor::TopLeft, {0.f, 0.f}, {200.f, 100.f}, {10.f, 10.f});
	panel.addComponent<scene::component::UiPanel>();
	auto button = makeWidget(*sc, panel, UiRect::Anchor::Center, {0.5f, 0.5f}, {80.f, 30.f}, {0.f, 0.f});
	button.addComponent<scene::component::UiButton>();
	sc->onViewportResize({640, 360});
	return sc;
}

}// namespace

TEST_F(HudPassTest, runtimeFrameDrawsTheHudOnceAndInputReusesIt) {
	const auto sc = makeHudScene();
	sc->status = scene::Scene::Status::Playing;
	sc->onRenderRuntime();
	auto& layout = sc->getHudLayout();
	EXPECT_EQ(layout.getComputeCount(), 1u);
	EXPECT_TRUE(layout.isComputedFor({640u, 360u}));
	EXPECT_EQ(renderer::Renderer2D::getStats().quadCount, 2u);
	ASSERT_EQ(layout.getNodes().size(), 2u);
	// The button is drawn centred in the panel, at half scale (360 / 720).
	const auto& button = layout.getNodes()[1];
	expectBounds(button.bounds, {35.f, 22.5f}, {40.f, 15.f});
	scene::UiInputSystem::update(sc.get(), {640u, 360u}, button.bounds.getCenter(), false);
	EXPECT_EQ(layout.getComputeCount(), 1u);
	EXPECT_EQ(scene::Entity(button.entity, sc.get()).getComponent<scene::component::UiButton>().state,
			  scene::component::UiButton::State::Hovered);
}

TEST_F(HudPassTest, editorDrawsTheHudInPixels) {
	const auto sc = makeHudScene();
	const renderer::CameraOrtho camera{0.f, 10.f, 0.f, 10.f};
	const core::Timestep step;
	sc->onUpdateEditor(step, camera);
	EXPECT_EQ(sc->getHudLayout().getNodes().size(), 2u);
	EXPECT_EQ(renderer::Renderer2D::getStats().quadCount, 2u);
}

TEST_F(HudPassTest, emptyHudDrawsNothing) {
	scene::Scene scn;
	scene::ui::HudPass::render(scn, {640u, 360u}, false);
	EXPECT_TRUE(scn.getHudLayout().getNodes().empty());
	EXPECT_EQ(scn.getHudLayout().getComputeCount(), 1u);
}

TEST_F(HudPassTest, hudAssetIsDrawnAfterTheSceneCanvases) {
	const auto hud = mkShared<scene::Scene>();
	auto canvas = makeCanvas(*hud, Canvas::ScaleMode::ConstantPixels);
	auto panel = makeWidget(*hud, canvas, UiRect::Anchor::TopLeft, {0.f, 0.f}, {50.f, 20.f}, {5.f, 5.f});
	panel.addComponent<scene::component::UiPanel>();
	const auto path = std::filesystem::temp_directory_path() / "owl_hud_pass_test.owlhud";
	ASSERT_TRUE(scene::SceneSerializer(hud, scene::DocumentKind::Hud).serialize(path));

	const auto sc = makeHudScene();
	sc->setHudPath(path.string());
	sc->onRenderRuntime();
	// The scene's panel and button, then the HUD's panel.
	EXPECT_EQ(renderer::Renderer2D::getStats().quadCount, 3u);
	const auto shared = sc->getHud();
	ASSERT_TRUE(shared);
	EXPECT_EQ(shared->getHudLayout().getNodes().size(), 1u);
	sc->setHudPath(std::string{});
	sc->onRenderRuntime();
	EXPECT_EQ(renderer::Renderer2D::getStats().quadCount, 2u);
	std::filesystem::remove(path);
}
