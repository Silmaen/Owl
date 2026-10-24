/**
 * @file LuaRuntimeGaps_test.cpp
 * @author Silmaen
 * @date 23/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <core/Timestep.h>
#include <scene/Entity.h>
#include <scene/PrefabSerializer.h>
#include <scene/Scene.h>
#include <scene/component/components.h>
#include <script/ScriptInstance.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <system_error>
#include <tuple>
#include <variant>
#include <vector>

using namespace owl;
using namespace owl::scene;

namespace {

/// A runtime scene with a camera, a folder of scripts and prefabs, and a way to run Lua bound to the scene.
class LuaRuntimeGaps : public testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_dir = std::filesystem::temp_directory_path() /
				("owl_lua_gaps_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
		std::filesystem::create_directories(m_dir);
		m_scene = mkShared<Scene>();
		auto camera = m_scene->createEntity("Camera");
		camera.getComponent<component::Transform>().transform.translation() = {0.f, 0.f, 5.f};
		camera.addComponent<component::Camera>().primary = true;
		m_scene->onViewportResize({800u, 600u});
	}

	void TearDown() override {
		m_scene->onEndRuntime();
		std::error_code error;
		std::filesystem::remove_all(m_dir, error);
		core::Log::invalidate();
	}

	[[nodiscard]] auto write(const std::string& iName, const std::string& iText) const -> std::string {
		const auto path = m_dir / iName;
		std::ofstream{path} << iText;
		return path.generic_string();
	}

	/// Write a prefab of one entity tagged `Enemy` running a script that marks its creation in the game state.
	[[nodiscard]] auto writeEnemyPrefab() const -> std::string {
		const auto script = write("enemy.lua", "function on_create()\n gamestate.set('enemy_created', true)\nend\n");
		auto source = mkShared<Scene>();
		auto enemy = source->createEntity("Enemy");
		enemy.addComponent<component::SpriteRenderer>();
		enemy.addComponent<component::LuaScript>().scriptPath = script;
		return write("enemy.owlprefab", PrefabSerializer::serializeToString(enemy, *source, "Enemy"));
	}

	/// Run a script on creation, bound to the scene, and return the instance holding its globals.
	auto run(const std::string& iBody) -> shared<script::ScriptInstance> {
		const std::string text = std::format("function on_create()\n{}\nend\n", iBody);
		const std::vector<uint8_t> data(text.begin(), text.end());
		auto instance = mkShared<script::ScriptInstance>();
		instance->setScene(m_scene.get());
		EXPECT_TRUE(instance->createFromBuffer(data, "gaps_test", 0));
		instance->onCreate();
		return instance;
	}

	void step(const int iMilliseconds = 16) const {
		core::Timestep timeStep;
		timeStep.forceUpdate(std::chrono::milliseconds(iMilliseconds));
		m_scene->onUpdateRuntime(timeStep, false);
	}

	std::filesystem::path m_dir;
	shared<Scene> m_scene;
};

/// Entity id as Lua reads it: the UUID bits as a signed integer.
auto luaId(const Entity& iEntity) -> int64_t { return static_cast<int64_t>(static_cast<uint64_t>(iEntity.getUUID())); }

}// namespace

TEST_F(LuaRuntimeGaps, spawnPrefabCreatesThePlacedInstanceAndStartsItsScript) {
	const auto prefab = writeEnemyPrefab();
	m_scene->onStartRuntime();
	const auto instance = run(std::format("id = scene.spawn_prefab('{}', 3, 4, 0)\nx, y = transform.get_position(id)\n"
										  "created_now = gamestate.get('enemy_created') == true",
										  prefab));
	const auto id = instance->getPropertyInt("id").value_or(0);
	ASSERT_NE(id, 0);
	EXPECT_FALSE(instance->getPropertyBool("created_now").value_or(true));
	EXPECT_NEAR(instance->getPropertyFloat("x").value_or(0.f), 3.f, 1e-5f);
	EXPECT_NEAR(instance->getPropertyFloat("y").value_or(0.f), 4.f, 1e-5f);
	const auto enemy = m_scene->findEntityByUUID(core::UUID{static_cast<uint64_t>(id)});
	ASSERT_TRUE(enemy);
	EXPECT_EQ(enemy.getName(), "Enemy");
	EXPECT_TRUE(enemy.hasComponent<component::PrefabLink>());
	step();
	ASSERT_TRUE(enemy.getComponent<component::LuaScript>().instance);
	const auto created = m_scene->getGameState().get("enemy_created");
	ASSERT_TRUE(created.has_value());
	EXPECT_TRUE(std::get<bool>(*created));
}

TEST_F(LuaRuntimeGaps, spawnedThenDestroyedInTheSameFrameNeverStarts) {
	const auto prefab = writeEnemyPrefab();
	m_scene->onStartRuntime();
	std::ignore = run(std::format("local id = scene.spawn_prefab('{}')\nscene.destroy_entity(id)", prefab));
	step();
	EXPECT_FALSE(m_scene->getGameState().get("enemy_created").has_value());
	for (const auto& entity: m_scene->getAllEntities()) EXPECT_NE(entity.getName(), "Enemy");
}

TEST_F(LuaRuntimeGaps, spawnPrefabGivesZeroForAMissingPrefab) {
	const auto instance = run("id = scene.spawn_prefab('prefabs/none.owlprefab')");
	EXPECT_EQ(instance->getPropertyInt("id").value_or(-1), 0);
}

TEST_F(LuaRuntimeGaps, addComponentAddsAnOptionalComponentOnly) {
	const auto box = m_scene->createEntity("Box");
	const auto instance =
			run(std::format("local id = {}\nsprite = entity.add_component(id, 'SpriteRenderer')\n"
							"again = entity.add_component(id, 'SpriteRenderer')\n"
							"tag = entity.add_component(id, 'Tag')\nbad = entity.add_component(id, 'Nothing')\n"
							"has = entity.has_component(id, 'SpriteRenderer')",
							luaId(box)));
	EXPECT_TRUE(instance->getPropertyBool("sprite").value_or(false));
	EXPECT_TRUE(instance->getPropertyBool("again").value_or(false));
	EXPECT_FALSE(instance->getPropertyBool("tag").value_or(true));
	EXPECT_FALSE(instance->getPropertyBool("bad").value_or(true));
	EXPECT_TRUE(instance->getPropertyBool("has").value_or(false));
}

TEST_F(LuaRuntimeGaps, timeScaleAndPauseDriveTheSceneClock) {
	m_scene->onStartRuntime();
	const auto instance = run("time.set_scale(2.5)\nscale = time.get_scale()\ntime.set_paused(true)\n"
							  "paused = time.is_paused()\ntime.set_scale(99)\nclamped = time.get_scale()");
	EXPECT_NEAR(instance->getPropertyFloat("scale").value_or(0.f), 2.5f, 1e-5f);
	EXPECT_TRUE(instance->getPropertyBool("paused").value_or(false));
	EXPECT_NEAR(instance->getPropertyFloat("clamped").value_or(0.f), Scene::maxTimeScale, 1e-5f);
	EXPECT_TRUE(m_scene->isPaused());
	const auto ticks = m_scene->getSimulationClock().getTotalTickCount();
	step(100);
	EXPECT_EQ(m_scene->getSimulationClock().getTotalTickCount(), ticks);
	EXPECT_DOUBLE_EQ(m_scene->getRuntimeSeconds(), 0.0);
	EXPECT_NEAR(m_scene->getRealFrameSeconds(), 0.1f, 1e-4f);
	const auto real = run("real = time.real_delta()");
	EXPECT_NEAR(real->getPropertyFloat("real").value_or(0.f), 0.1f, 1e-4f);
}

TEST_F(LuaRuntimeGaps, uiImageFrameAndTextureAreSetFromLua) {
	auto icon = m_scene->createEntity("Icon");
	auto& image = icon.addComponent<component::UiImage>();
	image.columns = 4;
	image.rows = 2;
	const auto instance =
			run(std::format("local id = {}\nset = ui.set_image_frame(id, 5)\nframe = ui.get_image_frame(id)\n"
							"ui.set_image_frame(id, 42)\nclamped = ui.get_image_frame(id)\n"
							"cleared = ui.set_image(id, '')\nnone = ui.set_image_frame({}, 1)",
							luaId(icon), luaId(m_scene->createEntity())));
	EXPECT_TRUE(instance->getPropertyBool("set").value_or(false));
	EXPECT_EQ(instance->getPropertyInt("frame").value_or(0), 5);
	EXPECT_EQ(instance->getPropertyInt("clamped").value_or(0), 7);
	EXPECT_TRUE(instance->getPropertyBool("cleared").value_or(false));
	EXPECT_FALSE(instance->getPropertyBool("none").value_or(true));
	EXPECT_EQ(icon.getComponent<component::UiImage>().texture, nullptr);
}

TEST_F(LuaRuntimeGaps, uiSetSizeResizesTheRectOfAWidget) {
	auto card = m_scene->createEntity("Card");
	card.addComponent<component::UiRect>().size = {1.f, 0.8f};
	const auto instance = run(std::format("done = ui.set_size({}, 1.2, 0.45)\nnone = ui.set_size({}, 1, 1)\n"
										  "clamped = ui.set_size({}, -3, 0.5)",
										  luaId(card), luaId(m_scene->createEntity()), luaId(card)));
	EXPECT_TRUE(instance->getPropertyBool("done").value_or(false));
	EXPECT_FALSE(instance->getPropertyBool("none").value_or(true));
	EXPECT_TRUE(instance->getPropertyBool("clamped").value_or(false));
	EXPECT_NEAR(card.getComponent<component::UiRect>().size.x(), 0.f, 1e-6f);
	EXPECT_NEAR(card.getComponent<component::UiRect>().size.y(), 0.5f, 1e-6f);
}

TEST_F(LuaRuntimeGaps, uiGetRectGivesTheLaidOutRectangleInPixels) {
	auto canvas = m_scene->createEntity("Canvas");
	canvas.addComponent<component::Canvas>().scaleMode = component::Canvas::ScaleMode::ConstantPixels;
	auto button = m_scene->createEntity("Button");
	auto& rect = button.addComponent<component::UiRect>();
	rect.setAnchor(component::UiRect::Anchor::TopLeft);
	rect.pivot = {0.f, 0.f};
	rect.size = {100.f, 50.f};
	rect.anchorOffset = {10.f, 20.f};
	m_scene->setParent(button, canvas);
	auto hidden = m_scene->createEntity("Hidden");
	hidden.addComponent<component::UiRect>();
	hidden.getComponent<component::Visibility>().gameVisible = false;
	m_scene->setParent(hidden, canvas);
	const auto instance = run(std::format("x, y, w, h = ui.get_rect({})\nnone = ui.get_rect({}) == nil\n"
										  "bare = ui.get_rect({}) == nil",
										  luaId(button), luaId(hidden), luaId(m_scene->createEntity())));
	EXPECT_NEAR(instance->getPropertyFloat("x").value_or(0.f), 10.f, 1e-3f);
	EXPECT_NEAR(instance->getPropertyFloat("y").value_or(0.f), 20.f, 1e-3f);
	EXPECT_NEAR(instance->getPropertyFloat("w").value_or(0.f), 100.f, 1e-3f);
	EXPECT_NEAR(instance->getPropertyFloat("h").value_or(0.f), 50.f, 1e-3f);
	EXPECT_TRUE(instance->getPropertyBool("none").value_or(false));
	EXPECT_TRUE(instance->getPropertyBool("bare").value_or(false));
}

TEST_F(LuaRuntimeGaps, worldIsBlockedWhileAWindowIsShown) {
	m_scene->onStartRuntime();
	auto window = m_scene->createEntity("Pause");
	window.addComponent<component::UiWindow>();
	const auto blocked = run("blocked = ui.is_world_blocked()");
	EXPECT_TRUE(blocked->getPropertyBool("blocked").value_or(false));
	window.getComponent<component::Visibility>().gameVisible = false;
	const auto free = run("blocked = ui.is_world_blocked()");
	EXPECT_FALSE(free->getPropertyBool("blocked").value_or(true));
}

TEST_F(LuaRuntimeGaps, pickEntityFindsTheSpriteUnderAPixel) {
	auto sprite = m_scene->createEntity("Target");
	sprite.addComponent<component::SpriteRenderer>();
	const auto instance = run(std::format("hit = scene.pick_entity(400, 300)\nmiss = scene.pick_entity(1, 1)"));
	EXPECT_EQ(instance->getPropertyInt("hit").value_or(0),
			  static_cast<int64_t>(static_cast<uint64_t>(sprite.getUUID())));
	EXPECT_EQ(instance->getPropertyInt("miss").value_or(-1), 0);
}

TEST_F(LuaRuntimeGaps, pickEntityWithATagSkipsTheOtherEntities) {
	auto tower = m_scene->createEntity("Tower");
	tower.addComponent<component::SpriteRenderer>();
	auto ring = m_scene->createEntity("Ring");
	ring.getComponent<component::Transform>().transform.translation() = {0.f, 0.f, 1.f};
	ring.addComponent<component::SpriteRenderer>();
	const auto instance = run("any = scene.pick_entity(400, 300)\ntower = scene.pick_entity(400, 300, 'Tower')");
	EXPECT_EQ(instance->getPropertyInt("any").value_or(0), static_cast<int64_t>(static_cast<uint64_t>(ring.getUUID())));
	EXPECT_EQ(instance->getPropertyInt("tower").value_or(0),
			  static_cast<int64_t>(static_cast<uint64_t>(tower.getUUID())));
}

TEST_F(LuaRuntimeGaps, viewportSizeAndMouseAreInViewportPixels) {
	m_scene->getUiInputState().hasMouse = true;
	m_scene->getUiInputState().mousePosition = {120.f, 45.f};
	const auto instance = run("w, h = scene.get_viewport_size()\nmx, my = scene.get_viewport_mouse()");
	EXPECT_EQ(instance->getPropertyInt("w").value_or(0), 800);
	EXPECT_EQ(instance->getPropertyInt("h").value_or(0), 600);
	EXPECT_NEAR(instance->getPropertyFloat("mx").value_or(0.f), 120.f, 1e-6f);
	EXPECT_NEAR(instance->getPropertyFloat("my").value_or(0.f), 45.f, 1e-6f);
}
