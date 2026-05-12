/**
 * @file LuaBindings_test.cpp
 * @author Silmaen
 * @date 09/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <scene/Entity.h>
#include <scene/GameState.h>
#include <scene/Scene.h>
#include <scene/ScreenTransition.h>
#include <scene/SettingsManager.h>
#include <scene/component/Transform.h>
#include <scene/component/components.h>
#include <script/ScriptEngine.h>
#include <script/ScriptInstance.h>

using namespace owl;
using namespace owl::script;

TEST(LuaBindings, transformGetSetPosition) {
	core::Log::init(core::Log::Level::Off);
	const auto scn = mkShared<scene::Scene>();
	const auto entity = scn->createEntity("TestEntity");
	auto& [transform] = entity.getComponent<scene::component::Transform>();
	transform.translation() = {1.0f, 2.0f, 3.0f};

	ScriptEngine::init(scn.get());

	const auto uuid = static_cast<uint64_t>(entity.getUUID());
	// Use entity_id global (set by ScriptInstance::create*) to avoid uint64 literal precision issues.
	const std::string script = "px = 0\npy = 0\npz = 0\n"
							   "function on_create()\n"
							   "  px, py, pz = transform.get_position(entity_id)\n"
							   "  transform.set_position(entity_id, 10.0, 20.0, 30.0)\n"
							   "end\n";
	const std::vector<uint8_t> data(script.begin(), script.end());

	const ScriptInstance inst;
	ASSERT_TRUE(inst.createFromBuffer(data, "transform_test", uuid));
	inst.onCreate();

	// Verify Lua read the original position.
	EXPECT_NEAR(inst.getPropertyFloat("px").value_or(0), 1.0f, 0.01f);
	EXPECT_NEAR(inst.getPropertyFloat("py").value_or(0), 2.0f, 0.01f);
	EXPECT_NEAR(inst.getPropertyFloat("pz").value_or(0), 3.0f, 0.01f);

	// Verify Lua wrote back to the component.
	EXPECT_NEAR(transform.translation().x(), 10.0f, 0.01f);
	EXPECT_NEAR(transform.translation().y(), 20.0f, 0.01f);
	EXPECT_NEAR(transform.translation().z(), 30.0f, 0.01f);

	ScriptEngine::shutdown();
	core::Log::invalidate();
}

TEST(LuaBindings, sceneFindEntity) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<scene::Scene>();
	auto entity = scn->createEntity("MyTarget");
	const auto expectedUuid = static_cast<uint64_t>(entity.getUUID());

	ScriptEngine::init(scn.get());

	const std::string script = "found_id = 0\n"
							   "function on_create()\n"
							   "  found_id = scene.find_entity('MyTarget')\n"
							   "end\n";
	const std::vector<uint8_t> data(script.begin(), script.end());

	const ScriptInstance inst;
	ASSERT_TRUE(inst.createFromBuffer(data, "find_test", 1));
	inst.onCreate();

	const auto foundId = inst.getPropertyInt("found_id");
	ASSERT_TRUE(foundId.has_value());
	EXPECT_EQ(static_cast<uint64_t>(foundId.value()), expectedUuid);

	ScriptEngine::shutdown();
	core::Log::invalidate();
}

TEST(LuaBindings, sceneFindEntityNotFound) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<scene::Scene>();
	ScriptEngine::init(scn.get());

	const std::string script = "found_id = -1\n"
							   "function on_create()\n"
							   "  found_id = scene.find_entity('NonExistent')\n"
							   "end\n";
	const std::vector<uint8_t> data(script.begin(), script.end());

	ScriptInstance inst;
	ASSERT_TRUE(inst.createFromBuffer(data, "notfound_test", 1));
	inst.onCreate();

	const auto foundId = inst.getPropertyInt("found_id");
	ASSERT_TRUE(foundId.has_value());
	EXPECT_EQ(foundId.value(), 0);

	ScriptEngine::shutdown();
	core::Log::invalidate();
}

TEST(LuaBindings, entityHasComponent) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<scene::Scene>();
	auto entity = scn->createEntity("CompTest");
	// Entity has Transform by default, no PhysicBody.

	ScriptEngine::init(scn.get());

	const auto uuid = static_cast<uint64_t>(entity.getUUID());
	const std::string script = "has_transform = false\n"
							   "has_physic = false\n"
							   "function on_create()\n"
							   "  has_transform = entity.has_component(entity_id, 'Transform')\n"
							   "  has_physic = entity.has_component(entity_id, 'PhysicBody')\n"
							   "end\n";
	const std::vector<uint8_t> data(script.begin(), script.end());

	ScriptInstance inst;
	ASSERT_TRUE(inst.createFromBuffer(data, "comp_test", uuid));
	inst.onCreate();

	EXPECT_TRUE(inst.getPropertyBool("has_transform").value_or(false));
	EXPECT_FALSE(inst.getPropertyBool("has_physic").value_or(true));

	ScriptEngine::shutdown();
	core::Log::invalidate();
}

TEST(LuaBindings, entityGetName) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<scene::Scene>();
	auto entity = scn->createEntity("NamedEntity");

	ScriptEngine::init(scn.get());

	const auto uuid = static_cast<uint64_t>(entity.getUUID());
	const std::string script = "ent_name = ''\n"
							   "function on_create()\n"
							   "  ent_name = entity.get_name(entity_id)\n"
							   "end\n";
	const std::vector<uint8_t> data(script.begin(), script.end());

	ScriptInstance inst;
	ASSERT_TRUE(inst.createFromBuffer(data, "name_test", uuid));
	inst.onCreate();

	EXPECT_EQ(inst.getPropertyString("ent_name").value_or(""), "NamedEntity");

	ScriptEngine::shutdown();
	core::Log::invalidate();
}

TEST(LuaBindings, logDoesNotCrash) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<scene::Scene>();
	ScriptEngine::init(scn.get());

	const std::string script = "function on_create()\n"
							   "  log.trace('trace msg')\n"
							   "  log.info('info msg')\n"
							   "  log.warn('warn msg')\n"
							   "  log.error('error msg')\n"
							   "end\n";
	const std::vector<uint8_t> data(script.begin(), script.end());

	ScriptInstance inst;
	ASSERT_TRUE(inst.createFromBuffer(data, "log_test", 1));
	inst.onCreate();// Should not crash.

	ScriptEngine::shutdown();
	core::Log::invalidate();
}

TEST(LuaBindings, timeDelta) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<scene::Scene>();
	ScriptEngine::init(scn.get());

	const std::string script = "dt_val = 0\n"
							   "function on_update(dt)\n"
							   "  dt_val = time.delta()\n"
							   "end\n";
	const std::vector<uint8_t> data(script.begin(), script.end());

	ScriptInstance inst;
	ASSERT_TRUE(inst.createFromBuffer(data, "time_test", 1));
	inst.onUpdate(0.033f);

	const auto dt = inst.getPropertyFloat("dt_val");
	ASSERT_TRUE(dt.has_value());
	EXPECT_NEAR(dt.value(), 0.033f, 0.001f);

	ScriptEngine::shutdown();
	core::Log::invalidate();
}

TEST(LuaBindings, transformRotationScaleRoundTrip) {
	core::Log::init(core::Log::Level::Off);
	const auto scn = mkShared<scene::Scene>();
	const auto entity = scn->createEntity("RotScale");
	auto& [transform] = entity.getComponent<scene::component::Transform>();
	transform.rotation() = {0.1f, 0.2f, 0.3f};
	transform.scale() = {2.f, 3.f, 4.f};

	ScriptEngine::init(scn.get());
	const auto uuid = static_cast<uint64_t>(entity.getUUID());
	const std::string script = "rx, ry, rz = 0, 0, 0\n"
							   "sx, sy, sz = 0, 0, 0\n"
							   "function on_create()\n"
							   "  rx, ry, rz = transform.get_rotation(entity_id)\n"
							   "  sx, sy, sz = transform.get_scale(entity_id)\n"
							   "  transform.set_rotation(entity_id, 0.5, 0.6, 0.7)\n"
							   "  transform.set_scale(entity_id, 5.0, 6.0, 7.0)\n"
							   "end\n";
	const std::vector<uint8_t> data(script.begin(), script.end());
	const ScriptInstance inst;
	ASSERT_TRUE(inst.createFromBuffer(data, "rot_scale_test", uuid));
	inst.onCreate();

	EXPECT_NEAR(inst.getPropertyFloat("rx").value_or(0), 0.1f, 0.01f);
	EXPECT_NEAR(inst.getPropertyFloat("sx").value_or(0), 2.f, 0.01f);
	EXPECT_NEAR(transform.rotation().x(), 0.5f, 0.01f);
	EXPECT_NEAR(transform.rotation().z(), 0.7f, 0.01f);
	EXPECT_NEAR(transform.scale().x(), 5.f, 0.01f);
	EXPECT_NEAR(transform.scale().z(), 7.f, 0.01f);

	ScriptEngine::shutdown();
	core::Log::invalidate();
}

// When the entity is missing we expect the safe fallbacks: position=0, scale=1.
TEST(LuaBindings, transformGettersFallbackOnMissingEntity) {
	core::Log::init(core::Log::Level::Off);
	const auto scn = mkShared<scene::Scene>();
	ScriptEngine::init(scn.get());
	const std::string script = "px, py, pz = 9, 9, 9\n"
							   "rx, ry, rz = 9, 9, 9\n"
							   "sx, sy, sz = 9, 9, 9\n"
							   "function on_create()\n"
							   "  px, py, pz = transform.get_position(99999)\n"
							   "  rx, ry, rz = transform.get_rotation(99999)\n"
							   "  sx, sy, sz = transform.get_scale(99999)\n"
							   "end\n";
	const std::vector<uint8_t> data(script.begin(), script.end());
	const ScriptInstance inst;
	ASSERT_TRUE(inst.createFromBuffer(data, "fallback_test", 1));
	inst.onCreate();
	EXPECT_NEAR(inst.getPropertyFloat("px").value_or(-1), 0.f, 0.001f);
	EXPECT_NEAR(inst.getPropertyFloat("rz").value_or(-1), 0.f, 0.001f);
	EXPECT_NEAR(inst.getPropertyFloat("sx").value_or(-1), 1.f, 0.001f);
	EXPECT_NEAR(inst.getPropertyFloat("sz").value_or(-1), 1.f, 0.001f);
	ScriptEngine::shutdown();
	core::Log::invalidate();
}

TEST(LuaBindings, sceneCreateAndDestroyEntity) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<scene::Scene>();
	ScriptEngine::init(scn.get());
	const std::string script = "id = 0\n"
							   "function on_create()\n"
							   "  id = scene.create_entity('NewEnt')\n"
							   "  scene.destroy_entity(id)\n"
							   "end\n";
	const std::vector<uint8_t> data(script.begin(), script.end());
	const ScriptInstance inst;
	ASSERT_TRUE(inst.createFromBuffer(data, "create_destroy_test", 1));
	inst.onCreate();
	const auto created = inst.getPropertyInt("id");
	ASSERT_TRUE(created.has_value());
	EXPECT_NE(created.value(), 0);
	// Entity must have been destroyed.
	EXPECT_FALSE(scn->findEntityByUUID(core::UUID{static_cast<uint64_t>(created.value())}));
	ScriptEngine::shutdown();
	core::Log::invalidate();
}

TEST(LuaBindings, sceneLoadAndQuit) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<scene::Scene>();
	ScriptEngine::init(scn.get());
	const std::string script = "function on_create()\n"
							   "  scene.load_scene('Other')\n"
							   "  scene.quit()\n"
							   "end\n";
	const std::vector<uint8_t> data(script.begin(), script.end());
	const ScriptInstance inst;
	ASSERT_TRUE(inst.createFromBuffer(data, "load_quit_test", 1));
	inst.onCreate();
	EXPECT_TRUE(scn->teleportRequest.pending);
	EXPECT_EQ(scn->teleportRequest.levelName, "Other");
	EXPECT_TRUE(scn->quitRequested);
	ScriptEngine::shutdown();
	core::Log::invalidate();
}

TEST(LuaBindings, sceneTransitionToParsesTypes) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<scene::Scene>();
	ScriptEngine::init(scn.get());
	// Default (no type) → fade pair.
	const std::string script = "function on_create()\n"
							   "  scene.transition_to('next.scene')\n"
							   "  scene.transition_to('next2.scene', 'wipe_left')\n"
							   "  scene.transition_to('next3.scene', 0.5)\n"
							   "  scene.transition_to('next4.scene', 'fade_out', 0.25)\n"
							   "end\n";
	const std::vector<uint8_t> data(script.begin(), script.end());
	const ScriptInstance inst;
	ASSERT_TRUE(inst.createFromBuffer(data, "transition_test", 1));
	inst.onCreate();// must not crash; ScreenTransition request paths exercised.
	ScriptEngine::shutdown();
	core::Log::invalidate();
}

TEST(LuaBindings, uiTextSetGetAndVisibility) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<scene::Scene>();
	auto ent = scn->createEntity("Ui");
	auto& uiText = ent.addComponent<scene::component::UiText>();
	uiText.text = "before";
	auto& slider = ent.addComponent<scene::component::UiSlider>();
	slider.value = 0.25f;
	ent.addComponent<scene::component::UiProgressBar>();
	auto& button = ent.addComponent<scene::component::UiButton>();
	button.state = scene::component::UiButton::State::Normal;

	ScriptEngine::init(scn.get());
	const auto uuid = static_cast<uint64_t>(ent.getUUID());
	const std::string script = "txt = ''\n"
							   "sv = -1\n"
							   "function on_create()\n"
							   "  ui.set_text(entity_id, 'after')\n"
							   "  txt = ui.get_text(entity_id)\n"
							   "  ui.set_visible(entity_id, false)\n"
							   "  ui.set_progress(entity_id, 0.7)\n"
							   "  ui.set_slider_value(entity_id, 0.9)\n"
							   "  sv = ui.get_slider_value(entity_id)\n"
							   "  ui.set_button_enabled(entity_id, false)\n"
							   "end\n";
	const std::vector<uint8_t> data(script.begin(), script.end());
	const ScriptInstance inst;
	ASSERT_TRUE(inst.createFromBuffer(data, "ui_test", uuid));
	inst.onCreate();

	EXPECT_EQ(uiText.text, "after");
	EXPECT_EQ(inst.getPropertyString("txt").value_or(""), "after");
	EXPECT_FALSE(ent.getComponent<scene::component::Visibility>().gameVisible);
	EXPECT_NEAR(ent.getComponent<scene::component::UiProgressBar>().value, 0.7f, 0.01f);
	EXPECT_NEAR(slider.value, 0.9f, 0.01f);
	EXPECT_NEAR(inst.getPropertyFloat("sv").value_or(0), 0.9f, 0.01f);
	EXPECT_EQ(button.state, scene::component::UiButton::State::Disabled);
	ScriptEngine::shutdown();
	core::Log::invalidate();
}

TEST(LuaBindings, uiGettersFallbackWhenComponentMissing) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<scene::Scene>();
	auto ent = scn->createEntity("noUi");
	ScriptEngine::init(scn.get());
	const auto uuid = static_cast<uint64_t>(ent.getUUID());
	const std::string script = "txt = 'init'\n"
							   "sv = 99\n"
							   "function on_create()\n"
							   "  txt = ui.get_text(entity_id)\n"
							   "  sv = ui.get_slider_value(entity_id)\n"
							   "end\n";
	const std::vector<uint8_t> data(script.begin(), script.end());
	const ScriptInstance inst;
	ASSERT_TRUE(inst.createFromBuffer(data, "ui_fallback_test", uuid));
	inst.onCreate();
	EXPECT_EQ(inst.getPropertyString("txt").value_or("?"), "");
	EXPECT_NEAR(inst.getPropertyFloat("sv").value_or(-1), 0.f, 0.001f);
	ScriptEngine::shutdown();
	core::Log::invalidate();
}

TEST(LuaBindings, uiTransitionsAndIsActive) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<scene::Scene>();
	ScriptEngine::init(scn.get());
	scene::ScreenTransition::reset();// reset
	const std::string script = "active1 = true\n"
							   "function on_create()\n"
							   "  active1 = ui.is_transition_active()\n"
							   "  ui.transition_fade_in(0.3)\n"
							   "  ui.transition_fade_out(0.3)\n"
							   "  ui.transition_play('wipe_up', 0.4)\n"
							   "  ui.transition_play('wipe_down', 0.4, 1.0, 0.5, 0.0)\n"
							   "  ui.transition_play('fade_in', 0.4, 0.1, 0.2, 0.3, 0.5)\n"
							   "  ui.transition_play('not_a_real_type', 0.4)\n"
							   "end\n";
	const std::vector<uint8_t> data(script.begin(), script.end());
	const ScriptInstance inst;
	ASSERT_TRUE(inst.createFromBuffer(data, "ui_trans_test", 1));
	inst.onCreate();
	EXPECT_FALSE(inst.getPropertyBool("active1").value_or(true));
	scene::ScreenTransition::reset();
	ScriptEngine::shutdown();
	core::Log::invalidate();
}

TEST(LuaBindings, gamestateSetGetRemoveClear) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<scene::Scene>();
	ScriptEngine::init(scn.get());
	const std::string script = "vbool = nil\n"
							   "vint = nil\n"
							   "vnum = nil\n"
							   "vstr = nil\n"
							   "vmiss = 'kept'\n"
							   "vmissdefault = nil\n"
							   "function on_create()\n"
							   "  gamestate.set('flag', true)\n"
							   "  gamestate.set('count', 42)\n"
							   "  gamestate.set('hp', 1.5)\n"
							   "  gamestate.set('name', 'arthur')\n"
							   "  vbool = gamestate.get('flag')\n"
							   "  vint = gamestate.get('count')\n"
							   "  vnum = gamestate.get('hp')\n"
							   "  vstr = gamestate.get('name')\n"
							   "  vmiss = gamestate.get('absent')\n"
							   "  vmissdefault = gamestate.get('absent', 'fallback')\n"
							   "  gamestate.remove('flag')\n"
							   "  gamestate.clear()\n"
							   "end\n";
	const std::vector<uint8_t> data(script.begin(), script.end());
	const ScriptInstance inst;
	ASSERT_TRUE(inst.createFromBuffer(data, "gs_test", 1));
	inst.onCreate();
	EXPECT_TRUE(inst.getPropertyBool("vbool").value_or(false));
	EXPECT_EQ(inst.getPropertyInt("vint").value_or(0), 42);
	EXPECT_NEAR(inst.getPropertyFloat("vnum").value_or(-1), 1.5f, 0.01f);
	EXPECT_EQ(inst.getPropertyString("vstr").value_or(""), "arthur");
	EXPECT_EQ(inst.getPropertyString("vmissdefault").value_or(""), "fallback");
	EXPECT_TRUE(scn->getGameState().get("flag") == std::nullopt);// removed + cleared
	ScriptEngine::shutdown();
	core::Log::invalidate();
}

TEST(LuaBindings, inputBindings) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<scene::Scene>();
	ScriptEngine::init(scn.get());
	const std::string script = "kp = nil\n"
							   "mp = nil\n"
							   "mx = nil\n"
							   "my = nil\n"
							   "function on_create()\n"
							   "  kp = input.is_key_pressed(65)\n"
							   "  mp = input.is_mouse_button_pressed(0)\n"
							   "  mx = input.get_mouse_x()\n"
							   "  my = input.get_mouse_y()\n"
							   "end\n";
	const std::vector<uint8_t> data(script.begin(), script.end());
	const ScriptInstance inst;
	ASSERT_TRUE(inst.createFromBuffer(data, "input_test", 1));
	inst.onCreate();
	// In headless tests no input device is registered → all return false / 0.
	EXPECT_FALSE(inst.getPropertyBool("kp").value_or(true));
	EXPECT_FALSE(inst.getPropertyBool("mp").value_or(true));
	EXPECT_NEAR(inst.getPropertyFloat("mx").value_or(-1), 0.f, 0.001f);
	EXPECT_NEAR(inst.getPropertyFloat("my").value_or(-1), 0.f, 0.001f);
	ScriptEngine::shutdown();
	core::Log::invalidate();
}

TEST(LuaBindings, soundBindingsWithoutSoundSystem) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<scene::Scene>();
	ScriptEngine::init(scn.get());
	// SoundSystem is not running → play returns invalidSoundHandle but does not crash.
	const std::string script = "h = nil\n"
							   "function on_create()\n"
							   "  h = sound.play('missing.wav')\n"
							   "  sound.stop(h)\n"
							   "  sound.pause(h)\n"
							   "  sound.resume(h)\n"
							   "  sound.set_volume(h, 0.5)\n"
							   "end\n";
	const std::vector<uint8_t> data(script.begin(), script.end());
	const ScriptInstance inst;
	ASSERT_TRUE(inst.createFromBuffer(data, "sound_test", 1));
	inst.onCreate();
	const auto h = inst.getPropertyInt("h");
	ASSERT_TRUE(h.has_value());
	// Invalid handle (sound system off) is 0.
	EXPECT_EQ(h.value(), 0);
	ScriptEngine::shutdown();
	core::Log::invalidate();
}

TEST(LuaBindings, sceneFunctionsWithoutActiveScene) {
	core::Log::init(core::Log::Level::Off);
	// No ScriptEngine::init() — so getActiveScene returns nullptr.
	// We have to manually create a Lua state without binding the scene.
	auto scn = mkShared<scene::Scene>();
	ScriptEngine::init(scn.get());
	ScriptEngine::shutdown();
	// Now active scene is nullptr.
	ScriptEngine::init(nullptr);

	const std::string script = "found = 99\n"
							   "created = 99\n"
							   "function on_create()\n"
							   "  found = scene.find_entity('x')\n"
							   "  created = scene.create_entity('x')\n"
							   "  scene.destroy_entity(123)\n"
							   "  scene.load_scene('y')\n"
							   "  scene.quit()\n"
							   "end\n";
	const std::vector<uint8_t> data(script.begin(), script.end());
	const ScriptInstance inst;
	ASSERT_TRUE(inst.createFromBuffer(data, "no_scene_test", 1));
	inst.onCreate();
	EXPECT_EQ(inst.getPropertyInt("found").value_or(-1), 0);
	EXPECT_EQ(inst.getPropertyInt("created").value_or(-1), 0);
	ScriptEngine::shutdown();
	core::Log::invalidate();
}

TEST(LuaBindings, gamestateOpsWithoutActiveScene) {
	core::Log::init(core::Log::Level::Off);
	ScriptEngine::init(nullptr);
	const std::string script = "v = 'kept'\n"
							   "function on_create()\n"
							   "  gamestate.set('a', 1)\n"
							   "  v = gamestate.get('a')\n"
							   "  gamestate.remove('a')\n"
							   "  gamestate.clear()\n"
							   "end\n";
	const std::vector<uint8_t> data(script.begin(), script.end());
	const ScriptInstance inst;
	ASSERT_TRUE(inst.createFromBuffer(data, "gs_no_scene_test", 1));
	inst.onCreate();
	// Without a scene, set is a no-op and get returns nil → property unchanged.
	ScriptEngine::shutdown();
	core::Log::invalidate();
}

TEST(LuaBindings, settingsBindings) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<scene::Scene>();
	ScriptEngine::init(scn.get());
	scene::SettingsManager::resetAllToDefaults();
	scene::SettingsManager::set("flag", true);
	const std::string script = "vbool = nil\n"
							   "vint = nil\n"
							   "vstr = nil\n"
							   "vmiss = nil\n"
							   "function on_create()\n"
							   "  settings.set('count', 7)\n"
							   "  settings.set('name', 'arthur')\n"
							   "  vbool = settings.get('flag')\n"
							   "  vint = settings.get('count')\n"
							   "  vstr = settings.get('name')\n"
							   "  vmiss = settings.get('absent', 'def')\n"
							   "  settings.reset('flag')\n"
							   "  settings.save()\n"
							   "  settings.load()\n"
							   "  settings.reset_all()\n"
							   "  settings.apply()\n"
							   "end\n";
	const std::vector<uint8_t> data(script.begin(), script.end());
	const ScriptInstance inst;
	ASSERT_TRUE(inst.createFromBuffer(data, "settings_test", 1));
	inst.onCreate();
	EXPECT_TRUE(inst.getPropertyBool("vbool").value_or(false));
	EXPECT_EQ(inst.getPropertyInt("vint").value_or(0), 7);
	EXPECT_EQ(inst.getPropertyString("vstr").value_or(""), "arthur");
	EXPECT_EQ(inst.getPropertyString("vmiss").value_or(""), "def");
	ScriptEngine::shutdown();
	core::Log::invalidate();
}

TEST(LuaBindings, triggerTimerBindings) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<scene::Scene>();
	auto ent = scn->createEntity("trig");
	auto& trig = ent.addComponent<scene::component::Trigger>();
	trig.trigger.type = scene::SceneTrigger::TriggerType::Timer;
	trig.trigger.timerDuration = 1.0f;

	ScriptEngine::init(scn.get());
	const auto uuid = static_cast<uint64_t>(ent.getUUID());
	const std::string script = "function on_create()\n"
							   "  trigger.start_timer(entity_id)\n"
							   "  trigger.reset_timer(entity_id)\n"
							   "  trigger.stop_timer(entity_id)\n"
							   "end\n";
	const std::vector<uint8_t> data(script.begin(), script.end());
	const ScriptInstance inst;
	ASSERT_TRUE(inst.createFromBuffer(data, "trigger_test", uuid));
	inst.onCreate();
	ScriptEngine::shutdown();
	core::Log::invalidate();
}

TEST(LuaBindings, triggerOnEntityWithoutTriggerIsNoOp) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<scene::Scene>();
	auto ent = scn->createEntity("noTrig");
	ScriptEngine::init(scn.get());
	const auto uuid = static_cast<uint64_t>(ent.getUUID());
	const std::string script = "function on_create()\n"
							   "  trigger.start_timer(entity_id)\n"
							   "  trigger.stop_timer(entity_id)\n"
							   "  trigger.reset_timer(entity_id)\n"
							   "end\n";
	const std::vector<uint8_t> data(script.begin(), script.end());
	const ScriptInstance inst;
	ASSERT_TRUE(inst.createFromBuffer(data, "trig_notrig_test", uuid));
	inst.onCreate();
	ScriptEngine::shutdown();
	core::Log::invalidate();
}

TEST(LuaBindings, triggerWithNullSceneIsNoOp) {
	core::Log::init(core::Log::Level::Off);
	ScriptEngine::init(nullptr);
	const std::string script = "function on_create()\n"
							   "  trigger.start_timer(123)\n"
							   "  trigger.stop_timer(123)\n"
							   "  trigger.reset_timer(123)\n"
							   "end\n";
	const std::vector<uint8_t> data(script.begin(), script.end());
	const ScriptInstance inst;
	ASSERT_TRUE(inst.createFromBuffer(data, "trig_null_test", 1));
	inst.onCreate();
	ScriptEngine::shutdown();
	core::Log::invalidate();
}

TEST(LuaBindings, saveQueriesAndDeferredRequests) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<scene::Scene>();
	ScriptEngine::init(scn.get());
	const std::string script = "has = nil\n"
							   "saves = nil\n"
							   "function on_create()\n"
							   "  save.save_game(7)\n"
							   "  has = save.has_save(99999)\n"
							   "  save.delete_save(99999)\n"
							   "  saves = save.list_saves()\n"
							   "  save.load_game(7)\n"
							   "end\n";
	const std::vector<uint8_t> data(script.begin(), script.end());
	const ScriptInstance inst;
	ASSERT_TRUE(inst.createFromBuffer(data, "save_test", 1));
	inst.onCreate();
	EXPECT_TRUE(scn->saveLoadRequest.pending);
	EXPECT_TRUE(scn->saveLoadRequest.isLoad);
	EXPECT_EQ(scn->saveLoadRequest.slot, 7u);
	EXPECT_FALSE(inst.getPropertyBool("has").value_or(true));
	ScriptEngine::shutdown();
	core::Log::invalidate();
}

TEST(LuaBindings, doorActivateTransitionsIdleToOpening) {
	core::Log::init(core::Log::Level::Off);
	const auto scn = mkShared<scene::Scene>();
	auto entity = scn->createEntity("Door");
	entity.addComponent<scene::component::RaycastDoor>();
	const auto uuid = static_cast<uint64_t>(entity.getUUID());
	const auto signedUuid = static_cast<int64_t>(uuid);

	ScriptEngine::init(scn.get());
	const std::string script = std::format("state = \"\"\n"
										   "function on_create()\n"
										   "  door.activate({})\n"
										   "  state = door.get_state({})\n"
										   "end\n",
										   signedUuid, signedUuid);
	const std::vector<uint8_t> data(script.begin(), script.end());
	const ScriptInstance inst;
	ASSERT_TRUE(inst.createFromBuffer(data, "door_test", uuid));
	inst.onCreate();
	EXPECT_EQ(entity.getComponent<scene::component::RaycastDoor>().state,
			  scene::component::RaycastDoor::State::Opening);
	const auto reported = inst.getPropertyString("state");
	ASSERT_TRUE(reported.has_value());
	EXPECT_EQ(reported.value(), "opening");
	ScriptEngine::shutdown();
	core::Log::invalidate();
}

TEST(LuaBindings, doorIsOpenAndCloseSwitchesState) {
	core::Log::init(core::Log::Level::Off);
	const auto scn = mkShared<scene::Scene>();
	auto entity = scn->createEntity("Door");
	auto& door = entity.addComponent<scene::component::RaycastDoor>();
	door.state = scene::component::RaycastDoor::State::Open;
	door.holdTimer = 10.f;
	door.currentOffset = 1.f;
	const auto uuid = static_cast<uint64_t>(entity.getUUID());
	const auto signedUuid = static_cast<int64_t>(uuid);

	ScriptEngine::init(scn.get());
	const std::string script = std::format("opened = false\n"
										   "function on_create()\n"
										   "  opened = door.is_open({})\n"
										   "  door.close({})\n"
										   "end\n",
										   signedUuid, signedUuid);
	const std::vector<uint8_t> data(script.begin(), script.end());
	const ScriptInstance inst;
	ASSERT_TRUE(inst.createFromBuffer(data, "door_close_test", uuid));
	inst.onCreate();
	EXPECT_TRUE(inst.getPropertyBool("opened").value_or(false));
	EXPECT_EQ(entity.getComponent<scene::component::RaycastDoor>().state,
			  scene::component::RaycastDoor::State::Closing);
	EXPECT_FLOAT_EQ(entity.getComponent<scene::component::RaycastDoor>().holdTimer, 0.f);
	ScriptEngine::shutdown();
	core::Log::invalidate();
}

TEST(LuaBindings, pushwallActivateAndHasMoved) {
	core::Log::init(core::Log::Level::Off);
	const auto scn = mkShared<scene::Scene>();
	auto entity = scn->createEntity("Push");
	entity.addComponent<scene::component::RaycastPushWall>();
	const auto uuid = static_cast<uint64_t>(entity.getUUID());
	const auto signedUuid = static_cast<int64_t>(uuid);

	ScriptEngine::init(scn.get());
	const std::string script = std::format("before = false\n"
										   "state = \"\"\n"
										   "function on_create()\n"
										   "  before = pushwall.has_moved({})\n"
										   "  pushwall.activate({})\n"
										   "  state = pushwall.get_state({})\n"
										   "end\n",
										   signedUuid, signedUuid, signedUuid);
	const std::vector<uint8_t> data(script.begin(), script.end());
	const ScriptInstance inst;
	ASSERT_TRUE(inst.createFromBuffer(data, "push_test", uuid));
	inst.onCreate();
	EXPECT_FALSE(inst.getPropertyBool("before").value_or(true));
	EXPECT_EQ(entity.getComponent<scene::component::RaycastPushWall>().state,
			  scene::component::RaycastPushWall::State::Moving);
	EXPECT_EQ(inst.getPropertyString("state").value_or(""), "moving");
	ScriptEngine::shutdown();
	core::Log::invalidate();
}

TEST(LuaBindings, doorPushwallApisHandleMissingEntity) {
	// Calling activate/close/get_state/is_open/has_moved with an unknown UUID must
	// not crash or mutate state — Lua errors silently fall back to default returns.
	core::Log::init(core::Log::Level::Off);
	const auto scn = mkShared<scene::Scene>();
	ScriptEngine::init(scn.get());
	const std::string script = "door.activate(0)\n"
							   "door.close(0)\n"
							   "open = door.is_open(0)\n"
							   "door_state = door.get_state(0)\n"
							   "pushwall.activate(0)\n"
							   "moved = pushwall.has_moved(0)\n"
							   "push_state = pushwall.get_state(0)\n";
	const std::vector<uint8_t> data(script.begin(), script.end());
	const ScriptInstance inst;
	ASSERT_TRUE(inst.createFromBuffer(data, "missing_test", 1));
	EXPECT_EQ(inst.getPropertyBool("open").value_or(true), false);
	EXPECT_EQ(inst.getPropertyBool("moved").value_or(true), false);
	EXPECT_EQ(inst.getPropertyString("door_state").value_or(""), "idle");
	EXPECT_EQ(inst.getPropertyString("push_state").value_or(""), "idle");
	ScriptEngine::shutdown();
	core::Log::invalidate();
}
