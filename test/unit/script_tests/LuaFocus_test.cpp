/**
 * @file LuaFocus_test.cpp
 * @author Silmaen
 * @date 16/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <app/EngineContext.h>
#include <core/Log.h>
#include <input/ActionFrame.h>
#include <input/ActionMap.h>
#include <input/InputFrame.h>
#include <input/KeyCodes.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/UiInputSystem.h>
#include <scene/component/components.h>
#include <script/ScriptInstance.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <format>
#include <string>
#include <system_error>
#include <vector>

using namespace owl;
using namespace owl::script;

namespace {

constexpr math::vec2ui g_viewport{800u, 600u};

auto makeButton(scene::Scene& ioScene, const scene::Entity& iParent, const std::string& iName, const float iY)
		-> scene::Entity {
	auto button = ioScene.createEntity(iName);
	auto& rect = button.addComponent<scene::component::UiRect>();
	rect.setAnchor(scene::component::UiRect::Anchor::TopLeft);
	rect.pivot = {0.f, 0.f};
	rect.size = {100.f, 30.f};
	rect.anchorOffset = {100.f, iY};
	button.addComponent<scene::component::UiButton>();
	ioScene.setParent(button, iParent);
	return button;
}

// A scene with a canvas running `iScript`, and three buttons under it.
struct FocusFixture {
	explicit FocusFixture(const std::string& iScript) {
		core::Log::init(core::Log::Level::Off);
		world->status = scene::Scene::Status::Playing;
		canvas = world->createEntity("Canvas");
		canvas.addComponent<scene::component::Canvas>().scaleMode = scene::component::Canvas::ScaleMode::ConstantPixels;
		first = makeButton(*world, canvas, "First", 100.f);
		second = makeButton(*world, canvas, "Second", 160.f);
		third = makeButton(*world, canvas, "Third", 220.f);
		auto& luaScript = canvas.addComponent<scene::component::LuaScript>();
		luaScript.instance = mkUniq<ScriptInstance>();
		luaScript.instance->setScene(world.get());
		const std::vector<uint8_t> data(iScript.begin(), iScript.end());
		EXPECT_TRUE(luaScript.instance->createFromBuffer(data, "focus_test", static_cast<uint64_t>(canvas.getUUID())));
	}

	~FocusFixture() { core::Log::invalidate(); }

	FocusFixture(const FocusFixture&) = delete;
	FocusFixture(FocusFixture&&) = delete;
	auto operator=(const FocusFixture&) -> FocusFixture& = delete;
	auto operator=(FocusFixture&&) -> FocusFixture& = delete;

	[[nodiscard]] auto script() const -> const ScriptInstance& {
		return *canvas.getComponent<scene::component::LuaScript>().instance;
	}

	void press(const input::KeyCode iKey) const {
		input::InputFrame frame;
		frame.current.setKey(iKey, true);
		input::ActionFrame actions;
		actions.update(input::ActionMap::getDefault(), frame);
		scene::UiInputSystem::navigate(*world, g_viewport, actions);
		scene::UiInputSystem::navigate(*world, g_viewport, input::ActionFrame{});
	}

	shared<scene::Scene> world = mkShared<scene::Scene>();
	scene::Entity canvas;
	scene::Entity first;
	scene::Entity second;
	scene::Entity third;
};

}// namespace

TEST(LuaFocus, setAndGetFocusFromAScript) {
	const FocusFixture fixture{"focused = -1\n"
							   "function on_create()\n"
							   "  before = ui.get_focus()\n"
							   "  accepted = ui.set_focus(ui.find('Second'))\n"
							   "  refused = ui.set_focus(entity_id)\n"
							   "  focused = ui.get_focus()\n"
							   "end\n"};
	fixture.script().onCreate();
	EXPECT_EQ(fixture.script().getPropertyInt("before").value_or(-1), 0);
	EXPECT_EQ(fixture.script().getPropertyBool("accepted").value_or(false), true);
	EXPECT_EQ(fixture.script().getPropertyBool("refused").value_or(true), false);
	EXPECT_EQ(fixture.script().getPropertyInt("focused").value_or(-1), static_cast<int64_t>(fixture.second.getUUID()));
	EXPECT_TRUE(fixture.world->getUiInputState().focusVisible);
}

TEST(LuaFocus, aFocusCallbackReceivesTheWidget) {
	FocusFixture fixture{"seen = 0\n"
						 "count = 0\n"
						 "function on_focus()\n"
						 "  seen = _focused_entity\n"
						 "  count = count + 1\n"
						 "end\n"};
	fixture.third.addComponent<scene::component::UiFocus>().onFocusCallback = "on_focus";
	fixture.press(input::key::Down);
	EXPECT_EQ(fixture.script().getPropertyInt("count").value_or(-1), 0);
	fixture.press(input::key::Down);
	fixture.press(input::key::Down);
	EXPECT_EQ(fixture.script().getPropertyInt("seen").value_or(-1), static_cast<int64_t>(fixture.third.getUUID()));
	EXPECT_EQ(fixture.script().getPropertyInt("count").value_or(-1), 1);
	// Pressing again on the last widget does not move: no new call.
	fixture.press(input::key::Down);
	EXPECT_EQ(fixture.script().getPropertyInt("count").value_or(-1), 1);
}

TEST(LuaFocus, acceptClicksTheFocusedButton) {
	const FocusFixture fixture{"clicks = 0\n"
							   "who = 0\n"
							   "function on_ok()\n"
							   "  clicks = clicks + 1\n"
							   "  who = _clicked_entity\n"
							   "end\n"};
	fixture.second.getComponent<scene::component::UiButton>().onClickCallback = "on_ok";
	fixture.press(input::key::Down);
	fixture.press(input::key::Enter);
	EXPECT_EQ(fixture.script().getPropertyInt("clicks").value_or(-1), 0);
	fixture.press(input::key::Down);
	EXPECT_EQ(scene::UiInputSystem::getFocus(*fixture.world).getName(), "Second");
	fixture.press(input::key::Enter);
	EXPECT_EQ(fixture.script().getPropertyInt("clicks").value_or(-1), 1);
	EXPECT_EQ(fixture.script().getPropertyInt("who").value_or(-1), static_cast<int64_t>(fixture.second.getUUID()));
}

TEST(LuaFocus, adjustingASliderRunsItsCallback) {
	FocusFixture fixture{"value = -1\n"
						 "function on_slide()\n"
						 "  value = _slider_value\n"
						 "end\n"};
	auto& slider = fixture.first.addComponent<scene::component::UiSlider>();
	slider.onValueChangedCallback = "on_slide";
	slider.step = 0.25f;
	fixture.press(input::key::Down);
	fixture.press(input::key::Right);
	EXPECT_NEAR(fixture.script().getPropertyFloat("value").value_or(-1.f), 0.25f, 1e-5f);
}

namespace {

// A scene and its HUD asset, each holding a button, from a temporary `.owlhud`.
struct HudFocusFixture {
	HudFocusFixture() {
		core::Log::init(core::Log::Level::Off);
		path = std::filesystem::temp_directory_path() /
			   std::format("owl_focus_hud_{}.owlhud", std::chrono::steady_clock::now().time_since_epoch().count());
		const auto hud = mkShared<scene::Scene>();
		auto canvas = hud->createEntity("HudCanvas");
		canvas.addComponent<scene::component::Canvas>().scaleMode = scene::component::Canvas::ScaleMode::ConstantPixels;
		makeButton(*hud, canvas, "HudButton", 100.f);
		EXPECT_TRUE(scene::SceneSerializer(hud, scene::DocumentKind::Hud).serialize(path));
		context.setDefaultHud(path.string());
		world->setEngineContext(&context);
		world->status = scene::Scene::Status::Playing;
		auto sceneCanvas = world->createEntity("SceneCanvas");
		sceneCanvas.addComponent<scene::component::Canvas>().scaleMode =
				scene::component::Canvas::ScaleMode::ConstantPixels;
		makeButton(*world, sceneCanvas, "SceneButton", 200.f);
	}

	~HudFocusFixture() {
		std::error_code ec;
		std::filesystem::remove(path, ec);
		core::Log::invalidate();
	}

	HudFocusFixture(const HudFocusFixture&) = delete;
	HudFocusFixture(HudFocusFixture&&) = delete;
	auto operator=(const HudFocusFixture&) -> HudFocusFixture& = delete;
	auto operator=(HudFocusFixture&&) -> HudFocusFixture& = delete;

	std::filesystem::path path;
	app::EngineContext context;
	shared<scene::Scene> world = mkShared<scene::Scene>();
};

}// namespace

TEST(LuaFocus, theFocusMovesBetweenTheSceneAndItsHud) {
	const HudFocusFixture fixture;
	const auto hud = fixture.world->getHud();
	ASSERT_TRUE(hud);
	input::InputFrame frame;
	frame.current.setKey(input::key::Down, true);
	input::ActionFrame actions;
	actions.update(input::ActionMap::getDefault(), frame);
	scene::UiInputSystem::navigate(*fixture.world, g_viewport, actions);
	// The scene's widgets come before the HUD's: the first press shows the scene button.
	EXPECT_EQ(scene::UiInputSystem::getFocus(*fixture.world).getName(), "SceneButton");
	scene::UiInputSystem::navigate(*fixture.world, g_viewport, input::ActionFrame{});
	// The HUD button is above the scene one: Up goes there.
	frame.current.setKey(input::key::Down, false);
	frame.current.setKey(input::key::Up, true);
	actions.update(input::ActionMap::getDefault(), frame);
	scene::UiInputSystem::navigate(*fixture.world, g_viewport, actions);
	const auto focus = scene::UiInputSystem::getFocus(*fixture.world);
	ASSERT_TRUE(focus);
	EXPECT_EQ(focus.getName(), "HudButton");
	EXPECT_TRUE(fixture.world->getUiInputState().focusInHud);
	EXPECT_EQ(focus.getScene(), hud.get());
}
