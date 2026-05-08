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
#include "scene/SaveManager.h"
#include "scene/Scene.h"
#include "scene/ScreenTransition.h"
#include "scene/SettingsManager.h"
#include "scene/component/components.h"
#include "script/ScriptEngine.h"
#include "sound/SoundCommand.h"
#include "sound/SoundSystem.h"

namespace owl::script {

namespace {
/**
 * @brief
 *  Helper: find entity by UUID in the active scene (reads first Lua argument).
 */
auto findEntity(lua_State* iState) -> std::optional<scene::Entity> {
	const auto* activeScene = ScriptEngine::getActiveScene();
	if (activeScene == nullptr)
		return std::nullopt;
	const auto uuid = static_cast<uint64_t>(luaL_checkinteger(iState, 1));
	if (auto entity = activeScene->findEntityByUUID(core::UUID{uuid}); entity)
		return entity;
	return std::nullopt;
}

auto parseTransitionType(const std::string_view iName) -> scene::ScreenTransition::Type {
	if (iName == "fade_in" || iName == "fade")
		return scene::ScreenTransition::Type::FadeIn;
	if (iName == "fade_out")
		return scene::ScreenTransition::Type::FadeOut;
	if (iName == "wipe_left")
		return scene::ScreenTransition::Type::WipeLeft;
	if (iName == "wipe_right")
		return scene::ScreenTransition::Type::WipeRight;
	if (iName == "wipe_up")
		return scene::ScreenTransition::Type::WipeUp;
	if (iName == "wipe_down")
		return scene::ScreenTransition::Type::WipeDown;
	return scene::ScreenTransition::Type::None;
}

// ============================================================
// transform table
// ============================================================
auto luaTransformGetPosition(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState); entity && entity->hasComponent<scene::component::Transform>()) {
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
	if (const auto entity = findEntity(iState); entity && entity->hasComponent<scene::component::Transform>()) {
		auto& t = entity->getComponent<scene::component::Transform>().transform;
		t.translation().x() = static_cast<float>(luaL_checknumber(iState, 2));
		t.translation().y() = static_cast<float>(luaL_checknumber(iState, 3));
		t.translation().z() = static_cast<float>(luaL_checknumber(iState, 4));
	}
	return 0;
}

auto luaTransformGetRotation(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState); entity && entity->hasComponent<scene::component::Transform>()) {
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
	if (const auto entity = findEntity(iState); entity && entity->hasComponent<scene::component::Transform>()) {
		auto& t = entity->getComponent<scene::component::Transform>().transform;
		t.rotation().x() = static_cast<float>(luaL_checknumber(iState, 2));
		t.rotation().y() = static_cast<float>(luaL_checknumber(iState, 3));
		t.rotation().z() = static_cast<float>(luaL_checknumber(iState, 4));
	}
	return 0;
}

auto luaTransformGetScale(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState); entity && entity->hasComponent<scene::component::Transform>()) {
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
	if (const auto entity = findEntity(iState); entity && entity->hasComponent<scene::component::Transform>()) {
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
	if (const auto entity = findEntity(iState)) {
		const auto fx = static_cast<float>(luaL_checknumber(iState, 2));
		const auto fy = static_cast<float>(luaL_checknumber(iState, 3));
		physic::PhysicCommand::impulse(*entity, {fx, fy});
	}
	return 0;
}

auto luaPhysicsGetVelocity(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState)) {
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
	if (const auto entity = findEntity(iState)) {
		const auto vx = static_cast<float>(luaL_checknumber(iState, 2));
		const auto vy = static_cast<float>(luaL_checknumber(iState, 3));
		physic::PhysicCommand::setVelocity(*entity, {vx, vy});
	}
	return 0;
}

auto luaPhysicsSetTransform(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState)) {
		const auto px = static_cast<float>(luaL_checknumber(iState, 2));
		const auto py = static_cast<float>(luaL_checknumber(iState, 3));
		const auto rot = static_cast<float>(luaL_checknumber(iState, 4));
		physic::PhysicCommand::setTransform(*entity, {px, py}, rot);
	}
	return 0;
}

auto luaPhysicsSetGravityScale(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState)) {
		const auto scale = static_cast<float>(luaL_checknumber(iState, 2));
		physic::PhysicCommand::setGravityScale(*entity, scale);
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
	// Auto-load the sound if not already in the library.
	const auto data = library.exists(assetPath) ? library.get(assetPath) : library.load(assetPath);
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
	const auto entity = activeScene->createEntity(name);
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

auto luaSceneTransitionTo(lua_State* iState) -> int {
	const char* path = luaL_checkstring(iState, 1);
	scene::ScreenTransition::SceneLoadRequest req;
	req.scenePath = path;
	const int top = lua_gettop(iState);
	std::string typeName{"fade"};
	if (top >= 2) {
		// Argument 2 may be either a type-name string or the duration when the
		// caller skips the type. Support both for ergonomic Lua.
		if (lua_isstring(iState, 2) != 0)
			typeName = lua_tostring(iState, 2);
		else if (lua_isnumber(iState, 2) != 0)
			req.outDuration = req.inDuration = static_cast<float>(lua_tonumber(iState, 2));
	}
	if (top >= 3 && lua_isnumber(iState, 3) != 0) {
		req.outDuration = req.inDuration = static_cast<float>(lua_tonumber(iState, 3));
	}
	req.outType = parseTransitionType(typeName);
	if (req.outType == scene::ScreenTransition::Type::None)
		req.outType = scene::ScreenTransition::Type::FadeOut;
	// In-anim mirrors the out-anim by default — fades pair with their reverse,
	// wipes pair with the matching direction (slides off, slides on).
	switch (req.outType) {
		case scene::ScreenTransition::Type::FadeOut:
			req.inType = scene::ScreenTransition::Type::FadeIn;
			break;
		case scene::ScreenTransition::Type::FadeIn:
			req.inType = scene::ScreenTransition::Type::FadeOut;
			break;
		case scene::ScreenTransition::Type::WipeLeft:
		case scene::ScreenTransition::Type::WipeRight:
		case scene::ScreenTransition::Type::WipeUp:
		case scene::ScreenTransition::Type::WipeDown:
			// Same direction on both ends — bar slides off, slides back on.
			req.inType = req.outType;
			break;
		case scene::ScreenTransition::Type::None:
			req.inType = scene::ScreenTransition::Type::FadeIn;
			break;
	}
	scene::ScreenTransition::requestSceneLoad(req);
	return 0;
}

auto luaSceneQuit([[maybe_unused]] lua_State* iState) -> int {
	auto* activeScene = ScriptEngine::getActiveScene();
	if (activeScene != nullptr)
		activeScene->quitRequested = true;
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
	const auto entity = findEntity(iState);
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
	else if (compName == "UiRect")
		has = entity->hasComponent<scene::component::UiRect>();
	else if (compName == "UiText")
		has = entity->hasComponent<scene::component::UiText>();
	else if (compName == "UiImage")
		has = entity->hasComponent<scene::component::UiImage>();
	else if (compName == "UiPanel")
		has = entity->hasComponent<scene::component::UiPanel>();
	else if (compName == "UiButton")
		has = entity->hasComponent<scene::component::UiButton>();
	else if (compName == "UiSlider")
		has = entity->hasComponent<scene::component::UiSlider>();
	else if (compName == "UiProgressBar")
		has = entity->hasComponent<scene::component::UiProgressBar>();
	lua_pushboolean(iState, has ? 1 : 0);
	return 1;
}

auto luaEntityGetName(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState)) {
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
	if (const auto entity = findEntity(iState); entity && entity->hasComponent<scene::component::UiText>())
		entity->getComponent<scene::component::UiText>().text = luaL_checkstring(iState, 2);
	return 0;
}

auto luaUiGetText(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState); entity && entity->hasComponent<scene::component::UiText>()) {
		lua_pushstring(iState, entity->getComponent<scene::component::UiText>().text.c_str());
		return 1;
	}
	lua_pushstring(iState, "");
	return 1;
}

auto luaUiSetVisible(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState); entity && entity->hasComponent<scene::component::Visibility>())
		entity->getComponent<scene::component::Visibility>().gameVisible = lua_toboolean(iState, 2) != 0;
	return 0;
}

auto luaUiSetProgress(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState); entity && entity->hasComponent<scene::component::UiProgressBar>())
		entity->getComponent<scene::component::UiProgressBar>().value = static_cast<float>(luaL_checknumber(iState, 2));
	return 0;
}

auto luaUiGetSliderValue(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState); entity && entity->hasComponent<scene::component::UiSlider>()) {
		lua_pushnumber(iState, static_cast<lua_Number>(entity->getComponent<scene::component::UiSlider>().value));
		return 1;
	}
	lua_pushnumber(iState, 0);
	return 1;
}

auto luaUiSetSliderValue(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState); entity && entity->hasComponent<scene::component::UiSlider>())
		entity->getComponent<scene::component::UiSlider>().value = static_cast<float>(luaL_checknumber(iState, 2));
	return 0;
}

auto luaUiSetButtonEnabled(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState); entity && entity->hasComponent<scene::component::UiButton>()) {
		auto& button = entity->getComponent<scene::component::UiButton>();
		button.state = lua_toboolean(iState, 2) != 0 ? scene::component::UiButton::State::Normal
													 : scene::component::UiButton::State::Disabled;
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

auto luaUiTransitionPlay(lua_State* iState) -> int {
	const std::string_view typeName = luaL_checkstring(iState, 1);
	const auto duration = static_cast<float>(luaL_checknumber(iState, 2));
	math::vec4 color{0.f, 0.f, 0.f, 1.f};
	const int top = lua_gettop(iState);
	if (top >= 6) {
		color = math::vec4{
				static_cast<float>(luaL_checknumber(iState, 3)), static_cast<float>(luaL_checknumber(iState, 4)),
				static_cast<float>(luaL_checknumber(iState, 5)), static_cast<float>(luaL_checknumber(iState, 6))};
	} else if (top >= 5) {
		color = math::vec4{static_cast<float>(luaL_checknumber(iState, 3)),
						   static_cast<float>(luaL_checknumber(iState, 4)),
						   static_cast<float>(luaL_checknumber(iState, 5)), 1.f};
	}
	const auto type = parseTransitionType(typeName);
	if (type == scene::ScreenTransition::Type::None) {
		OWL_CORE_WARN("Lua ui.transition_play: unknown transition type '{}'.", typeName)
		return 0;
	}
	scene::ScreenTransition::play(type, duration, color);
	return 0;
}

// ============================================================
// gamestate table
// ============================================================
auto luaGamestateSet(lua_State* iState) -> int {
	auto* activeScene = ScriptEngine::getActiveScene();
	if (activeScene == nullptr)
		return 0;
	const char* key = luaL_checkstring(iState, 1);
	if (lua_isboolean(iState, 2) != 0)
		activeScene->getGameState().set(key, lua_toboolean(iState, 2) != 0);
	else if (lua_isinteger(iState, 2) != 0)
		activeScene->getGameState().set(key, static_cast<int64_t>(lua_tointeger(iState, 2)));
	else if (lua_isnumber(iState, 2) != 0)
		activeScene->getGameState().set(key, static_cast<float>(lua_tonumber(iState, 2)));
	else if (lua_isstring(iState, 2) != 0)
		activeScene->getGameState().set(key, std::string(lua_tostring(iState, 2)));
	return 0;
}

auto luaGamestateGet(lua_State* iState) -> int {
	auto* activeScene = ScriptEngine::getActiveScene();
	if (activeScene == nullptr) {
		lua_pushnil(iState);
		return 1;
	}
	const char* key = luaL_checkstring(iState, 1);
	const auto val = activeScene->getGameState().get(key);
	if (!val.has_value()) {
		// Return default (arg 2) or nil.
		if (lua_gettop(iState) >= 2) {
			lua_pushvalue(iState, 2);
		} else {
			lua_pushnil(iState);
		}
		return 1;
	}
	std::visit(
			[iState]<typename T0>(const T0& iVal) -> void {
				using T = std::decay_t<T0>;
				if constexpr (std::is_same_v<T, int64_t>)
					lua_pushinteger(iState, static_cast<lua_Integer>(iVal));
				else if constexpr (std::is_same_v<T, float>)
					lua_pushnumber(iState, static_cast<lua_Number>(iVal));
				else if constexpr (std::is_same_v<T, std::string>)
					lua_pushstring(iState, iVal.c_str());
				else if constexpr (std::is_same_v<T, bool>)
					lua_pushboolean(iState, iVal ? 1 : 0);
			},
			val.value());
	return 1;
}

auto luaGamestateRemove(lua_State* iState) -> int {
	auto* activeScene = ScriptEngine::getActiveScene();
	if (activeScene != nullptr)
		activeScene->getGameState().remove(luaL_checkstring(iState, 1));
	return 0;
}

auto luaGamestateClear([[maybe_unused]] lua_State* iState) -> int {// NOLINT(readability-non-const-parameter)
	auto* activeScene = ScriptEngine::getActiveScene();
	if (activeScene != nullptr)
		activeScene->getGameState().clear();
	return 0;
}

// ============================================================
// save table
// ============================================================
auto luaSaveSaveGame(lua_State* iState) -> int {
	auto* activeScene = ScriptEngine::getActiveScene();
	if (activeScene == nullptr)
		return 0;
	const auto slot = static_cast<uint32_t>(luaL_checkinteger(iState, 1));
	// Deferred save — handled by RunnerLayer/EditorLayer after onUpdateRuntime.
	activeScene->saveLoadRequest.pending = true;
	activeScene->saveLoadRequest.isLoad = false;
	activeScene->saveLoadRequest.slot = slot;
	return 0;
}

auto luaSaveLoadGame(lua_State* iState) -> int {
	auto* activeScene = ScriptEngine::getActiveScene();
	if (activeScene == nullptr)
		return 0;
	const auto slot = static_cast<uint32_t>(luaL_checkinteger(iState, 1));
	activeScene->saveLoadRequest.pending = true;
	activeScene->saveLoadRequest.isLoad = true;
	activeScene->saveLoadRequest.slot = slot;
	return 0;
}

auto luaSaveHasSave(lua_State* iState) -> int {
	const auto slot = static_cast<uint32_t>(luaL_checkinteger(iState, 1));
	lua_pushboolean(iState, scene::SaveManager::hasSave(slot) ? 1 : 0);
	return 1;
}

auto luaSaveDeleteSave(lua_State* iState) -> int {
	const auto slot = static_cast<uint32_t>(luaL_checkinteger(iState, 1));
	scene::SaveManager::deleteSave(slot);
	return 0;
}

auto luaSaveListSaves(lua_State* iState) -> int {
	const auto saves = scene::SaveManager::listSaves();
	lua_newtable(iState);
	for (size_t i = 0; i < saves.size(); ++i) {
		lua_newtable(iState);
		lua_pushinteger(iState, static_cast<lua_Integer>(saves[i].slot));
		lua_setfield(iState, -2, "slot");
		lua_pushstring(iState, saves[i].timestamp.c_str());
		lua_setfield(iState, -2, "timestamp");
		lua_pushstring(iState, saves[i].scenePath.c_str());
		lua_setfield(iState, -2, "scene");
		lua_rawseti(iState, -2, static_cast<lua_Integer>(i) + 1);
	}
	return 1;
}

/**
 * @brief
 *  Helper: register a table of C functions.
 */
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
		{"set_gravity_scale", luaPhysicsSetGravityScale},
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
		{"transition_to", luaSceneTransitionTo},
		{"quit", luaSceneQuit},
		{nullptr, nullptr}
	};
	static  constexpr luaL_Reg timeFuncs[] = {
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
	static  constexpr luaL_Reg entityFuncs[] = {
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
		{"transition_play", luaUiTransitionPlay},
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

	// clang-format off
	static const luaL_Reg gamestateFuncs[] = {
		{"set", luaGamestateSet},
		{"get", luaGamestateGet},
		{"remove", luaGamestateRemove},
		{"clear", luaGamestateClear},
		{nullptr, nullptr}
	};
	static const luaL_Reg saveFuncs[] = {
		{"save_game", luaSaveSaveGame},
		{"load_game", luaSaveLoadGame},
		{"has_save", luaSaveHasSave},
		{"delete_save", luaSaveDeleteSave},
		{"list_saves", luaSaveListSaves},
		{nullptr, nullptr}
	};
	// clang-format on
	// clang-format off
	static const luaL_Reg settingsFuncs[] = {
		{"set", [](lua_State* s) -> int {
			const char* key = luaL_checkstring(s, 1);
			if (lua_isboolean(s, 2) != 0)

				scene::SettingsManager::set(key, lua_toboolean(s, 2) != 0);
			else if (lua_isinteger(s, 2) != 0)

				scene::SettingsManager::set(key, static_cast<int64_t>(lua_tointeger(s, 2)));
			else if (lua_isnumber(s, 2) != 0)

				scene::SettingsManager::set(key, static_cast<float>(lua_tonumber(s, 2)));
			else if (lua_isstring(s, 2) != 0)

				scene::SettingsManager::set(key, std::string(lua_tostring(s, 2)));
			return 0;
		}},
		{"get", [](lua_State* s) -> int {
			const char* key = luaL_checkstring(s, 1);
			const auto val = scene::SettingsManager::get(key);
			if (!val.has_value()) {
				if (lua_gettop(s) >= 2)

					lua_pushvalue(s, 2);
				else

					lua_pushnil(s);
				return 1;
			}

			std::visit([s]<typename T0>(const T0& v) -> void {
				using T = std::decay_t<T0>;
				if constexpr (std::is_same_v<T, int64_t>)
					lua_pushinteger(s, static_cast<lua_Integer>(v));
				else if constexpr (std::is_same_v<T, float>)
					lua_pushnumber(s, static_cast<lua_Number>(v));
				else if constexpr (std::is_same_v<T, std::string>)
					lua_pushstring(s, v.c_str());
				else if constexpr (std::is_same_v<T, bool>)
					lua_pushboolean(s, v ? 1 : 0);
			}, val.value());
			return 1;
		}},
		{"save", [](lua_State*) -> int { scene::SettingsManager::saveUserSettings(); return 0; }},
		{"load", [](lua_State*) -> int { scene::SettingsManager::loadUserSettings(); return 0; }},
		{"reset", [](lua_State* s) -> int { scene::SettingsManager::resetToDefault(luaL_checkstring(s, 1)); return 0; }},
		{"reset_all", [](lua_State*) -> int { scene::SettingsManager::resetAllToDefaults(); return 0; }},
		{"apply", [](lua_State*) -> int { scene::SettingsManager::applyBuiltins(); return 0; }},
		{nullptr, nullptr}
	};
	// clang-format on
	// clang-format off
	static  constexpr luaL_Reg triggerFuncs[] = {
		{"start_timer", [](lua_State* s) -> int {
			const auto* activeScene = ScriptEngine::getActiveScene();
			if (activeScene == nullptr) return 0;
			const auto uid = static_cast<uint64_t>(luaL_checkinteger(s, 1));
			if (const auto entity = activeScene->findEntityByUUID(core::UUID{uid});
				entity && entity.hasComponent<scene::component::Trigger>())
				entity.getComponent<scene::component::Trigger>().trigger.startTimer();
			return 0;
		}},
		{"stop_timer", [](lua_State* s) -> int {
			const auto* activeScene = ScriptEngine::getActiveScene();
			if (activeScene == nullptr) return 0;
			const auto uid = static_cast<uint64_t>(luaL_checkinteger(s, 1));
			if (const auto entity = activeScene->findEntityByUUID(core::UUID{uid});
				entity && entity.hasComponent<scene::component::Trigger>())
				entity.getComponent<scene::component::Trigger>().trigger.stopTimer();
			return 0;
		}},
		{"reset_timer", [](lua_State* s) -> int {
			const auto* activeScene = ScriptEngine::getActiveScene();
			if (activeScene == nullptr) return 0;
			const auto uid = static_cast<uint64_t>(luaL_checkinteger(s, 1));
			if (const auto entity = activeScene->findEntityByUUID(core::UUID{uid});
				entity && entity.hasComponent<scene::component::Trigger>())
				entity.getComponent<scene::component::Trigger>().trigger.resetTimer();
			return 0;
		}},
		{nullptr, nullptr}
	};

	// clang-format on
	registerTable(iState, "gamestate", gamestateFuncs);

	registerTable(iState, "save", saveFuncs);

	registerTable(iState, "settings", settingsFuncs);

	registerTable(iState, "trigger", triggerFuncs);
}

}// namespace owl::script
