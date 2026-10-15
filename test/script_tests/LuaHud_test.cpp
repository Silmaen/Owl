/**
 * @file LuaHud_test.cpp
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <app/EngineContext.h>
#include <core/Log.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
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

/// A scene whose HUD holds the texts `Score` and `Both` (the scene holds its own `Both`), from a temporary `.owlhud`.
struct HudSceneFixture {
	HudSceneFixture() {
		path = std::filesystem::temp_directory_path() /
			   std::format("owl_lua_hud_{}.owlhud", std::chrono::steady_clock::now().time_since_epoch().count());
		const auto hud = mkShared<scene::Scene>();
		hud->createEntity("Score").addComponent<scene::component::UiText>().text = "0";
		hud->createEntity("Both").addComponent<scene::component::UiText>().text = "hud";
		EXPECT_TRUE(scene::SceneSerializer(hud, scene::DocumentKind::Hud).serialize(path));
		context.setDefaultHud(path.string());
		world->setEngineContext(&context);
		world->createEntity("Both").addComponent<scene::component::UiText>().text = "scene";
	}

	~HudSceneFixture() {
		std::error_code ec;
		std::filesystem::remove(path, ec);
	}

	HudSceneFixture(const HudSceneFixture&) = delete;
	HudSceneFixture(HudSceneFixture&&) = delete;
	auto operator=(const HudSceneFixture&) -> HudSceneFixture& = delete;
	auto operator=(HudSceneFixture&&) -> HudSceneFixture& = delete;

	std::filesystem::path path;
	app::EngineContext context;
	shared<scene::Scene> world = mkShared<scene::Scene>();
};

}// namespace

TEST(LuaHud, uiFindSearchesTheHudThenTheScene) {
	core::Log::init(core::Log::Level::Off);
	const HudSceneFixture fixture;
	const std::string script = "score = 0\nboth_text = ''\nmissing = -1\n"
							   "function on_create()\n"
							   "  score = ui.find('Score')\n"
							   "  ui.set_text(score, '42')\n"
							   "  both_text = ui.get_text(ui.find('Both'))\n"
							   "  missing = ui.find('Nope')\n"
							   "end\n";
	const std::vector<uint8_t> data(script.begin(), script.end());
	const ScriptInstance inst;
	inst.setScene(fixture.world.get());
	ASSERT_TRUE(inst.createFromBuffer(data, "ui_find_test", 1));
	inst.onCreate();
	const auto score = inst.getPropertyInt("score");
	ASSERT_TRUE(score.has_value());
	EXPECT_NE(score.value(), 0);
	const auto widget = fixture.world->getHud()->findEntityByUUID(core::UUID{static_cast<uint64_t>(score.value())});
	ASSERT_TRUE(static_cast<bool>(widget));
	EXPECT_EQ(widget.getComponent<scene::component::UiText>().text, "42");
	EXPECT_EQ(inst.getPropertyString("both_text").value_or(""), "hud");
	EXPECT_EQ(inst.getPropertyInt("missing").value_or(-1), 0);
	core::Log::invalidate();
}

TEST(LuaHud, sceneFindEntityFallsBackToTheHud) {
	core::Log::init(core::Log::Level::Off);
	const HudSceneFixture fixture;
	const std::string script = "score = 0\nboth_text = ''\n"
							   "function on_create()\n"
							   "  score = scene.find_entity('Score')\n"
							   "  both_text = ui.get_text(scene.find_entity('Both'))\n"
							   "end\n";
	const std::vector<uint8_t> data(script.begin(), script.end());
	const ScriptInstance inst;
	inst.setScene(fixture.world.get());
	ASSERT_TRUE(inst.createFromBuffer(data, "find_hud_test", 1));
	inst.onCreate();
	EXPECT_NE(inst.getPropertyInt("score").value_or(0), 0);
	EXPECT_EQ(inst.getPropertyString("both_text").value_or(""), "scene");
	core::Log::invalidate();
}
