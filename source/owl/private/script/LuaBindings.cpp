/**
 * @file LuaBindings.cpp
 * @author Silmaen
 * @date 09/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "LuaBindings.h"

#include "app/EngineContext.h"
#include "core/Macros.h"
#include "core/external/lua.h"
#include "input/Input.h"
#include "physics/PhysicCommand.h"
#include "scene/Entity.h"
#include "scene/SaveManager.h"
#include "scene/Scene.h"
#include "scene/ScreenTransition.h"
#include "scene/SettingsManager.h"
#include "scene/component/components.h"
#include "script/LuaEngine.h"
#include "script/ScriptEngine.h"
#include "sound/SoundCommand.h"
#include "sound/SoundSystem.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <format>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace owl::script {

namespace {
// A light C function per binding: unlike a closure over the binding, it allocates nothing in each Lua state.
template<LuaFunction Function>
auto guarded(lua_State* iState) -> int {
	return LuaEngine::callGuarded(iState, Function);
}

auto contextOf(lua_State* iState) -> app::EngineContext* {
	const auto* boundScene = getBoundScene(iState);
	return boundScene != nullptr ? boundScene->getEngineContext() : nullptr;
}

auto transitionOf(lua_State* iState) -> scene::ScreenTransition* {
	auto* context = contextOf(iState);
	return context != nullptr ? &context->getScreenTransition() : nullptr;
}

auto settingsOf(lua_State* iState) -> scene::SettingsManager* {
	auto* context = contextOf(iState);
	return context != nullptr ? &context->getSettings() : nullptr;
}

auto findEntity(lua_State* iState) -> std::optional<scene::Entity> {
	const auto* activeScene = getBoundScene(iState);
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

auto luaPhysicsImpulse(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState)) {
		const auto fx = static_cast<float>(luaL_checknumber(iState, 2));
		const auto fy = static_cast<float>(luaL_checknumber(iState, 3));
		physics::PhysicCommand::impulse(*entity, {fx, fy});
	}
	return 0;
}

auto luaPhysicsGetVelocity(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState)) {
		const auto vel = physics::PhysicCommand::getVelocity(*entity);
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
		physics::PhysicCommand::setVelocity(*entity, {vx, vy});
	}
	return 0;
}

auto luaPhysicsSetTransform(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState)) {
		const auto px = static_cast<float>(luaL_checknumber(iState, 2));
		const auto py = static_cast<float>(luaL_checknumber(iState, 3));
		const auto rot = static_cast<float>(luaL_checknumber(iState, 4));
		physics::PhysicCommand::setTransform(*entity, {px, py}, rot);
	}
	return 0;
}

auto luaPhysicsSetGravityScale(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState)) {
		const auto scale = static_cast<float>(luaL_checknumber(iState, 2));
		physics::PhysicCommand::setGravityScale(*entity, scale);
	}
	return 0;
}

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

auto luaSceneFindEntity(lua_State* iState) -> int {
	auto* activeScene = getBoundScene(iState);
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
	auto* activeScene = getBoundScene(iState);
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
	auto* activeScene = getBoundScene(iState);
	if (activeScene == nullptr)
		return 0;
	const auto uuid = static_cast<uint64_t>(luaL_checkinteger(iState, 1));
	activeScene->destroyEntityDeferred(activeScene->findEntityByUUID(core::UUID{uuid}));
	return 0;
}

auto luaSceneLoadScene(lua_State* iState) -> int {
	auto* activeScene = getBoundScene(iState);
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
	if (auto* transition = transitionOf(iState); transition != nullptr)
		transition->requestSceneLoad(req);
	return 0;
}

auto luaSceneQuit([[maybe_unused]] lua_State* iState) -> int {
	auto* activeScene = getBoundScene(iState);
	if (activeScene != nullptr)
		activeScene->quitRequested = true;
	return 0;
}

auto luaTimeDelta(lua_State* iState) -> int {
	lua_pushnumber(iState, static_cast<lua_Number>(LuaEngine::getDeltaTime(iState)));
	return 1;
}

auto luaLogTrace(lua_State* iState) -> int {
	// Checked outside the macro: a disabled level evaluates none of its arguments.
	const char* message = luaL_checkstring(iState, 1);
	OWL_TRACE("{}", message)
	return 0;
}

auto luaLogInfo(lua_State* iState) -> int {
	const char* message = luaL_checkstring(iState, 1);
	OWL_INFO("{}", message)
	return 0;
}

auto luaLogWarn(lua_State* iState) -> int {
	const char* message = luaL_checkstring(iState, 1);
	OWL_WARN("{}", message)
	return 0;
}

auto luaLogError(lua_State* iState) -> int {
	const char* message = luaL_checkstring(iState, 1);
	OWL_ERROR("{}", message)
	return 0;
}

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
	if (auto* transition = transitionOf(iState); transition != nullptr)
		transition->start(scene::ScreenTransition::Type::FadeIn, duration);
	return 0;
}

auto luaUiTransitionFadeOut(lua_State* iState) -> int {
	const auto duration = static_cast<float>(luaL_checknumber(iState, 1));
	if (auto* transition = transitionOf(iState); transition != nullptr)
		transition->start(scene::ScreenTransition::Type::FadeOut, duration);
	return 0;
}

auto luaUiIsTransitionActive(lua_State* iState) -> int {
	const auto* transition = transitionOf(iState);
	lua_pushboolean(iState, transition != nullptr && transition->isActive() ? 1 : 0);
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
	if (auto* transition = transitionOf(iState); transition != nullptr)
		transition->play(type, duration, color);
	return 0;
}

auto luaGamestateSet(lua_State* iState) -> int {
	auto* activeScene = getBoundScene(iState);
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
	auto* activeScene = getBoundScene(iState);
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
	auto* activeScene = getBoundScene(iState);
	if (activeScene != nullptr)
		activeScene->getGameState().remove(luaL_checkstring(iState, 1));
	return 0;
}

auto luaGamestateClear([[maybe_unused]] lua_State* iState) -> int {// NOLINT(readability-non-const-parameter)
	auto* activeScene = getBoundScene(iState);
	if (activeScene != nullptr)
		activeScene->getGameState().clear();
	return 0;
}

auto luaSaveSaveGame(lua_State* iState) -> int {
	auto* activeScene = getBoundScene(iState);
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
	auto* activeScene = getBoundScene(iState);
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

auto luaDoorActivate(lua_State* iState) -> int {
	const auto* activeScene = getBoundScene(iState);
	if (activeScene == nullptr)
		return 0;
	const auto uid = static_cast<uint64_t>(luaL_checkinteger(iState, 1));
	const auto entity = activeScene->findEntityByUUID(core::UUID{uid});
	if (!entity || !entity.hasComponent<scene::component::RaycastDoor>())
		return 0;
	auto& door = entity.getComponent<scene::component::RaycastDoor>();
	if (door.state == scene::component::RaycastDoor::State::Idle)
		door.state = scene::component::RaycastDoor::State::Opening;
	return 0;
}

auto luaDoorClose(lua_State* iState) -> int {
	const auto* activeScene = getBoundScene(iState);
	if (activeScene == nullptr)
		return 0;
	const auto uid = static_cast<uint64_t>(luaL_checkinteger(iState, 1));
	const auto entity = activeScene->findEntityByUUID(core::UUID{uid});
	if (!entity || !entity.hasComponent<scene::component::RaycastDoor>())
		return 0;
	auto& door = entity.getComponent<scene::component::RaycastDoor>();
	if (door.state == scene::component::RaycastDoor::State::Open ||
		door.state == scene::component::RaycastDoor::State::Opening) {
		door.state = scene::component::RaycastDoor::State::Closing;
		door.holdTimer = 0.f;
	}
	return 0;
}

auto luaDoorIsOpen(lua_State* iState) -> int {
	const auto* activeScene = getBoundScene(iState);
	if (activeScene == nullptr) {
		lua_pushboolean(iState, 0);
		return 1;
	}
	const auto uid = static_cast<uint64_t>(luaL_checkinteger(iState, 1));
	const auto entity = activeScene->findEntityByUUID(core::UUID{uid});
	if (!entity || !entity.hasComponent<scene::component::RaycastDoor>()) {
		lua_pushboolean(iState, 0);
		return 1;
	}
	const auto& door = entity.getComponent<scene::component::RaycastDoor>();
	lua_pushboolean(iState, door.state == scene::component::RaycastDoor::State::Open ? 1 : 0);
	return 1;
}

auto luaDoorGetState(lua_State* iState) -> int {
	const auto* activeScene = getBoundScene(iState);
	const char* defaultState = "idle";
	if (activeScene == nullptr) {
		lua_pushstring(iState, defaultState);
		return 1;
	}
	const auto uid = static_cast<uint64_t>(luaL_checkinteger(iState, 1));
	const auto entity = activeScene->findEntityByUUID(core::UUID{uid});
	if (!entity || !entity.hasComponent<scene::component::RaycastDoor>()) {
		lua_pushstring(iState, defaultState);
		return 1;
	}
	const auto& door = entity.getComponent<scene::component::RaycastDoor>();
	switch (door.state) {
		case scene::component::RaycastDoor::State::Idle:
			lua_pushstring(iState, "idle");
			break;
		case scene::component::RaycastDoor::State::Opening:
			lua_pushstring(iState, "opening");
			break;
		case scene::component::RaycastDoor::State::Open:
			lua_pushstring(iState, "open");
			break;
		case scene::component::RaycastDoor::State::Closing:
			lua_pushstring(iState, "closing");
			break;
	}
	return 1;
}

auto luaPushwallActivate(lua_State* iState) -> int {
	const auto* activeScene = getBoundScene(iState);
	if (activeScene == nullptr)
		return 0;
	const auto uid = static_cast<uint64_t>(luaL_checkinteger(iState, 1));
	const auto entity = activeScene->findEntityByUUID(core::UUID{uid});
	if (!entity || !entity.hasComponent<scene::component::RaycastPushWall>())
		return 0;
	auto& push = entity.getComponent<scene::component::RaycastPushWall>();
	if (push.state == scene::component::RaycastPushWall::State::Idle)
		push.state = scene::component::RaycastPushWall::State::Moving;
	return 0;
}

auto luaPushwallHasMoved(lua_State* iState) -> int {
	const auto* activeScene = getBoundScene(iState);
	if (activeScene == nullptr) {
		lua_pushboolean(iState, 0);
		return 1;
	}
	const auto uid = static_cast<uint64_t>(luaL_checkinteger(iState, 1));
	const auto entity = activeScene->findEntityByUUID(core::UUID{uid});
	if (!entity || !entity.hasComponent<scene::component::RaycastPushWall>()) {
		lua_pushboolean(iState, 0);
		return 1;
	}
	const auto& push = entity.getComponent<scene::component::RaycastPushWall>();
	lua_pushboolean(iState, push.state == scene::component::RaycastPushWall::State::Final ? 1 : 0);
	return 1;
}

auto luaPushwallGetState(lua_State* iState) -> int {
	const auto* activeScene = getBoundScene(iState);
	const char* defaultState = "idle";
	if (activeScene == nullptr) {
		lua_pushstring(iState, defaultState);
		return 1;
	}
	const auto uid = static_cast<uint64_t>(luaL_checkinteger(iState, 1));
	const auto entity = activeScene->findEntityByUUID(core::UUID{uid});
	if (!entity || !entity.hasComponent<scene::component::RaycastPushWall>()) {
		lua_pushstring(iState, defaultState);
		return 1;
	}
	const auto& push = entity.getComponent<scene::component::RaycastPushWall>();
	switch (push.state) {
		case scene::component::RaycastPushWall::State::Idle:
			lua_pushstring(iState, "idle");
			break;
		case scene::component::RaycastPushWall::State::Moving:
			lua_pushstring(iState, "moving");
			break;
		case scene::component::RaycastPushWall::State::Final:
			lua_pushstring(iState, "final");
			break;
	}
	return 1;
}

auto luaSettingsSet(lua_State* iState) -> int {
	const char* key = luaL_checkstring(iState, 1);
	auto* settings = settingsOf(iState);
	if (settings == nullptr)
		return 0;
	if (lua_isboolean(iState, 2) != 0)
		settings->set(key, lua_toboolean(iState, 2) != 0);
	else if (lua_isinteger(iState, 2) != 0)
		settings->set(key, static_cast<int64_t>(lua_tointeger(iState, 2)));
	else if (lua_isnumber(iState, 2) != 0)
		settings->set(key, static_cast<float>(lua_tonumber(iState, 2)));
	else if (lua_isstring(iState, 2) != 0)
		settings->set(key, std::string(lua_tostring(iState, 2)));
	return 0;
}

auto luaSettingsGet(lua_State* iState) -> int {
	const char* key = luaL_checkstring(iState, 1);
	const auto* settings = settingsOf(iState);
	const auto val = settings != nullptr ? settings->get(key) : std::nullopt;
	if (!val.has_value()) {
		if (lua_gettop(iState) >= 2)
			lua_pushvalue(iState, 2);
		else
			lua_pushnil(iState);
		return 1;
	}
	std::visit(
			[iState]<typename T0>(const T0& iValue) -> void {
				using T = std::decay_t<T0>;
				if constexpr (std::is_same_v<T, int64_t>)
					lua_pushinteger(iState, static_cast<lua_Integer>(iValue));
				else if constexpr (std::is_same_v<T, float>)
					lua_pushnumber(iState, static_cast<lua_Number>(iValue));
				else if constexpr (std::is_same_v<T, std::string>)
					lua_pushstring(iState, iValue.c_str());
				else if constexpr (std::is_same_v<T, bool>)
					lua_pushboolean(iState, iValue ? 1 : 0);
			},
			val.value());
	return 1;
}

auto luaSettingsSave(lua_State* iState) -> int {
	const auto* settings = settingsOf(iState);
	lua_pushboolean(iState, settings != nullptr && settings->saveUserSettings() ? 1 : 0);
	return 1;
}

auto luaSettingsLoad(lua_State* iState) -> int {
	if (auto* settings = settingsOf(iState); settings != nullptr)
		settings->loadUserSettings();
	return 0;
}

auto luaSettingsReset(lua_State* iState) -> int {
	const char* key = luaL_checkstring(iState, 1);
	if (auto* settings = settingsOf(iState); settings != nullptr)
		settings->resetToDefault(key);
	return 0;
}

auto luaSettingsResetAll(lua_State* iState) -> int {
	if (auto* settings = settingsOf(iState); settings != nullptr)
		settings->resetAllToDefaults();
	return 0;
}

auto luaSettingsApply(lua_State* iState) -> int {
	if (const auto* settings = settingsOf(iState); settings != nullptr)
		settings->applyBuiltins();
	return 0;
}

template<typename Action>
auto withTrigger(lua_State* iState, Action&& iAction) -> int {
	const auto* activeScene = getBoundScene(iState);
	if (activeScene == nullptr)
		return 0;
	const auto uid = static_cast<uint64_t>(luaL_checkinteger(iState, 1));
	if (const auto entity = activeScene->findEntityByUUID(core::UUID{uid});
		entity && entity.hasComponent<scene::component::Trigger>())
		std::forward<Action>(iAction)(entity.getComponent<scene::component::Trigger>().trigger);
	return 0;
}

auto luaTriggerStartTimer(lua_State* iState) -> int {
	return withTrigger(iState, [](scene::SceneTrigger& ioTrigger) -> void { ioTrigger.startTimer(); });
}

auto luaTriggerStopTimer(lua_State* iState) -> int {
	return withTrigger(iState, [](scene::SceneTrigger& ioTrigger) -> void { ioTrigger.stopTimer(); });
}

auto luaTriggerResetTimer(lua_State* iState) -> int {
	return withTrigger(iState, [](scene::SceneTrigger& ioTrigger) -> void { ioTrigger.resetTimer(); });
}

using enum LuaType;

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wmissing-designated-field-initializers")
OWL_DIAG_DISABLE_GCC("-Wmissing-field-initializers")
auto declareBindings() -> std::vector<LuaBinding> {
	const LuaValue entityId{.name = "entity_id", .type = Entity};
	// clang-format off
	return {
		{.table = "transform", .name = "get_position", .function = guarded<luaTransformGetPosition>,
		 .description = "Local position of the entity (zeros when it has no transform).",
		 .params = {entityId}, .returns = {{"x", Number}, {"y", Number}, {"z", Number}}},
		{.table = "transform", .name = "set_position", .function = guarded<luaTransformSetPosition>,
		 .description = "Set the local position.",
		 .params = {entityId, {"x", Number}, {"y", Number}, {"z", Number}}},
		{.table = "transform", .name = "get_rotation", .function = guarded<luaTransformGetRotation>,
		 .description = "Local rotation, in radians.",
		 .params = {entityId}, .returns = {{"rx", Number}, {"ry", Number}, {"rz", Number}}},
		{.table = "transform", .name = "set_rotation", .function = guarded<luaTransformSetRotation>,
		 .description = "Set the local rotation, in radians.",
		 .params = {entityId, {"rx", Number}, {"ry", Number}, {"rz", Number}}},
		{.table = "transform", .name = "get_scale", .function = guarded<luaTransformGetScale>,
		 .description = "Local scale.",
		 .params = {entityId}, .returns = {{"sx", Number}, {"sy", Number}, {"sz", Number}}},
		{.table = "transform", .name = "set_scale", .function = guarded<luaTransformSetScale>,
		 .description = "Set the local scale.",
		 .params = {entityId, {"sx", Number}, {"sy", Number}, {"sz", Number}}},
		{.table = "physics", .name = "impulse", .function = guarded<luaPhysicsImpulse>,
		 .description = "Apply a linear impulse to the body.",
		 .params = {entityId, {"fx", Number}, {"fy", Number}}},
		{.table = "physics", .name = "get_velocity", .function = guarded<luaPhysicsGetVelocity>,
		 .description = "Linear velocity of the body (zeros without a body).",
		 .params = {entityId}, .returns = {{"vx", Number}, {"vy", Number}}},
		{.table = "physics", .name = "set_velocity", .function = guarded<luaPhysicsSetVelocity>,
		 .description = "Set the linear velocity of the body.",
		 .params = {entityId, {"vx", Number}, {"vy", Number}}},
		{.table = "physics", .name = "set_transform", .function = guarded<luaPhysicsSetTransform>,
		 .description = "Move the body to a world position and rotation (radians).",
		 .params = {entityId, {"x", Number}, {"y", Number}, {"rotation", Number}}},
		{.table = "physics", .name = "set_gravity_scale", .function = guarded<luaPhysicsSetGravityScale>,
		 .description = "Scale the world gravity for this body (0 = none).",
		 .params = {entityId, {"scale", Number}}},
		{.table = "input", .name = "is_key_pressed", .function = guarded<luaInputIsKeyPressed>,
		 .description = "Whether a key is held (GLFW key code: 65 = A, 87 = W, 32 = Space).",
		 .params = {{"keycode", Integer}}, .returns = {{"pressed", Boolean}}},
		{.table = "input", .name = "is_mouse_button_pressed", .function = guarded<luaInputIsMouseButtonPressed>,
		 .description = "Whether a mouse button is held (0 = left, 1 = right, 2 = middle).",
		 .params = {{"button", Integer}}, .returns = {{"pressed", Boolean}}},
		{.table = "input", .name = "get_mouse_x", .function = guarded<luaInputGetMouseX>,
		 .description = "Mouse X position in the window, in pixels.", .returns = {{"x", Number}}},
		{.table = "input", .name = "get_mouse_y", .function = guarded<luaInputGetMouseY>,
		 .description = "Mouse Y position in the window, in pixels.", .returns = {{"y", Number}}},
		{.table = "sound", .name = "play", .function = guarded<luaSoundPlay>,
		 .description = "Play a sound asset (loaded on first use); an invalid handle when sound is off or missing.",
		 .params = {{"asset_path", String}}, .returns = {{"handle", Integer}}},
		{.table = "sound", .name = "stop", .function = guarded<luaSoundStop>,
		 .description = "Stop a playing sound.", .params = {{"handle", Integer}}},
		{.table = "sound", .name = "pause", .function = guarded<luaSoundPause>,
		 .description = "Pause a playing sound.", .params = {{"handle", Integer}}},
		{.table = "sound", .name = "resume", .function = guarded<luaSoundResume>,
		 .description = "Resume a paused sound.", .params = {{"handle", Integer}}},
		{.table = "sound", .name = "set_volume", .function = guarded<luaSoundSetVolume>,
		 .description = "Set the volume of a sound (0.0 to 2.0).",
		 .params = {{"handle", Integer}, {"volume", Number}}},
		{.table = "scene", .name = "find_entity", .function = guarded<luaSceneFindEntity>,
		 .description = "First entity with this tag (0 when none); scans every entity, cache the result.",
		 .params = {{"name", String}}, .returns = {{"entity_id", Entity}}},
		{.table = "scene", .name = "create_entity", .function = guarded<luaSceneCreateEntity>,
		 .description = "Create an empty entity.",
		 .params = {{"name", String}}, .returns = {{"entity_id", Entity}}},
		{.table = "scene", .name = "destroy_entity", .function = guarded<luaSceneDestroyEntity>,
		 .description = "Destroy an entity and its children at the end of the frame.", .params = {entityId}},
		{.table = "scene", .name = "load_scene", .function = guarded<luaSceneLoadScene>,
		 .description = "Load another level after this frame, keeping the game state (no transition).",
		 .params = {{"level", String}}},
		{.table = "scene", .name = "transition_to", .function = guarded<luaSceneTransitionTo>,
		 .description = "Load a level behind a screen transition (kind as in `ui.transition_play`, `fade` by default).",
		 .params = {{"scene_path", String}, {"kind", String, true}, {"duration", Number, true}}},
		{.table = "scene", .name = "quit", .function = guarded<luaSceneQuit>,
		 .description = "Quit the game (stop Play in the editor) after this frame."},
		{.table = "time", .name = "delta", .function = guarded<luaTimeDelta>,
		 .description = "Duration of the current frame, in seconds.", .returns = {{"seconds", Number}}},
		{.table = "log", .name = "trace", .function = guarded<luaLogTrace>,
		 .description = "Log a message at trace level.", .params = {{"message", String}}},
		{.table = "log", .name = "info", .function = guarded<luaLogInfo>,
		 .description = "Log a message at info level.", .params = {{"message", String}}},
		{.table = "log", .name = "warn", .function = guarded<luaLogWarn>,
		 .description = "Log a message at warning level.", .params = {{"message", String}}},
		{.table = "log", .name = "error", .function = guarded<luaLogError>,
		 .description = "Log a message at error level.", .params = {{"message", String}}},
		{.table = "entity", .name = "has_component", .function = guarded<luaEntityHasComponent>,
		 .description = "Whether the entity has a component: `Transform`, `PhysicBody`, `SpriteRenderer`, `Camera`, "
						"`Text`, `SoundSource`, `Canvas` or a `Ui*` component.",
		 .params = {entityId, {"component", String}}, .returns = {{"has", Boolean}}},
		{.table = "entity", .name = "get_name", .function = guarded<luaEntityGetName>,
		 .description = "Tag of the entity (empty when unknown).", .params = {entityId}, .returns = {{"name", String}}},
		{.table = "ui", .name = "set_text", .function = guarded<luaUiSetText>,
		 .description = "Set the text of a `UiText`.", .params = {entityId, {"text", String}}},
		{.table = "ui", .name = "get_text", .function = guarded<luaUiGetText>,
		 .description = "Text of a `UiText`.", .params = {entityId}, .returns = {{"text", String}}},
		{.table = "ui", .name = "set_visible", .function = guarded<luaUiSetVisible>,
		 .description = "Show or hide the entity in the game.", .params = {entityId, {"visible", Boolean}}},
		{.table = "ui", .name = "set_progress", .function = guarded<luaUiSetProgress>,
		 .description = "Set the value of a `UiProgressBar` (0 to 1).", .params = {entityId, {"value", Number}}},
		{.table = "ui", .name = "get_slider_value", .function = guarded<luaUiGetSliderValue>,
		 .description = "Value of a `UiSlider`.", .params = {entityId}, .returns = {{"value", Number}}},
		{.table = "ui", .name = "set_slider_value", .function = guarded<luaUiSetSliderValue>,
		 .description = "Set the value of a `UiSlider`.", .params = {entityId, {"value", Number}}},
		{.table = "ui", .name = "set_button_enabled", .function = guarded<luaUiSetButtonEnabled>,
		 .description = "Enable or disable a `UiButton`.", .params = {entityId, {"enabled", Boolean}}},
		{.table = "ui", .name = "transition_fade_in", .function = guarded<luaUiTransitionFadeIn>,
		 .description = "Start a fade-in screen transition.", .params = {{"duration", Number}}},
		{.table = "ui", .name = "transition_fade_out", .function = guarded<luaUiTransitionFadeOut>,
		 .description = "Start a fade-out screen transition.", .params = {{"duration", Number}}},
		{.table = "ui", .name = "transition_play", .function = guarded<luaUiTransitionPlay>,
		 .description = "Start a screen transition of a kind (see Transition kinds), opaque black by default.",
		 .params = {{"kind", String}, {"duration", Number}, {"r", Number, true}, {"g", Number, true},
					{"b", Number, true}, {"a", Number, true}}},
		{.table = "ui", .name = "is_transition_active", .function = guarded<luaUiIsTransitionActive>,
		 .description = "Whether a screen transition is running.", .returns = {{"active", Boolean}}},
		{.table = "gamestate", .name = "set", .function = guarded<luaGamestateSet>,
		 .description = "Store a value (integer, number, string or boolean) kept across levels and in saves.",
		 .params = {{"key", String}, {"value", Any}}},
		{.table = "gamestate", .name = "get", .function = guarded<luaGamestateGet>,
		 .description = "Stored value, or `default` (nil when absent) when the key is missing.",
		 .params = {{"key", String}, {"default", Any, true}}, .returns = {{"value", Any}}},
		{.table = "gamestate", .name = "remove", .function = guarded<luaGamestateRemove>,
		 .description = "Remove a key.", .params = {{"key", String}}},
		{.table = "gamestate", .name = "clear", .function = guarded<luaGamestateClear>,
		 .description = "Remove every key."},
		{.table = "save", .name = "save_game", .function = guarded<luaSaveSaveGame>,
		 .description = "Save the level and the game state to a slot after this frame.", .params = {{"slot", Integer}}},
		{.table = "save", .name = "load_game", .function = guarded<luaSaveLoadGame>,
		 .description = "Load a slot after this frame; the level keeps running when the slot does not load.",
		 .params = {{"slot", Integer}}},
		{.table = "save", .name = "has_save", .function = guarded<luaSaveHasSave>,
		 .description = "Whether a slot holds a save.", .params = {{"slot", Integer}}, .returns = {{"exists", Boolean}}},
		{.table = "save", .name = "delete_save", .function = guarded<luaSaveDeleteSave>,
		 .description = "Delete the save of a slot.", .params = {{"slot", Integer}}},
		{.table = "save", .name = "list_saves", .function = guarded<luaSaveListSaves>,
		 .description = "Every save, as a list of `{slot, timestamp, scene}`.", .returns = {{"saves", Table}}},
		{.table = "settings", .name = "get", .function = guarded<luaSettingsGet>,
		 .description = "Setting value: user override, else game default, else `default` (nil when absent).",
		 .params = {{"key", String}, {"default", Any, true}}, .returns = {{"value", Any}}},
		{.table = "settings", .name = "set", .function = guarded<luaSettingsSet>,
		 .description = "Set a user override (integer, number, string or boolean).",
		 .params = {{"key", String}, {"value", Any}}},
		{.table = "settings", .name = "save", .function = guarded<luaSettingsSave>,
		 .description = "Write the user overrides to `settings.yml`.", .returns = {{"ok", Boolean}}},
		{.table = "settings", .name = "load", .function = guarded<luaSettingsLoad>,
		 .description = "Reload the user overrides from `settings.yml`."},
		{.table = "settings", .name = "reset", .function = guarded<luaSettingsReset>,
		 .description = "Remove a user override (back to the game default).", .params = {{"key", String}}},
		{.table = "settings", .name = "reset_all", .function = guarded<luaSettingsResetAll>,
		 .description = "Remove every user override."},
		{.table = "settings", .name = "apply", .function = guarded<luaSettingsApply>,
		 .description = "Apply the built-in keys to the window and the sound."},
		{.table = "trigger", .name = "start_timer", .function = guarded<luaTriggerStartTimer>,
		 .description = "Start or restart a Timer trigger.", .params = {entityId}},
		{.table = "trigger", .name = "stop_timer", .function = guarded<luaTriggerStopTimer>,
		 .description = "Stop a Timer trigger.", .params = {entityId}},
		{.table = "trigger", .name = "reset_timer", .function = guarded<luaTriggerResetTimer>,
		 .description = "Reset the elapsed time of a Timer trigger to 0.", .params = {entityId}},
		{.table = "door", .name = "activate", .function = guarded<luaDoorActivate>,
		 .description = "Open a closed raycast door.", .params = {entityId}},
		{.table = "door", .name = "close", .function = guarded<luaDoorClose>,
		 .description = "Close an open or opening raycast door.", .params = {entityId}},
		{.table = "door", .name = "is_open", .function = guarded<luaDoorIsOpen>,
		 .description = "Whether the raycast door is fully open.", .params = {entityId}, .returns = {{"open", Boolean}}},
		{.table = "door", .name = "get_state", .function = guarded<luaDoorGetState>,
		 .description = "State of the raycast door: `idle`, `opening`, `open` or `closing`.",
		 .params = {entityId}, .returns = {{"state", String}}},
		{.table = "pushwall", .name = "activate", .function = guarded<luaPushwallActivate>,
		 .description = "Start pushing an idle raycast push-wall.", .params = {entityId}},
		{.table = "pushwall", .name = "has_moved", .function = guarded<luaPushwallHasMoved>,
		 .description = "Whether the push-wall reached its final position.",
		 .params = {entityId}, .returns = {{"moved", Boolean}}},
		{.table = "pushwall", .name = "get_state", .function = guarded<luaPushwallGetState>,
		 .description = "State of the push-wall: `idle`, `moving` or `final`.",
		 .params = {entityId}, .returns = {{"state", String}}},
	};
	// clang-format on
}
OWL_DIAG_POP

auto typeName(const LuaType iType) -> std::string_view {
	switch (iType) {
		case Boolean:
			return "boolean";
		case Integer:
			return "integer";
		case Number:
			return "number";
		case String:
			return "string";
		case Table:
			return "table";
		case Entity:
			return "entity";
		case Any:
			return "any";
	}
	return "";
}

auto describeSignature(const LuaBinding& iBinding) -> std::string {
	std::string params;
	for (const auto& param: iBinding.params) {
		if (!params.empty())
			params += ", ";
		const auto text = std::format("{}: {}", param.name, typeName(param.type));
		params += param.optional ? std::format("[{}]", text) : text;
	}
	return std::format("`{}.{}({})`", iBinding.table, iBinding.name, params);
}

auto describeReturns(const LuaBinding& iBinding) -> std::string {
	if (iBinding.returns.empty())
		return {};
	std::string returns;
	for (const auto& value: iBinding.returns) {
		if (!returns.empty())
			returns += ", ";
		returns += std::format("`{}: {}`", value.name, typeName(value.type));
	}
	return returns;
}

auto markdownTable(const std::vector<std::array<std::string, 3>>& iRows) -> std::string {
	std::array<size_t, 3> widths{};
	for (const auto& row: iRows)
		for (size_t col = 0; col < 3; ++col) widths[col] = std::max(widths[col], row[col].size());
	const auto line = [&widths](const std::array<std::string, 3>& iRow) -> std::string {
		std::string out = "|";
		for (size_t col = 0; col < 3; ++col)
			out += " " + iRow[col] + std::string(widths[col] - iRow[col].size(), ' ') + " |";
		return out + "\n";
	};
	std::string out = line(iRows.front());
	out += "|";
	for (const auto width: widths) out += std::string(width + 2, '-') + "|";
	out += "\n";
	for (const auto& row: iRows | std::views::drop(1)) out += line(row);
	return out;
}

}// namespace

auto getLuaBindings() -> const std::vector<LuaBinding>& {
	static const std::vector<LuaBinding> s_bindings = declareBindings();
	return s_bindings;
}

auto getLuaTables() -> const std::vector<LuaTable>& {
	static const std::vector<LuaTable> s_tables{
			{.name = "transform", .description = "Local transform of an entity."},
			{.name = "physics", .description = "Box2D body of an entity."},
			{.name = "input", .description = "Keyboard and mouse state."},
			{.name = "sound", .description = "Sound playback, by handle."},
			{.name = "scene", .description = "Entities of the running level and level changes."},
			{.name = "time", .description = "Frame time."},
			{.name = "log", .description = "Engine log."},
			{.name = "entity", .description = "Entity queries."},
			{.name = "ui", .description = "HUD widgets and screen transitions."},
			{.name = "gamestate", .description = "Values kept across levels and stored in saves."},
			{.name = "save", .description = "Save slots (applied after the frame)."},
			{.name = "settings", .description = "Game settings: defaults from `game_settings.yml`, user overrides."},
			{.name = "trigger", .description = "Timer triggers."},
			{.name = "door", .description = "Raycast doors."},
			{.name = "pushwall", .description = "Raycast push-walls."},
	};
	return s_tables;
}

auto generateLuaReference() -> std::string {
	std::string page = "# Lua API reference {#page-lua-api}\n\n[TOC]\n\n"
					   "Every table and function a script can call. This page is generated from the binding registry "
					   "(`getLuaBindings()` in `source/owl/private/script/LuaBindings.cpp`): do not edit it by hand. "
					   "When a binding changes, `owl_script_tests_unit_test` fails and writes the new page to "
					   "the temporary folder (the failure names the file): copy it over this one. See [Lua "
					   "scripting](scripting.md) for the "
					   "callbacks, the properties and the sandbox.\n\n"
					   "An `entity` is the integer UUID of an entity (`entity_id` in its own script, "
					   "`scene.find_entity` for the others); `[name: type]` is optional.\n";
	for (const auto& table: getLuaTables()) {
		page += std::format("\n## `{}`\n\n{}\n\n", table.name, table.description);
		std::vector<std::array<std::string, 3>> rows{{"Function", "Returns", "Description"}};
		for (const auto& binding: getLuaBindings()) {
			if (binding.table == table.name)
				rows.push_back(
						{describeSignature(binding), describeReturns(binding), std::string{binding.description}});
		}
		page += markdownTable(rows);
	}
	return page;
}

void setBoundScene(lua_State* iState, scene::Scene* iScene) { LuaEngine::setHostPointer(iState, iScene); }

auto getBoundScene(lua_State* iState) -> scene::Scene* {
	return static_cast<scene::Scene*>(LuaEngine::getHostPointer(iState));
}

void registerBindings(lua_State* iState) {
	OWL_PROFILE_FUNCTION()

	struct Registration {
		std::string table;
		std::deque<std::string> names;
		std::vector<luaL_Reg> functions;
	};
	static const auto s_registrations = []() -> std::vector<Registration> {
		std::vector<Registration> tables;
		// No reallocation: the registered names point into each deque.
		tables.reserve(getLuaTables().size());
		for (const auto& table: getLuaTables()) {
			auto& registration =
					tables.emplace_back(Registration{.table = std::string{table.name}, .names = {}, .functions = {}});
			for (const auto& binding: getLuaBindings()) {
				if (binding.table == table.name)
					registration.functions.push_back(
							{registration.names.emplace_back(binding.name).c_str(), binding.function});
			}
			registration.functions.push_back({nullptr, nullptr});
		}
		return tables;
	}();
	for (const auto& registration: s_registrations)
		LuaEngine::registerTable(iState, registration.table.c_str(), registration.functions.data(),
								 static_cast<int>(registration.functions.size() - 1));
}

}// namespace owl::script
