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
#include "input/ActionFrame.h"
#include "input/Gamepad.h"
#include "input/Input.h"
#include "input/InputFrame.h"
#include "physics/PhysicCommand.h"
#include "renderer/Renderer.h"
#include "renderer/RendererIsometricLayer.h"
#include "scene/Entity.h"
#include "scene/SaveManager.h"
#include "scene/Scene.h"
#include "scene/ScreenTransition.h"
#include "scene/SettingsManager.h"
#include "scene/TilemapTerrain.h"
#include "scene/UiInputSystem.h"
#include "scene/component/components.h"
#include "scene/nav/NavWorld.h"
#include "scene/nav/Pathfinder.h"
#include "scene/ui/HudLayout.h"
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
#include <optional>
#include <ranges>
#include <span>
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
	// The widgets of the scene's HUD asset answer to their id like the scene's own entities.
	if (const auto hud = activeScene->getHud(); hud) {
		if (auto entity = hud->findEntityByUUID(core::UUID{uuid}); entity)
			return entity;
	}
	return std::nullopt;
}

// First entity of a scene with this tag, as a Lua id (0 when none).
auto findByTag(const scene::Scene& iScene, const char* iName) -> uint64_t {
	for (const auto view = iScene.registry.view<scene::component::Tag>(); const auto entity: view) {
		if (view.get<scene::component::Tag>(entity).tag == iName)
			return static_cast<uint64_t>(iScene.registry.get<scene::component::ID>(entity).id);
	}
	return 0;
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

auto inputFrameOf(lua_State* iState) -> const input::InputFrame* {
	const auto* boundScene = getBoundScene(iState);
	return boundScene != nullptr ? &boundScene->getInputFrame() : nullptr;
}

template<bool (input::InputFrame::*Edge)(input::KeyCode) const>
auto luaInputKeyEdge(lua_State* iState) -> int {
	const auto keyCode = static_cast<input::KeyCode>(luaL_checkinteger(iState, 1));
	const auto* frame = inputFrameOf(iState);
	lua_pushboolean(iState, frame != nullptr && (frame->*Edge)(keyCode) ? 1 : 0);
	return 1;
}

template<bool (input::InputFrame::*Edge)(input::MouseCode) const>
auto luaInputMouseButtonEdge(lua_State* iState) -> int {
	const auto button = static_cast<input::MouseCode>(luaL_checkinteger(iState, 1));
	const auto* frame = inputFrameOf(iState);
	lua_pushboolean(iState, frame != nullptr && (frame->*Edge)(button) ? 1 : 0);
	return 1;
}

auto actionsOf(lua_State* iState) -> const input::ActionFrame* {
	const auto* boundScene = getBoundScene(iState);
	return boundScene != nullptr ? &boundScene->getActions() : nullptr;
}

auto luaInputHasAction(lua_State* iState) -> int {
	const char* name = luaL_checkstring(iState, 1);
	const auto* actions = actionsOf(iState);
	lua_pushboolean(iState, actions != nullptr && actions->has(name) ? 1 : 0);
	return 1;
}

template<bool (input::ActionFrame::*Query)(std::string_view) const>
auto luaInputActionQuery(lua_State* iState) -> int {
	const char* name = luaL_checkstring(iState, 1);
	const auto* actions = actionsOf(iState);
	lua_pushboolean(iState, actions != nullptr && (actions->*Query)(name) ? 1 : 0);
	return 1;
}

auto luaInputGetActionValue(lua_State* iState) -> int {
	const char* name = luaL_checkstring(iState, 1);
	const auto* actions = actionsOf(iState);
	lua_pushnumber(iState, actions != nullptr ? static_cast<lua_Number>(actions->getValue(name)) : lua_Number{0});
	return 1;
}

auto luaInputGetActionVector(lua_State* iState) -> int {
	const char* name = luaL_checkstring(iState, 1);
	const auto* actions = actionsOf(iState);
	const math::vec2 vector = actions != nullptr ? actions->getVector(name) : math::vec2{0.f, 0.f};
	lua_pushnumber(iState, static_cast<lua_Number>(vector.x()));
	lua_pushnumber(iState, static_cast<lua_Number>(vector.y()));
	return 2;
}

auto luaInputGetMouseWheel(lua_State* iState) -> int {
	const auto* frame = inputFrameOf(iState);
	const math::vec2 wheel = frame != nullptr ? frame->getWheel() : math::vec2{0.f, 0.f};
	lua_pushnumber(iState, static_cast<lua_Number>(wheel.x()));
	lua_pushnumber(iState, static_cast<lua_Number>(wheel.y()));
	return 2;
}

auto checkGamepadSlot(lua_State* iState, const int iArg) -> std::size_t {
	const auto pad = luaL_checkinteger(iState, iArg);
	return pad >= 0 && pad < static_cast<lua_Integer>(input::g_GamepadCount) ? static_cast<std::size_t>(pad)
																			 : input::g_GamepadCount;
}

template<typename Code, std::size_t Count>
auto checkGamepadCode(lua_State* iState, const int iArg) -> Code {
	const auto code = luaL_checkinteger(iState, iArg);
	return code >= 0 && code < static_cast<lua_Integer>(Count) ? static_cast<Code>(code) : static_cast<Code>(Count);
}

auto luaInputIsGamepadConnected(lua_State* iState) -> int {
	const auto pad = checkGamepadSlot(iState, 1);
	lua_pushboolean(iState, input::Input::isGamepadConnected(pad) ? 1 : 0);
	return 1;
}

auto luaInputGetGamepadName(lua_State* iState) -> int {
	const auto pad = checkGamepadSlot(iState, 1);
	const auto name = input::Input::getGamepadName(pad);
	lua_pushlstring(iState, name.data(), name.size());
	return 1;
}

auto luaInputIsGamepadButtonPressed(lua_State* iState) -> int {
	const auto pad = checkGamepadSlot(iState, 1);
	const auto button = checkGamepadCode<input::GamepadButton, input::g_GamepadButtonCount>(iState, 2);
	lua_pushboolean(iState, input::Input::getGamepadState(pad).isButtonPressed(button) ? 1 : 0);
	return 1;
}

template<bool (input::InputFrame::*Edge)(std::size_t, input::GamepadButton) const>
auto luaInputGamepadButtonEdge(lua_State* iState) -> int {
	const auto pad = checkGamepadSlot(iState, 1);
	const auto button = checkGamepadCode<input::GamepadButton, input::g_GamepadButtonCount>(iState, 2);
	const auto* frame = inputFrameOf(iState);
	lua_pushboolean(iState, frame != nullptr && (frame->*Edge)(pad, button) ? 1 : 0);
	return 1;
}

auto luaInputGetGamepadAxis(lua_State* iState) -> int {
	const auto pad = checkGamepadSlot(iState, 1);
	const auto axis = checkGamepadCode<input::GamepadAxis, input::g_GamepadAxisCount>(iState, 2);
	lua_pushnumber(iState, static_cast<lua_Number>(input::Input::getGamepadState(pad).getAxis(axis)));
	return 1;
}

auto luaInputGetGamepadStick(lua_State* iState) -> int {
	const auto pad = checkGamepadSlot(iState, 1);
	const auto stick = luaL_checkinteger(iState, 2) == 1 ? input::gamepad::Stick::Right : input::gamepad::Stick::Left;
	const input::AxisResponse response{.deadZone = static_cast<float>(luaL_optnumber(
											   iState, 3, lua_Number{input::AxisResponse::defaultDeadZone}))};
	const auto shaped = input::applyStickResponse(input::Input::getGamepadState(pad).getStick(stick), response);
	lua_pushnumber(iState, static_cast<lua_Number>(shaped.x()));
	lua_pushnumber(iState, static_cast<lua_Number>(shaped.y()));
	return 2;
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
	uint64_t found = findByTag(*activeScene, name);
	if (const auto hud = activeScene->getHud(); found == 0 && hud)
		found = findByTag(*hud, name);
	lua_pushinteger(iState, static_cast<lua_Integer>(found));
	return 1;
}

auto luaSceneFindEntities(lua_State* iState) -> int {
	lua_newtable(iState);
	auto* activeScene = getBoundScene(iState);
	if (activeScene == nullptr)
		return 1;
	const char* name = luaL_checkstring(iState, 1);
	lua_Integer index = 0;
	for (const auto view = activeScene->registry.view<scene::component::Tag>(); const auto entity: view) {
		if (view.get<scene::component::Tag>(entity).tag != name)
			continue;
		lua_pushinteger(iState, static_cast<lua_Integer>(activeScene->registry.get<scene::component::ID>(entity).id));
		lua_rawseti(iState, -2, ++index);
	}
	return 1;
}

auto luaUiFind(lua_State* iState) -> int {
	auto* activeScene = getBoundScene(iState);
	if (activeScene == nullptr) {
		lua_pushinteger(iState, 0);
		return 1;
	}
	const char* name = luaL_checkstring(iState, 1);
	uint64_t found = 0;
	if (const auto hud = activeScene->getHud(); hud)
		found = findByTag(*hud, name);
	if (found == 0)
		found = findByTag(*activeScene, name);
	lua_pushinteger(iState, static_cast<lua_Integer>(found));
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

auto luaTimeTickAlpha(lua_State* iState) -> int {
	const auto* activeScene = getBoundScene(iState);
	lua_pushnumber(iState, activeScene != nullptr
								   ? static_cast<lua_Number>(activeScene->getSimulationClock().getAlpha())
								   : 0.0);
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
	else if (compName == "UiWindow")
		has = entity->hasComponent<scene::component::UiWindow>();
	else if (compName == "UiLayout")
		has = entity->hasComponent<scene::component::UiLayout>();
	else if (compName == "UiTooltip")
		has = entity->hasComponent<scene::component::UiTooltip>();
	else if (compName == "UiToolCursor")
		has = entity->hasComponent<scene::component::UiToolCursor>();
	else if (compName == "UiFocus")
		has = entity->hasComponent<scene::component::UiFocus>();
	else if (compName == "UiScrollView")
		has = entity->hasComponent<scene::component::UiScrollView>();
	else if (compName == "UiList")
		has = entity->hasComponent<scene::component::UiList>();
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

auto luaEntityGetLink(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState); entity && entity->hasComponent<scene::component::EntityLink>()) {
		auto& link = entity->getComponent<scene::component::EntityLink>();
		const auto* activeScene = getBoundScene(iState);
		if (activeScene != nullptr && activeScene->resolveEntityLink(link)) {
			lua_pushinteger(iState, static_cast<lua_Integer>(static_cast<uint64_t>(link.linkedEntity.getUUID())));
			return 1;
		}
	}
	lua_pushinteger(iState, 0);
	return 1;
}

auto luaEntitySetTint(lua_State* iState) -> int {
	const auto red = static_cast<float>(luaL_checknumber(iState, 2));
	const auto green = static_cast<float>(luaL_checknumber(iState, 3));
	const auto blue = static_cast<float>(luaL_checknumber(iState, 4));
	const auto alpha = static_cast<float>(luaL_optnumber(iState, 5, 1.0));
	const auto entity = findEntity(iState);
	const math::vec4 tint{red, green, blue, alpha};
	if (entity && entity->hasComponent<scene::component::AnimatedSpriteRenderer>())
		entity->getComponent<scene::component::AnimatedSpriteRenderer>().color = tint;
	else if (entity && entity->hasComponent<scene::component::SpriteRenderer>())
		entity->getComponent<scene::component::SpriteRenderer>().color = tint;
	else {
		lua_pushboolean(iState, 0);
		return 1;
	}
	lua_pushboolean(iState, 1);
	return 1;
}

auto luaEntityGetTint(lua_State* iState) -> int {
	const auto entity = findEntity(iState);
	if (!entity)
		return 0;
	math::vec4 tint;
	if (entity->hasComponent<scene::component::AnimatedSpriteRenderer>())
		tint = entity->getComponent<scene::component::AnimatedSpriteRenderer>().color;
	else if (entity->hasComponent<scene::component::SpriteRenderer>())
		tint = entity->getComponent<scene::component::SpriteRenderer>().color;
	else
		return 0;
	lua_pushnumber(iState, static_cast<lua_Number>(tint.x()));
	lua_pushnumber(iState, static_cast<lua_Number>(tint.y()));
	lua_pushnumber(iState, static_cast<lua_Number>(tint.z()));
	lua_pushnumber(iState, static_cast<lua_Number>(tint.w()));
	return 4;
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

auto luaUiRaiseWindow(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState); entity && entity->hasComponent<scene::component::UiWindow>())
		scene::UiInputSystem::raiseWindow(*entity->getScene(), static_cast<entt::entity>(*entity));
	return 0;
}

auto luaUiSetWindowTitle(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState); entity && entity->hasComponent<scene::component::UiWindow>())
		entity->getComponent<scene::component::UiWindow>().title = luaL_checkstring(iState, 2);
	return 0;
}

auto luaUiSetTooltip(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState); entity && entity->hasComponent<scene::component::UiTooltip>())
		entity->getComponent<scene::component::UiTooltip>().text = luaL_checkstring(iState, 2);
	return 0;
}

auto luaUiSetToolCursor(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState); entity && entity->hasComponent<scene::component::UiToolCursor>())
		entity->getComponent<scene::component::UiToolCursor>().active = lua_toboolean(iState, 2) != 0;
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

auto luaUiSetFocus(lua_State* iState) -> int {
	auto* activeScene = getBoundScene(iState);
	const auto entity = findEntity(iState);
	const bool focused = activeScene != nullptr && entity && scene::UiInputSystem::setFocus(*activeScene, *entity);
	lua_pushboolean(iState, focused ? 1 : 0);
	return 1;
}

auto luaUiGetFocus(lua_State* iState) -> int {
	auto* activeScene = getBoundScene(iState);
	const auto focus = activeScene != nullptr ? scene::UiInputSystem::getFocus(*activeScene) : scene::Entity{};
	lua_pushinteger(iState, focus ? static_cast<lua_Integer>(static_cast<uint64_t>(focus.getUUID())) : 0);
	return 1;
}

auto luaUiSetButtonEnabled(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState); entity && entity->hasComponent<scene::component::UiButton>()) {
		auto& button = entity->getComponent<scene::component::UiButton>();
		button.state = lua_toboolean(iState, 2) != 0 ? scene::component::UiButton::State::Normal
													 : scene::component::UiButton::State::Disabled;
	}
	return 0;
}

auto luaUiScrollGet(lua_State* iState) -> int {
	lua_Number x = 0;
	lua_Number y = 0;
	if (const auto entity = findEntity(iState)) {
		if (entity->hasComponent<scene::component::UiScrollView>()) {
			const auto& offset = entity->getComponent<scene::component::UiScrollView>().scrollOffset;
			x = static_cast<lua_Number>(offset.x());
			y = static_cast<lua_Number>(offset.y());
		} else if (entity->hasComponent<scene::component::UiList>()) {
			y = static_cast<lua_Number>(entity->getComponent<scene::component::UiList>().scrollOffset);
		}
	}
	lua_pushnumber(iState, x);
	lua_pushnumber(iState, y);
	return 2;
}

auto luaUiScrollSet(lua_State* iState) -> int {
	const auto x = static_cast<float>(luaL_checknumber(iState, 2));
	const auto y = static_cast<float>(luaL_checknumber(iState, 3));
	if (const auto entity = findEntity(iState)) {
		if (entity->hasComponent<scene::component::UiScrollView>())
			entity->getComponent<scene::component::UiScrollView>().scrollOffset = {x, y};
		else if (entity->hasComponent<scene::component::UiList>())
			entity->getComponent<scene::component::UiList>().scrollOffset = y;
	}
	return 0;
}

auto luaUiListSetCount(lua_State* iState) -> int {
	const auto count = luaL_checkinteger(iState, 2);
	if (const auto entity = findEntity(iState); entity && entity->hasComponent<scene::component::UiList>())
		entity->getComponent<scene::component::UiList>().setCount(
				static_cast<uint32_t>(std::max<lua_Integer>(count, 0)));
	return 0;
}

auto luaUiListGetCount(lua_State* iState) -> int {
	lua_Integer count = 0;
	if (const auto entity = findEntity(iState); entity && entity->hasComponent<scene::component::UiList>())
		count = static_cast<lua_Integer>(entity->getComponent<scene::component::UiList>().itemCount);
	lua_pushinteger(iState, count);
	return 1;
}

auto luaUiListSetRow(lua_State* iState) -> int {
	const auto index = luaL_checkinteger(iState, 2);
	const std::string text = luaL_checkstring(iState, 3);
	const std::string detail = luaL_optstring(iState, 4, "");
	const std::string icon = luaL_optstring(iState, 5, "");
	const auto entity = findEntity(iState);
	if (!entity || !entity->hasComponent<scene::component::UiList>())
		return 0;
	auto& list = entity->getComponent<scene::component::UiList>();
	if (index < 1 || index > static_cast<lua_Integer>(list.itemCount))
		return 0;
	list.rows.resize(list.itemCount);
	auto& row = list.rows[static_cast<size_t>(index) - 1];
	row.text = text;
	row.detail = detail;
	row.icon = list.loadIcon(icon);
	row.bound = true;
	return 0;
}

auto luaUiListInvalidate(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState); entity && entity->hasComponent<scene::component::UiList>())
		entity->getComponent<scene::component::UiList>().invalidateRows();
	return 0;
}

auto luaUiListGetSelected(lua_State* iState) -> int {
	lua_Integer selected = 0;
	if (const auto entity = findEntity(iState); entity && entity->hasComponent<scene::component::UiList>())
		selected = static_cast<lua_Integer>(entity->getComponent<scene::component::UiList>().selectedIndex) + 1;
	lua_pushinteger(iState, selected);
	return 1;
}

auto luaUiListSetSelected(lua_State* iState) -> int {
	const auto index = luaL_checkinteger(iState, 2);
	if (const auto entity = findEntity(iState); entity && entity->hasComponent<scene::component::UiList>())
		entity->getComponent<scene::component::UiList>().select(index - 1);
	return 0;
}

auto luaUiListGetVisible(lua_State* iState) -> int {
	lua_Integer first = 0;
	lua_Integer last = 0;
	if (const auto entity = findEntity(iState); entity && entity->hasComponent<scene::component::UiList>()) {
		const auto& layout = entity->getScene()->getHudLayout();
		if (const auto* node = layout.findNode(static_cast<entt::entity>(*entity));
			node != nullptr && node->scroll != scene::ui::HudNode::noParent) {
			const auto& scroll = layout.getScrolls()[node->scroll];
			const auto [begin, end] = scene::ui::visibleRows(scroll.offset.y(), scroll.geometry.viewport.size.y(),
															 scroll.rowHeight, scroll.rowSpacing, scroll.rowCount);
			if (end > begin) {
				first = static_cast<lua_Integer>(begin) + 1;
				last = static_cast<lua_Integer>(end);
			}
		}
	}
	lua_pushinteger(iState, first);
	lua_pushinteger(iState, last);
	return 2;
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

auto luaUiIsMouseOverUi(lua_State* iState) -> int {
	const auto* boundScene = getBoundScene(iState);
	lua_pushboolean(iState, boundScene != nullptr && scene::UiInputSystem::isUIConsuming(*boundScene) ? 1 : 0);
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

// The tilemap of an isometric map entity, with its world origin.
struct IsometricMap {
	scene::TilemapAsset* asset = nullptr;
	math::vec3 origin{0.f, 0.f, 0.f};
};

// The isometric map of the entity at argument 1, if it carries a loaded tilemap.
auto findIsometricMap(lua_State* iState) -> std::optional<IsometricMap> {
	const auto entity = findEntity(iState);
	if (!entity || !entity->hasComponent<scene::component::Tilemap>())
		return std::nullopt;
	auto* asset = entity->getComponent<scene::component::Tilemap>().asset.get();
	if (asset == nullptr)
		return std::nullopt;
	return IsometricMap{.asset = asset, .origin = getBoundScene(iState)->getWorldTransform(*entity).translation()};
}

// The camera the isometric layer draws with: the layer's presentation and zoom, centred on the primary camera.
auto makeIsometricCamera(const scene::Scene& iScene) -> renderer::IsometricCamera {
	renderer::IsometricCamera camera;
	for (const auto& layer: renderer::Renderer::getRenderStack().getLayers()) {
		if (const auto* isometric = dynamic_cast<const renderer::RendererIsometricLayer*>(layer.get());
			isometric != nullptr) {
			camera.setConfig(isometric->getConfig());
			camera.setZoomLevel(isometric->getZoomLevel());
			break;
		}
	}
	const auto& viewport = iScene.getViewportSize();
	camera.setViewportSize(viewport.x() == 0 || viewport.y() == 0 ? math::vec2ui{1280u, 720u} : viewport);
	if (const auto primary = iScene.getPrimaryCamera(); primary) {
		const auto eye = iScene.getWorldTransform(primary).translation();
		camera.setTarget({eye.x(), eye.y(), 0.f});
	}
	return camera;
}

auto luaIsometricPickCell(lua_State* iState) -> int {
	// The viewport mouse of the last HUD update: in Owl Nest the game is drawn inside a panel, not at the window origin.
	const auto& uiState = getBoundScene(iState)->getUiInputState();
	const auto mouse = uiState.hasMouse ? uiState.mousePosition : input::Input::getMousePos();
	const auto windowX = static_cast<float>(luaL_optnumber(iState, 2, static_cast<lua_Number>(mouse.x())));
	const auto windowY = static_cast<float>(luaL_optnumber(iState, 3, static_cast<lua_Number>(mouse.y())));
	const auto map = findIsometricMap(iState);
	if (!map) {
		lua_pushnil(iState);
		return 1;
	}
	const auto camera = makeIsometricCamera(*getBoundScene(iState));
	if (const auto hit = scene::terrain::pickTerrain(*map->asset, camera, {windowX, windowY}, map->origin); hit) {
		lua_pushinteger(iState, static_cast<lua_Integer>(hit->cell.x()));
		lua_pushinteger(iState, static_cast<lua_Integer>(hit->cell.y()));
		return 2;
	}
	lua_pushnil(iState);
	return 1;
}

auto luaIsometricGetCell(lua_State* iState) -> int {
	const auto layer = luaL_checkinteger(iState, 2);
	const auto cellX = luaL_checkinteger(iState, 3);
	const auto cellY = luaL_checkinteger(iState, 4);
	const auto map = findIsometricMap(iState);
	if (!map || layer < 0 || cellX < 0 || cellY < 0 || static_cast<size_t>(layer) >= map->asset->layers.size()) {
		lua_pushinteger(iState, scene::component::g_EmptyTileIndex);
		lua_pushinteger(iState, scene::component::g_EmptyTileIndex);
		return 2;
	}
	const auto cell = map->asset->layers[static_cast<size_t>(layer)].getCell(static_cast<uint32_t>(cellX),
																			 static_cast<uint32_t>(cellY));
	lua_pushinteger(iState, cell.terrain);
	lua_pushinteger(iState, cell.overlay);
	return 2;
}

auto luaIsometricSetCell(lua_State* iState) -> int {
	const auto layer = luaL_checkinteger(iState, 2);
	const auto cellX = luaL_checkinteger(iState, 3);
	const auto cellY = luaL_checkinteger(iState, 4);
	const auto terrain = luaL_checkinteger(iState, 5);
	const auto overlay = luaL_checkinteger(iState, 6);
	const auto map = findIsometricMap(iState);
	bool done = false;
	if (map && layer >= 0 && cellX >= 0 && cellY >= 0 && static_cast<size_t>(layer) < map->asset->layers.size()) {
		auto& tiles = map->asset->layers[static_cast<size_t>(layer)];
		const auto x = static_cast<uint32_t>(cellX);
		const auto y = static_cast<uint32_t>(cellY);
		auto cell = tiles.getCell(x, y);
		cell.terrain = static_cast<int32_t>(terrain);
		cell.overlay = static_cast<int32_t>(overlay);
		done = tiles.setCell(x, y, cell);
	}
	lua_pushboolean(iState, done ? 1 : 0);
	return 1;
}

auto luaIsometricGetSlope(lua_State* iState) -> int {
	const auto cellX = luaL_checkinteger(iState, 2);
	const auto cellY = luaL_checkinteger(iState, 3);
	const auto map = findIsometricMap(iState);
	const bool inside = map && cellX >= 0 && cellY >= 0;
	lua_pushinteger(iState,
					inside ? map->asset->getCellSlope(static_cast<uint32_t>(cellX), static_cast<uint32_t>(cellY))
						   : scene::terrain::flatSlope);
	return 1;
}

auto luaIsometricGetHeight(lua_State* iState) -> int {
	const auto worldX = static_cast<float>(luaL_checknumber(iState, 2));
	const auto worldY = static_cast<float>(luaL_checknumber(iState, 3));
	const auto map = findIsometricMap(iState);
	if (!map) {
		lua_pushnumber(iState, 0);
		return 1;
	}
	const float top =
			map->origin.z() + static_cast<float>(map->asset->getMaxCornerHeight()) * scene::terrain::heightStep + 1.f;
	const auto hit = scene::terrain::pickTerrain(*map->asset, map->origin, {worldX, worldY, top}, {0.f, 0.f, -1.f});
	lua_pushnumber(iState, static_cast<lua_Number>(hit ? hit->point.z() : map->origin.z()));
	return 1;
}

auto luaIsometricSetSpriteFrame(lua_State* iState) -> int {
	const auto frame = luaL_checkinteger(iState, 2);
	const auto entity = findEntity(iState);
	if (!entity || !entity->hasComponent<scene::component::AnimatedSpriteRenderer>()) {
		lua_pushboolean(iState, 0);
		return 1;
	}
	auto& sprite = entity->getComponent<scene::component::AnimatedSpriteRenderer>();
	const auto count = static_cast<int64_t>(sprite.columns) * static_cast<int64_t>(sprite.rows);
	const auto index = static_cast<uint32_t>(std::clamp<int64_t>(frame, 0, std::max<int64_t>(count - 1, 0)));
	sprite.firstFrame = index;
	sprite.lastFrame = index;
	sprite.m_currentFrame = index;
	sprite.m_elapsedTime = 0.f;
	lua_pushboolean(iState, 1);
	return 1;
}

auto luaIsometricGetSpriteFrame(lua_State* iState) -> int {
	const auto entity = findEntity(iState);
	if (!entity || !entity->hasComponent<scene::component::AnimatedSpriteRenderer>())
		return 0;
	lua_pushinteger(iState, entity->getComponent<scene::component::AnimatedSpriteRenderer>().m_currentFrame);
	return 1;
}

// The navigation entry of the tilemap entity at argument 1: its graph, synchronised, and its request queue.
auto findNavigation(lua_State* iState) -> scene::nav::NavWorld::Entry* {
	const auto entity = findEntity(iState);
	if (!entity || !entity->hasComponent<scene::component::Tilemap>())
		return nullptr;
	const auto& tilemap = entity->getComponent<scene::component::Tilemap>();
	if (!tilemap.asset || tilemap.navigation.isEmpty())
		return nullptr;
	return &getBoundScene(iState)->getNavigation().acquire(static_cast<uint64_t>(entity->getUUID()), *tilemap.asset,
														   tilemap.navigation);
}

auto readNavMode(lua_State* iState, const int iIndex) -> scene::nav::PathMode {
	if (lua_istable(iState, iIndex))
		return scene::nav::PathMode::Auto;
	const std::string_view mode = luaL_optstring(iState, iIndex, "auto");
	if (mode == "flat")
		return scene::nav::PathMode::Flat;
	if (mode == "hierarchical")
		return scene::nav::PathMode::Hierarchical;
	return scene::nav::PathMode::Auto;
}

// Options table at `iIndex` (or in place of the mode): `{heading_x = dx, heading_y = dy, no_reverse = true}`.
auto readNavOptions(lua_State* iState, const int iIndex) -> scene::nav::PathOptions {
	scene::nav::PathOptions options;
	if (!lua_istable(iState, iIndex))
		return options;
	lua_getfield(iState, iIndex, "no_reverse");
	options.noReverse = lua_toboolean(iState, -1) != 0;
	lua_getfield(iState, iIndex, "heading_x");
	lua_getfield(iState, iIndex, "heading_y");
	const auto headingX = lua_tointeger(iState, -2);
	const auto headingY = lua_tointeger(iState, -1);
	lua_pop(iState, 3);
	if (headingX > 0 && headingY == 0)
		options.heading = scene::nav::Direction::East;
	else if (headingX < 0 && headingY == 0)
		options.heading = scene::nav::Direction::West;
	else if (headingX == 0 && headingY > 0)
		options.heading = scene::nav::Direction::South;
	else if (headingX == 0 && headingY < 0)
		options.heading = scene::nav::Direction::North;
	return options;
}

// A cell from two Lua integers; false when one is negative.
auto readNavCell(lua_State* iState, const int iIndex) -> std::optional<math::vec2ui> {
	const auto x = luaL_checkinteger(iState, iIndex);
	const auto y = luaL_checkinteger(iState, iIndex + 1);
	if (x < 0 || y < 0)
		return std::nullopt;
	return math::vec2ui{static_cast<uint32_t>(x), static_cast<uint32_t>(y)};
}

// The cells as a flat table `{x1, y1, x2, y2, ...}` then the cost in cells.
auto pushNavPath(lua_State* iState, const scene::nav::PathResult& iPath) -> int {
	lua_createtable(iState, static_cast<int>(iPath.cells.size() * 2), 0);
	lua_Integer index = 1;
	for (const auto& cell: iPath.cells) {
		lua_pushinteger(iState, static_cast<lua_Integer>(cell.x()));
		lua_rawseti(iState, -2, index++);
		lua_pushinteger(iState, static_cast<lua_Integer>(cell.y()));
		lua_rawseti(iState, -2, index++);
	}
	lua_pushnumber(iState, static_cast<lua_Number>(iPath.getCostInCells()));
	return 2;
}

auto luaPathFind(lua_State* iState) -> int {
	const auto from = readNavCell(iState, 2);
	const auto to = readNavCell(iState, 4);
	const auto mode = readNavMode(iState, 6);
	const auto options = readNavOptions(iState, lua_istable(iState, 6) ? 6 : 7);
	auto* navigation = findNavigation(iState);
	lua_pushinteger(iState,
					navigation != nullptr && from && to ? navigation->queue.request(*from, *to, mode, options) : 0);
	return 1;
}

auto luaPathFindNow(lua_State* iState) -> int {
	const auto from = readNavCell(iState, 2);
	const auto to = readNavCell(iState, 4);
	const auto mode = readNavMode(iState, 6);
	const auto options = readNavOptions(iState, lua_istable(iState, 6) ? 6 : 7);
	auto* navigation = findNavigation(iState);
	if (navigation == nullptr || !from || !to) {
		lua_pushnil(iState);
		return 1;
	}
	scene::nav::Pathfinder finder;
	scene::nav::PathResult path;
	if (!finder.find(navigation->map, *from, *to, mode, path, options)) {
		lua_pushnil(iState);
		return 1;
	}
	return pushNavPath(iState, path);
}

auto luaPathPoll(lua_State* iState) -> int {
	const auto request = static_cast<scene::nav::PathRequestId>(luaL_checkinteger(iState, 2));
	const auto* navigation = findNavigation(iState);
	auto status = scene::nav::PathStatus::Unknown;
	if (navigation != nullptr)
		status = navigation->queue.getStatus(request);
	switch (status) {
		case scene::nav::PathStatus::Pending:
			lua_pushstring(iState, "pending");
			break;
		case scene::nav::PathStatus::Found:
			lua_pushstring(iState, "found");
			break;
		case scene::nav::PathStatus::NotFound:
			lua_pushstring(iState, "not_found");
			break;
		case scene::nav::PathStatus::Unknown:
			lua_pushstring(iState, "unknown");
			break;
	}
	return 1;
}

auto luaPathGet(lua_State* iState) -> int {
	const auto request = static_cast<scene::nav::PathRequestId>(luaL_checkinteger(iState, 2));
	const auto* navigation = findNavigation(iState);
	const auto* path = navigation != nullptr ? navigation->queue.getResult(request) : nullptr;
	if (path == nullptr) {
		lua_pushnil(iState);
		return 1;
	}
	return pushNavPath(iState, *path);
}

auto luaPathRelease(lua_State* iState) -> int {
	const auto request = static_cast<scene::nav::PathRequestId>(luaL_checkinteger(iState, 2));
	if (auto* navigation = findNavigation(iState); navigation != nullptr)
		navigation->queue.release(request);
	return 0;
}

auto luaPathIsValid(lua_State* iState) -> int {
	const auto request = static_cast<scene::nav::PathRequestId>(luaL_checkinteger(iState, 2));
	const auto first = std::max<lua_Integer>(luaL_optinteger(iState, 3, 1), 1);
	const auto* navigation = findNavigation(iState);
	const auto* path = navigation != nullptr ? navigation->queue.getResult(request) : nullptr;
	bool valid = false;
	if (path != nullptr && static_cast<size_t>(first) <= path->cells.size()) {
		const std::span<const math::vec2ui> remaining{path->cells.begin() + (first - 1), path->cells.end()};
		valid = scene::nav::isPathValid(navigation->map, remaining);
	}
	lua_pushboolean(iState, valid ? 1 : 0);
	return 1;
}

auto luaPathGetEdgeCost(lua_State* iState) -> int {
	const auto from = readNavCell(iState, 2);
	const auto to = readNavCell(iState, 4);
	const auto* navigation = findNavigation(iState);
	if (navigation == nullptr || !from || !to) {
		lua_pushnil(iState);
		return 1;
	}
	for (const auto direction: scene::nav::directions) {
		if (static_cast<int64_t>(from->x()) + scene::nav::offsetX(direction) != static_cast<int64_t>(to->x()) ||
			static_cast<int64_t>(from->y()) + scene::nav::offsetY(direction) != static_cast<int64_t>(to->y()))
			continue;
		if (const auto cost = navigation->map.getEdgeCost(from->x(), from->y(), direction); cost != 0) {
			lua_pushnumber(iState, static_cast<lua_Number>(scene::nav::NavMap::unitsToCells(cost)));
			return 1;
		}
	}
	lua_pushnil(iState);
	return 1;
}

auto luaPathIsWalkable(lua_State* iState) -> int {
	const auto cell = readNavCell(iState, 2);
	const auto* navigation = findNavigation(iState);
	lua_pushboolean(iState, navigation != nullptr && cell && navigation->map.isWalkable(cell->x(), cell->y()) ? 1 : 0);
	return 1;
}

auto luaPathGetVersion(lua_State* iState) -> int {
	const auto* navigation = findNavigation(iState);
	lua_pushinteger(iState, navigation != nullptr ? static_cast<lua_Integer>(navigation->map.getVersion()) : 0);
	return 1;
}

auto luaPathSetBudget(lua_State* iState) -> int {
	const auto budget = luaL_checkinteger(iState, 1);
	getBoundScene(iState)->getNavigation().setBudget(
			static_cast<uint32_t>(std::clamp<lua_Integer>(budget, 1, 1000000)));
	return 0;
}

// Draw (or stop drawing, with no cells) a route of the map in the debug overlay: `path.show_route(map, id, cells, r, g, b)`.
auto luaPathShowRoute(lua_State* iState) -> int {
	luaL_checkinteger(iState, 1);
	const auto routeId = static_cast<uint64_t>(luaL_checkinteger(iState, 2));
	const bool hasCells = lua_istable(iState, 3);
	const auto red = static_cast<float>(luaL_optnumber(iState, 4, 1.));
	const auto green = static_cast<float>(luaL_optnumber(iState, 5, 1.));
	const auto blue = static_cast<float>(luaL_optnumber(iState, 6, 1.));
	std::vector<math::vec2ui> cells;
	if (hasCells) {
		const auto count = static_cast<lua_Integer>(lua_rawlen(iState, 3)) / 2;
		cells.reserve(static_cast<size_t>(count));
		for (lua_Integer i = 1; i <= count; ++i) {
			lua_rawgeti(iState, 3, i * 2 - 1);
			lua_rawgeti(iState, 3, i * 2);
			cells.emplace_back(static_cast<uint32_t>(std::max<lua_Integer>(lua_tointeger(iState, -2), 0)),
							   static_cast<uint32_t>(std::max<lua_Integer>(lua_tointeger(iState, -1), 0)));
			lua_pop(iState, 2);
		}
	}
	if (const auto entity = findEntity(iState); entity && findNavigation(iState) != nullptr)
		getBoundScene(iState)->getNavigation().setDebugRoute(static_cast<uint64_t>(entity->getUUID()), routeId, cells,
															 math::vec3{red, green, blue});
	return 0;
}

auto luaPathClearRoutes(lua_State* iState) -> int {
	luaL_checkinteger(iState, 1);
	if (const auto entity = findEntity(iState); entity && findNavigation(iState) != nullptr)
		getBoundScene(iState)->getNavigation().clearDebugRoutes(static_cast<uint64_t>(entity->getUUID()));
	return 0;
}

auto luaPathSetDebug(lua_State* iState) -> int {
	luaL_checkany(iState, 1);
	getBoundScene(iState)->getNavigation().setDebugVisible(scene::nav::NavWorld::DebugSource::Script,
														   lua_toboolean(iState, 1) != 0);
	return 0;
}

auto luaPathGetDebug(lua_State* iState) -> int {
	lua_pushboolean(iState, getBoundScene(iState)->getNavigation().isDebugVisible() ? 1 : 0);
	return 1;
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
		{.table = "input", .name = "has_action", .function = guarded<luaInputHasAction>,
		 .description = "Whether the game's action map defines an action (a misspelt name reads as released).",
		 .params = {{"action", String}}, .returns = {{"exists", Boolean}}},
		{.table = "input", .name = "is_action_pressed",
		 .function = guarded<luaInputActionQuery<&input::ActionFrame::isHeld>>,
		 .description = "Whether an action is held (a button down, an axis or vector past half deflection).",
		 .params = {{"action", String}}, .returns = {{"pressed", Boolean}}},
		{.table = "input", .name = "is_action_just_pressed",
		 .function = guarded<luaInputActionQuery<&input::ActionFrame::isJustPressed>>,
		 .description = "Whether an action went down since the previous tick (in `on_tick`) or frame (in `on_update`).",
		 .params = {{"action", String}}, .returns = {{"pressed", Boolean}}},
		{.table = "input", .name = "is_action_just_released",
		 .function = guarded<luaInputActionQuery<&input::ActionFrame::isJustReleased>>,
		 .description = "Whether an action went up since the previous tick (in `on_tick`) or frame (in `on_update`).",
		 .params = {{"action", String}}, .returns = {{"released", Boolean}}},
		{.table = "input", .name = "get_action_value", .function = guarded<luaInputGetActionValue>,
		 .description = "Reading of a button (0 or 1) or axis action, in [-1, 1].",
		 .params = {{"action", String}}, .returns = {{"value", Number}}},
		{.table = "input", .name = "get_action_vector", .function = guarded<luaInputGetActionVector>,
		 .description = "Reading of a vector action in the unit disc (x right, y down).",
		 .params = {{"action", String}}, .returns = {{"x", Number}, {"y", Number}}},
		{.table = "input", .name = "is_key_pressed", .function = guarded<luaInputIsKeyPressed>,
		 .description = "Deprecated, use `input.is_action_pressed`: whether a key is held (GLFW key code: 65 = A, 87 = W, 32 = Space).",
		 .params = {{"keycode", Integer}}, .returns = {{"pressed", Boolean}}},
		{.table = "input", .name = "is_mouse_button_pressed", .function = guarded<luaInputIsMouseButtonPressed>,
		 .description = "Deprecated, use `input.is_action_pressed`: whether a mouse button is held (0 = left, 1 = right, 2 = middle).",
		 .params = {{"button", Integer}}, .returns = {{"pressed", Boolean}}},
		{.table = "input", .name = "get_mouse_x", .function = guarded<luaInputGetMouseX>,
		 .description = "Mouse X position in the window, in pixels.", .returns = {{"x", Number}}},
		{.table = "input", .name = "get_mouse_y", .function = guarded<luaInputGetMouseY>,
		 .description = "Mouse Y position in the window, in pixels.", .returns = {{"y", Number}}},
		{.table = "input", .name = "is_key_just_pressed",
		 .function = guarded<luaInputKeyEdge<&input::InputFrame::isKeyJustPressed>>,
		 .description = "Deprecated, use `input.is_action_just_pressed`: whether a key went down since the previous tick (in `on_tick`) or frame (in `on_update`).",
		 .params = {{"keycode", Integer}}, .returns = {{"pressed", Boolean}}},
		{.table = "input", .name = "is_key_just_released",
		 .function = guarded<luaInputKeyEdge<&input::InputFrame::isKeyJustReleased>>,
		 .description = "Deprecated, use `input.is_action_just_released`: whether a key went up since the previous tick (in `on_tick`) or frame (in `on_update`).",
		 .params = {{"keycode", Integer}}, .returns = {{"released", Boolean}}},
		{.table = "input", .name = "is_mouse_button_just_pressed",
		 .function = guarded<luaInputMouseButtonEdge<&input::InputFrame::isMouseButtonJustPressed>>,
		 .description = "Deprecated, use `input.is_action_just_pressed`: whether a mouse button went down since the previous tick or frame.",
		 .params = {{"button", Integer}}, .returns = {{"pressed", Boolean}}},
		{.table = "input", .name = "is_mouse_button_just_released",
		 .function = guarded<luaInputMouseButtonEdge<&input::InputFrame::isMouseButtonJustReleased>>,
		 .description = "Deprecated, use `input.is_action_just_released`: whether a mouse button went up since the previous tick or frame.",
		 .params = {{"button", Integer}}, .returns = {{"released", Boolean}}},
		{.table = "input", .name = "get_mouse_wheel", .function = guarded<luaInputGetMouseWheel>,
		 .description = "Wheel offset scrolled since the previous tick or frame (horizontal, vertical).",
		 .returns = {{"dx", Number}, {"dy", Number}}},
		{.table = "input", .name = "is_gamepad_connected", .function = guarded<luaInputIsGamepadConnected>,
		 .description = "Whether a gamepad is connected in a slot (0 to 15).",
		 .params = {{"pad", Integer}}, .returns = {{"connected", Boolean}}},
		{.table = "input", .name = "get_gamepad_name", .function = guarded<luaInputGetGamepadName>,
		 .description = "Name of the gamepad of a slot, empty when none is connected.",
		 .params = {{"pad", Integer}}, .returns = {{"name", String}}},
		{.table = "input", .name = "is_gamepad_button_pressed", .function = guarded<luaInputIsGamepadButtonPressed>,
		 .description = "Whether a gamepad button is held (0 = A, 1 = B, 2 = X, 3 = Y, 7 = Start, 11-14 = D-pad).",
		 .params = {{"pad", Integer}, {"button", Integer}}, .returns = {{"pressed", Boolean}}},
		{.table = "input", .name = "is_gamepad_button_just_pressed",
		 .function = guarded<luaInputGamepadButtonEdge<&input::InputFrame::isGamepadButtonJustPressed>>,
		 .description = "Whether a gamepad button went down since the previous tick or frame.",
		 .params = {{"pad", Integer}, {"button", Integer}}, .returns = {{"pressed", Boolean}}},
		{.table = "input", .name = "is_gamepad_button_just_released",
		 .function = guarded<luaInputGamepadButtonEdge<&input::InputFrame::isGamepadButtonJustReleased>>,
		 .description = "Whether a gamepad button went up since the previous tick or frame.",
		 .params = {{"pad", Integer}, {"button", Integer}}, .returns = {{"released", Boolean}}},
		{.table = "input", .name = "get_gamepad_axis", .function = guarded<luaInputGetGamepadAxis>,
		 .description = "Raw gamepad axis (0-3 = sticks in [-1, 1], y down; 4, 5 = triggers in [0, 1]).",
		 .params = {{"pad", Integer}, {"axis", Integer}}, .returns = {{"value", Number}}},
		{.table = "input", .name = "get_gamepad_stick", .function = guarded<luaInputGetGamepadStick>,
		 .description = "Gamepad stick (0 = left, 1 = right) through a radial dead zone (0.15 by default), y down.",
		 .params = {{"pad", Integer}, {"stick", Integer}, {"dead_zone", Number, true}},
		 .returns = {{"x", Number}, {"y", Number}}},
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
		 .description = "First entity with this tag (0 when none), in the scene then in its HUD; scans every entity, "
						"cache the result.",
		 .params = {{"name", String}}, .returns = {{"entity_id", Entity}}},
		{.table = "scene", .name = "find_entities", .function = guarded<luaSceneFindEntities>,
		 .description = "Every entity of the scene with this tag, as an array of ids (empty when none); scans every "
						"entity, cache the result.",
		 .params = {{"name", String}}, .returns = {{"entity_ids", Table}}},
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
		 .description = "Duration of the current frame, or of the fixed tick inside `on_tick`, in seconds.",
		 .returns = {{"seconds", Number}}},
		{.table = "time", .name = "tick_alpha", .function = guarded<luaTimeTickAlpha>,
		 .description = "Progress of the frame between the last two fixed ticks, in [0, 1), to blend what is drawn.",
		 .returns = {{"alpha", Number}}},
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
		{.table = "entity", .name = "get_link", .function = guarded<luaEntityGetLink>,
		 .description = "Entity an `EntityLink` of this entity points at (0 when it has none or the target is missing).",
		 .params = {entityId}, .returns = {{"linked_id", Entity}}},
		{.table = "entity", .name = "set_tint", .function = guarded<luaEntitySetTint>,
		 .description = "Set the colour multiplied into a `SpriteRenderer` or `AnimatedSpriteRenderer` (hit flash, fade); "
						"false when the entity has no sprite. `a` is 1 when omitted.",
		 .params = {entityId, {"r", Number}, {"g", Number}, {"b", Number}, {"a", Number, true}},
		 .returns = {{"done", Boolean}}},
		{.table = "entity", .name = "get_tint", .function = guarded<luaEntityGetTint>,
		 .description = "Colour multiplied into a `SpriteRenderer` or `AnimatedSpriteRenderer`; nothing when the entity "
						"has no sprite.",
		 .params = {entityId},
		 .returns = {{"r", Number}, {"g", Number}, {"b", Number}, {"a", Number}}},
		{.table = "ui", .name = "find", .function = guarded<luaUiFind>,
		 .description = "First widget with this tag, searched in the scene's HUD asset then in the scene (0 when none); "
						"cache the result.",
		 .params = {{"name", String}}, .returns = {{"entity_id", Entity}}},
		{.table = "ui", .name = "set_text", .function = guarded<luaUiSetText>,
		 .description = "Set the text of a `UiText`.", .params = {entityId, {"text", String}}},
		{.table = "ui", .name = "get_text", .function = guarded<luaUiGetText>,
		 .description = "Text of a `UiText`.", .params = {entityId}, .returns = {{"text", String}}},
		{.table = "ui", .name = "set_visible", .function = guarded<luaUiSetVisible>,
		 .description = "Show or hide the entity in the game.", .params = {entityId, {"visible", Boolean}}},
		{.table = "ui", .name = "set_progress", .function = guarded<luaUiSetProgress>,
		 .description = "Set the value of a `UiProgressBar` (0 to 1).", .params = {entityId, {"value", Number}}},
		{.table = "ui", .name = "raise_window", .function = guarded<luaUiRaiseWindow>,
		 .description = "Draw a `UiWindow` over its siblings (a click on it does the same).", .params = {entityId}},
		{.table = "ui", .name = "set_window_title", .function = guarded<luaUiSetWindowTitle>,
		 .description = "Set the title of a `UiWindow`.", .params = {entityId, {"title", String}}},
		{.table = "ui", .name = "set_tooltip", .function = guarded<luaUiSetTooltip>,
		 .description = "Set the text of a `UiTooltip`.", .params = {entityId, {"text", String}}},
		{.table = "ui", .name = "set_tool_cursor", .function = guarded<luaUiSetToolCursor>,
		 .description = "Show or hide a `UiToolCursor`, the sprite following the mouse over the HUD.",
		 .params = {entityId, {"active", Boolean}}},
		{.table = "ui", .name = "get_slider_value", .function = guarded<luaUiGetSliderValue>,
		 .description = "Value of a `UiSlider`.", .params = {entityId}, .returns = {{"value", Number}}},
		{.table = "ui", .name = "set_slider_value", .function = guarded<luaUiSetSliderValue>,
		 .description = "Set the value of a `UiSlider`.", .params = {entityId, {"value", Number}}},
		{.table = "ui", .name = "set_focus", .function = guarded<luaUiSetFocus>,
		 .description = "Give the keyboard and gamepad focus to a `UiButton` or a `UiSlider` and show it (false when "
						"the widget cannot take it); its `UiFocus` callback runs.",
		 .params = {entityId}, .returns = {{"focused", Boolean}}},
		{.table = "ui", .name = "get_focus", .function = guarded<luaUiGetFocus>,
		 .description = "Widget holding the keyboard and gamepad focus (0 when none).",
		 .returns = {{"entity_id", Entity}}},
		{.table = "ui", .name = "set_button_enabled", .function = guarded<luaUiSetButtonEnabled>,
		 .description = "Enable or disable a `UiButton`.", .params = {entityId, {"enabled", Boolean}}},
		{.table = "ui", .name = "scroll_get", .function = guarded<luaUiScrollGet>,
		 .description = "Scroll offset of a `UiScrollView` (a `UiList` has `y` only), in reference pixels from the top "
						"left of the content.",
		 .params = {entityId}, .returns = {{"x", Number}, {"y", Number}}},
		{.table = "ui", .name = "scroll_set", .function = guarded<luaUiScrollSet>,
		 .description = "Scroll a `UiScrollView` or a `UiList` to an offset, in reference pixels; it is kept inside the "
						"content when the HUD is laid out.",
		 .params = {entityId, {"x", Number}, {"y", Number}}},
		{.table = "ui", .name = "list_set_count", .function = guarded<luaUiListSetCount>,
		 .description = "Set the number of rows of a `UiList`; the rows kept stay filled, a selection past the end is "
						"dropped.",
		 .params = {entityId, {"count", Integer}}},
		{.table = "ui", .name = "list_get_count", .function = guarded<luaUiListGetCount>,
		 .description = "Number of rows of a `UiList`.", .params = {entityId}, .returns = {{"count", Integer}}},
		{.table = "ui", .name = "list_set_row", .function = guarded<luaUiListSetRow>,
		 .description = "Fill a row of a `UiList` (1-based): its text, a right-aligned detail and an icon texture path. "
						"Only the rows in view need it, from the list's `onBindCallback` (reads `_row_index`).",
		 .params = {entityId, {"index", Integer}, {"text", String}, {"detail", String, true}, {"icon", String, true}}},
		{.table = "ui", .name = "list_invalidate", .function = guarded<luaUiListInvalidate>,
		 .description = "Forget what the rows of a `UiList` show: the ones in view are bound again.",
		 .params = {entityId}},
		{.table = "ui", .name = "list_get_selected", .function = guarded<luaUiListGetSelected>,
		 .description = "Selected row of a `UiList`, 1-based (0 for none).", .params = {entityId},
		 .returns = {{"index", Integer}}},
		{.table = "ui", .name = "list_set_selected", .function = guarded<luaUiListSetSelected>,
		 .description = "Select a row of a `UiList`, 1-based (0 or out of range for none) and scroll it into view; "
						"`onSelectCallback` does not run.",
		 .params = {entityId, {"index", Integer}}},
		{.table = "ui", .name = "list_get_visible", .function = guarded<luaUiListGetVisible>,
		 .description = "Rows of a `UiList` in view at the last frame drawn, 1-based and inclusive (0, 0 when none).",
		 .params = {entityId}, .returns = {{"first", Integer}, {"last", Integer}}},
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
		{.table = "ui", .name = "is_mouse_over_ui", .function = guarded<luaUiIsMouseOverUi>,
		 .description = "Whether the mouse is over a HUD widget or window that takes it (a click there is not for the scene).",
		 .returns = {{"over", Boolean}}},
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
		{.table = "isometric", .name = "pick_cell", .function = guarded<luaIsometricPickCell>,
		 .description = "Terrain cell under a viewport pixel (the mouse by default), slope-aware; nothing when it is off the map.",
		 .params = {entityId, {"x", Number, true}, {"y", Number, true}}, .returns = {{"cell_x", Integer}, {"cell_y", Integer}}},
		{.table = "isometric", .name = "get_cell", .function = guarded<luaIsometricGetCell>,
		 .description = "Terrain and overlay tile of a cell of a layer of the tilemap (layers from 0, -1 for none or outside).",
		 .params = {entityId, {"layer", Integer}, {"cell_x", Integer}, {"cell_y", Integer}},
		 .returns = {{"terrain", Integer}, {"overlay", Integer}}},
		{.table = "isometric", .name = "set_cell", .function = guarded<luaIsometricSetCell>,
		 .description = "Set the terrain and overlay tile of a cell of a layer (-1 for none), without undo; false outside the grid.",
		 .params = {entityId, {"layer", Integer}, {"cell_x", Integer}, {"cell_y", Integer}, {"terrain", Integer}, {"overlay", Integer}},
		 .returns = {{"done", Boolean}}},
		{.table = "isometric", .name = "get_slope", .function = guarded<luaIsometricGetSlope>,
		 .description = "Slope of a cell, from its corner heights (0 is flat, 1 to 18 are the other slopes of the tileset).",
		 .params = {entityId, {"cell_x", Integer}, {"cell_y", Integer}}, .returns = {{"slope", Integer}}},
		{.table = "isometric", .name = "get_height", .function = guarded<luaIsometricGetHeight>,
		 .description = "World height of the terrain surface at a world position (the map's own height outside the map).",
		 .params = {entityId, {"x", Number}, {"y", Number}}, .returns = {{"z", Number}}},
		{.table = "isometric", .name = "set_sprite_frame", .function = guarded<luaIsometricSetSpriteFrame>,
		 .description = "Show one frame (from 0, row by row) of the sheet of an `AnimatedSpriteRenderer` and stop its animation there, so one entity picks its picture from its heading; false when the entity has no such sprite.",
		 .params = {entityId, {"frame", Integer}}, .returns = {{"done", Boolean}}},
		{.table = "isometric", .name = "get_sprite_frame", .function = guarded<luaIsometricGetSpriteFrame>,
		 .description = "Frame an `AnimatedSpriteRenderer` shows now; nothing when the entity has no such sprite.",
		 .params = {entityId}, .returns = {{"frame", Integer}}},
		{.table = "path", .name = "find", .function = guarded<luaPathFind>,
		 .description = "Queue a shortest-path query between two cells of the tilemap's graph (its `navigation` rules); 0 when the entity has no graph. The path is searched over the next frames, `mode` is `auto`, `flat` or `hierarchical`. `options` (a table, which may replace `mode`) is `{no_reverse = true, heading_x = dx, heading_y = dy}`: the route never goes back to the cell it just left, except from a dead end (a cell with a single exit), and its first move goes on in the heading (an axis direction) when there is one; the search is then always flat.",
		 .params = {entityId, {"from_x", Integer}, {"from_y", Integer}, {"to_x", Integer}, {"to_y", Integer}, {"mode", String, true}, {"options", Table, true}},
		 .returns = {{"request", Integer}}},
		{.table = "path", .name = "find_now", .function = guarded<luaPathFindNow>,
		 .description = "Search a shortest path at once (a long query on a big map costs frame time, prefer `path.find`): the cells as `{x1, y1, x2, y2, ...}` and the cost in cells, nothing when no path joins the cells. `options` as in `path.find`.",
		 .params = {entityId, {"from_x", Integer}, {"from_y", Integer}, {"to_x", Integer}, {"to_y", Integer}, {"mode", String, true}, {"options", Table, true}},
		 .returns = {{"cells", Table}, {"cost", Number}}},
		{.table = "path", .name = "poll", .function = guarded<luaPathPoll>,
		 .description = "State of a request: `pending`, `found`, `not_found` or `unknown` (released or never made).",
		 .params = {entityId, {"request", Integer}}, .returns = {{"status", String}}},
		{.table = "path", .name = "get", .function = guarded<luaPathGet>,
		 .description = "Path of a found request: the cells as `{x1, y1, x2, y2, ...}` (start and goal included) and the cost in cells; nothing otherwise.",
		 .params = {entityId, {"request", Integer}}, .returns = {{"cells", Table}, {"cost", Number}}},
		{.table = "path", .name = "release", .function = guarded<luaPathRelease>,
		 .description = "Forget a request, pending or finished.", .params = {entityId, {"request", Integer}}},
		{.table = "path", .name = "is_valid", .function = guarded<luaPathIsValid>,
		 .description = "Whether every step of a found path still exists (false after a road of the path was bulldozed), from its `from_cell`-th cell (1 for the whole path).",
		 .params = {entityId, {"request", Integer}, {"from_cell", Integer, true}}, .returns = {{"valid", Boolean}}},
		{.table = "path", .name = "get_edge_cost", .function = guarded<luaPathGetEdgeCost>,
		 .description = "Cost, in cells, of the move between two neighbouring cells; nothing when the move does not exist.",
		 .params = {entityId, {"from_x", Integer}, {"from_y", Integer}, {"to_x", Integer}, {"to_y", Integer}},
		 .returns = {{"cost", Number}}},
		{.table = "path", .name = "is_walkable", .function = guarded<luaPathIsWalkable>,
		 .description = "Whether a cell is a node of the graph.",
		 .params = {entityId, {"cell_x", Integer}, {"cell_y", Integer}}, .returns = {{"walkable", Boolean}}},
		{.table = "path", .name = "get_version", .function = guarded<luaPathGetVersion>,
		 .description = "Counter that grows each time the graph changes (a road built or bulldozed): compare it to know when to search again.",
		 .params = {entityId}, .returns = {{"version", Integer}}},
		{.table = "path", .name = "set_budget", .function = guarded<luaPathSetBudget>,
		 .description = "Node expansions the scene spends on path requests each frame (4000 by default).",
		 .params = {{"nodes", Integer}}},
		{.table = "path", .name = "set_debug", .function = guarded<luaPathSetDebug>,
		 .description = "Show or hide the navigation overlay over the scene: the graph of every tilemap (green two-way moves, orange one-way moves with an arrow) and the routes given to `path.show_route`. Drawn in the editor's Play view and in the game.",
		 .params = {{"visible", Boolean}}},
		{.table = "path", .name = "get_debug", .function = guarded<luaPathGetDebug>,
		 .description = "Whether the navigation overlay is shown, by a script or by the editor.",
		 .returns = {{"visible", Boolean}}},
		{.table = "path", .name = "show_route", .function = guarded<luaPathShowRoute>,
		 .description = "Give the overlay a route to draw over the tilemap, from its first cell to its last, in a colour (components 0 to 1, white by default): `route_id` names it (a vehicle), a new route with the same id replaces the old one and no cells removes it. Cells are `{x1, y1, x2, y2, ...}` as `path.get` returns them.",
		 .params = {entityId, {"route_id", Integer}, {"cells", Table, true}, {"r", Number, true}, {"g", Number, true}, {"b", Number, true}}},
		{.table = "path", .name = "clear_routes", .function = guarded<luaPathClearRoutes>,
		 .description = "Remove every route given to `path.show_route` for the tilemap.",
		 .params = {entityId}},
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
			{.name = "input",
			 .description = "Actions of the game's input map, then the raw keyboard, mouse and gamepad state."},
			{.name = "sound", .description = "Sound playback, by handle."},
			{.name = "scene", .description = "Entities of the running level and level changes."},
			{.name = "time", .description = "Frame and fixed-tick time."},
			{.name = "log", .description = "Engine log."},
			{.name = "entity", .description = "Entity queries."},
			{.name = "ui", .description = "HUD widgets and screen transitions."},
			{.name = "gamestate", .description = "Values kept across levels and stored in saves."},
			{.name = "save", .description = "Save slots (applied after the frame)."},
			{.name = "settings", .description = "Game settings: defaults from `game_settings.yml`, user overrides."},
			{.name = "trigger", .description = "Timer triggers."},
			{.name = "door", .description = "Raycast doors."},
			{.name = "pushwall", .description = "Raycast push-walls."},
			{.name = "isometric", .description = "Terrain picking and tile editing of an isometric tilemap."},
			{.name = "path",
			 .description = "Shortest paths over the graph a tilemap's `navigation` rules describe (roads, tracks), "
							"searched over several frames."},
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
