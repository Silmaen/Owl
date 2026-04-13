/**
 * @file LuaBindings.cpp
 * @author Silmaen
 * @date 09/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "LuaBindings.h"

#include "core/external/lua.h"
#include "input/Input.h"
#include "physic/PhysicCommand.h"
#include "scene/Entity.h"
#include "scene/Scene.h"
#include "scene/ScreenTransition.h"
#include "scene/component/components.h"
#include "script/ScriptEngine.h"
#include "sound/SoundCommand.h"
#include "sound/SoundSystem.h"

namespace owl::script {
namespace {

/// Helper: find entity by UUID in the active scene.
auto findEntity(lua_State* iState, const int iArgIndex) -> std::optional<scene::Entity> {
	auto* activeScene = ScriptEngine::getActiveScene();
	if (activeScene == nullptr)
		return std::nullopt;
	const auto uuid = static_cast<uint64_t>(luaL_checkinteger(iState, iArgIndex));
	if (auto entity = activeScene->findEntityByUUID(core::UUID{uuid}); entity)
		return entity;
	return std::nullopt;
}

// ============================================================
// transform table
// ============================================================

auto luaTransformGetPosition(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState, 1); entity && entity->hasComponent<scene::component::Transform>()) {
		const auto& t = entity->getComponent<scene::component::Transform>().transform;
		lua_pushnumber(iState, static_cast<lua_Number>(t.translation().x()));
		lua_pushnumber(iState, static_cast<lua_Number>(t.translation().y()));
		lua_pushnumber(iState, static_cast<lua_Number>(t.translation().z()));
		return 3;
	}
	lua_pushnumber(iState, 0);
	lua_pushnumber(iState, 0);
	lua_pushnumber(iState, 0);
	return 3;
}

auto luaTransformSetPosition(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState, 1); entity && entity->hasComponent<scene::component::Transform>()) {
		auto& t = entity->getComponent<scene::component::Transform>().transform;
		t.translation().x() = static_cast<float>(luaL_checknumber(iState, 2));
		t.translation().y() = static_cast<float>(luaL_checknumber(iState, 3));
		t.translation().z() = static_cast<float>(luaL_checknumber(iState, 4));
	}
	return 0;
}

auto luaTransformGetRotation(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState, 1); entity && entity->hasComponent<scene::component::Transform>()) {
		const auto& t = entity->getComponent<scene::component::Transform>().transform;
		lua_pushnumber(iState, static_cast<lua_Number>(t.rotation().x()));
		lua_pushnumber(iState, static_cast<lua_Number>(t.rotation().y()));
		lua_pushnumber(iState, static_cast<lua_Number>(t.rotation().z()));
		return 3;
	}
	lua_pushnumber(iState, 0);
	lua_pushnumber(iState, 0);
	lua_pushnumber(iState, 0);
	return 3;
}

auto luaTransformSetRotation(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState, 1); entity && entity->hasComponent<scene::component::Transform>()) {
		auto& t = entity->getComponent<scene::component::Transform>().transform;
		t.rotation().x() = static_cast<float>(luaL_checknumber(iState, 2));
		t.rotation().y() = static_cast<float>(luaL_checknumber(iState, 3));
		t.rotation().z() = static_cast<float>(luaL_checknumber(iState, 4));
	}
	return 0;
}

auto luaTransformGetScale(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState, 1); entity && entity->hasComponent<scene::component::Transform>()) {
		const auto& t = entity->getComponent<scene::component::Transform>().transform;
		lua_pushnumber(iState, static_cast<lua_Number>(t.scale().x()));
		lua_pushnumber(iState, static_cast<lua_Number>(t.scale().y()));
		lua_pushnumber(iState, static_cast<lua_Number>(t.scale().z()));
		return 3;
	}
	lua_pushnumber(iState, 1);
	lua_pushnumber(iState, 1);
	lua_pushnumber(iState, 1);
	return 3;
}

auto luaTransformSetScale(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState, 1); entity && entity->hasComponent<scene::component::Transform>()) {
		auto& t = entity->getComponent<scene::component::Transform>().transform;
		t.scale().x() = static_cast<float>(luaL_checknumber(iState, 2));
		t.scale().y() = static_cast<float>(luaL_checknumber(iState, 3));
		t.scale().z() = static_cast<float>(luaL_checknumber(iState, 4));
	}
	return 0;
}

// ============================================================
// physics table
// ============================================================

auto luaPhysicsImpulse(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState, 1)) {
		const auto fx = static_cast<float>(luaL_checknumber(iState, 2));
		const auto fy = static_cast<float>(luaL_checknumber(iState, 3));
		physic::PhysicCommand::impulse(*entity, {fx, fy});
	}
	return 0;
}

auto luaPhysicsGetVelocity(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState, 1)) {
		const auto vel = physic::PhysicCommand::getVelocity(*entity);
		lua_pushnumber(iState, static_cast<lua_Number>(vel.x()));
		lua_pushnumber(iState, static_cast<lua_Number>(vel.y()));
		return 2;
	}
	lua_pushnumber(iState, 0);
	lua_pushnumber(iState, 0);
	return 2;
}

auto luaPhysicsSetVelocity(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState, 1)) {
		const auto vx = static_cast<float>(luaL_checknumber(iState, 2));
		const auto vy = static_cast<float>(luaL_checknumber(iState, 3));
		physic::PhysicCommand::setVelocity(*entity, {vx, vy});
	}
	return 0;
}

auto luaPhysicsSetTransform(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState, 1)) {
		const auto px = static_cast<float>(luaL_checknumber(iState, 2));
		const auto py = static_cast<float>(luaL_checknumber(iState, 3));
		const auto rot = static_cast<float>(luaL_checknumber(iState, 4));
		physic::PhysicCommand::setTransform(*entity, {px, py}, rot);
	}
	return 0;
}

// ============================================================
// input table
// ============================================================

auto luaInputIsKeyPressed(lua_State* iState) -> int {
	const auto keyCode = static_cast<input::KeyCode>(luaL_checkinteger(iState, 1));
	lua_pushboolean(iState, input::Input::isKeyPressed(keyCode) ? 1 : 0);
	return 1;
}

auto luaInputIsMouseButtonPressed(lua_State* iState) -> int {
	const auto button = static_cast<input::MouseCode>(luaL_checkinteger(iState, 1));
	lua_pushboolean(iState, input::Input::isMouseButtonPressed(button) ? 1 : 0);
	return 1;
}

auto luaInputGetMouseX(lua_State* iState) -> int {
	lua_pushnumber(iState, static_cast<lua_Number>(input::Input::getMouseX()));
	return 1;
}

auto luaInputGetMouseY(lua_State* iState) -> int {
	lua_pushnumber(iState, static_cast<lua_Number>(input::Input::getMouseY()));
	return 1;
}

// ============================================================
// sound table
// ============================================================

auto luaSoundPlay(lua_State* iState) -> int {
	const char* assetPath = luaL_checkstring(iState, 1);
	if (sound::SoundSystem::getState() != sound::SoundSystem::State::Running) {
		lua_pushinteger(iState, static_cast<lua_Integer>(sound::invalidSoundHandle));
		return 1;
	}
	auto& library = sound::SoundSystem::getSoundLibrary();
	const auto data = library.get(assetPath);
	if (!data) {
		lua_pushinteger(iState, static_cast<lua_Integer>(sound::invalidSoundHandle));
		return 1;
	}
	const auto handle = sound::SoundCommand::play(data, sound::PlayParams{});
	lua_pushinteger(iState, static_cast<lua_Integer>(handle));
	return 1;
}

auto luaSoundStop(lua_State* iState) -> int {
	const auto handle = static_cast<sound::SoundHandle>(luaL_checkinteger(iState, 1));
	sound::SoundCommand::stop(handle);
	return 0;
}

auto luaSoundPause(lua_State* iState) -> int {
	const auto handle = static_cast<sound::SoundHandle>(luaL_checkinteger(iState, 1));
	sound::SoundCommand::pause(handle);
	return 0;
}

auto luaSoundResume(lua_State* iState) -> int {
	const auto handle = static_cast<sound::SoundHandle>(luaL_checkinteger(iState, 1));
	sound::SoundCommand::resume(handle);
	return 0;
}

auto luaSoundSetVolume(lua_State* iState) -> int {
	const auto handle = static_cast<sound::SoundHandle>(luaL_checkinteger(iState, 1));
	const auto vol = static_cast<float>(luaL_checknumber(iState, 2));
	sound::SoundCommand::setVolume(handle, vol);
	return 0;
}

// ============================================================
// scene table
// ============================================================

auto luaSceneFindEntity(lua_State* iState) -> int {
	auto* activeScene = ScriptEngine::getActiveScene();
	if (activeScene == nullptr) {
		lua_pushinteger(iState, 0);
		return 1;
	}
	const char* name = luaL_checkstring(iState, 1);
	for (const auto view = activeScene->registry.view<scene::component::Tag>(); const auto entity: view) {
		if (view.get<scene::component::Tag>(entity).tag == name) {
			const auto uuid = activeScene->registry.get<scene::component::ID>(entity).id;
			lua_pushinteger(iState, static_cast<lua_Integer>(static_cast<uint64_t>(uuid)));
			return 1;
		}
	}
	lua_pushinteger(iState, 0);
	return 1;
}

auto luaSceneCreateEntity(lua_State* iState) -> int {
	auto* activeScene = ScriptEngine::getActiveScene();
	if (activeScene == nullptr) {
		lua_pushinteger(iState, 0);
		return 1;
	}
	const char* name = luaL_checkstring(iState, 1);
	auto entity = activeScene->createEntity(name);
	lua_pushinteger(iState, static_cast<lua_Integer>(static_cast<uint64_t>(entity.getUUID())));
	return 1;
}

auto luaSceneDestroyEntity(lua_State* iState) -> int {
	auto* activeScene = ScriptEngine::getActiveScene();
	if (activeScene == nullptr)
		return 0;
	const auto uuid = static_cast<uint64_t>(luaL_checkinteger(iState, 1));
	if (auto entity = activeScene->findEntityByUUID(core::UUID{uuid}); entity)
		activeScene->destroyEntity(entity);
	return 0;
}

auto luaSceneLoadScene(lua_State* iState) -> int {
	auto* activeScene = ScriptEngine::getActiveScene();
	if (activeScene == nullptr)
		return 0;
	const char* levelName = luaL_checkstring(iState, 1);
	activeScene->teleportRequest.pending = true;
	activeScene->teleportRequest.levelName = levelName;
	activeScene->teleportRequest.targetName.clear();
	activeScene->teleportRequest.initialVelocity = {0.f, 0.f};
	activeScene->teleportRequest.rotationDelta = 0.f;
	return 0;
}

// ============================================================
// time table (delta stored per-frame by ScriptEngine)
// ============================================================

auto luaTimeDelta(lua_State* iState) -> int {
	// Delta time is stored in registry key "owl_dt".
	lua_getfield(iState, LUA_REGISTRYINDEX, "owl_dt");
	if (lua_isnumber(iState, -1) == 0) {
		lua_pop(iState, 1);
		lua_pushnumber(iState, 0);
	}
	return 1;
}

// ============================================================
// log table
// ============================================================

auto luaLogTrace(lua_State* iState) -> int {
	OWL_TRACE("{}", luaL_checkstring(iState, 1))
	return 0;
}

auto luaLogInfo(lua_State* iState) -> int {
	OWL_INFO("{}", luaL_checkstring(iState, 1))
	return 0;
}

auto luaLogWarn(lua_State* iState) -> int {
	OWL_WARN("{}", luaL_checkstring(iState, 1))
	return 0;
}

auto luaLogError(lua_State* iState) -> int {
	OWL_ERROR("{}", luaL_checkstring(iState, 1))
	return 0;
}

// ============================================================
// entity table
// ============================================================

auto luaEntityHasComponent(lua_State* iState) -> int {
	const auto entity = findEntity(iState, 1);
	if (!entity) {
		lua_pushboolean(iState, 0);
		return 1;
	}
	const std::string_view compName = luaL_checkstring(iState, 2);
	bool has = false;
	if (compName == "Transform")
		has = entity->hasComponent<scene::component::Transform>();
	else if (compName == "PhysicBody")
		has = entity->hasComponent<scene::component::PhysicBody>();
	else if (compName == "SpriteRenderer")
		has = entity->hasComponent<scene::component::SpriteRenderer>();
	else if (compName == "Camera")
		has = entity->hasComponent<scene::component::Camera>();
	else if (compName == "Text")
		has = entity->hasComponent<scene::component::Text>();
	else if (compName == "SoundSource")
		has = entity->hasComponent<scene::component::SoundSource>();
	else if (compName == "Canvas")
		has = entity->hasComponent<scene::component::Canvas>();
	else if (compName == "UIRect")
		has = entity->hasComponent<scene::component::UIRect>();
	else if (compName == "UIText")
		has = entity->hasComponent<scene::component::UIText>();
	else if (compName == "UIImage")
		has = entity->hasComponent<scene::component::UIImage>();
	else if (compName == "UIPanel")
		has = entity->hasComponent<scene::component::UIPanel>();
	else if (compName == "UIButton")
		has = entity->hasComponent<scene::component::UIButton>();
	else if (compName == "UISlider")
		has = entity->hasComponent<scene::component::UISlider>();
	else if (compName == "UIProgressBar")
		has = entity->hasComponent<scene::component::UIProgressBar>();
	lua_pushboolean(iState, has ? 1 : 0);
	return 1;
}

auto luaEntityGetName(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState, 1)) {
		lua_pushstring(iState, entity->getName().c_str());
		return 1;
	}
	lua_pushstring(iState, "");
	return 1;
}

// ============================================================
// ui table
// ============================================================

auto luaUiSetText(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState, 1); entity && entity->hasComponent<scene::component::UIText>())
		entity->getComponent<scene::component::UIText>().text = luaL_checkstring(iState, 2);
	return 0;
}

auto luaUiGetText(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState, 1); entity && entity->hasComponent<scene::component::UIText>()) {
		lua_pushstring(iState, entity->getComponent<scene::component::UIText>().text.c_str());
		return 1;
	}
	lua_pushstring(iState, "");
	return 1;
}

auto luaUiSetVisible(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState, 1); entity && entity->hasComponent<scene::component::Visibility>())
		entity->getComponent<scene::component::Visibility>().gameVisible = lua_toboolean(iState, 2) != 0;
	return 0;
}

auto luaUiSetProgress(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState, 1); entity && entity->hasComponent<scene::component::UIProgressBar>())
		entity->getComponent<scene::component::UIProgressBar>().value = static_cast<float>(luaL_checknumber(iState, 2));
	return 0;
}

auto luaUiGetSliderValue(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState, 1); entity && entity->hasComponent<scene::component::UISlider>()) {
		lua_pushnumber(iState, static_cast<lua_Number>(entity->getComponent<scene::component::UISlider>().value));
		return 1;
	}
	lua_pushnumber(iState, 0);
	return 1;
}

auto luaUiSetSliderValue(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState, 1); entity && entity->hasComponent<scene::component::UISlider>())
		entity->getComponent<scene::component::UISlider>().value = static_cast<float>(luaL_checknumber(iState, 2));
	return 0;
}

auto luaUiSetButtonEnabled(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState, 1); entity && entity->hasComponent<scene::component::UIButton>()) {
		auto& button = entity->getComponent<scene::component::UIButton>();
		button.state = lua_toboolean(iState, 2) != 0 ? scene::component::UIButton::State::Normal
													  : scene::component::UIButton::State::Disabled;
	}
	return 0;
}

auto luaUiTransitionFadeIn(lua_State* iState) -> int {
	const auto duration = static_cast<float>(luaL_checknumber(iState, 1));
	scene::ScreenTransition::start(scene::ScreenTransition::Type::FadeIn, duration);
	return 0;
}

auto luaUiTransitionFadeOut(lua_State* iState) -> int {
	const auto duration = static_cast<float>(luaL_checknumber(iState, 1));
	scene::ScreenTransition::start(scene::ScreenTransition::Type::FadeOut, duration);
	return 0;
}

auto luaUiIsTransitionActive(lua_State* iState) -> int {
	lua_pushboolean(iState, scene::ScreenTransition::isActive() ? 1 : 0);
	return 1;
}

/// Helper: register a table of C functions.
void registerTable(lua_State* iState, const char* iTableName, const luaL_Reg* iFunctions) {
	lua_newtable(iState);
	luaL_setfuncs(iState, iFunctions, 0);
	lua_setglobal(iState, iTableName);
}

}// namespace

void registerBindings(lua_State* iState) {
	OWL_PROFILE_FUNCTION()
	// clang-format off
	static const luaL_Reg transformFuncs[] = {
		{"get_position", luaTransformGetPosition},
		{"set_position", luaTransformSetPosition},
		{"get_rotation", luaTransformGetRotation},
		{"set_rotation", luaTransformSetRotation},
		{"get_scale", luaTransformGetScale},
		{"set_scale", luaTransformSetScale},
		{nullptr, nullptr}
	};
	static const luaL_Reg physicsFuncs[] = {
		{"impulse", luaPhysicsImpulse},
		{"get_velocity", luaPhysicsGetVelocity},
		{"set_velocity", luaPhysicsSetVelocity},
		{"set_transform", luaPhysicsSetTransform},
		{nullptr, nullptr}
	};
	static const luaL_Reg inputFuncs[] = {
		{"is_key_pressed", luaInputIsKeyPressed},
		{"is_mouse_button_pressed", luaInputIsMouseButtonPressed},
		{"get_mouse_x", luaInputGetMouseX},
		{"get_mouse_y", luaInputGetMouseY},
		{nullptr, nullptr}
	};
	static const luaL_Reg soundFuncs[] = {
		{"play", luaSoundPlay},
		{"stop", luaSoundStop},
		{"pause", luaSoundPause},
		{"resume", luaSoundResume},
		{"set_volume", luaSoundSetVolume},
		{nullptr, nullptr}
	};
	static const luaL_Reg sceneFuncs[] = {
		{"find_entity", luaSceneFindEntity},
		{"create_entity", luaSceneCreateEntity},
		{"destroy_entity", luaSceneDestroyEntity},
		{"load_scene", luaSceneLoadScene},
		{nullptr, nullptr}
	};
	static const luaL_Reg timeFuncs[] = {
		{"delta", luaTimeDelta},
		{nullptr, nullptr}
	};
	static const luaL_Reg logFuncs[] = {
		{"trace", luaLogTrace},
		{"info", luaLogInfo},
		{"warn", luaLogWarn},
		{"error", luaLogError},
		{nullptr, nullptr}
	};
	static const luaL_Reg entityFuncs[] = {
		{"has_component", luaEntityHasComponent},
		{"get_name", luaEntityGetName},
		{nullptr, nullptr}
	};
	static const luaL_Reg uiFuncs[] = {
		{"set_text", luaUiSetText},
		{"get_text", luaUiGetText},
		{"set_visible", luaUiSetVisible},
		{"set_progress", luaUiSetProgress},
		{"get_slider_value", luaUiGetSliderValue},
		{"set_slider_value", luaUiSetSliderValue},
		{"set_button_enabled", luaUiSetButtonEnabled},
		{"transition_fade_in", luaUiTransitionFadeIn},
		{"transition_fade_out", luaUiTransitionFadeOut},
		{"is_transition_active", luaUiIsTransitionActive},
		{nullptr, nullptr}
	};
	// clang-format on

	registerTable(iState, "transform", transformFuncs);
	registerTable(iState, "physics", physicsFuncs);
	registerTable(iState, "input", inputFuncs);
	registerTable(iState, "sound", soundFuncs);
	registerTable(iState, "scene", sceneFuncs);
	registerTable(iState, "time", timeFuncs);
	registerTable(iState, "log", logFuncs);
	registerTable(iState, "entity", entityFuncs);
	registerTable(iState, "ui", uiFuncs);
}

}// namespace owl::script
