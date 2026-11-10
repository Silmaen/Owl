/**
 * @file LuaBindings.cpp
 * @author Silmaen
 * @date 09/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "LuaBindings.h"

#include "EntityHandle.h"

#include "app/EngineContext.h"
#include "core/Core.h"
#include "core/Macros.h"
#include "core/external/lua.h"
#include "data/assets/AssetReference.h"
#include "data/assets/AssetSearchPaths.h"
#include "data/dialogue/DialogueLibrary.h"
#include "data/table/DataTableLibrary.h"
#include "input/ActionFrame.h"
#include "input/Gamepad.h"
#include "input/Input.h"
#include "input/InputFrame.h"
#include "math/RandomStream.h"
#include "math/elementaryFunctions.h"
#include "physics/PhysicCommand.h"
#include "renderer/MeshCache.h"
#include "renderer/Renderer.h"
#include "renderer/RendererIsometricLayer.h"
#include "renderer/RendererParticles.h"
#include "scene/ComponentRegistry.h"
#include "scene/Entity.h"
#include "scene/PrefabSerializer.h"
#include "scene/SaveManager.h"
#include "scene/Scene.h"
#include "scene/ScenePicking.h"
#include "scene/ScreenTransition.h"
#include "scene/SequencePlayback.h"
#include "scene/SettingsManager.h"
#include "scene/TilemapTerrain.h"
#include "scene/UiInputSystem.h"
#include "scene/WorldStreamer.h"
#include "scene/component/components.h"
#include "scene/nav/NavBake.h"
#include "scene/nav/NavWorld.h"
#include "scene/nav/Pathfinder.h"
#include "scene/nav/WalkQueries.h"
#include "scene/ui/HudLayout.h"
#include "script/LuaEngine.h"
#include "script/LuaValues.h"
#include "script/ScriptEngine.h"
#include "sound/Bus.h"
#include "sound/SoundCommand.h"
#include "sound/SoundHelper.h"
#include "sound/SoundSystem.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <format>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
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

// Disk or decoder work: a slow disk must not count against the time quota of the calling script.
template<LuaFunction Function>
auto blocking(lua_State* iState) -> int {
	const int results = LuaEngine::callGuarded(iState, Function);
	LuaEngine::restartCallClock(iState);
	return results;
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

// The entity of the handle at a stack index: of the bound scene or of its HUD.
auto entityAt(lua_State* iState, const int iIndex) -> std::optional<scene::Entity> {
	auto* activeScene = getBoundScene(iState);
	if (activeScene == nullptr)
		return std::nullopt;
	const auto handle = luaL_checknumber(iState, iIndex);
	if (handle < 1 || handle != std::floor(handle))
		return std::nullopt;
	return resolveEntityHandle(*activeScene, static_cast<uint64_t>(handle));
}

auto findEntity(lua_State* iState) -> std::optional<scene::Entity> { return entityAt(iState, 1); }

void pushHandle(lua_State* iState, const uint64_t iHandle) { pushInteger(iState, static_cast<int64_t>(iHandle)); }

void pushEntity(lua_State* iState, const scene::Entity& iEntity) {
	const auto* activeScene = getBoundScene(iState);
	pushHandle(iState, activeScene == nullptr ? 0 : getEntityHandle(*activeScene, iEntity));
}

void pushEntity(lua_State* iState, const core::UUID iUuid) {
	const auto* activeScene = getBoundScene(iState);
	pushHandle(iState, activeScene == nullptr ? 0 : getEntityHandle(*activeScene, iUuid));
}

// Handle of the first entity of a scene with this tag (0 when none).
auto findByTag(const scene::Scene& iBound, scene::Scene& iScene, const char* iName) -> uint64_t {
	for (const auto view = iScene.registry.view<scene::component::Tag>(); const auto entity: view) {
		if (view.get<scene::component::Tag>(entity).tag == iName)
			return getEntityHandle(iBound, scene::Entity{entity, &iScene});
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

auto pushVector(lua_State* iState, const math::vec3d& iVector) -> int {
	lua_pushnumber(iState, static_cast<lua_Number>(iVector.x()));
	lua_pushnumber(iState, static_cast<lua_Number>(iVector.y()));
	lua_pushnumber(iState, static_cast<lua_Number>(iVector.z()));
	return 3;
}

auto luaTransformGetWorldPosition(lua_State* iState) -> int {
	const auto entity = findEntity(iState);
	if (!entity || !entity->hasComponent<scene::component::Transform>())
		return pushVector(iState, {0.0, 0.0, 0.0});
	auto& activeScene = *entity->getScene();
	if (entity->hasComponent<scene::component::WorldPosition>()) {
		activeScene.syncWorldPositions();
		return pushVector(iState, entity->getComponent<scene::component::WorldPosition>().position);
	}
	const auto local = activeScene.getWorldTransform(*entity).translation();
	return pushVector(iState, activeScene.getWorldOrigin() + math::vec3d{static_cast<double>(local.x()),
																		 static_cast<double>(local.y()),
																		 static_cast<double>(local.z())});
}

auto luaTransformSetWorldPosition(lua_State* iState) -> int {
	const math::vec3d position{luaL_checknumber(iState, 2), luaL_checknumber(iState, 3), luaL_checknumber(iState, 4)};
	auto entity = findEntity(iState);
	if (!entity || !entity->hasComponent<scene::component::Transform>())
		return 0;
	if (entity->hasComponent<scene::component::Hierarchy>() &&
		entity->getComponent<scene::component::Hierarchy>().parentId != core::UUID{0}) {
		OWL_CORE_WARN_ONCE("Lua: transform.set_world_position ignores the child entity {}, a child follows its parent.",
						   static_cast<uint64_t>(entity->getUUID()))
		return 0;
	}
	if (!entity->hasComponent<scene::component::WorldPosition>())
		entity->addComponent<scene::component::WorldPosition>();
	entity->getComponent<scene::component::WorldPosition>().position = position;
	entity->getScene()->syncWorldPositions();
	return 0;
}

auto luaSceneGetWorldOrigin(lua_State* iState) -> int {
	const auto* activeScene = getBoundScene(iState);
	return pushVector(iState, activeScene != nullptr ? activeScene->getWorldOrigin() : math::vec3d{0.0, 0.0, 0.0});
}

// The `lua` stream of the bound scene, else a stream kept in the registry of a state bound to no scene.
auto luaRandomStream(lua_State* iState) -> math::RandomStream& {
	if (auto* activeScene = getBoundScene(iState); activeScene != nullptr)
		return activeScene->getRandomStream("lua");
	static char key = 0;
	if (lua_rawgetp(iState, LUA_REGISTRYINDEX, &key) != LUA_TUSERDATA) {
		lua_pop(iState, 1);
		std::construct_at(static_cast<math::RandomStream*>(lua_newuserdata(iState, sizeof(math::RandomStream))), 0,
						  "lua");
		lua_pushvalue(iState, -1);
		lua_rawsetp(iState, LUA_REGISTRYINDEX, &key);
	}
	auto* stream = static_cast<math::RandomStream*>(lua_touserdata(iState, -1));
	lua_pop(iState, 1);
	return *stream;
}

auto luaMathRandom(lua_State* iState) -> int {
	auto& stream = luaRandomStream(iState);
	int64_t low = 1;
	int64_t high = 0;
	switch (lua_gettop(iState)) {
		case 0:
			lua_pushnumber(iState, static_cast<lua_Number>(stream.nextDouble()));
			return 1;
		case 1:
			high = checkInteger(iState, 1);
			if (high == 0) {
				pushInteger(iState, static_cast<int64_t>(stream.nextU64()));
				return 1;
			}
			break;
		case 2:
			low = checkInteger(iState, 1);
			high = checkInteger(iState, 2);
			break;
		default:
			luaL_error(iState, "wrong number of arguments");
	}
	luaL_argcheck(iState, low <= high, lua_gettop(iState), "interval is empty");
	pushInteger(iState, static_cast<int64_t>(stream.nextInt(low, high)));
	return 1;
}

auto luaMathRandomSeed(lua_State* iState) -> int {
	auto& stream = luaRandomStream(iState);
	if (lua_isnoneornil(iState, 1)) {
		const auto* activeScene = getBoundScene(iState);
		stream = math::RandomStream{activeScene != nullptr ? activeScene->getRandomSeed() : 0, "lua"};
		return 0;
	}
	const auto seed = isInteger(iState, 1) ? toInteger(iState, 1) : static_cast<int64_t>(luaL_checknumber(iState, 1));
	stream.reseed(static_cast<uint64_t>(seed));
	return 0;
}

template<auto Function>
auto luaMathUnary(lua_State* iState) -> int {
	lua_pushnumber(iState, static_cast<lua_Number>(Function(static_cast<double>(luaL_checknumber(iState, 1)))));
	return 1;
}

auto luaMathAtan(lua_State* iState) -> int {
	const auto y = static_cast<double>(luaL_checknumber(iState, 1));
	const auto x = static_cast<double>(luaL_optnumber(iState, 2, 1.0));
	lua_pushnumber(iState, static_cast<lua_Number>(math::atan2(y, x)));
	return 1;
}

auto luaMathLog(lua_State* iState) -> int {
	const auto x = static_cast<double>(luaL_checknumber(iState, 1));
	if (lua_isnoneornil(iState, 2)) {
		lua_pushnumber(iState, static_cast<lua_Number>(math::log(x)));
		return 1;
	}
	const auto base = static_cast<double>(luaL_checknumber(iState, 2));
	lua_pushnumber(iState, static_cast<lua_Number>(math::log(x) / math::log(base)));
	return 1;
}

auto luaMathPow(lua_State* iState) -> int {
	const auto x = static_cast<double>(luaL_checknumber(iState, 1));
	const auto y = static_cast<double>(luaL_checknumber(iState, 2));
	lua_pushnumber(iState, static_cast<lua_Number>(math::pow(x, y)));
	return 1;
}

// A table key that sorts the same on every run: a boolean, a number or a string.
struct SortableKey {
	int type = LUA_TNIL;
	bool isInteger = false;
	int64_t integer = 0;
	lua_Number number = 0;
	std::string text;
	bool boolean = false;
};

auto isBefore(const SortableKey& iLeft, const SortableKey& iRight) -> bool {
	if (iLeft.type != iRight.type)
		return iLeft.type < iRight.type;
	switch (iLeft.type) {
		case LUA_TBOOLEAN:
			return !iLeft.boolean && iRight.boolean;
		case LUA_TNUMBER:
			if (iLeft.isInteger && iRight.isInteger)
				return iLeft.integer < iRight.integer;
			if (iLeft.number < iRight.number || iRight.number < iLeft.number)
				return iLeft.number < iRight.number;
			return iLeft.isInteger && !iRight.isInteger;
		default:
			return iLeft.text < iRight.text;
	}
}

auto luaTableSortedKeys(lua_State* iState) -> int {
	luaL_checktype(iState, 1, LUA_TTABLE);
	lua_pushnil(iState);
	while (lua_next(iState, 1) != 0) {
		lua_pop(iState, 1);
		if (const int type = lua_type(iState, -1); type != LUA_TBOOLEAN && type != LUA_TNUMBER && type != LUA_TSTRING)
			luaL_error(iState, "sorted_keys: a %s key has no stable order", luaL_typename(iState, -1));
	}
	std::vector<SortableKey> keys;
	lua_pushnil(iState);
	while (lua_next(iState, 1) != 0) {
		lua_pop(iState, 1);
		SortableKey& key = keys.emplace_back();
		key.type = lua_type(iState, -1);
		if (key.type == LUA_TBOOLEAN) {
			key.boolean = lua_toboolean(iState, -1) != 0;
		} else if (key.type == LUA_TNUMBER) {
			key.isInteger = isInteger(iState, -1);
			key.integer = key.isInteger ? toInteger(iState, -1) : 0;
			key.number = lua_tonumber(iState, -1);
		} else {
			size_t length = 0;
			const char* chars = lua_tolstring(iState, -1, &length);
			key.text.assign(chars, length);
		}
	}
	std::ranges::sort(keys, isBefore);
	lua_createtable(iState, static_cast<int>(keys.size()), 0);
	int64_t index = 0;
	for (const auto& key: keys) {
		if (key.type == LUA_TBOOLEAN)
			lua_pushboolean(iState, key.boolean ? 1 : 0);
		else if (key.type == LUA_TNUMBER && key.isInteger)
			pushInteger(iState, key.integer);
		else if (key.type == LUA_TNUMBER)
			lua_pushnumber(iState, key.number);
		else
			lua_pushlstring(iState, key.text.data(), key.text.size());
		lua_rawseti(iState, -2, static_cast<int>(++index));
	}
	return 1;
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

// A 3D PhysicBody: its velocity has three components.
auto isSpaceBody(const scene::Entity& iEntity) -> bool {
	return iEntity.hasComponent<scene::component::PhysicBody>() &&
		   iEntity.getComponent<scene::component::PhysicBody>().body.dimension == scene::SceneBody::Dimension::Space;
}

auto luaPhysicsImpulse(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState)) {
		const auto fx = static_cast<float>(luaL_checknumber(iState, 2));
		const auto fy = static_cast<float>(luaL_checknumber(iState, 3));
		if (lua_isnoneornil(iState, 4))
			physics::PhysicCommand::impulse(*entity, {fx, fy});
		else
			physics::PhysicCommand::impulse3D(*entity, {fx, fy, static_cast<float>(luaL_checknumber(iState, 4))});
	}
	return 0;
}

auto luaPhysicsGetVelocity(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState)) {
		const auto vel = physics::PhysicCommand::getVelocity3D(*entity);
		lua_pushnumber(iState, static_cast<lua_Number>(vel.x()));
		lua_pushnumber(iState, static_cast<lua_Number>(vel.y()));
		if (!isSpaceBody(*entity))
			return 2;
		lua_pushnumber(iState, static_cast<lua_Number>(vel.z()));
		return 3;
	}
	lua_pushnumber(iState, 0);
	lua_pushnumber(iState, 0);
	return 2;
}

auto luaPhysicsSetVelocity(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState)) {
		const auto vx = static_cast<float>(luaL_checknumber(iState, 2));
		const auto vy = static_cast<float>(luaL_checknumber(iState, 3));
		if (lua_isnoneornil(iState, 4))
			physics::PhysicCommand::setVelocity(*entity, {vx, vy});
		else
			physics::PhysicCommand::setVelocity3D(*entity, {vx, vy, static_cast<float>(luaL_checknumber(iState, 4))});
	}
	return 0;
}

auto luaPhysicsSetTransform(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState)) {
		const auto px = static_cast<float>(luaL_checknumber(iState, 2));
		const auto py = static_cast<float>(luaL_checknumber(iState, 3));
		const auto third = static_cast<float>(luaL_checknumber(iState, 4));
		if (lua_isnoneornil(iState, 5)) {
			physics::PhysicCommand::setTransform(*entity, {px, py}, third);
			return 0;
		}
		const math::vec3f rotation{static_cast<float>(luaL_checknumber(iState, 5)),
								   static_cast<float>(luaL_checknumber(iState, 6)),
								   static_cast<float>(luaL_checknumber(iState, 7))};
		physics::PhysicCommand::setTransform3D(*entity, {px, py, third}, rotation);
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

auto luaPhysicsGetGravity(lua_State* iState) -> int {
	const bool space = !lua_isnoneornil(iState, 3);
	const math::vec3f point{static_cast<float>(luaL_checknumber(iState, 1)),
							static_cast<float>(luaL_checknumber(iState, 2)),
							space ? static_cast<float>(luaL_checknumber(iState, 3)) : 0.f};
	const auto* boundScene = getBoundScene(iState);
	const math::vec3f gravity = boundScene != nullptr ? physics::PhysicCommand::getGravityAt(*boundScene, point)
													  : math::vec3f{0.f, 0.f, 0.f};
	lua_pushnumber(iState, static_cast<lua_Number>(gravity.x()));
	lua_pushnumber(iState, static_cast<lua_Number>(gravity.y()));
	if (!space)
		return 2;
	lua_pushnumber(iState, static_cast<lua_Number>(gravity.z()));
	return 3;
}

auto luaPhysicsPredictTrajectory(lua_State* iState) -> int {
	const auto steps = static_cast<uint32_t>(std::clamp<int64_t>(
			checkInteger(iState, 2), 0, static_cast<int64_t>(physics::PhysicCommand::maxPredictionSteps)));
	const auto entity = findEntity(iState);
	const auto path = entity ? physics::PhysicCommand::predictTrajectory(*entity, steps) : std::vector<math::vec3f>{};
	const bool space = entity && isSpaceBody(*entity);
	lua_createtable(iState, static_cast<int>(path.size()), 0);
	for (size_t i = 0; i < path.size(); ++i) {
		lua_createtable(iState, 0, space ? 3 : 2);
		lua_pushnumber(iState, static_cast<lua_Number>(path[i].x()));
		lua_setfield(iState, -2, "x");
		lua_pushnumber(iState, static_cast<lua_Number>(path[i].y()));
		lua_setfield(iState, -2, "y");
		if (space) {
			lua_pushnumber(iState, static_cast<lua_Number>(path[i].z()));
			lua_setfield(iState, -2, "z");
		}
		lua_rawseti(iState, -2, static_cast<int>(i) + 1);
	}
	return 1;
}

auto luaPhysicsIsOnRails(lua_State* iState) -> int {
	const auto* boundScene = getBoundScene(iState);
	lua_pushboolean(iState, boundScene != nullptr && physics::PhysicCommand::isOnRails(*boundScene) ? 1 : 0);
	return 1;
}

auto luaEntityIsDemoted(lua_State* iState) -> int {
	const auto entity = findEntity(iState);
	lua_pushboolean(iState, entity && entity->hasComponent<scene::component::SimulationLod>() &&
											entity->getComponent<scene::component::SimulationLod>().m_demoted
									? 1
									: 0);
	return 1;
}

auto luaArg(lua_State* iState, const int iIndex) -> float {
	return static_cast<float>(luaL_checknumber(iState, iIndex));
}

auto luaMask(lua_State* iState, const int iIndex) -> uint8_t {
	return lua_isnoneornil(iState, iIndex) ? scene::SceneBody::allLayers
										   : static_cast<uint8_t>(checkInteger(iState, iIndex) & 0xff);
}

// A hit as `{entity, x, y, nx, ny, distance}`, with z and nz for a 3D query; nil without one.
auto pushHit(lua_State* iState, const std::optional<physics::PhysicCommand::QueryHit>& iHit, const bool iSpace) -> int {
	if (!iHit) {
		lua_pushnil(iState);
		return 1;
	}
	lua_newtable(iState);
	pushEntity(iState, core::UUID{static_cast<uint64_t>(iHit->entity)});
	lua_setfield(iState, -2, "entity");
	const auto field = [iState](const char* iName, const float iValue) -> void {
		lua_pushnumber(iState, static_cast<lua_Number>(iValue));
		lua_setfield(iState, -2, iName);
	};
	field("x", iHit->point.x());
	field("y", iHit->point.y());
	field("nx", iHit->normal.x());
	field("ny", iHit->normal.y());
	if (iSpace) {
		field("z", iHit->point.z());
		field("nz", iHit->normal.z());
	}
	field("distance", iHit->distance);
	return 1;
}

auto pushEntities(lua_State* iState, const std::vector<core::UUID>& iEntities) -> int {
	lua_createtable(iState, static_cast<int>(iEntities.size()), 0);
	for (size_t i = 0; i < iEntities.size(); ++i) {
		pushEntity(iState, core::UUID{static_cast<uint64_t>(iEntities[i])});
		lua_rawseti(iState, -2, static_cast<int>(i) + 1);
	}
	return 1;
}

auto luaPhysicsRaycast(lua_State* iState) -> int {
	const math::vec3f origin{luaArg(iState, 1), luaArg(iState, 2), 0.f};
	const math::vec3f direction{luaArg(iState, 3), luaArg(iState, 4), 0.f};
	const float distance = luaArg(iState, 5);
	const auto* boundScene = getBoundScene(iState);
	if (boundScene == nullptr)
		return pushHit(iState, std::nullopt, false);
	return pushHit(iState,
				   physics::PhysicCommand::raycast(*boundScene, origin, direction, distance, luaMask(iState, 6)),
				   false);
}

auto luaPhysicsRaycast3D(lua_State* iState) -> int {
	const math::vec3f origin{luaArg(iState, 1), luaArg(iState, 2), luaArg(iState, 3)};
	const math::vec3f direction{luaArg(iState, 4), luaArg(iState, 5), luaArg(iState, 6)};
	const float distance = luaArg(iState, 7);
	const auto* boundScene = getBoundScene(iState);
	if (boundScene == nullptr)
		return pushHit(iState, std::nullopt, true);
	return pushHit(iState,
				   physics::PhysicCommand::raycast(*boundScene, origin, direction, distance, luaMask(iState, 8)), true);
}

auto luaPhysicsCircleCast(lua_State* iState) -> int {
	const math::vec3f origin{luaArg(iState, 1), luaArg(iState, 2), 0.f};
	const float radius = luaArg(iState, 3);
	const math::vec3f direction{luaArg(iState, 4), luaArg(iState, 5), 0.f};
	const float distance = luaArg(iState, 6);
	const auto* boundScene = getBoundScene(iState);
	if (boundScene == nullptr)
		return pushHit(iState, std::nullopt, false);
	const physics::PhysicCommand::QueryShape shape{.sphere = true, .halfExtents = {radius, radius, radius}};
	return pushHit(
			iState,
			physics::PhysicCommand::shapeCast(*boundScene, shape, origin, direction, distance, luaMask(iState, 7)),
			false);
}

auto luaPhysicsBoxCast(lua_State* iState) -> int {
	const math::vec3f origin{luaArg(iState, 1), luaArg(iState, 2), 0.f};
	const physics::PhysicCommand::QueryShape shape{.sphere = false,
												   .halfExtents = {luaArg(iState, 3), luaArg(iState, 4), 0.5f},
												   .rotation = {0.f, 0.f, luaArg(iState, 5)}};
	const math::vec3f direction{luaArg(iState, 6), luaArg(iState, 7), 0.f};
	const float distance = luaArg(iState, 8);
	const auto* boundScene = getBoundScene(iState);
	if (boundScene == nullptr)
		return pushHit(iState, std::nullopt, false);
	return pushHit(
			iState,
			physics::PhysicCommand::shapeCast(*boundScene, shape, origin, direction, distance, luaMask(iState, 9)),
			false);
}

auto luaPhysicsSphereCast(lua_State* iState) -> int {
	const math::vec3f origin{luaArg(iState, 1), luaArg(iState, 2), luaArg(iState, 3)};
	const float radius = luaArg(iState, 4);
	const math::vec3f direction{luaArg(iState, 5), luaArg(iState, 6), luaArg(iState, 7)};
	const float distance = luaArg(iState, 8);
	const auto* boundScene = getBoundScene(iState);
	if (boundScene == nullptr)
		return pushHit(iState, std::nullopt, true);
	const physics::PhysicCommand::QueryShape shape{.sphere = true, .halfExtents = {radius, radius, radius}};
	return pushHit(
			iState,
			physics::PhysicCommand::shapeCast(*boundScene, shape, origin, direction, distance, luaMask(iState, 9)),
			true);
}

auto luaPhysicsOverlapCircle(lua_State* iState) -> int {
	const math::vec3f centre{luaArg(iState, 1), luaArg(iState, 2), 0.f};
	const float radius = luaArg(iState, 3);
	const auto* boundScene = getBoundScene(iState);
	if (boundScene == nullptr)
		return pushEntities(iState, {});
	const physics::PhysicCommand::QueryShape shape{.sphere = true, .halfExtents = {radius, radius, radius}};
	return pushEntities(iState, physics::PhysicCommand::overlap(*boundScene, shape, centre, luaMask(iState, 4)));
}

auto luaPhysicsOverlapBox(lua_State* iState) -> int {
	const math::vec3f centre{luaArg(iState, 1), luaArg(iState, 2), 0.f};
	const physics::PhysicCommand::QueryShape shape{.sphere = false,
												   .halfExtents = {luaArg(iState, 3), luaArg(iState, 4), 0.5f},
												   .rotation = {0.f, 0.f, luaArg(iState, 5)}};
	const auto* boundScene = getBoundScene(iState);
	if (boundScene == nullptr)
		return pushEntities(iState, {});
	return pushEntities(iState, physics::PhysicCommand::overlap(*boundScene, shape, centre, luaMask(iState, 6)));
}

auto luaPhysicsOverlapSphere(lua_State* iState) -> int {
	const math::vec3f centre{luaArg(iState, 1), luaArg(iState, 2), luaArg(iState, 3)};
	const float radius = luaArg(iState, 4);
	const auto* boundScene = getBoundScene(iState);
	if (boundScene == nullptr)
		return pushEntities(iState, {});
	const physics::PhysicCommand::QueryShape shape{.sphere = true, .halfExtents = {radius, radius, radius}};
	return pushEntities(iState, physics::PhysicCommand::overlap(*boundScene, shape, centre, luaMask(iState, 5)));
}

auto luaPhysicsSetJointMotor(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState)) {
		const float speed = luaArg(iState, 2);
		const bool enabled = lua_isnoneornil(iState, 3) || lua_toboolean(iState, 3) != 0;
		physics::PhysicCommand::setJointMotor(*entity, enabled, speed);
	}
	return 0;
}

auto luaPhysicsGetJointPosition(lua_State* iState) -> int {
	const auto entity = findEntity(iState);
	lua_pushnumber(iState, entity ? static_cast<lua_Number>(physics::PhysicCommand::getJointPosition(*entity)) : 0.);
	return 1;
}

auto luaPhysicsAddForce(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState))
		physics::PhysicCommand::addForce(
				*entity, {luaArg(iState, 2), luaArg(iState, 3), lua_isnoneornil(iState, 4) ? 0.f : luaArg(iState, 4)});
	return 0;
}

auto luaPhysicsAddForceAt(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState))
		physics::PhysicCommand::addForceAtPoint(*entity, {luaArg(iState, 2), luaArg(iState, 3), 0.f},
												{luaArg(iState, 4), luaArg(iState, 5), 0.f});
	return 0;
}

auto luaPhysicsAddForceAt3d(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState))
		physics::PhysicCommand::addForceAtPoint(*entity, {luaArg(iState, 2), luaArg(iState, 3), luaArg(iState, 4)},
												{luaArg(iState, 5), luaArg(iState, 6), luaArg(iState, 7)});
	return 0;
}

auto luaPhysicsAddTorque(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState))
		physics::PhysicCommand::addTorque(*entity, {0.f, 0.f, luaArg(iState, 2)});
	return 0;
}

auto luaPhysicsAddTorque3d(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState))
		physics::PhysicCommand::addTorque(*entity, {luaArg(iState, 2), luaArg(iState, 3), luaArg(iState, 4)});
	return 0;
}

auto luaPhysicsAttach(lua_State* iState) -> int {
	const auto entity = findEntity(iState);
	lua_pushboolean(iState, entity && physics::PhysicCommand::attach(*entity) ? 1 : 0);
	return 1;
}

auto luaPhysicsDetach(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState))
		physics::PhysicCommand::detach(*entity);
	return 0;
}

auto luaPhysicsGetAssembly(lua_State* iState) -> int {
	const auto entity = findEntity(iState);
	const core::UUID root = entity ? physics::PhysicCommand::getAssemblyRoot(*entity) : core::UUID{0};
	if (root == core::UUID{0})
		lua_pushnil(iState);
	else
		pushEntity(iState, root);
	return 1;
}

auto luaPhysicsGetMass(lua_State* iState) -> int {
	const auto entity = findEntity(iState);
	lua_pushnumber(iState, entity ? static_cast<lua_Number>(physics::PhysicCommand::getMass(*entity)) : 0.);
	return 1;
}

auto luaPhysicsGetCenterOfMass(lua_State* iState) -> int {
	const auto entity = findEntity(iState);
	const math::vec3f centre = entity ? physics::PhysicCommand::getCenterOfMass(*entity) : math::vec3f{0.f, 0.f, 0.f};
	lua_pushnumber(iState, static_cast<lua_Number>(centre.x()));
	lua_pushnumber(iState, static_cast<lua_Number>(centre.y()));
	if (!entity || !isSpaceBody(*entity))
		return 2;
	lua_pushnumber(iState, static_cast<lua_Number>(centre.z()));
	return 3;
}

auto inputFrameOf(lua_State* iState) -> const input::InputFrame* {
	const auto* boundScene = getBoundScene(iState);
	return boundScene != nullptr ? &boundScene->getInputFrame() : nullptr;
}

// The device state of the running step: the scene's (replayed in a replay), the devices without a scene.
auto inputStateOf(lua_State* iState) -> input::InputState {
	const auto* frame = inputFrameOf(iState);
	return frame != nullptr ? frame->current : input::Input::readState();
}

auto luaInputIsKeyPressed(lua_State* iState) -> int {
	const auto keyCode = static_cast<input::KeyCode>(checkInteger(iState, 1));
	lua_pushboolean(iState, inputStateOf(iState).isKeyPressed(keyCode) ? 1 : 0);
	return 1;
}

auto luaInputIsMouseButtonPressed(lua_State* iState) -> int {
	const auto button = static_cast<input::MouseCode>(checkInteger(iState, 1));
	lua_pushboolean(iState, inputStateOf(iState).isMouseButtonPressed(button) ? 1 : 0);
	return 1;
}

auto luaInputGetMouseX(lua_State* iState) -> int {
	lua_pushnumber(iState, static_cast<lua_Number>(inputStateOf(iState).cursor.x()));
	return 1;
}

auto luaInputGetMouseY(lua_State* iState) -> int {
	lua_pushnumber(iState, static_cast<lua_Number>(inputStateOf(iState).cursor.y()));
	return 1;
}

template<bool (input::InputFrame::*Edge)(input::KeyCode) const>
auto luaInputKeyEdge(lua_State* iState) -> int {
	const auto keyCode = static_cast<input::KeyCode>(checkInteger(iState, 1));
	const auto* frame = inputFrameOf(iState);
	lua_pushboolean(iState, frame != nullptr && (frame->*Edge)(keyCode) ? 1 : 0);
	return 1;
}

template<bool (input::InputFrame::*Edge)(input::MouseCode) const>
auto luaInputMouseButtonEdge(lua_State* iState) -> int {
	const auto button = static_cast<input::MouseCode>(checkInteger(iState, 1));
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

auto luaInputSetCursorVisible(lua_State* iState) -> int {
	if (auto* activeScene = getBoundScene(iState); activeScene != nullptr)
		activeScene->setCursorHidden(lua_toboolean(iState, 1) == 0);
	return 0;
}

auto luaInputIsCursorVisible(lua_State* iState) -> int {
	const auto* activeScene = getBoundScene(iState);
	lua_pushboolean(iState, activeScene == nullptr || !activeScene->wantsCursorHidden() ? 1 : 0);
	return 1;
}

auto checkGamepadSlot(lua_State* iState, const int iArg) -> std::size_t {
	const auto pad = checkInteger(iState, iArg);
	return pad >= 0 && pad < static_cast<int64_t>(input::g_GamepadCount) ? static_cast<std::size_t>(pad)
																		 : input::g_GamepadCount;
}

template<typename Code, std::size_t Count>
auto checkGamepadCode(lua_State* iState, const int iArg) -> Code {
	const auto code = checkInteger(iState, iArg);
	return code >= 0 && code < static_cast<int64_t>(Count) ? static_cast<Code>(code) : static_cast<Code>(Count);
}

auto luaInputIsGamepadConnected(lua_State* iState) -> int {
	const auto pad = checkGamepadSlot(iState, 1);
	lua_pushboolean(iState, inputStateOf(iState).isGamepadConnected(pad) ? 1 : 0);
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
	lua_pushboolean(iState, inputStateOf(iState).isGamepadButtonPressed(pad, button) ? 1 : 0);
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
	lua_pushnumber(iState, static_cast<lua_Number>(inputStateOf(iState).getGamepad(pad).getAxis(axis)));
	return 1;
}

auto luaInputGetGamepadStick(lua_State* iState) -> int {
	const auto pad = checkGamepadSlot(iState, 1);
	const auto stick = checkInteger(iState, 2) == 1 ? input::gamepad::Stick::Right : input::gamepad::Stick::Left;
	const input::AxisResponse response{.deadZone = static_cast<float>(luaL_optnumber(
											   iState, 3, lua_Number{input::AxisResponse::defaultDeadZone}))};
	const auto shaped = input::applyStickResponse(inputStateOf(iState).getGamepad(pad).getStick(stick), response);
	lua_pushnumber(iState, static_cast<lua_Number>(shaped.x()));
	lua_pushnumber(iState, static_cast<lua_Number>(shaped.y()));
	return 2;
}

auto luaSoundPlay(lua_State* iState) -> int {
	const auto assetPath = data::assets::toAssetPath(luaL_checkstring(iState, 1));
	if (sound::SoundSystem::getState() != sound::SoundSystem::State::Running) {
		pushInteger(iState, static_cast<int64_t>(sound::invalidSoundHandle));
		return 1;
	}
	const auto handle = sound::SoundHelper::playAsset(assetPath, sound::PlayParams{});
	pushInteger(iState, static_cast<int64_t>(handle));
	return 1;
}

auto luaSoundStop(lua_State* iState) -> int {
	const auto handle = static_cast<sound::SoundHandle>(checkInteger(iState, 1));
	sound::SoundCommand::stop(handle);
	return 0;
}

auto luaSoundPause(lua_State* iState) -> int {
	const auto handle = static_cast<sound::SoundHandle>(checkInteger(iState, 1));
	sound::SoundCommand::pause(handle);
	return 0;
}

auto luaSoundResume(lua_State* iState) -> int {
	const auto handle = static_cast<sound::SoundHandle>(checkInteger(iState, 1));
	sound::SoundCommand::resume(handle);
	return 0;
}

auto luaSoundSetVolume(lua_State* iState) -> int {
	const auto handle = static_cast<sound::SoundHandle>(checkInteger(iState, 1));
	const auto vol = static_cast<float>(luaL_checknumber(iState, 2));
	sound::SoundCommand::setVolume(handle, vol);
	return 0;
}

auto checkBus(lua_State* iState, const int iIndex) -> std::optional<sound::Bus> {
	const char* name = luaL_checkstring(iState, iIndex);
	const auto bus = sound::parseBus(name);
	if (!bus)
		OWL_CORE_WARN("Lua: Unknown sound bus '{}' (Master, Music, SFX, Ambient, Voice).", name)
	return bus;
}

auto luaSoundSetBusVolume(lua_State* iState) -> int {
	const auto bus = checkBus(iState, 1);
	const auto volume = static_cast<float>(luaL_checknumber(iState, 2));
	if (auto* mixer = sound::SoundCommand::getMixer(); bus && mixer != nullptr)
		mixer->setBusVolume(*bus, volume);
	return 0;
}

auto luaSoundGetBusVolume(lua_State* iState) -> int {
	const auto bus = checkBus(iState, 1);
	const auto* mixer = sound::SoundCommand::getMixer();
	lua_pushnumber(iState, bus && mixer != nullptr ? static_cast<lua_Number>(mixer->getBusVolume(*bus)) : 0.0);
	return 1;
}

auto luaSoundSetBusMuted(lua_State* iState) -> int {
	const auto bus = checkBus(iState, 1);
	const bool muted = lua_toboolean(iState, 2) != 0;
	if (auto* mixer = sound::SoundCommand::getMixer(); bus && mixer != nullptr)
		mixer->setBusMuted(*bus, muted);
	return 0;
}

auto luaSoundIsBusMuted(lua_State* iState) -> int {
	const auto bus = checkBus(iState, 1);
	const auto* mixer = sound::SoundCommand::getMixer();
	lua_pushboolean(iState, bus && mixer != nullptr && mixer->isBusMuted(*bus) ? 1 : 0);
	return 1;
}

auto luaSoundPlayMusic(lua_State* iState) -> int {
	const auto assetPath = data::assets::toAssetPath(luaL_checkstring(iState, 1));
	const auto fade = static_cast<float>(luaL_optnumber(iState, 2, 1.0));
	pushInteger(iState, static_cast<int64_t>(sound::SoundHelper::playMusic(assetPath, fade)));
	return 1;
}

auto luaSoundStopMusic(lua_State* iState) -> int {
	const auto fade = static_cast<float>(luaL_optnumber(iState, 1, 1.0));
	if (auto* mixer = sound::SoundCommand::getMixer(); mixer != nullptr)
		mixer->stopMusic(fade);
	return 0;
}

auto luaSoundCrossfade(lua_State* iState) -> int {
	const auto from = static_cast<sound::SoundHandle>(checkInteger(iState, 1));
	const auto to = static_cast<sound::SoundHandle>(checkInteger(iState, 2));
	const auto seconds = static_cast<float>(luaL_checknumber(iState, 3));
	if (auto* mixer = sound::SoundCommand::getMixer(); mixer != nullptr)
		mixer->crossfade(from, to, seconds);
	return 0;
}

auto luaSoundSetMusicState(lua_State* iState) -> int {
	const std::string state = luaL_checkstring(iState, 1);
	auto* activeScene = getBoundScene(iState);
	lua_pushboolean(iState, activeScene != nullptr && activeScene->setMusicState(state) ? 1 : 0);
	return 1;
}

auto luaSoundGetMusicState(lua_State* iState) -> int {
	const auto* activeScene = getBoundScene(iState);
	const std::string state = activeScene != nullptr ? activeScene->getMusicState() : std::string{};
	lua_pushstring(iState, state.c_str());
	return 1;
}

auto luaSceneFindEntity(lua_State* iState) -> int {
	auto* activeScene = getBoundScene(iState);
	if (activeScene == nullptr) {
		pushInteger(iState, 0);
		return 1;
	}
	const char* name = luaL_checkstring(iState, 1);
	uint64_t found = findByTag(*activeScene, *activeScene, name);
	if (const auto hud = activeScene->getHud(); found == 0 && hud)
		found = findByTag(*activeScene, *hud, name);
	pushHandle(iState, found);
	return 1;
}

auto luaSceneFindByUuid(lua_State* iState) -> int {
	const char* text = luaL_checkstring(iState, 1);
	const auto* activeScene = getBoundScene(iState);
	uint64_t uuid = 0;
	const std::string_view view{text};
	if (const auto [ptr, ec] = std::from_chars(view.data(), view.data() + view.size(), uuid);
		activeScene == nullptr || ec != std::errc{} || ptr != view.data() + view.size()) {
		pushHandle(iState, 0);
		return 1;
	}
	pushHandle(iState, getEntityHandle(*activeScene, core::UUID{uuid}));
	return 1;
}

auto luaSceneFindEntities(lua_State* iState) -> int {
	lua_newtable(iState);
	auto* activeScene = getBoundScene(iState);
	if (activeScene == nullptr)
		return 1;
	const char* name = luaL_checkstring(iState, 1);
	int64_t index = 0;
	for (const auto view = activeScene->registry.view<scene::component::Tag>(); const auto entity: view) {
		if (view.get<scene::component::Tag>(entity).tag != name)
			continue;
		pushEntity(iState, scene::Entity{entity, activeScene});
		lua_rawseti(iState, -2, static_cast<int>(++index));
	}
	return 1;
}

auto luaUiFind(lua_State* iState) -> int {
	auto* activeScene = getBoundScene(iState);
	if (activeScene == nullptr) {
		pushInteger(iState, 0);
		return 1;
	}
	const char* name = luaL_checkstring(iState, 1);
	uint64_t found = 0;
	if (const auto hud = activeScene->getHud(); hud)
		found = findByTag(*activeScene, *hud, name);
	if (found == 0)
		found = findByTag(*activeScene, *activeScene, name);
	pushHandle(iState, found);
	return 1;
}

auto luaSceneCreateEntity(lua_State* iState) -> int {
	auto* activeScene = getBoundScene(iState);
	if (activeScene == nullptr) {
		pushInteger(iState, 0);
		return 1;
	}
	const char* name = luaL_checkstring(iState, 1);
	const auto entity = activeScene->createEntity(name);
	pushEntity(iState, entity);
	return 1;
}

auto luaSceneDestroyEntity(lua_State* iState) -> int {
	auto* activeScene = getBoundScene(iState);
	if (activeScene == nullptr)
		return 0;
	if (const auto entity = entityAt(iState, 1); entity)
		activeScene->destroyEntityDeferred(*entity);
	return 0;
}

auto luaSceneSpawnPrefab(lua_State* iState) -> int {
	const char* path = luaL_checkstring(iState, 1);
	const bool placed = !lua_isnoneornil(iState, 2);
	const auto x = static_cast<float>(luaL_optnumber(iState, 2, 0.0));
	const auto y = static_cast<float>(luaL_optnumber(iState, 3, 0.0));
	const auto z = static_cast<float>(luaL_optnumber(iState, 4, 0.0));
	auto* activeScene = getBoundScene(iState);
	if (activeScene == nullptr) {
		pushInteger(iState, 0);
		return 1;
	}
	const auto text = data::assets::readAssetText(path);
	if (!text) {
		OWL_CORE_WARN("Lua: Prefab '{}' not found, nothing spawned.", path)
		pushInteger(iState, 0);
		return 1;
	}
	const auto root = scene::PrefabSerializer::instantiateFromString(*text, *activeScene, path);
	if (!root) {
		pushInteger(iState, 0);
		return 1;
	}
	if (placed)
		root.getComponent<scene::component::Transform>().transform.translation() = {x, y, z};
	activeScene->startSpawnedSubtree(root);
	pushEntity(iState, root);
	return 1;
}

auto luaScenePickEntity(lua_State* iState) -> int {
	auto* activeScene = getBoundScene(iState);
	if (activeScene == nullptr) {
		pushInteger(iState, 0);
		return 1;
	}
	// The viewport mouse of the last HUD update: in Owl Nest the game is drawn inside a panel, not at the window origin.
	const auto& uiState = activeScene->getUiInputState();
	const auto mouse = uiState.hasMouse ? uiState.mousePosition : activeScene->getInputFrame().getCursor();
	const auto pointX = static_cast<float>(luaL_optnumber(iState, 1, static_cast<lua_Number>(mouse.x())));
	const auto pointY = static_cast<float>(luaL_optnumber(iState, 2, static_cast<lua_Number>(mouse.y())));
	const std::string_view tag = luaL_optstring(iState, 3, "");
	const auto picked = scene::pickEntity(*activeScene, {pointX, pointY}, tag);
	pushEntity(iState, picked);
	return 1;
}

auto luaSceneIsWorldCellLoaded(lua_State* iState) -> int {
	const math::vec3i cell{static_cast<int32_t>(checkInteger(iState, 1)), static_cast<int32_t>(checkInteger(iState, 2)),
						   static_cast<int32_t>(checkInteger(iState, 3))};
	auto* activeScene = getBoundScene(iState);
	const bool loaded = activeScene != nullptr && activeScene->hasWorldStreamer() &&
						activeScene->getWorldStreamer().getState(cell) == scene::WorldCellState::Loaded;
	lua_pushboolean(iState, loaded ? 1 : 0);
	return 1;
}

auto luaSceneGetLoadedWorldCells(lua_State* iState) -> int {
	auto* activeScene = getBoundScene(iState);
	const auto count = activeScene != nullptr && activeScene->hasWorldStreamer()
							   ? activeScene->getWorldStreamer().getStats().loaded
							   : 0u;
	pushInteger(iState, static_cast<int64_t>(count));
	return 1;
}

auto luaSceneSetWorldStreaming(lua_State* iState) -> int {
	const bool enabled = lua_toboolean(iState, 1) != 0;
	if (auto* activeScene = getBoundScene(iState); activeScene != nullptr)
		activeScene->getWorldPartitionSettings().enabled = enabled;
	return 0;
}

auto luaSceneGetPlanetAltitude(lua_State* iState) -> int {
	auto planet = entityAt(iState, 1).value_or(scene::Entity{});
	const auto entity = entityAt(iState, 2).value_or(scene::Entity{});
	auto* activeScene = getBoundScene(iState);
	if (activeScene == nullptr) {
		lua_pushnumber(iState, 0.0);
		return 1;
	}
	if (!planet || !entity || !planet.hasComponent<scene::component::Planet>()) {
		lua_pushnumber(iState, 0.0);
		return 1;
	}
	const math::vec3d offset =
			activeScene->getEntityWorldPosition(entity) - activeScene->getEntityWorldPosition(planet);
	const double distance = offset.norm();
	const auto& surface = planet.getComponent<scene::component::Planet>().getSurface();
	const double ground =
			distance > 0.0 ? surface->getSurfaceRadius(offset * (1.0 / distance)) : surface->getSettings().radius;
	lua_pushnumber(iState, static_cast<lua_Number>(distance - ground));
	return 1;
}

auto luaSceneGetViewportSize(lua_State* iState) -> int {
	const auto* activeScene = getBoundScene(iState);
	const math::vec2ui size = activeScene != nullptr ? activeScene->getViewportSize() : math::vec2ui{0u, 0u};
	pushInteger(iState, static_cast<int64_t>(size.x()));
	pushInteger(iState, static_cast<int64_t>(size.y()));
	return 2;
}

auto luaSceneGetViewportMouse(lua_State* iState) -> int {
	auto* activeScene = getBoundScene(iState);
	// The viewport mouse of the last HUD update, as `scene.pick_entity` reads it (Owl Nest draws the game in a panel).
	math::vec2 mouse = inputStateOf(iState).cursor;
	if (activeScene != nullptr && activeScene->getUiInputState().hasMouse)
		mouse = activeScene->getUiInputState().mousePosition;
	lua_pushnumber(iState, static_cast<lua_Number>(mouse.x()));
	lua_pushnumber(iState, static_cast<lua_Number>(mouse.y()));
	return 2;
}

auto luaSceneLoadScene(lua_State* iState) -> int {
	auto* activeScene = getBoundScene(iState);
	if (activeScene == nullptr)
		return 0;
	const char* levelName = luaL_checkstring(iState, 1);
	activeScene->teleportRequest.pending = true;
	activeScene->teleportRequest.levelName = data::assets::toAssetPath(levelName);
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
	if (top >= 6) {
		req.color = math::vec4{static_cast<float>(luaL_checknumber(iState, 4)),
							   static_cast<float>(luaL_checknumber(iState, 5)),
							   static_cast<float>(luaL_checknumber(iState, 6)),
							   top >= 7 ? static_cast<float>(luaL_checknumber(iState, 7)) : 1.f};
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

auto luaEngineVersion(lua_State* iState) -> int {
	static const std::string s_version = getVersionString();
	lua_pushlstring(iState, s_version.data(), s_version.size());
	return 1;
}

auto luaTimeDelta(lua_State* iState) -> int {
	lua_pushnumber(iState, static_cast<lua_Number>(LuaEngine::getDeltaTime(iState)));
	return 1;
}

auto luaTimeRealDelta(lua_State* iState) -> int {
	const auto* activeScene = getBoundScene(iState);
	lua_pushnumber(iState, activeScene != nullptr ? static_cast<lua_Number>(activeScene->getRealFrameSeconds())
												  : static_cast<lua_Number>(LuaEngine::getDeltaTime(iState)));
	return 1;
}

auto luaTimeSetScale(lua_State* iState) -> int {
	const auto scale = static_cast<float>(luaL_checknumber(iState, 1));
	if (auto* activeScene = getBoundScene(iState); activeScene != nullptr)
		activeScene->setTimeScale(scale);
	return 0;
}

auto luaTimeGetScale(lua_State* iState) -> int {
	const auto* activeScene = getBoundScene(iState);
	lua_pushnumber(iState, activeScene != nullptr ? static_cast<lua_Number>(activeScene->getTimeScale()) : 1.0);
	return 1;
}

auto luaTimeSetPaused(lua_State* iState) -> int {
	const bool paused = lua_toboolean(iState, 1) != 0;
	if (auto* activeScene = getBoundScene(iState); activeScene != nullptr)
		activeScene->setPaused(paused);
	return 0;
}

auto luaTimeIsPaused(lua_State* iState) -> int {
	const auto* activeScene = getBoundScene(iState);
	lua_pushboolean(iState, activeScene != nullptr && activeScene->isPaused() ? 1 : 0);
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

auto luaLogDebug(lua_State* iState) -> int {
	const char* message = luaL_checkstring(iState, 1);
	OWL_DEBUG("{}", message)
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
	else if (compName == "MeshRenderer")
		has = entity->hasComponent<scene::component::MeshRenderer>();
	else if (compName == "Light")
		has = entity->hasComponent<scene::component::Light>();
	else if (compName == "Light2D")
		has = entity->hasComponent<scene::component::Light2D>();
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
	else if (compName == "HudMarker")
		has = entity->hasComponent<scene::component::HudMarker>();
	else if (compName == "UiFocus")
		has = entity->hasComponent<scene::component::UiFocus>();
	else if (compName == "UiScrollView")
		has = entity->hasComponent<scene::component::UiScrollView>();
	else if (compName == "UiList")
		has = entity->hasComponent<scene::component::UiList>();
	lua_pushboolean(iState, has ? 1 : 0);
	return 1;
}

auto luaEntityAddComponent(lua_State* iState) -> int {
	const char* key = luaL_checkstring(iState, 2);
	auto entity = findEntity(iState);
	const auto* descriptor = scene::ComponentRegistry::find(key);
	if (!entity || descriptor == nullptr || !descriptor->optional || !descriptor->add) {
		lua_pushboolean(iState, 0);
		return 1;
	}
	if (!descriptor->has(*entity))
		descriptor->add(*entity);
	lua_pushboolean(iState, 1);
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
			pushEntity(iState, link.linkedEntity);
			return 1;
		}
	}
	pushInteger(iState, 0);
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
	else if (entity && entity->hasComponent<scene::component::MeshRenderer>())
		entity->getComponent<scene::component::MeshRenderer>().color = tint;
	else if (entity && entity->hasComponent<scene::component::ParticleEmitter>())
		entity->getComponent<scene::component::ParticleEmitter>().settings.color = tint;
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
	else if (entity->hasComponent<scene::component::MeshRenderer>())
		tint = entity->getComponent<scene::component::MeshRenderer>().color;
	else if (entity->hasComponent<scene::component::ParticleEmitter>())
		tint = entity->getComponent<scene::component::ParticleEmitter>().settings.color;
	else
		return 0;
	lua_pushnumber(iState, static_cast<lua_Number>(tint.x()));
	lua_pushnumber(iState, static_cast<lua_Number>(tint.y()));
	lua_pushnumber(iState, static_cast<lua_Number>(tint.z()));
	lua_pushnumber(iState, static_cast<lua_Number>(tint.w()));
	return 4;
}

auto luaEntitySetMesh(lua_State* iState) -> int {
	const char* path = luaL_checkstring(iState, 2);
	const auto entity = findEntity(iState);
	if (!entity || !entity->hasComponent<scene::component::MeshRenderer>()) {
		lua_pushboolean(iState, 0);
		return 1;
	}
	entity->getComponent<scene::component::MeshRenderer>().setMeshPath(data::assets::toAssetPath(path));
	lua_pushboolean(iState, 1);
	return 1;
}

auto luaEntityGetMesh(lua_State* iState) -> int {
	const auto entity = findEntity(iState);
	if (!entity || !entity->hasComponent<scene::component::MeshRenderer>())
		return 0;
	lua_pushstring(iState, entity->getComponent<scene::component::MeshRenderer>().meshPath.generic_string().c_str());
	return 1;
}

auto luaEntitySetMaterial(lua_State* iState) -> int {
	const auto slot = static_cast<int64_t>(checkInteger(iState, 2));
	const std::string path = data::assets::toAssetPath(luaL_checkstring(iState, 3));
	const auto entity = findEntity(iState);
	if (!entity || !entity->hasComponent<scene::component::MeshRenderer>() || slot < -1) {
		lua_pushboolean(iState, 0);
		return 1;
	}
	auto& meshRenderer = entity->getComponent<scene::component::MeshRenderer>();
	const size_t known = std::max(meshRenderer.materials.size(),
								  meshRenderer.mesh ? meshRenderer.mesh->getSubmeshes().size() : size_t{0});
	const size_t count = slot < 0 ? known : static_cast<size_t>(slot) + 1;
	if (count == 0) {
		lua_pushboolean(iState, 0);
		return 1;
	}
	if (meshRenderer.materials.size() < count)
		meshRenderer.materials.resize(count);
	for (size_t index = slot < 0 ? 0 : static_cast<size_t>(slot); index < count; ++index) {
		meshRenderer.materials[index].material = path;
		meshRenderer.materials[index].invalidate();
	}
	lua_pushboolean(iState, 1);
	return 1;
}

auto luaEntityGetMaterial(lua_State* iState) -> int {
	const auto slot = static_cast<int64_t>(checkInteger(iState, 2));
	const auto entity = findEntity(iState);
	if (!entity || !entity->hasComponent<scene::component::MeshRenderer>() || slot < 0)
		return 0;
	const auto& materials = entity->getComponent<scene::component::MeshRenderer>().materials;
	const auto index = static_cast<size_t>(slot);
	lua_pushstring(iState, index < materials.size() ? materials[index].material.generic_string().c_str() : "");
	return 1;
}

// The fields the light bindings reach on an entity's `Light`, or else its `Light2D` (range: the radius).
struct LightFields {
	math::vec3* color = nullptr;
	float* intensity = nullptr;
	bool* enabled = nullptr;
	float* range = nullptr;
};

auto lightFieldsOf(const std::optional<scene::Entity>& iEntity) -> std::optional<LightFields> {
	if (!iEntity)
		return std::nullopt;
	if (iEntity->hasComponent<scene::component::Light>()) {
		auto& light = iEntity->getComponent<scene::component::Light>();
		return LightFields{.color = &light.color,
						   .intensity = &light.intensity,
						   .enabled = &light.enabled,
						   .range = &light.range};
	}
	if (iEntity->hasComponent<scene::component::Light2D>()) {
		auto& light = iEntity->getComponent<scene::component::Light2D>();
		return LightFields{.color = &light.color,
						   .intensity = &light.intensity,
						   .enabled = &light.enabled,
						   .range = &light.radius};
	}
	return std::nullopt;
}

auto luaEntitySetLightColor(lua_State* iState) -> int {
	const auto red = static_cast<float>(luaL_checknumber(iState, 2));
	const auto green = static_cast<float>(luaL_checknumber(iState, 3));
	const auto blue = static_cast<float>(luaL_checknumber(iState, 4));
	const auto fields = lightFieldsOf(findEntity(iState));
	if (fields)
		*fields->color = math::vec3{red, green, blue};
	lua_pushboolean(iState, fields ? 1 : 0);
	return 1;
}

auto luaEntityGetLightColor(lua_State* iState) -> int {
	const auto fields = lightFieldsOf(findEntity(iState));
	if (!fields)
		return 0;
	lua_pushnumber(iState, static_cast<lua_Number>(fields->color->x()));
	lua_pushnumber(iState, static_cast<lua_Number>(fields->color->y()));
	lua_pushnumber(iState, static_cast<lua_Number>(fields->color->z()));
	return 3;
}

auto luaEntitySetLightIntensity(lua_State* iState) -> int {
	const auto intensity = static_cast<float>(luaL_checknumber(iState, 2));
	const auto fields = lightFieldsOf(findEntity(iState));
	if (fields)
		*fields->intensity = std::max(0.f, intensity);
	lua_pushboolean(iState, fields ? 1 : 0);
	return 1;
}

// The particle system of the entity's emitter, made when a script reaches it before its first frame.
auto particleSystemOf(lua_State* iState) -> renderer::ParticleSystem* {
	const auto entity = findEntity(iState);
	if (!entity || !entity->hasComponent<scene::component::ParticleEmitter>())
		return nullptr;
	auto& emitter = entity->getComponent<scene::component::ParticleEmitter>();
	if (!emitter.m_slot.system)
		emitter.m_slot.system = renderer::RendererParticles::createSystem();
	return emitter.m_slot.system.get();
}

// `entity.particles_<action>(id)`: one call of the entity's particle system.
template<void (renderer::ParticleSystem::*Action)()>
auto luaEntityParticles(lua_State* iState) -> int {
	auto* const system = particleSystemOf(iState);
	if (system != nullptr)
		(system->*Action)();
	lua_pushboolean(iState, system != nullptr ? 1 : 0);
	return 1;
}

auto luaEntityParticlesEmit(lua_State* iState) -> int {
	const auto count = checkInteger(iState, 2);
	auto* const system = particleSystemOf(iState);
	if (system != nullptr && count > 0)
		system->emit(static_cast<uint32_t>(std::min<int64_t>(count, renderer::g_maxParticles)));
	lua_pushboolean(iState, system != nullptr ? 1 : 0);
	return 1;
}

auto luaEntityParticlesIsEmitting(lua_State* iState) -> int {
	const auto* const system = particleSystemOf(iState);
	if (system == nullptr)
		return 0;
	lua_pushboolean(iState, system->isEmitting() && system->isPlaying() ? 1 : 0);
	return 1;
}

// `entity.set_<x>_enabled(id, enabled)` of a behaviour component with an `enabled` field.
template<typename Component>
auto luaEntitySetBehaviourEnabled(lua_State* iState) -> int {
	const bool enabled = lua_toboolean(iState, 2) != 0;
	const auto entity = findEntity(iState);
	if (!entity || !entity->hasComponent<Component>()) {
		lua_pushboolean(iState, 0);
		return 1;
	}
	entity->getComponent<Component>().enabled = enabled;
	lua_pushboolean(iState, 1);
	return 1;
}

// `entity.is_<x>_enabled(id)` of a behaviour component with an `enabled` field.
template<typename Component>
auto luaEntityIsBehaviourEnabled(lua_State* iState) -> int {
	const auto entity = findEntity(iState);
	if (!entity || !entity->hasComponent<Component>())
		return 0;
	lua_pushboolean(iState, entity->getComponent<Component>().enabled ? 1 : 0);
	return 1;
}

auto luaEntityGetTimeOfDay(lua_State* iState) -> int {
	const auto entity = findEntity(iState);
	if (!entity || !entity->hasComponent<scene::component::DayNightCycle>())
		return 0;
	lua_pushnumber(iState, static_cast<lua_Number>(entity->getComponent<scene::component::DayNightCycle>().timeOfDay));
	return 1;
}

auto luaEntityGetShipVelocity(lua_State* iState) -> int {
	const auto entity = findEntity(iState);
	if (!entity || !entity->hasComponent<scene::component::ShipController>())
		return 0;
	const auto& velocity = entity->getComponent<scene::component::ShipController>().m_velocity;
	lua_pushnumber(iState, static_cast<lua_Number>(velocity.x()));
	lua_pushnumber(iState, static_cast<lua_Number>(velocity.y()));
	lua_pushnumber(iState, static_cast<lua_Number>(velocity.z()));
	return 3;
}

auto luaEntityGetShipSpeed(lua_State* iState) -> int {
	const auto entity = findEntity(iState);
	if (!entity || !entity->hasComponent<scene::component::ShipController>())
		return 0;
	lua_pushnumber(iState,
				   static_cast<lua_Number>(entity->getComponent<scene::component::ShipController>().getSpeed()));
	return 1;
}

auto luaEntityGetShipThrottle(lua_State* iState) -> int {
	const auto entity = findEntity(iState);
	if (!entity || !entity->hasComponent<scene::component::ShipController>())
		return 0;
	lua_pushnumber(iState,
				   static_cast<lua_Number>(entity->getComponent<scene::component::ShipController>().m_throttle));
	return 1;
}

auto luaEntitySetShipAssist(lua_State* iState) -> int {
	const bool assist = lua_toboolean(iState, 2) != 0;
	const auto entity = findEntity(iState);
	if (!entity || !entity->hasComponent<scene::component::ShipController>()) {
		lua_pushboolean(iState, 0);
		return 1;
	}
	entity->getComponent<scene::component::ShipController>().flightAssist = assist;
	lua_pushboolean(iState, 1);
	return 1;
}

auto luaEntityIsShipAssistOn(lua_State* iState) -> int {
	const auto entity = findEntity(iState);
	if (!entity || !entity->hasComponent<scene::component::ShipController>())
		return 0;
	lua_pushboolean(iState, entity->getComponent<scene::component::ShipController>().flightAssist ? 1 : 0);
	return 1;
}

// `animator.set_float` / `set_bool`: a parameter of the machine of the entity's `Animator`.
auto setAnimatorParameter(lua_State* iState, const std::string_view iName, const float iValue) -> int {
	const auto entity = findEntity(iState);
	if (!entity || !entity->hasComponent<scene::component::Animator>()) {
		lua_pushboolean(iState, 0);
		return 1;
	}
	auto& animator = entity->getComponent<scene::component::Animator>();
	lua_pushboolean(iState,
					animator.player.isReady() && animator.player.setParameter(animator.machine, iName, iValue) ? 1 : 0);
	return 1;
}

auto luaAnimatorSetFloat(lua_State* iState) -> int {
	const char* name = luaL_checkstring(iState, 2);
	const auto value = static_cast<float>(luaL_checknumber(iState, 3));
	return setAnimatorParameter(iState, name, value);
}

auto luaAnimatorSetBool(lua_State* iState) -> int {
	const char* name = luaL_checkstring(iState, 2);
	const bool value = lua_toboolean(iState, 3) != 0;
	return setAnimatorParameter(iState, name, value ? 1.f : 0.f);
}

auto luaAnimatorTrigger(lua_State* iState) -> int {
	const char* name = luaL_checkstring(iState, 2);
	return setAnimatorParameter(iState, name, 1.f);
}

auto luaAnimatorGetParameter(lua_State* iState) -> int {
	const char* name = luaL_checkstring(iState, 2);
	const auto entity = findEntity(iState);
	if (!entity || !entity->hasComponent<scene::component::Animator>())
		return 0;
	const auto& animator = entity->getComponent<scene::component::Animator>();
	const auto value = animator.player.isReady() ? animator.player.getParameter(animator.machine, name) : std::nullopt;
	if (!value)
		return 0;
	lua_pushnumber(iState, static_cast<lua_Number>(*value));
	return 1;
}

auto luaAnimatorGetState(lua_State* iState) -> int {
	const auto entity = findEntity(iState);
	if (!entity || !entity->hasComponent<scene::component::Animator>())
		return 0;
	const auto& animator = entity->getComponent<scene::component::Animator>();
	const auto state = animator.player.getCurrentState();
	if (!state || *state >= animator.machine.states.size())
		return 0;
	const auto& name = animator.machine.states[*state].name;
	lua_pushlstring(iState, name.data(), name.size());
	return 1;
}

auto luaAnimatorPlay(lua_State* iState) -> int {
	const char* state = luaL_checkstring(iState, 2);
	const auto fade = static_cast<float>(luaL_optnumber(iState, 3, 0.2));
	const auto entity = findEntity(iState);
	if (!entity || !entity->hasComponent<scene::component::Animator>()) {
		lua_pushboolean(iState, 0);
		return 1;
	}
	auto& animator = entity->getComponent<scene::component::Animator>();
	lua_pushboolean(iState, animator.player.isReady() && animator.player.play(animator.machine, state, fade) ? 1 : 0);
	return 1;
}

auto luaAnimatorSetPlaying(lua_State* iState) -> int {
	const bool playing = lua_toboolean(iState, 2) != 0;
	const auto entity = findEntity(iState);
	if (!entity || !entity->hasComponent<scene::component::Animator>()) {
		lua_pushboolean(iState, 0);
		return 1;
	}
	entity->getComponent<scene::component::Animator>().playing = playing;
	lua_pushboolean(iState, 1);
	return 1;
}

auto luaSocketAttach(lua_State* iState) -> int {
	const auto owner = entityAt(iState, 2).value_or(scene::Entity{});
	const char* socket = luaL_checkstring(iState, 3);
	auto* activeScene = getBoundScene(iState);
	const auto entity = findEntity(iState);
	if (activeScene == nullptr || !entity) {
		lua_pushboolean(iState, 0);
		return 1;
	}
	lua_pushboolean(iState, activeScene->attachToSocket(*entity, owner, socket) ? 1 : 0);
	return 1;
}

auto luaSocketDetach(lua_State* iState) -> int {
	auto* activeScene = getBoundScene(iState);
	const auto entity = findEntity(iState);
	lua_pushboolean(iState, activeScene != nullptr && entity && activeScene->detachFromSocket(*entity) ? 1 : 0);
	return 1;
}

auto luaSocketGetPosition(lua_State* iState) -> int {
	const char* socket = luaL_checkstring(iState, 2);
	const auto* activeScene = getBoundScene(iState);
	const auto entity = findEntity(iState);
	if (activeScene == nullptr || !entity)
		return 0;
	const auto local = activeScene->getSocketTransform(*entity, socket);
	if (!local)
		return 0;
	const math::mat4 world = activeScene->getWorldTransform(*entity)() * *local;
	lua_pushnumber(iState, static_cast<lua_Number>(world(0, 3)));
	lua_pushnumber(iState, static_cast<lua_Number>(world(1, 3)));
	lua_pushnumber(iState, static_cast<lua_Number>(world(2, 3)));
	return 3;
}

auto luaEntitySetTimeOfDay(lua_State* iState) -> int {
	const auto hours = static_cast<float>(luaL_checknumber(iState, 2));
	const auto entity = findEntity(iState);
	if (!entity || !entity->hasComponent<scene::component::DayNightCycle>()) {
		lua_pushboolean(iState, 0);
		return 1;
	}
	entity->getComponent<scene::component::DayNightCycle>().timeOfDay = std::fmod(std::fmod(hours, 24.f) + 24.f, 24.f);
	lua_pushboolean(iState, 1);
	return 1;
}

auto luaEntityGetLightIntensity(lua_State* iState) -> int {
	const auto fields = lightFieldsOf(findEntity(iState));
	if (!fields)
		return 0;
	lua_pushnumber(iState, static_cast<lua_Number>(*fields->intensity));
	return 1;
}

auto luaEntitySetLightEnabled(lua_State* iState) -> int {
	const bool enabled = lua_toboolean(iState, 2) != 0;
	const auto fields = lightFieldsOf(findEntity(iState));
	if (fields)
		*fields->enabled = enabled;
	lua_pushboolean(iState, fields ? 1 : 0);
	return 1;
}

auto luaEntityIsLightEnabled(lua_State* iState) -> int {
	const auto fields = lightFieldsOf(findEntity(iState));
	if (!fields)
		return 0;
	lua_pushboolean(iState, *fields->enabled ? 1 : 0);
	return 1;
}

auto luaEntitySetLightRange(lua_State* iState) -> int {
	const auto range = static_cast<float>(luaL_checknumber(iState, 2));
	const auto fields = lightFieldsOf(findEntity(iState));
	if (fields)
		*fields->range = std::max(0.01f, range);
	lua_pushboolean(iState, fields ? 1 : 0);
	return 1;
}

auto luaUiSetText(lua_State* iState) -> int {
	const char* text = luaL_checkstring(iState, 2);
	if (const auto entity = findEntity(iState); entity) {
		if (entity->hasComponent<scene::component::UiText>())
			entity->getComponent<scene::component::UiText>().text = text;
		else if (entity->hasComponent<scene::component::Text>())
			entity->getComponent<scene::component::Text>().text = text;
	}
	return 0;
}

auto luaUiGetText(lua_State* iState) -> int {
	if (const auto entity = findEntity(iState); entity && entity->hasComponent<scene::component::UiText>()) {
		lua_pushstring(iState, entity->getComponent<scene::component::UiText>().text.c_str());
		return 1;
	}
	if (const auto entity = findEntity(iState); entity && entity->hasComponent<scene::component::Text>()) {
		lua_pushstring(iState, entity->getComponent<scene::component::Text>().text.c_str());
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

auto luaUiSetSize(lua_State* iState) -> int {
	const auto width = static_cast<float>(luaL_checknumber(iState, 2));
	const auto height = static_cast<float>(luaL_checknumber(iState, 3));
	const auto entity = findEntity(iState);
	if (!entity || !entity->hasComponent<scene::component::UiRect>()) {
		lua_pushboolean(iState, 0);
		return 1;
	}
	entity->getComponent<scene::component::UiRect>().size = {std::max(0.f, width), std::max(0.f, height)};
	lua_pushboolean(iState, 1);
	return 1;
}

auto luaUiGetRect(lua_State* iState) -> int {
	auto* activeScene = getBoundScene(iState);
	const auto entity = findEntity(iState);
	if (activeScene == nullptr || !entity || !entity->hasComponent<scene::component::UiRect>()) {
		lua_pushnil(iState);
		return 1;
	}
	auto* surface = entity->getScene();
	auto& layout = surface->getHudLayout();
	if (const auto viewport = activeScene->getViewportSize(); !layout.isComputedFor(viewport))
		layout.compute(*surface, viewport, false);
	const auto* node = layout.findNode(static_cast<entt::entity>(*entity));
	if (node == nullptr) {
		lua_pushnil(iState);
		return 1;
	}
	lua_pushnumber(iState, static_cast<lua_Number>(node->bounds.position.x()));
	lua_pushnumber(iState, static_cast<lua_Number>(node->bounds.position.y()));
	lua_pushnumber(iState, static_cast<lua_Number>(node->bounds.size.x()));
	lua_pushnumber(iState, static_cast<lua_Number>(node->bounds.size.y()));
	return 4;
}

auto luaUiSetImage(lua_State* iState) -> int {
	const auto entity = findEntity(iState);
	const std::string_view texture = luaL_checkstring(iState, 2);
	if (!entity || !entity->hasComponent<scene::component::UiImage>()) {
		lua_pushboolean(iState, 0);
		return 1;
	}
	auto& image = entity->getComponent<scene::component::UiImage>();
	if (texture.empty()) {
		image.texture = nullptr;
		lua_pushboolean(iState, 1);
		return 1;
	}
	// A bare asset path names the texture as the serialized form `nam:` does.
	const std::string serialized =
			texture.size() > 4 && texture[3] == ':' ? std::string{texture} : std::format("nam:{}", texture);
	if (!image.texture || image.texture->getSerializeString() != serialized)
		image.texture = renderer::gpu::Texture2D::createFromSerializedForDeserialize(serialized);
	lua_pushboolean(iState, image.texture ? 1 : 0);
	return 1;
}

auto luaUiSetImageFrame(lua_State* iState) -> int {
	const auto frame = checkInteger(iState, 2);
	const auto entity = findEntity(iState);
	if (!entity || !entity->hasComponent<scene::component::UiImage>()) {
		lua_pushboolean(iState, 0);
		return 1;
	}
	auto& image = entity->getComponent<scene::component::UiImage>();
	const auto count =
			static_cast<int64_t>(std::max(image.columns, 1u)) * static_cast<int64_t>(std::max(image.rows, 1u));
	image.frame = static_cast<uint32_t>(std::clamp<int64_t>(frame, 0, count - 1));
	lua_pushboolean(iState, 1);
	return 1;
}

auto luaUiGetImageFrame(lua_State* iState) -> int {
	const auto entity = findEntity(iState);
	const bool has = entity && entity->hasComponent<scene::component::UiImage>();
	pushInteger(iState, has ? static_cast<int64_t>(entity->getComponent<scene::component::UiImage>().frame) : 0);
	return 1;
}

auto luaUiIsWorldBlocked(lua_State* iState) -> int {
	auto* activeScene = getBoundScene(iState);
	lua_pushboolean(iState,
					activeScene != nullptr && scene::UiInputSystem::isWorldInteractionBlocked(*activeScene) ? 1 : 0);
	return 1;
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

// Every widget component of the entity follows the style; true when it holds at least one.
auto luaUiSetStyle(lua_State* iState) -> int {
	const std::string name = luaL_checkstring(iState, 2);
	bool styled = false;
	if (const auto entity = findEntity(iState); entity) {
		const auto setStyle = [&styled]<typename Widget>(Widget* ioWidget, const std::string& iName) -> void {
			if (ioWidget == nullptr)
				return;
			ioWidget->style.name = iName;
			styled = true;
		};
		auto& registry = entity->getScene()->registry;
		const auto handle = static_cast<entt::entity>(*entity);
		setStyle(registry.try_get<scene::component::UiPanel>(handle), name);
		setStyle(registry.try_get<scene::component::UiButton>(handle), name);
		setStyle(registry.try_get<scene::component::UiText>(handle), name);
		setStyle(registry.try_get<scene::component::UiSlider>(handle), name);
		setStyle(registry.try_get<scene::component::UiProgressBar>(handle), name);
		setStyle(registry.try_get<scene::component::UiList>(handle), name);
		setStyle(registry.try_get<scene::component::UiWindow>(handle), name);
	}
	lua_pushboolean(iState, styled ? 1 : 0);
	return 1;
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

auto luaEntitySetHudMarkerLabel(lua_State* iState) -> int {
	const std::string label = luaL_checkstring(iState, 2);
	const auto entity = findEntity(iState);
	if (!entity || !entity->hasComponent<scene::component::HudMarker>()) {
		lua_pushboolean(iState, 0);
		return 1;
	}
	entity->getComponent<scene::component::HudMarker>().label = label;
	lua_pushboolean(iState, 1);
	return 1;
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
	pushEntity(iState, focus);
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
	const auto count = checkInteger(iState, 2);
	if (const auto entity = findEntity(iState); entity && entity->hasComponent<scene::component::UiList>())
		entity->getComponent<scene::component::UiList>().setCount(static_cast<uint32_t>(std::max<int64_t>(count, 0)));
	return 0;
}

auto luaUiListGetCount(lua_State* iState) -> int {
	int64_t count = 0;
	if (const auto entity = findEntity(iState); entity && entity->hasComponent<scene::component::UiList>())
		count = static_cast<int64_t>(entity->getComponent<scene::component::UiList>().itemCount);
	pushInteger(iState, count);
	return 1;
}

auto luaUiListSetRow(lua_State* iState) -> int {
	const auto index = checkInteger(iState, 2);
	const std::string text = luaL_checkstring(iState, 3);
	const std::string detail = luaL_optstring(iState, 4, "");
	const std::string icon = luaL_optstring(iState, 5, "");
	const auto entity = findEntity(iState);
	if (!entity || !entity->hasComponent<scene::component::UiList>())
		return 0;
	auto& list = entity->getComponent<scene::component::UiList>();
	if (index < 1 || index > static_cast<int64_t>(list.itemCount))
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
	int64_t selected = 0;
	if (const auto entity = findEntity(iState); entity && entity->hasComponent<scene::component::UiList>())
		selected = static_cast<int64_t>(entity->getComponent<scene::component::UiList>().selectedIndex) + 1;
	pushInteger(iState, selected);
	return 1;
}

auto luaUiListSetSelected(lua_State* iState) -> int {
	const auto index = checkInteger(iState, 2);
	if (const auto entity = findEntity(iState); entity && entity->hasComponent<scene::component::UiList>())
		entity->getComponent<scene::component::UiList>().select(index - 1);
	return 0;
}

auto luaUiListGetVisible(lua_State* iState) -> int {
	int64_t first = 0;
	int64_t last = 0;
	if (const auto entity = findEntity(iState); entity && entity->hasComponent<scene::component::UiList>()) {
		const auto& layout = entity->getScene()->getHudLayout();
		if (const auto* node = layout.findNode(static_cast<entt::entity>(*entity));
			node != nullptr && node->scroll != scene::ui::HudNode::noParent) {
			const auto& scroll = layout.getScrolls()[node->scroll];
			const auto [begin, end] = scene::ui::visibleRows(scroll.offset.y(), scroll.geometry.viewport.size.y(),
															 scroll.rowHeight, scroll.rowSpacing, scroll.rowCount);
			if (end > begin) {
				first = static_cast<int64_t>(begin) + 1;
				last = static_cast<int64_t>(end);
			}
		}
	}
	pushInteger(iState, first);
	pushInteger(iState, last);
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

auto luaUiShowSubtitle(lua_State* iState) -> int {
	const char* text = luaL_checkstring(iState, 1);
	const auto seconds = static_cast<float>(luaL_optnumber(iState, 2, 3.0));
	if (auto* boundScene = getBoundScene(iState); boundScene != nullptr)
		boundScene->getSubtitles().show(text, seconds);
	return 0;
}

auto luaUiClearSubtitles(lua_State* iState) -> int {
	if (auto* boundScene = getBoundScene(iState); boundScene != nullptr)
		boundScene->getSubtitles().clear();
	return 0;
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
		OWL_CORE_WARN("Lua: Unknown transition type '{}' in ui.transition_play.", typeName)
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
	// A string stays a string even when it reads as a number ("0101" would become a float).
	if (lua_isboolean(iState, 2) != 0)
		activeScene->getGameState().set(key, lua_toboolean(iState, 2) != 0);
	else if (isInteger(iState, 2))
		activeScene->getGameState().set(key, static_cast<int64_t>(toInteger(iState, 2)));
	else if (lua_type(iState, 2) == LUA_TNUMBER)
		activeScene->getGameState().set(key, static_cast<float>(lua_tonumber(iState, 2)));
	else if (lua_type(iState, 2) == LUA_TSTRING)
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
					pushInteger(iState, static_cast<int64_t>(iVal));
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
	const auto slot = static_cast<uint32_t>(checkInteger(iState, 1));
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
	const auto slot = static_cast<uint32_t>(checkInteger(iState, 1));
	activeScene->saveLoadRequest.pending = true;
	activeScene->saveLoadRequest.isLoad = true;
	activeScene->saveLoadRequest.slot = slot;
	return 0;
}

auto luaSaveHasSave(lua_State* iState) -> int {
	const auto slot = static_cast<uint32_t>(checkInteger(iState, 1));
	lua_pushboolean(iState, scene::SaveManager::hasSave(slot) ? 1 : 0);
	return 1;
}

auto luaSaveDeleteSave(lua_State* iState) -> int {
	const auto slot = static_cast<uint32_t>(checkInteger(iState, 1));
	scene::SaveManager::deleteSave(slot);
	return 0;
}

auto luaSaveListSaves(lua_State* iState) -> int {
	const auto saves = scene::SaveManager::listSaves();
	lua_newtable(iState);
	for (size_t i = 0; i < saves.size(); ++i) {
		lua_newtable(iState);
		pushInteger(iState, static_cast<int64_t>(saves[i].slot));
		lua_setfield(iState, -2, "slot");
		lua_pushstring(iState, saves[i].timestamp.c_str());
		lua_setfield(iState, -2, "timestamp");
		lua_pushstring(iState, saves[i].scenePath.c_str());
		lua_setfield(iState, -2, "scene");
		lua_rawseti(iState, -2, static_cast<int>(i) + 1);
	}
	return 1;
}

auto luaDoorActivate(lua_State* iState) -> int {
	const auto* activeScene = getBoundScene(iState);
	if (activeScene == nullptr)
		return 0;
	const auto entity = entityAt(iState, 1).value_or(scene::Entity{});
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
	const auto entity = entityAt(iState, 1).value_or(scene::Entity{});
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
	const auto entity = entityAt(iState, 1).value_or(scene::Entity{});
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
	const auto entity = entityAt(iState, 1).value_or(scene::Entity{});
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
	const auto entity = entityAt(iState, 1).value_or(scene::Entity{});
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
	const auto entity = entityAt(iState, 1).value_or(scene::Entity{});
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
	const auto entity = entityAt(iState, 1).value_or(scene::Entity{});
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
	// A string stays a string even when it reads as a number ("0101" would become a float).
	if (lua_isboolean(iState, 2) != 0)
		settings->set(key, lua_toboolean(iState, 2) != 0);
	else if (isInteger(iState, 2))
		settings->set(key, static_cast<int64_t>(toInteger(iState, 2)));
	else if (lua_type(iState, 2) == LUA_TNUMBER)
		settings->set(key, static_cast<float>(lua_tonumber(iState, 2)));
	else if (lua_type(iState, 2) == LUA_TSTRING)
		settings->set(key, std::string(lua_tostring(iState, 2)));
	return 0;
}

auto luaLocaleTr(lua_State* iState) -> int {
	size_t length = 0;
	const char* key = luaL_checklstring(iState, 1, &length);
	const auto* context = contextOf(iState);
	const auto text = context != nullptr ? context->translate({key, length}) : std::string_view{key, length};
	lua_pushlstring(iState, text.data(), text.size());
	return 1;
}

auto luaLocaleGetLanguage(lua_State* iState) -> int {
	const auto* context = contextOf(iState);
	lua_pushstring(iState, context != nullptr ? context->getLanguage().c_str() : "");
	return 1;
}

auto luaLocaleSetLanguage(lua_State* iState) -> int {
	const char* code = luaL_checkstring(iState, 1);
	auto* context = contextOf(iState);
	if (context == nullptr)
		return 0;
	context->setLanguage(code);
	context->getSettings().set(scene::SettingsManager::KeyLanguage, context->getLanguage());
	return 0;
}

auto luaLocaleGetLanguages(lua_State* iState) -> int {
	const auto* context = contextOf(iState);
	lua_newtable(iState);
	if (context == nullptr)
		return 1;
	const auto& languages = context->getLocalisation().languages;
	for (size_t i = 0; i < languages.size(); ++i) {
		lua_newtable(iState);
		lua_pushstring(iState, languages[i].code.c_str());
		lua_setfield(iState, -2, "code");
		lua_pushstring(iState, languages[i].name.c_str());
		lua_setfield(iState, -2, "name");
		lua_rawseti(iState, -2, static_cast<int>(i) + 1);
	}
	return 1;
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
					pushInteger(iState, static_cast<int64_t>(iValue));
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
	if (const auto entity = entityAt(iState, 1).value_or(scene::Entity{});
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

auto luaIsometricPickCell(lua_State* iState) -> int {
	// The viewport mouse of the last HUD update: in Owl Nest the game is drawn inside a panel, not at the window origin.
	const auto& uiState = getBoundScene(iState)->getUiInputState();
	const auto mouse = uiState.hasMouse ? uiState.mousePosition : inputStateOf(iState).cursor;
	const auto windowX = static_cast<float>(luaL_optnumber(iState, 2, static_cast<lua_Number>(mouse.x())));
	const auto windowY = static_cast<float>(luaL_optnumber(iState, 3, static_cast<lua_Number>(mouse.y())));
	const auto map = findIsometricMap(iState);
	if (!map) {
		lua_pushnil(iState);
		return 1;
	}
	const auto camera = scene::makeRuntimeIsometricCamera(*getBoundScene(iState));
	if (const auto hit = scene::terrain::pickTerrain(*map->asset, camera, {windowX, windowY}, map->origin); hit) {
		pushInteger(iState, static_cast<int64_t>(hit->cell.x()));
		pushInteger(iState, static_cast<int64_t>(hit->cell.y()));
		return 2;
	}
	lua_pushnil(iState);
	return 1;
}

auto luaIsometricGetCell(lua_State* iState) -> int {
	const auto layer = checkInteger(iState, 2);
	const auto cellX = checkInteger(iState, 3);
	const auto cellY = checkInteger(iState, 4);
	const auto map = findIsometricMap(iState);
	if (!map || layer < 0 || cellX < 0 || cellY < 0 || static_cast<size_t>(layer) >= map->asset->layers.size()) {
		pushInteger(iState, scene::component::g_EmptyTileIndex);
		pushInteger(iState, scene::component::g_EmptyTileIndex);
		return 2;
	}
	const auto cell = map->asset->layers[static_cast<size_t>(layer)].getCell(static_cast<uint32_t>(cellX),
																			 static_cast<uint32_t>(cellY));
	pushInteger(iState, cell.terrain);
	pushInteger(iState, cell.overlay);
	return 2;
}

auto luaIsometricSetCell(lua_State* iState) -> int {
	const auto layer = checkInteger(iState, 2);
	const auto cellX = checkInteger(iState, 3);
	const auto cellY = checkInteger(iState, 4);
	const auto terrain = checkInteger(iState, 5);
	const auto overlay = checkInteger(iState, 6);
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

auto luaIsometricGetSize(lua_State* iState) -> int {
	const auto map = findIsometricMap(iState);
	if (!map) {
		lua_pushnil(iState);
		return 1;
	}
	pushInteger(iState, static_cast<int64_t>(map->asset->width));
	pushInteger(iState, static_cast<int64_t>(map->asset->height));
	lua_pushnumber(iState, static_cast<lua_Number>(map->asset->cellSize));
	return 3;
}

auto luaIsometricGetSlope(lua_State* iState) -> int {
	const auto cellX = checkInteger(iState, 2);
	const auto cellY = checkInteger(iState, 3);
	const auto map = findIsometricMap(iState);
	const bool inside = map && cellX >= 0 && cellY >= 0;
	pushInteger(iState, inside ? map->asset->getCellSlope(static_cast<uint32_t>(cellX), static_cast<uint32_t>(cellY))
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
	const auto frame = checkInteger(iState, 2);
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
	pushInteger(iState, entity->getComponent<scene::component::AnimatedSpriteRenderer>().m_currentFrame);
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
	const auto headingX = toInteger(iState, -2);
	const auto headingY = toInteger(iState, -1);
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
	const auto x = checkInteger(iState, iIndex);
	const auto y = checkInteger(iState, iIndex + 1);
	if (x < 0 || y < 0)
		return std::nullopt;
	return math::vec2ui{static_cast<uint32_t>(x), static_cast<uint32_t>(y)};
}

// The cells as a flat table `{x1, y1, x2, y2, ...}` then the cost in cells.
auto pushNavPath(lua_State* iState, const scene::nav::PathResult& iPath) -> int {
	lua_createtable(iState, static_cast<int>(iPath.cells.size() * 2), 0);
	int64_t index = 1;
	for (const auto& cell: iPath.cells) {
		pushInteger(iState, static_cast<int64_t>(cell.x()));
		lua_rawseti(iState, -2, static_cast<int>(index++));
		pushInteger(iState, static_cast<int64_t>(cell.y()));
		lua_rawseti(iState, -2, static_cast<int>(index++));
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
	pushInteger(iState, navigation != nullptr && from && to ? navigation->queue.request(*from, *to, mode, options) : 0);
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
	const auto request = static_cast<scene::nav::PathRequestId>(checkInteger(iState, 2));
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
	const auto request = static_cast<scene::nav::PathRequestId>(checkInteger(iState, 2));
	const auto* navigation = findNavigation(iState);
	const auto* path = navigation != nullptr ? navigation->queue.getResult(request) : nullptr;
	if (path == nullptr) {
		lua_pushnil(iState);
		return 1;
	}
	return pushNavPath(iState, *path);
}

auto luaPathRelease(lua_State* iState) -> int {
	const auto request = static_cast<scene::nav::PathRequestId>(checkInteger(iState, 2));
	if (auto* navigation = findNavigation(iState); navigation != nullptr)
		navigation->queue.release(request);
	return 0;
}

auto luaPathIsValid(lua_State* iState) -> int {
	const auto request = static_cast<scene::nav::PathRequestId>(checkInteger(iState, 2));
	const auto first = std::max<int64_t>(optInteger(iState, 3, 1), 1);
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
	pushInteger(iState, navigation != nullptr ? static_cast<int64_t>(navigation->map.getVersion()) : 0);
	return 1;
}

auto luaPathSetBudget(lua_State* iState) -> int {
	const auto budget = checkInteger(iState, 1);
	getBoundScene(iState)->getNavigation().setBudget(static_cast<uint32_t>(std::clamp<int64_t>(budget, 1, 1000000)));
	return 0;
}

// Draw (or stop drawing, with no cells) a route of the map in the debug overlay: `path.show_route(map, id, cells, r, g, b)`.
auto luaPathShowRoute(lua_State* iState) -> int {
	std::ignore = checkInteger(iState, 1);
	const auto routeId = static_cast<uint64_t>(checkInteger(iState, 2));
	const bool hasCells = lua_istable(iState, 3);
	const auto red = static_cast<float>(luaL_optnumber(iState, 4, 1.));
	const auto green = static_cast<float>(luaL_optnumber(iState, 5, 1.));
	const auto blue = static_cast<float>(luaL_optnumber(iState, 6, 1.));
	std::vector<math::vec2ui> cells;
	if (hasCells) {
		const auto count = static_cast<int64_t>(lua_objlen(iState, 3)) / 2;
		cells.reserve(static_cast<size_t>(count));
		for (int64_t i = 1; i <= count; ++i) {
			lua_rawgeti(iState, 3, static_cast<int>(i * 2 - 1));
			lua_rawgeti(iState, 3, static_cast<int>(i * 2));
			cells.emplace_back(static_cast<uint32_t>(std::max<int64_t>(toInteger(iState, -2), 0)),
							   static_cast<uint32_t>(std::max<int64_t>(toInteger(iState, -1), 0)));
			lua_pop(iState, 2);
		}
	}
	if (const auto entity = findEntity(iState); entity && findNavigation(iState) != nullptr)
		getBoundScene(iState)->getNavigation().setDebugRoute(static_cast<uint64_t>(entity->getUUID()), routeId, cells,
															 math::vec3{red, green, blue});
	return 0;
}

auto luaPathClearRoutes(lua_State* iState) -> int {
	std::ignore = checkInteger(iState, 1);
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

// The entity of the first argument when it holds a component.
template<typename Component>
auto entityWith(lua_State* iState) -> std::optional<scene::Entity> {
	if (auto entity = findEntity(iState); entity && entity->hasComponent<Component>())
		return entity;
	return std::nullopt;
}

auto readPoint(lua_State* iState, const int iIndex) -> math::vec2 {
	return {static_cast<float>(luaL_checknumber(iState, iIndex)),
			static_cast<float>(luaL_checknumber(iState, iIndex + 1))};
}

auto luaWalkFindPath(lua_State* iState) -> int {
	const auto from = readPoint(iState, 2);
	const auto to = readPoint(iState, 4);
	const auto area = entityWith<scene::component::WalkArea>(iState);
	std::vector<math::vec2> path;
	if (!area || !scene::nav::findWalkPath(*getBoundScene(iState), *area, from, to, path)) {
		lua_pushnil(iState);
		return 1;
	}
	lua_createtable(iState, static_cast<int>(path.size() * 2), 0);
	int64_t index = 1;
	for (const auto& point: path) {
		lua_pushnumber(iState, static_cast<lua_Number>(point.x()));
		lua_rawseti(iState, -2, static_cast<int>(index++));
		lua_pushnumber(iState, static_cast<lua_Number>(point.y()));
		lua_rawseti(iState, -2, static_cast<int>(index++));
	}
	return 1;
}

auto luaWalkIsInside(lua_State* iState) -> int {
	const auto point = readPoint(iState, 2);
	const auto area = entityWith<scene::component::WalkArea>(iState);
	lua_pushboolean(iState, area && scene::nav::isOnWalkArea(*getBoundScene(iState), *area, point) ? 1 : 0);
	return 1;
}

auto luaWalkGetClosestPoint(lua_State* iState) -> int {
	const auto point = readPoint(iState, 2);
	const auto area = entityWith<scene::component::WalkArea>(iState);
	if (!area)
		return 0;
	const auto closest = scene::nav::getWalkClosestPoint(*getBoundScene(iState), *area, point);
	if (!closest)
		return 0;
	lua_pushnumber(iState, static_cast<lua_Number>(closest->x()));
	lua_pushnumber(iState, static_cast<lua_Number>(closest->y()));
	return 2;
}

auto luaWalkGetScaleAt(lua_State* iState) -> int {
	const auto point = readPoint(iState, 2);
	const auto area = entityWith<scene::component::WalkArea>(iState);
	lua_pushnumber(iState, static_cast<lua_Number>(
								   area ? scene::nav::getWalkScaleAt(*getBoundScene(iState), *area, point) : 1.f));
	return 1;
}

auto luaWalkGoTo(lua_State* iState) -> int {
	const auto target = readPoint(iState, 2);
	const auto agent = entityWith<scene::component::WalkAgent>(iState);
	lua_pushboolean(iState, agent && scene::nav::walkAgentTo(*getBoundScene(iState), *agent, target) ? 1 : 0);
	return 1;
}

auto luaWalkStop(lua_State* iState) -> int {
	if (const auto agent = entityWith<scene::component::WalkAgent>(iState); agent)
		agent->getComponent<scene::component::WalkAgent>().stop();
	return 0;
}

auto luaWalkIsMoving(lua_State* iState) -> int {
	const auto agent = entityWith<scene::component::WalkAgent>(iState);
	lua_pushboolean(iState, agent && agent->getComponent<scene::component::WalkAgent>().isMoving() ? 1 : 0);
	return 1;
}

auto luaWalkGetTarget(lua_State* iState) -> int {
	const auto agent = entityWith<scene::component::WalkAgent>(iState);
	if (!agent)
		return 0;
	const auto& walker = agent->getComponent<scene::component::WalkAgent>();
	if (!walker.isMoving())
		return 0;
	lua_pushnumber(iState, static_cast<lua_Number>(walker.m_path.back().x()));
	lua_pushnumber(iState, static_cast<lua_Number>(walker.m_path.back().y()));
	return 2;
}

auto luaWalkSetSpeed(lua_State* iState) -> int {
	const auto speed = static_cast<float>(luaL_checknumber(iState, 2));
	if (const auto agent = entityWith<scene::component::WalkAgent>(iState); agent)
		agent->getComponent<scene::component::WalkAgent>().speed = std::max(0.f, speed);
	return 0;
}

auto luaWalkSetArea(lua_State* iState) -> int {
	const auto area = entityAt(iState, 2).value_or(scene::Entity{});
	const auto agent = entityWith<scene::component::WalkAgent>(iState);
	if (!agent || !area || !area.hasComponent<scene::component::WalkArea>()) {
		lua_pushboolean(iState, 0);
		return 1;
	}
	auto& walker = agent->getComponent<scene::component::WalkAgent>();
	walker.area = area.getUUID();
	walker.stop();
	lua_pushboolean(iState, 1);
	return 1;
}

auto readPoint3(lua_State* iState, const int iIndex) -> math::vec3 {
	return {static_cast<float>(luaL_checknumber(iState, iIndex)),
			static_cast<float>(luaL_checknumber(iState, iIndex + 1)),
			static_cast<float>(luaL_checknumber(iState, iIndex + 2))};
}

auto pushPoint3(lua_State* iState, const math::vec3& iPoint) -> int {
	lua_pushnumber(iState, static_cast<lua_Number>(iPoint.x()));
	lua_pushnumber(iState, static_cast<lua_Number>(iPoint.y()));
	lua_pushnumber(iState, static_cast<lua_Number>(iPoint.z()));
	return 3;
}

auto luaNavFindPath(lua_State* iState) -> int {
	const auto from = readPoint3(iState, 2);
	const auto to = readPoint3(iState, 5);
	const auto volume = entityWith<scene::component::NavMeshVolume>(iState);
	std::vector<scene::nav::NavPathPoint> path;
	if (!volume || !volume->getComponent<scene::component::NavMeshVolume>().getMesh().findPath(from, to, path)) {
		lua_pushnil(iState);
		return 1;
	}
	lua_createtable(iState, static_cast<int>(path.size() * 3), 0);
	lua_Integer index = 1;
	for (const auto& point: path) {
		for (size_t axis = 0; axis < 3; ++axis) {
			lua_pushnumber(iState, static_cast<lua_Number>(point.position[axis]));
			lua_rawseti(iState, -2, index++);
		}
	}
	return 1;
}

auto luaNavGetClosestPoint(lua_State* iState) -> int {
	const auto point = readPoint3(iState, 2);
	const auto volume = entityWith<scene::component::NavMeshVolume>(iState);
	if (!volume)
		return 0;
	const auto nearest = volume->getComponent<scene::component::NavMeshVolume>().getMesh().findNearest(point);
	return nearest.has_value() ? pushPoint3(iState, nearest->position) : 0;
}

auto luaNavBake(lua_State* iState) -> int {
	const auto volume = entityWith<scene::component::NavMeshVolume>(iState);
	lua_pushboolean(iState, volume && scene::nav::bakeNavMesh(*getBoundScene(iState), *volume).has_value() ? 1 : 0);
	return 1;
}

auto luaNavGoTo(lua_State* iState) -> int {
	const auto target = readPoint3(iState, 2);
	const auto agent = entityWith<scene::component::NavAgent>(iState);
	if (agent)
		agent->getComponent<scene::component::NavAgent>().goTo(target);
	lua_pushboolean(iState, agent ? 1 : 0);
	return 1;
}

auto luaNavStop(lua_State* iState) -> int {
	if (const auto agent = entityWith<scene::component::NavAgent>(iState); agent)
		agent->getComponent<scene::component::NavAgent>().stop();
	return 0;
}

auto luaNavIsMoving(lua_State* iState) -> int {
	const auto agent = entityWith<scene::component::NavAgent>(iState);
	lua_pushboolean(iState, agent && agent->getComponent<scene::component::NavAgent>().isMoving() ? 1 : 0);
	return 1;
}

auto luaNavGetTarget(lua_State* iState) -> int {
	const auto agent = entityWith<scene::component::NavAgent>(iState);
	if (!agent)
		return 0;
	const auto& walker = agent->getComponent<scene::component::NavAgent>();
	if (walker.m_pendingTarget)
		return pushPoint3(iState, walker.m_target);
	return walker.isMoving() ? pushPoint3(iState, walker.m_path.back().position) : 0;
}

auto luaNavSetSpeed(lua_State* iState) -> int {
	const auto speed = static_cast<float>(luaL_checknumber(iState, 2));
	if (const auto agent = entityWith<scene::component::NavAgent>(iState); agent)
		agent->getComponent<scene::component::NavAgent>().speed = std::max(0.f, speed);
	return 0;
}

auto splineEntity(lua_State* iState) -> std::optional<scene::Entity> {
	if (auto entity = findEntity(iState); entity && entity->hasComponent<scene::component::Spline>())
		return entity;
	return std::nullopt;
}

auto worldPoint(const math::mat4& iMatrix, const math::vec3& iPoint) -> math::vec3 {
	const math::vec4 point = iMatrix * math::vec4{iPoint.x(), iPoint.y(), iPoint.z(), 1.f};
	return {point.x(), point.y(), point.z()};
}

auto pushVec3(lua_State* iState, const math::vec3& iVector) -> int {
	lua_pushnumber(iState, static_cast<lua_Number>(iVector.x()));
	lua_pushnumber(iState, static_cast<lua_Number>(iVector.y()));
	lua_pushnumber(iState, static_cast<lua_Number>(iVector.z()));
	return 3;
}

auto luaSplineGetLength(lua_State* iState) -> int {
	const auto entity = splineEntity(iState);
	if (!entity)
		return 0;
	lua_pushnumber(iState, static_cast<lua_Number>(entity->getComponent<scene::component::Spline>().curve.getLength()));
	return 1;
}

auto luaSplineSample(lua_State* iState) -> int {
	const auto distance = static_cast<float>(luaL_checknumber(iState, 2));
	const char* lane = luaL_optstring(iState, 3, "");
	const auto entity = splineEntity(iState);
	if (!entity)
		return 0;
	const auto sample = entity->getComponent<scene::component::Spline>().sample(distance, lane);
	return pushVec3(iState, worldPoint(getBoundScene(iState)->getWorldTransform(*entity)(), sample.position));
}

auto luaSplineGetTangent(lua_State* iState) -> int {
	const auto distance = static_cast<float>(luaL_checknumber(iState, 2));
	const auto entity = splineEntity(iState);
	if (!entity)
		return 0;
	const auto sample = entity->getComponent<scene::component::Spline>().curve.sampleAtDistance(distance);
	const math::mat4 world = getBoundScene(iState)->getWorldTransform(*entity)();
	const math::vec4 direction = world * math::vec4{sample.tangent.x(), sample.tangent.y(), sample.tangent.z(), 0.f};
	math::vec3 tangent{direction.x(), direction.y(), direction.z()};
	if (const float length = tangent.norm(); length > 0.f)
		tangent = tangent * (1.f / length);
	return pushVec3(iState, tangent);
}

auto luaSplineGetNearest(lua_State* iState) -> int {
	const math::vec3 position{static_cast<float>(luaL_checknumber(iState, 2)),
							  static_cast<float>(luaL_checknumber(iState, 3)),
							  static_cast<float>(luaL_checknumber(iState, 4))};
	const auto entity = splineEntity(iState);
	if (!entity)
		return 0;
	const math::mat4 toLocal = math::inverse(getBoundScene(iState)->getWorldTransform(*entity)());
	const auto& curve = entity->getComponent<scene::component::Spline>().curve;
	lua_pushnumber(iState, static_cast<lua_Number>(curve.getNearestDistance(worldPoint(toLocal, position))));
	return 1;
}

auto luaSplineGetPointCount(lua_State* iState) -> int {
	const auto entity = splineEntity(iState);
	if (!entity)
		return 0;
	const auto& curve = entity->getComponent<scene::component::Spline>().curve;
	pushInteger(iState, static_cast<int64_t>(curve.getPoints().size()));
	return 1;
}

auto luaSplineGetPoint(lua_State* iState) -> int {
	const auto index = checkInteger(iState, 2);
	const auto entity = splineEntity(iState);
	if (!entity)
		return 0;
	const auto& points = entity->getComponent<scene::component::Spline>().curve.getPoints();
	if (index < 1 || std::cmp_greater(index, points.size()))
		return 0;
	return pushVec3(iState, worldPoint(getBoundScene(iState)->getWorldTransform(*entity)(),
									   points[static_cast<size_t>(index - 1)].position));
}

auto luaSplineSetPoint(lua_State* iState) -> int {
	const auto index = checkInteger(iState, 2);
	const math::vec3 position{static_cast<float>(luaL_checknumber(iState, 3)),
							  static_cast<float>(luaL_checknumber(iState, 4)),
							  static_cast<float>(luaL_checknumber(iState, 5))};
	const auto entity = splineEntity(iState);
	if (!entity) {
		lua_pushboolean(iState, 0);
		return 1;
	}
	auto& curve = entity->getComponent<scene::component::Spline>().curve;
	auto points = curve.getPoints();
	if (index < 1 || std::cmp_greater(index, points.size())) {
		lua_pushboolean(iState, 0);
		return 1;
	}
	const math::mat4 toLocal = math::inverse(getBoundScene(iState)->getWorldTransform(*entity)());
	points[static_cast<size_t>(index - 1)].position = worldPoint(toLocal, position);
	curve.setPoints(std::move(points));
	lua_pushboolean(iState, 1);
	return 1;
}

auto followerOf(lua_State* iState) -> scene::component::SplineFollower* {
	if (const auto entity = findEntity(iState); entity && entity->hasComponent<scene::component::SplineFollower>())
		return &entity->getComponent<scene::component::SplineFollower>();
	return nullptr;
}

auto luaSplineGetDistance(lua_State* iState) -> int {
	const auto* follower = followerOf(iState);
	if (follower == nullptr)
		return 0;
	lua_pushnumber(iState,
				   static_cast<lua_Number>(follower->m_started ? follower->m_distance : follower->startDistance));
	return 1;
}

auto luaSplineSetDistance(lua_State* iState) -> int {
	const auto distance = static_cast<float>(luaL_checknumber(iState, 2));
	auto* follower = followerOf(iState);
	if (follower == nullptr) {
		lua_pushboolean(iState, 0);
		return 1;
	}
	follower->m_distance = distance;
	follower->m_travelled = 0.f;
	follower->m_started = true;
	lua_pushboolean(iState, 1);
	return 1;
}

auto luaSplineIsWaiting(lua_State* iState) -> int {
	const auto* follower = followerOf(iState);
	if (follower == nullptr)
		return 0;
	lua_pushboolean(iState, follower->isWaiting() ? 1 : 0);
	return 1;
}

auto luaSplineGetSpeed(lua_State* iState) -> int {
	const auto* follower = followerOf(iState);
	if (follower == nullptr)
		return 0;
	lua_pushnumber(iState, static_cast<lua_Number>(follower->speed));
	return 1;
}

auto luaSplineSetSpeed(lua_State* iState) -> int {
	const auto speed = static_cast<float>(luaL_checknumber(iState, 2));
	auto* follower = followerOf(iState);
	if (follower == nullptr) {
		lua_pushboolean(iState, 0);
		return 1;
	}
	follower->speed = speed;
	lua_pushboolean(iState, 1);
	return 1;
}

auto luaSplineSetLane(lua_State* iState) -> int {
	const char* lane = luaL_checkstring(iState, 2);
	auto* follower = followerOf(iState);
	if (follower == nullptr) {
		lua_pushboolean(iState, 0);
		return 1;
	}
	follower->lane = lane;
	lua_pushboolean(iState, 1);
	return 1;
}

auto tableOf(lua_State* iState, const char* iPath) -> shared<const data::table::DataTable> {
	auto* context = contextOf(iState);
	return context != nullptr ? context->getDataTables().get(iPath) : nullptr;
}

void pushCell(lua_State* iState, const data::table::CellValue& iValue) {
	if (const auto* integer = std::get_if<int64_t>(&iValue); integer != nullptr)
		pushInteger(iState, static_cast<int64_t>(*integer));
	else if (const auto* number = std::get_if<double>(&iValue); number != nullptr)
		lua_pushnumber(iState, static_cast<lua_Number>(*number));
	else if (const auto* flag = std::get_if<bool>(&iValue); flag != nullptr)
		lua_pushboolean(iState, *flag ? 1 : 0);
	else
		lua_pushstring(iState, std::get<std::string>(iValue).c_str());
}

auto luaDataTableGet(lua_State* iState) -> int {
	const char* path = luaL_checkstring(iState, 1);
	const char* row = luaL_checkstring(iState, 2);
	const char* column = luaL_checkstring(iState, 3);
	const auto table = tableOf(iState, path);
	const auto* cell = table ? table->getCell(row, column) : nullptr;
	if (cell == nullptr)
		return 0;
	pushCell(iState, *cell);
	return 1;
}

auto luaDataTableGetRow(lua_State* iState) -> int {
	const char* path = luaL_checkstring(iState, 1);
	const char* key = luaL_checkstring(iState, 2);
	const auto table = tableOf(iState, path);
	const auto row = table ? table->findRow(key) : std::nullopt;
	if (!row)
		return 0;
	lua_newtable(iState);
	for (size_t column = 0; column < table->columns.size(); ++column) {
		pushCell(iState, table->rows[*row].cells[column]);
		lua_setfield(iState, -2, table->columns[column].name.c_str());
	}
	return 1;
}

auto luaDataTableGetKeys(lua_State* iState) -> int {
	const char* path = luaL_checkstring(iState, 1);
	const auto table = tableOf(iState, path);
	if (!table)
		return 0;
	lua_newtable(iState);
	for (size_t index = 0; index < table->rows.size(); ++index) {
		lua_pushstring(iState, table->rows[index].key.c_str());
		lua_rawseti(iState, -2, static_cast<int>(index) + 1);
	}
	return 1;
}

auto luaDataTableGetColumns(lua_State* iState) -> int {
	const char* path = luaL_checkstring(iState, 1);
	const auto table = tableOf(iState, path);
	if (!table)
		return 0;
	lua_newtable(iState);
	for (size_t index = 0; index < table->columns.size(); ++index) {
		lua_pushstring(iState, table->columns[index].name.c_str());
		lua_rawseti(iState, -2, static_cast<int>(index) + 1);
	}
	return 1;
}

auto luaDataTableGetCount(lua_State* iState) -> int {
	const char* path = luaL_checkstring(iState, 1);
	const auto table = tableOf(iState, path);
	if (!table)
		return 0;
	pushInteger(iState, static_cast<int64_t>(table->rows.size()));
	return 1;
}

auto luaDataTableHasRow(lua_State* iState) -> int {
	const char* path = luaL_checkstring(iState, 1);
	const char* key = luaL_checkstring(iState, 2);
	const auto table = tableOf(iState, path);
	lua_pushboolean(iState, table && table->findRow(key) ? 1 : 0);
	return 1;
}

auto luaDataTableResolve(lua_State* iState) -> int {
	const char* path = luaL_checkstring(iState, 1);
	const char* row = luaL_checkstring(iState, 2);
	const char* column = luaL_checkstring(iState, 3);
	const auto table = tableOf(iState, path);
	const auto index = table ? table->findColumn(column) : std::nullopt;
	const auto* cell = table ? table->getCell(row, column) : nullptr;
	if (!index || cell == nullptr || table->columns[*index].type != data::table::ColumnType::Reference)
		return 0;
	const auto& target = table->columns[*index].table;
	lua_pushstring(iState, target.empty() ? path : target.c_str());
	lua_pushstring(iState, std::get<std::string>(*cell).c_str());
	return 2;
}

auto readLuaCell(lua_State* iState, const int iIndex) -> data::table::CellValue {
	if (lua_isboolean(iState, iIndex))
		return lua_toboolean(iState, iIndex) != 0;
	if (isInteger(iState, iIndex))
		return static_cast<int64_t>(toInteger(iState, iIndex));
	if (lua_isnumber(iState, iIndex) != 0)
		return static_cast<double>(lua_tonumber(iState, iIndex));
	const char* text = lua_tostring(iState, iIndex);
	return std::string{text != nullptr ? text : ""};
}

auto luaDataTableFind(lua_State* iState) -> int {
	const char* path = luaL_checkstring(iState, 1);
	const char* column = luaL_checkstring(iState, 2);
	luaL_checkany(iState, 3);
	const auto value = readLuaCell(iState, 3);
	const auto table = tableOf(iState, path);
	const auto index = table ? table->findColumn(column) : std::nullopt;
	if (!index)
		return 0;
	const auto wanted = data::table::convertValue(value, table->columns[*index]);
	for (const auto& row: table->rows) {
		const bool same = data::table::isSameValue(row.cells[*index], wanted);
		if (same) {
			lua_pushstring(iState, row.key.c_str());
			return 1;
		}
	}
	return 0;
}

// A condition or an action of a dialogue: a global function of the script driving the conversation.
auto callDialogueHook(lua_State* iState, const std::string_view iName) -> bool {
	const std::string name{iName};
	lua_getglobal(iState, name.c_str());
	if (!lua_isfunction(iState, -1)) {
		lua_pop(iState, 1);
		OWL_CORE_WARN("Lua: Dialogue function '{}' is not defined by the script.", name)
		return false;
	}
	if (lua_pcall(iState, 0, 1, 0) != LUA_OK) {
		OWL_CORE_WARN("Lua: Dialogue function '{}' failed: {}.", name, lua_tostring(iState, -1))
		lua_pop(iState, 1);
		return false;
	}
	const bool result = lua_toboolean(iState, -1) != 0;
	lua_pop(iState, 1);
	return result;
}

auto dialogueHooksOf(lua_State* iState) -> data::dialogue::ConversationHooks {
	return {.condition = [iState](const std::string_view iName) -> bool { return callDialogueHook(iState, iName); },
			.action = [iState](const std::string_view iName) -> void {
				std::ignore = callDialogueHook(iState, iName);
			}};
}

auto dialoguesOf(lua_State* iState) -> data::dialogue::DialogueLibrary* {
	auto* context = contextOf(iState);
	return context != nullptr ? &context->getDialogues() : nullptr;
}

// A text in the language of the game: the translation of its key, else its own value.
void pushDialogueText(lua_State* iState, const data::dialogue::DialogueText& iText) {
	const auto* context = contextOf(iState);
	std::string_view value = iText.text;
	if (!iText.key.empty() && context != nullptr) {
		if (const auto translated = context->translate(iText.key); translated != iText.key || iText.text.empty())
			value = translated;
	}
	lua_pushlstring(iState, value.data(), value.size());
}

auto luaDialogueStart(lua_State* iState) -> int {
	const char* path = luaL_checkstring(iState, 1);
	auto* dialogues = dialoguesOf(iState);
	const auto id = dialogues != nullptr ? dialogues->start(path, dialogueHooksOf(iState)) : std::nullopt;
	if (!id)
		return 0;
	pushInteger(iState, static_cast<int64_t>(*id));
	return 1;
}

auto luaDialogueGetLine(lua_State* iState) -> int {
	const auto id = static_cast<uint64_t>(checkInteger(iState, 1));
	auto* dialogues = dialoguesOf(iState);
	const auto* conversation = dialogues != nullptr ? dialogues->find(id) : nullptr;
	if (conversation == nullptr)
		return 0;
	const auto& node = *conversation->getCurrent();
	lua_newtable(iState);
	lua_pushstring(iState, std::string{data::dialogue::getNodeKindName(node.kind)}.c_str());
	lua_setfield(iState, -2, "kind");
	pushDialogueText(iState, node.speaker);
	lua_setfield(iState, -2, "speaker");
	pushDialogueText(iState, node.text);
	lua_setfield(iState, -2, "text");
	lua_pushstring(iState, node.sound.c_str());
	lua_setfield(iState, -2, "sound");
	lua_newtable(iState);
	int64_t index = 1;
	for (const size_t option: conversation->getOfferedOptions()) {
		pushDialogueText(iState, node.options[option].text);
		lua_rawseti(iState, -2, static_cast<int>(index++));
	}
	lua_setfield(iState, -2, "choices");
	return 1;
}

auto luaDialogueNext(lua_State* iState) -> int {
	const auto id = static_cast<uint64_t>(checkInteger(iState, 1));
	auto* dialogues = dialoguesOf(iState);
	lua_pushboolean(iState, dialogues != nullptr && dialogues->advance(id, dialogueHooksOf(iState)) ? 1 : 0);
	return 1;
}

auto luaDialogueChoose(lua_State* iState) -> int {
	const auto id = static_cast<uint64_t>(checkInteger(iState, 1));
	const auto choice = checkInteger(iState, 2);
	auto* dialogues = dialoguesOf(iState);
	const bool running = dialogues != nullptr && choice >= 1 &&
						 dialogues->choose(id, static_cast<size_t>(choice - 1), dialogueHooksOf(iState));
	lua_pushboolean(iState, running ? 1 : 0);
	return 1;
}

auto luaDialogueIsRunning(lua_State* iState) -> int {
	const auto id = static_cast<uint64_t>(checkInteger(iState, 1));
	auto* dialogues = dialoguesOf(iState);
	lua_pushboolean(iState, dialogues != nullptr && dialogues->find(id) != nullptr ? 1 : 0);
	return 1;
}

auto luaDialogueStop(lua_State* iState) -> int {
	const auto id = static_cast<uint64_t>(checkInteger(iState, 1));
	if (auto* dialogues = dialoguesOf(iState); dialogues != nullptr)
		dialogues->stop(id);
	return 0;
}

auto sequencePlayerOf(const std::optional<scene::Entity>& iEntity) -> scene::component::SequencePlayer* {
	if (iEntity && iEntity->hasComponent<scene::component::SequencePlayer>())
		return &iEntity->getComponent<scene::component::SequencePlayer>();
	return nullptr;
}

auto luaSequencePlay(lua_State* iState) -> int {
	const auto from = static_cast<float>(luaL_optnumber(iState, 2, 0.0));
	const auto entity = findEntity(iState);
	lua_pushboolean(iState, entity && scene::playSequence(*entity, from) ? 1 : 0);
	return 1;
}

auto luaSequenceStop(lua_State* iState) -> int {
	const auto entity = findEntity(iState);
	lua_pushboolean(iState, entity && scene::stopSequence(*entity) ? 1 : 0);
	return 1;
}

auto luaSequenceSeek(lua_State* iState) -> int {
	const auto time = static_cast<float>(luaL_checknumber(iState, 2));
	const auto entity = findEntity(iState);
	lua_pushboolean(iState, entity && scene::seekSequence(*entity, time) ? 1 : 0);
	return 1;
}

auto luaSequenceIsPlaying(lua_State* iState) -> int {
	const auto* player = sequencePlayerOf(findEntity(iState));
	if (player == nullptr)
		return 0;
	lua_pushboolean(iState, player->m_playing ? 1 : 0);
	return 1;
}

auto luaSequenceGetTime(lua_State* iState) -> int {
	const auto* player = sequencePlayerOf(findEntity(iState));
	if (player == nullptr)
		return 0;
	lua_pushnumber(iState, static_cast<lua_Number>(player->m_time));
	return 1;
}

auto luaSequenceGetDuration(lua_State* iState) -> int {
	const auto entity = findEntity(iState);
	const auto* player = sequencePlayerOf(entity);
	if (player == nullptr)
		return 0;
	auto sequence = player->m_sequence;
	if (!sequence) {
		if (auto* context = contextOf(iState); context != nullptr && !player->sequence.empty())
			sequence = context->getSequences().get(player->sequence);
	}
	if (!sequence)
		return 0;
	lua_pushnumber(iState, static_cast<lua_Number>(sequence->duration));
	return 1;
}

auto luaSequenceSetSpeed(lua_State* iState) -> int {
	const auto speed = static_cast<float>(luaL_checknumber(iState, 2));
	auto* player = sequencePlayerOf(findEntity(iState));
	if (player != nullptr)
		player->speed = std::max(0.f, speed);
	lua_pushboolean(iState, player != nullptr ? 1 : 0);
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
		{.table = "transform", .name = "get_world_position", .function = guarded<luaTransformGetWorldPosition>,
		 .description = "Position of the entity in the world, in double precision: its `WorldPosition`, else the world "
						"origin plus its world transform.",
		 .params = {entityId}, .returns = {{"x", Number}, {"y", Number}, {"z", Number}}},
		{.table = "transform", .name = "set_world_position", .function = guarded<luaTransformSetWorldPosition>,
		 .description = "Place a root entity in the world, in double precision (adds a `WorldPosition` when it has "
						"none); its local position is written around the world origin. Ignored on a child.",
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
		 .description = "Apply a linear impulse to the body; fz for a 3D body.",
		 .params = {entityId, {"fx", Number}, {"fy", Number}, {"fz", Number, true}}},
		{.table = "physics", .name = "get_velocity", .function = guarded<luaPhysicsGetVelocity>,
		 .description = "Linear velocity of the body (zeros without a body); vz for a 3D body.",
		 .params = {entityId},
		 .returns = {{"vx", Number}, {"vy", Number}, {"vz", Number, true}}},
		{.table = "physics", .name = "set_velocity", .function = guarded<luaPhysicsSetVelocity>,
		 .description = "Set the linear velocity of the body; vz for a 3D body.",
		 .params = {entityId, {"vx", Number}, {"vy", Number}, {"vz", Number, true}}},
		{.table = "physics", .name = "set_transform", .function = guarded<luaPhysicsSetTransform>,
		 .description = "Move the body: x, y and its rotation (radians) in 2D; x, y, z and Euler angles rx, ry, rz in 3D.",
		 .params = {entityId,
					{"x", Number},
					{"y", Number},
					{"rotation_or_z", Number},
					{"rx", Number, true},
					{"ry", Number, true},
					{"rz", Number, true}}},
		{.table = "physics", .name = "set_gravity_scale", .function = guarded<luaPhysicsSetGravityScale>,
		 .description = "Scale the world gravity for this body (0 = none).",
		 .params = {entityId, {"scale", Number}}},
		{.table = "physics", .name = "get_gravity", .function = guarded<luaPhysicsGetGravity>,
		 .description = "Gravity at a point: the world gravity plus the `GravitySource` fields; gz with z for a 3D point.",
		 .params = {{"x", Number}, {"y", Number}, {"z", Number, true}},
		 .returns = {{"gx", Number}, {"gy", Number}, {"gz", Number, true}}},
		{.table = "physics", .name = "predict_trajectory", .function = guarded<luaPhysicsPredictTrajectory>,
		 .description = "Future positions `{x, y}` (with z for a 3D body) of the body under the gravity, one per fixed "
						"step, collisions ignored.",
		 .params = {entityId, {"steps", Integer}}, .returns = {{"points", Table}}},
		{.table = "physics", .name = "is_on_rails", .function = guarded<luaPhysicsIsOnRails>,
		 .description = "Whether the last physics step ran on rails: a time warp beyond the scene's rails time scale.",
		 .returns = {{"on_rails", Boolean}}},
		{.table = "physics", .name = "raycast", .function = guarded<luaPhysicsRaycast>,
		 .description = "First body a 2D ray meets, sensors ignored: `{entity, x, y, nx, ny, distance}`, nil for none.",
		 .params = {{"x", Number}, {"y", Number}, {"dx", Number}, {"dy", Number}, {"distance", Number},
					{"mask", Integer, true}},
		 .returns = {{"hit", Table, true}}},
		{.table = "physics", .name = "raycast_3d", .function = guarded<luaPhysicsRaycast3D>,
		 .description = "First body a 3D ray meets: `{entity, x, y, z, nx, ny, nz, distance}`, nil for none.",
		 .params = {{"x", Number}, {"y", Number}, {"z", Number}, {"dx", Number}, {"dy", Number}, {"dz", Number},
					{"distance", Number}, {"mask", Integer, true}},
		 .returns = {{"hit", Table, true}}},
		{.table = "physics", .name = "circle_cast", .function = guarded<luaPhysicsCircleCast>,
		 .description = "First body a disc meets moving along (dx, dy); the hit as for `raycast`.",
		 .params = {{"x", Number}, {"y", Number}, {"radius", Number}, {"dx", Number}, {"dy", Number},
					{"distance", Number}, {"mask", Integer, true}},
		 .returns = {{"hit", Table, true}}},
		{.table = "physics", .name = "box_cast", .function = guarded<luaPhysicsBoxCast>,
		 .description = "First body a box (half sizes, rotation in radians) meets moving along (dx, dy).",
		 .params = {{"x", Number}, {"y", Number}, {"half_width", Number}, {"half_height", Number},
					{"rotation", Number}, {"dx", Number}, {"dy", Number}, {"distance", Number},
					{"mask", Integer, true}},
		 .returns = {{"hit", Table, true}}},
		{.table = "physics", .name = "sphere_cast", .function = guarded<luaPhysicsSphereCast>,
		 .description = "First body a sphere meets moving along (dx, dy, dz); the hit as for `raycast_3d`.",
		 .params = {{"x", Number}, {"y", Number}, {"z", Number}, {"radius", Number}, {"dx", Number},
					{"dy", Number}, {"dz", Number}, {"distance", Number}, {"mask", Integer, true}},
		 .returns = {{"hit", Table, true}}},
		{.table = "physics", .name = "overlap_circle", .function = guarded<luaPhysicsOverlapCircle>,
		 .description = "Entities whose bodies overlap a disc, sensors ignored, in a fixed order.",
		 .params = {{"x", Number}, {"y", Number}, {"radius", Number}, {"mask", Integer, true}},
		 .returns = {{"entity_ids", Table}}},
		{.table = "physics", .name = "overlap_box", .function = guarded<luaPhysicsOverlapBox>,
		 .description = "Entities whose bodies overlap a box (half sizes, rotation in radians).",
		 .params = {{"x", Number}, {"y", Number}, {"half_width", Number}, {"half_height", Number},
					{"rotation", Number}, {"mask", Integer, true}},
		 .returns = {{"entity_ids", Table}}},
		{.table = "physics", .name = "overlap_sphere", .function = guarded<luaPhysicsOverlapSphere>,
		 .description = "Entities whose bodies overlap a sphere.",
		 .params = {{"x", Number}, {"y", Number}, {"z", Number}, {"radius", Number}, {"mask", Integer, true}},
		 .returns = {{"entity_ids", Table}}},
		{.table = "physics", .name = "set_joint_motor", .function = guarded<luaPhysicsSetJointMotor>,
		 .description = "Drive the hinge or slider joint of the entity at a speed (radians or units per second); "
						"enabled false stops the motor.",
		 .params = {entityId, {"speed", Number}, {"enabled", Boolean, true}}},
		{.table = "physics", .name = "get_joint_position", .function = guarded<luaPhysicsGetJointPosition>,
		 .description = "Angle of the entity's hinge joint (radians) or position of its slider; 0 otherwise.",
		 .params = {entityId}, .returns = {{"position", Number}}},
		{.table = "physics", .name = "add_force", .function = guarded<luaPhysicsAddForce>,
		 .description = "Push the body with a force (newtons) over the next step, at its centre of mass or at a part; "
						"call it in `on_tick` for a steady thrust; fz for a 3D body.",
		 .params = {entityId, {"fx", Number}, {"fy", Number}, {"fz", Number, true}}},
		{.table = "physics", .name = "add_force_at", .function = guarded<luaPhysicsAddForceAt>,
		 .description = "Push a 2D body with a force applied at a world point (px, py): off the centre, it turns.",
		 .params = {entityId, {"fx", Number}, {"fy", Number}, {"px", Number}, {"py", Number}}},
		{.table = "physics", .name = "add_force_at_3d", .function = guarded<luaPhysicsAddForceAt3d>,
		 .description = "Push a 3D body with a force applied at a world point.",
		 .params = {entityId, {"fx", Number}, {"fy", Number}, {"fz", Number}, {"px", Number}, {"py", Number},
					{"pz", Number}}},
		{.table = "physics", .name = "add_torque", .function = guarded<luaPhysicsAddTorque>,
		 .description = "Turn a 2D body with a torque (newton metres) over the next step.",
		 .params = {entityId, {"torque", Number}}},
		{.table = "physics", .name = "add_torque_3d", .function = guarded<luaPhysicsAddTorque3d>,
		 .description = "Turn a 3D body with a torque around each axis over the next step.",
		 .params = {entityId, {"tx", Number}, {"ty", Number}, {"tz", Number}}},
		{.table = "physics", .name = "attach", .function = guarded<luaPhysicsAttach>,
		 .description = "Merge the entity's collider into the body of its nearest ancestor with a body (one rigid "
						"assembly); false without one.",
		 .params = {entityId}, .returns = {{"attached", Boolean}}},
		{.table = "physics", .name = "detach", .function = guarded<luaPhysicsDetach>,
		 .description = "Take a part out of its assembly: it flies on with a body of its own.", .params = {entityId}},
		{.table = "physics", .name = "get_assembly", .function = guarded<luaPhysicsGetAssembly>,
		 .description = "Entity whose body carries the part, nil when the entity is not a merged part.",
		 .params = {entityId}, .returns = {{"root_id", Entity, true}}},
		{.table = "physics", .name = "get_mass", .function = guarded<luaPhysicsGetMass>,
		 .description = "Mass of the body the entity moves (its whole assembly for a part), 0 when static.",
		 .params = {entityId}, .returns = {{"mass", Number}}},
		{.table = "physics", .name = "get_center_of_mass", .function = guarded<luaPhysicsGetCenterOfMass>,
		 .description = "Centre of mass of the body the entity moves, in the world; z for a 3D body.",
		 .params = {entityId}, .returns = {{"x", Number}, {"y", Number}, {"z", Number, true}}},
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
		{.table = "input", .name = "set_cursor_visible", .function = guarded<luaInputSetCursorVisible>,
		 .description = "Show or hide the system cursor over the game, for a game drawing its own (an action icon, the "
						"object in hand); shown again when the play stops.",
		 .params = {{"visible", Boolean}}},
		{.table = "input", .name = "is_cursor_visible", .function = guarded<luaInputIsCursorVisible>,
		 .description = "Whether the system cursor is shown over the game (`input.set_cursor_visible`).",
		 .returns = {{"visible", Boolean}}},
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
		{.table = "sound", .name = "play", .function = blocking<luaSoundPlay>,
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
		{.table = "sound", .name = "set_bus_volume", .function = guarded<luaSoundSetBusVolume>,
		 .description = "Set the volume of a mixer bus (`Master`, `Music`, `SFX`, `Ambient`, `Voice`; 0.0 to 2.0).",
		 .params = {{"bus", String}, {"volume", Number}}},
		{.table = "sound", .name = "get_bus_volume", .function = guarded<luaSoundGetBusVolume>,
		 .description = "Volume of a mixer bus.", .params = {{"bus", String}}, .returns = {{"volume", Number}}},
		{.table = "sound", .name = "set_bus_muted", .function = guarded<luaSoundSetBusMuted>,
		 .description = "Mute or unmute a mixer bus.", .params = {{"bus", String}, {"muted", Boolean}}},
		{.table = "sound", .name = "is_bus_muted", .function = guarded<luaSoundIsBusMuted>,
		 .description = "Whether a mixer bus is muted.", .params = {{"bus", String}}, .returns = {{"muted", Boolean}}},
		{.table = "sound", .name = "play_music", .function = blocking<luaSoundPlayMusic>,
		 .description = "Play a looping track on the Music bus, crossfading from the track playing (1 s by default).",
		 .params = {{"asset_path", String}, {"fade", Number, true}}, .returns = {{"handle", Integer}}},
		{.table = "sound", .name = "stop_music", .function = guarded<luaSoundStopMusic>,
		 .description = "Fade out and stop the music track (1 s by default).", .params = {{"fade", Number, true}}},
		{.table = "sound", .name = "crossfade", .function = guarded<luaSoundCrossfade>,
		 .description = "Crossfade two sounds: the first fades out and stops, the second fades in from silence.",
		 .params = {{"from", Integer}, {"to", Integer}, {"seconds", Number}}},
		{.table = "sound", .name = "set_music_state", .function = blocking<luaSoundSetMusicState>,
		 .description = "Switch the adaptive musics of the scene to a state (fading the layers, playing its stinger); "
						"false when none knows it.",
		 .params = {{"state", String}}, .returns = {{"known", Boolean}}},
		{.table = "sound", .name = "get_music_state", .function = guarded<luaSoundGetMusicState>,
		 .description = "State of the adaptive music of the scene, empty when none plays.",
		 .returns = {{"state", String}}},
		{.table = "math", .name = "random", .function = guarded<luaMathRandom>,
		 .description = "Random value of the `lua` stream of the scene, the same on every run of the same seed: a "
						"number in [0, 1) without argument, an integer in [1, m] or [m, n], 64 random bits for 0.",
		 .params = {{"m", Integer, true}, {"n", Integer, true}}, .returns = {{"value", Number}}},
		{.table = "math", .name = "randomseed", .function = guarded<luaMathRandomSeed>,
		 .description = "Restart the `lua` stream from a seed, or from the seed of the scene without argument.",
		 .params = {{"seed", Integer, true}}},
		{.table = "math", .name = "sin", .function = guarded<luaMathUnary<static_cast<double (*)(double)>(math::sin)>>,
		 .description = "Sine of an angle in radians, the same bits on every target.", .params = {{"x", Number}},
		 .returns = {{"value", Number}}},
		{.table = "math", .name = "cos", .function = guarded<luaMathUnary<static_cast<double (*)(double)>(math::cos)>>,
		 .description = "Cosine of an angle in radians, the same bits on every target.", .params = {{"x", Number}},
		 .returns = {{"value", Number}}},
		{.table = "math", .name = "tan", .function = guarded<luaMathUnary<static_cast<double (*)(double)>(math::tan)>>,
		 .description = "Tangent of an angle in radians, the same bits on every target.", .params = {{"x", Number}},
		 .returns = {{"value", Number}}},
		{.table = "math", .name = "asin", .function = guarded<luaMathUnary<static_cast<double (*)(double)>(math::asin)>>,
		 .description = "Arc sine in radians, the same bits on every target.", .params = {{"x", Number}},
		 .returns = {{"angle", Number}}},
		{.table = "math", .name = "acos", .function = guarded<luaMathUnary<static_cast<double (*)(double)>(math::acos)>>,
		 .description = "Arc cosine in radians, the same bits on every target.", .params = {{"x", Number}},
		 .returns = {{"angle", Number}}},
		{.table = "math", .name = "atan", .function = guarded<luaMathAtan>,
		 .description = "Arc tangent of y / x in radians, in the quadrant of (x, y) (x is 1 when omitted), the same bits "
						"on every target.",
		 .params = {{"y", Number}, {"x", Number, true}}, .returns = {{"angle", Number}}},
		{.table = "math", .name = "exp", .function = guarded<luaMathUnary<static_cast<double (*)(double)>(math::exp)>>,
		 .description = "e to the power x, the same bits on every target.", .params = {{"x", Number}},
		 .returns = {{"value", Number}}},
		{.table = "math", .name = "log", .function = guarded<luaMathLog>,
		 .description = "Logarithm of x, natural or in a base, the same bits on every target.",
		 .params = {{"x", Number}, {"base", Number, true}}, .returns = {{"value", Number}}},
		{.table = "math", .name = "pow", .function = guarded<luaMathPow>,
		 .description = "x to the power y, the same bits on every target, unlike the `^` operator which uses the "
						"platform's maths.",
		 .params = {{"x", Number}, {"y", Number}}, .returns = {{"value", Number}}},
		{.table = "table", .name = "sorted_keys", .function = guarded<luaTableSortedKeys>,
		 .description = "Keys of a table in a fixed order (booleans, then numbers ascending, then strings bytewise), the "
						"same on every run whatever the insertion history: `for _, k in ipairs(table.sorted_keys(t))`. "
						"A key that is a table, a function or a userdata raises an error.",
		 .params = {{"t", Table}}, .returns = {{"keys", Table}}},
		{.table = "scene", .name = "get_world_origin", .function = guarded<luaSceneGetWorldOrigin>,
		 .description = "World position of the scene origin, which local positions are relative to.",
		 .returns = {{"x", Number}, {"y", Number}, {"z", Number}}},
		{.table = "scene", .name = "find_entity", .function = guarded<luaSceneFindEntity>,
		 .description = "First entity with this tag (0 when none), in the scene then in its HUD; scans every entity, "
						"cache the result.",
		 .params = {{"name", String}}, .returns = {{"entity_id", Entity}}},
		{.table = "scene", .name = "find_by_uuid", .function = guarded<luaSceneFindByUuid>,
		 .description = "Entity of a UUID written as text, as the scene file saves it (0 when none), in the scene then "
						"in its HUD.",
		 .params = {{"uuid", String}}, .returns = {{"entity_id", Entity}}},
		{.table = "scene", .name = "find_entities", .function = guarded<luaSceneFindEntities>,
		 .description = "Every entity of the scene with this tag, as an array of ids (empty when none); scans every "
						"entity, cache the result.",
		 .params = {{"name", String}}, .returns = {{"entity_ids", Table}}},
		{.table = "scene", .name = "create_entity", .function = guarded<luaSceneCreateEntity>,
		 .description = "Create an empty entity.",
		 .params = {{"name", String}}, .returns = {{"entity_id", Entity}}},
		{.table = "scene", .name = "destroy_entity", .function = guarded<luaSceneDestroyEntity>,
		 .description = "Destroy an entity and its children at the end of the frame.", .params = {entityId}},
		{.table = "scene", .name = "spawn_prefab", .function = blocking<luaSceneSpawnPrefab>,
		 .description = "Instantiate a prefab asset (`.owlprefab`) at runtime, its root placed at a position when one is "
						"given; its scripts get `on_create` before or after the scripts of the frame. 0 on failure.",
		 .params = {{"prefab_path", String}, {"x", Number, true}, {"y", Number, true}, {"z", Number, true}},
		 .returns = {{"entity_id", Entity}}},
		{.table = "scene", .name = "pick_entity", .function = guarded<luaScenePickEntity>,
		 .description = "Entity drawn under a viewport pixel (the mouse by default), nearest to the viewer: sprites and "
						"circles (2D, isometric or billboards) and meshes, not the UI; with a tag, only the entities of that tag, "
						"a hit on a descendant giving its tagged ancestor (a tower built of parts). 0 when none.",
		 .params = {{"x", Number, true}, {"y", Number, true}, {"tag", String, true}},
		 .returns = {{"entity_id", Entity}}},
		{.table = "scene", .name = "is_world_cell_loaded", .function = guarded<luaSceneIsWorldCellLoaded>,
		 .description = "Whether the world partition cell at integer coordinates is merged into the scene.",
		 .params = {{"x", Integer}, {"y", Integer}, {"z", Integer}},
		 .returns = {{"loaded", Boolean}}},
		{.table = "scene", .name = "get_loaded_world_cells", .function = guarded<luaSceneGetLoadedWorldCells>,
		 .description = "Number of world partition cells merged into the scene.",
		 .returns = {{"count", Integer}}},
		{.table = "scene", .name = "set_world_streaming", .function = guarded<luaSceneSetWorldStreaming>,
		 .description = "Switch the streaming of the world partition on or off; off unloads every cell.",
		 .params = {{"enabled", Boolean}}},
		{.table = "scene", .name = "get_planet_altitude", .function = guarded<luaSceneGetPlanetAltitude>,
		 .description = "Height of an entity above the ground (or the sea) of a planet, in metres; negative below "
						"it, 0 when either entity is missing.",
		 .params = {{"planet_id", Entity}, {"entity_id", Entity}},
		 .returns = {{"altitude", Number}}},
		{.table = "scene", .name = "get_viewport_size", .function = guarded<luaSceneGetViewportSize>,
		 .description = "Size of the viewport the scene is drawn into, in pixels (the window, or the game panel of "
						"Owl Nest); zeros before the first frame.",
		 .returns = {{"width", Integer}, {"height", Integer}}},
		{.table = "scene", .name = "get_viewport_mouse", .function = guarded<luaSceneGetViewportMouse>,
		 .description = "Mouse position in the viewport, in pixels from its top left (the pixel `scene.pick_entity` "
						"reads by default).",
		 .returns = {{"x", Number}, {"y", Number}}},
		{.table = "scene", .name = "load_scene", .function = guarded<luaSceneLoadScene>,
		 .description = "Load another level after this frame, keeping the game state (no transition).",
		 .params = {{"level", String}}},
		{.table = "scene", .name = "transition_to", .function = guarded<luaSceneTransitionTo>,
		 .description = "Load a level behind a screen transition (kind as in `ui.transition_play`, `fade` by default), "
						"in a colour (opaque black by default).",
		 .params = {{"scene_path", String},
					{"kind", String, true},
					{"duration", Number, true},
					{"r", Number, true},
					{"g", Number, true},
					{"b", Number, true},
					{"a", Number, true}}},
		{.table = "scene", .name = "quit", .function = guarded<luaSceneQuit>,
		 .description = "Quit the game (stop Play in the editor) after this frame."},
		{.table = "engine", .name = "version", .function = guarded<luaEngineVersion>,
		 .description = "Version of the engine running the game, as `major.minor.patch`.",
		 .returns = {{"version", String}}},
		{.table = "time", .name = "delta", .function = guarded<luaTimeDelta>,
		 .description = "Duration of the current frame, or of the fixed tick inside `on_tick`, in seconds.",
		 .returns = {{"seconds", Number}}},
		{.table = "time", .name = "real_delta", .function = guarded<luaTimeRealDelta>,
		 .description = "Real duration of the current frame, in seconds, before the time scale and the pause (menus).",
		 .returns = {{"seconds", Number}}},
		{.table = "time", .name = "set_scale", .function = guarded<luaTimeSetScale>,
		 .description = "Speed of the scene clock (0 to 100000, 1 by default): `time.delta`, the ticks, the physics, "
						"the animations and the behaviours follow it; the UI keeps the real time. Above the scene's "
						"rails time scale the time warps: the bodies follow their arcs (`physics.is_on_rails`).",
		 .params = {{"scale", Number}}},
		{.table = "time", .name = "get_scale", .function = guarded<luaTimeGetScale>,
		 .description = "Speed of the scene clock set by `time.set_scale`.", .returns = {{"scale", Number}}},
		{.table = "time", .name = "set_paused", .function = guarded<luaTimeSetPaused>,
		 .description = "Pause or resume the scene clock: paused, `on_update` still runs with a zero `time.delta`, no "
						"tick, physics step nor animation runs.",
		 .params = {{"paused", Boolean}}},
		{.table = "time", .name = "is_paused", .function = guarded<luaTimeIsPaused>,
		 .description = "Whether the scene clock is paused.", .returns = {{"paused", Boolean}}},
		{.table = "time", .name = "tick_alpha", .function = guarded<luaTimeTickAlpha>,
		 .description = "Progress of the frame between the last two fixed ticks, in [0, 1), to blend what is drawn.",
		 .returns = {{"alpha", Number}}},
		{.table = "log", .name = "trace", .function = guarded<luaLogTrace>,
		 .description = "Log a message at trace level.", .params = {{"message", String}}},
		{.table = "log", .name = "debug", .function = guarded<luaLogDebug>,
		 .description = "Log a message at debug level.", .params = {{"message", String}}},
		{.table = "log", .name = "info", .function = guarded<luaLogInfo>,
		 .description = "Log a message at info level.", .params = {{"message", String}}},
		{.table = "log", .name = "warn", .function = guarded<luaLogWarn>,
		 .description = "Log a message at warning level.", .params = {{"message", String}}},
		{.table = "log", .name = "error", .function = guarded<luaLogError>,
		 .description = "Log a message at error level.", .params = {{"message", String}}},
		{.table = "entity", .name = "has_component", .function = guarded<luaEntityHasComponent>,
		 .description = "Whether the entity has a component: `Transform`, `PhysicBody`, `SpriteRenderer`, "
						"`MeshRenderer`, `Light`, `Light2D`, `Camera`, `Text`, `SoundSource`, `Canvas` or a `Ui*` "
						"component.",
		 .params = {entityId, {"component", String}}, .returns = {{"has", Boolean}}},
		{.table = "entity", .name = "add_component", .function = guarded<luaEntityAddComponent>,
		 .description = "Add a component with its default values by YAML key (`SpriteRenderer`, `PhysicBody`, ...), "
						"kept when the entity has it already; false for an unknown or mandatory component.",
		 .params = {entityId, {"component", String}}, .returns = {{"added", Boolean}}},
		{.table = "entity", .name = "get_name", .function = guarded<luaEntityGetName>,
		 .description = "Tag of the entity (empty when unknown).", .params = {entityId}, .returns = {{"name", String}}},
		{.table = "entity", .name = "get_link", .function = guarded<luaEntityGetLink>,
		 .description = "Entity an `EntityLink` of this entity points at (0 when it has none or the target is missing).",
		 .params = {entityId}, .returns = {{"linked_id", Entity}}},
		{.table = "entity", .name = "set_tint", .function = guarded<luaEntitySetTint>,
		 .description = "Set the colour multiplied into a `SpriteRenderer`, `AnimatedSpriteRenderer`, `MeshRenderer` or "
						"`ParticleEmitter` (hit flash, fade, a burst in the colour of what broke); false when the entity "
						"has none of them. `a` is 1 when omitted.",
		 .params = {entityId, {"r", Number}, {"g", Number}, {"b", Number}, {"a", Number, true}},
		 .returns = {{"done", Boolean}}},
		{.table = "entity", .name = "get_tint", .function = guarded<luaEntityGetTint>,
		 .description = "Colour multiplied into a `SpriteRenderer`, `AnimatedSpriteRenderer`, `MeshRenderer` or "
						"`ParticleEmitter`; nothing when the entity has none of them.",
		 .params = {entityId},
		 .returns = {{"r", Number}, {"g", Number}, {"b", Number}, {"a", Number}}},
		{.table = "entity", .name = "set_mesh", .function = guarded<luaEntitySetMesh>,
		 .description = "Draw another mesh file (`.gltf`, `.glb`, `.obj`, asset path) with the entity's `MeshRenderer`, "
						"loaded before the next frame; false when the entity has no `MeshRenderer`.",
		 .params = {entityId, {"path", String}}, .returns = {{"done", Boolean}}},
		{.table = "entity", .name = "get_mesh", .function = guarded<luaEntityGetMesh>,
		 .description = "Asset path of the mesh file of the entity's `MeshRenderer`; nothing when it has none.",
		 .params = {entityId}, .returns = {{"path", String}}},
		{.table = "entity", .name = "set_material", .function = guarded<luaEntitySetMaterial>,
		 .description = "Give a submesh slot (0-based, -1 for every slot) of the entity's `MeshRenderer` a `.owlmat` "
						"material (asset path, empty for the file's own material); false when the entity has no "
						"`MeshRenderer` or the slot is invalid.",
		 .params = {entityId, {"slot", Number}, {"path", String}}, .returns = {{"done", Boolean}}},
		{.table = "entity", .name = "get_material", .function = guarded<luaEntityGetMaterial>,
		 .description = "Asset path of the `.owlmat` of a submesh slot (0-based) of the entity's `MeshRenderer`, empty "
						"for the file's own material; nothing when it has no `MeshRenderer`.",
		 .params = {entityId, {"slot", Number}}, .returns = {{"path", String}}},
		{.table = "entity", .name = "set_light_color", .function = guarded<luaEntitySetLightColor>,
		 .description = "Set the colour (sRGB, 0 to 1) of the entity's `Light` or `Light2D`; false when it has none.",
		 .params = {entityId, {"r", Number}, {"g", Number}, {"b", Number}}, .returns = {{"done", Boolean}}},
		{.table = "entity", .name = "get_light_color", .function = guarded<luaEntityGetLightColor>,
		 .description = "Colour of the entity's `Light` or `Light2D`; nothing when it has none.",
		 .params = {entityId}, .returns = {{"r", Number}, {"g", Number}, {"b", Number}}},
		{.table = "entity", .name = "set_light_intensity", .function = guarded<luaEntitySetLightIntensity>,
		 .description = "Set the intensity (at least 0) of the entity's `Light` or `Light2D`; false when it has "
						"none.",
		 .params = {entityId, {"intensity", Number}}, .returns = {{"done", Boolean}}},
		{.table = "entity", .name = "get_light_intensity", .function = guarded<luaEntityGetLightIntensity>,
		 .description = "Intensity of the entity's `Light` or `Light2D`; nothing when it has none.",
		 .params = {entityId}, .returns = {{"intensity", Number}}},
		{.table = "entity", .name = "set_light_enabled", .function = guarded<luaEntitySetLightEnabled>,
		 .description = "Switch the entity's `Light` or `Light2D` on or off (it keeps its settings); false when it has none.",
		 .params = {entityId, {"enabled", Boolean}}, .returns = {{"done", Boolean}}},
		{.table = "entity", .name = "is_light_enabled", .function = guarded<luaEntityIsLightEnabled>,
		 .description = "Whether the entity's `Light` or `Light2D` shines; nothing when it has none.",
		 .params = {entityId}, .returns = {{"enabled", Boolean}}},
		{.table = "entity", .name = "set_light_range", .function = guarded<luaEntitySetLightRange>,
		 .description = "Set the range (world units, at least 0.01) of the entity's point or spot `Light`, or the "
						"radius of its `Light2D`; false when it has neither.",
		 .params = {entityId, {"range", Number}}, .returns = {{"done", Boolean}}},
		{.table = "entity", .name = "set_flicker_enabled",
		 .function = guarded<luaEntitySetBehaviourEnabled<scene::component::LightFlicker>>,
		 .description = "Switch the entity's `LightFlicker` on or off (the light keeps its authored values while off); "
						"false when it has none.",
		 .params = {entityId, {"enabled", Boolean}}, .returns = {{"done", Boolean}}},
		{.table = "entity", .name = "is_flicker_enabled",
		 .function = guarded<luaEntityIsBehaviourEnabled<scene::component::LightFlicker>>,
		 .description = "Whether the entity's `LightFlicker` runs; nothing when it has none.",
		 .params = {entityId}, .returns = {{"enabled", Boolean}}},
		{.table = "entity", .name = "particles_play",
		 .function = guarded<luaEntityParticles<&renderer::ParticleSystem::play>>,
		 .description = "Start or resume the entity's `ParticleEmitter` (its emission and its particles); false when it "
						"has none.",
		 .params = {entityId}, .returns = {{"done", Boolean}}},
		{.table = "entity", .name = "particles_pause",
		 .function = guarded<luaEntityParticles<&renderer::ParticleSystem::pause>>,
		 .description = "Freeze the particles and the emission of the entity's `ParticleEmitter` where they are; false "
						"when it has none.",
		 .params = {entityId}, .returns = {{"done", Boolean}}},
		{.table = "entity", .name = "particles_stop",
		 .function = guarded<luaEntityParticles<&renderer::ParticleSystem::stop>>,
		 .description = "Stop the emission of the entity's `ParticleEmitter`; the living particles finish their life. "
						"False when it has none.",
		 .params = {entityId}, .returns = {{"done", Boolean}}},
		{.table = "entity", .name = "particles_restart",
		 .function = guarded<luaEntityParticles<&renderer::ParticleSystem::restart>>,
		 .description = "Kill the particles of the entity's `ParticleEmitter` and start its emission from the beginning; "
						"false when it has none.",
		 .params = {entityId}, .returns = {{"done", Boolean}}},
		{.table = "entity", .name = "particles_emit", .function = guarded<luaEntityParticlesEmit>,
		 .description = "Spawn a burst of particles from the entity's `ParticleEmitter` at the next frame, emitting or "
						"not; false when it has none.",
		 .params = {entityId, {"count", Integer}}, .returns = {{"done", Boolean}}},
		{.table = "entity", .name = "particles_is_emitting", .function = guarded<luaEntityParticlesIsEmitting>,
		 .description = "Whether the entity's `ParticleEmitter` plays and emits; nothing when it has none.",
		 .params = {entityId}, .returns = {{"emitting", Boolean}}},
		{.table = "entity", .name = "set_trail_enabled",
		 .function = guarded<luaEntitySetBehaviourEnabled<scene::component::Trail>>,
		 .description = "Switch the entity's `Trail` on or off (off clears it); false when it has none.",
		 .params = {entityId, {"enabled", Boolean}}, .returns = {{"done", Boolean}}},
		{.table = "entity", .name = "set_oscillator_enabled",
		 .function = guarded<luaEntitySetBehaviourEnabled<scene::component::Oscillator>>,
		 .description = "Switch the entity's `Oscillator` on or off (it stays where it is while off); false when it "
						"has none.",
		 .params = {entityId, {"enabled", Boolean}}, .returns = {{"done", Boolean}}},
		{.table = "entity", .name = "is_oscillator_enabled",
		 .function = guarded<luaEntityIsBehaviourEnabled<scene::component::Oscillator>>,
		 .description = "Whether the entity's `Oscillator` moves it; nothing when it has none.",
		 .params = {entityId}, .returns = {{"enabled", Boolean}}},
		{.table = "entity", .name = "set_patrol_enabled",
		 .function = guarded<luaEntitySetBehaviourEnabled<scene::component::Patrol>>,
		 .description = "Switch the entity's `Patrol` on or off (it stops where it is while off); false when it has "
						"none.",
		 .params = {entityId, {"enabled", Boolean}}, .returns = {{"done", Boolean}}},
		{.table = "entity", .name = "is_patrol_enabled",
		 .function = guarded<luaEntityIsBehaviourEnabled<scene::component::Patrol>>,
		 .description = "Whether the entity's `Patrol` walks it; nothing when it has none.",
		 .params = {entityId}, .returns = {{"enabled", Boolean}}},
		{.table = "entity", .name = "set_drift_enabled",
		 .function = guarded<luaEntitySetBehaviourEnabled<scene::component::Drift>>,
		 .description = "Switch the entity's `Drift` on or off (it stays where it is while off); false when it has "
						"none.",
		 .params = {entityId, {"enabled", Boolean}}, .returns = {{"done", Boolean}}},
		{.table = "entity", .name = "is_drift_enabled",
		 .function = guarded<luaEntityIsBehaviourEnabled<scene::component::Drift>>,
		 .description = "Whether the entity's `Drift` moves it; nothing when it has none.",
		 .params = {entityId}, .returns = {{"enabled", Boolean}}},
		{.table = "entity", .name = "set_gravity_source_enabled",
		 .function = guarded<luaEntitySetBehaviourEnabled<scene::component::GravitySource>>,
		 .description = "Switch the entity's `GravitySource` field on or off; false when it has none.",
		 .params = {entityId, {"enabled", Boolean}}, .returns = {{"done", Boolean}}},
		{.table = "entity", .name = "is_gravity_source_enabled",
		 .function = guarded<luaEntityIsBehaviourEnabled<scene::component::GravitySource>>,
		 .description = "Whether the entity's `GravitySource` pulls; nothing when it has none.",
		 .params = {entityId}, .returns = {{"enabled", Boolean}}},
		{.table = "entity", .name = "set_simulation_lod_enabled",
		 .function = guarded<luaEntitySetBehaviourEnabled<scene::component::SimulationLod>>,
		 .description = "Allow the entity's `SimulationLod` to demote it, or keep it fully simulated; false when it has "
						"none.",
		 .params = {entityId, {"enabled", Boolean}}, .returns = {{"done", Boolean}}},
		{.table = "entity", .name = "is_simulation_lod_enabled",
		 .function = guarded<luaEntityIsBehaviourEnabled<scene::component::SimulationLod>>,
		 .description = "Whether the entity's `SimulationLod` may demote it; nothing when it has none.",
		 .params = {entityId}, .returns = {{"enabled", Boolean}}},
		{.table = "entity", .name = "is_demoted", .function = guarded<luaEntityIsDemoted>,
		 .description = "Whether the entity's `SimulationLod` demoted it: its script runs `on_coarse_tick` instead of "
						"`on_update` and `on_tick`.",
		 .params = {entityId}, .returns = {{"demoted", Boolean}}},
		{.table = "entity", .name = "set_hud_marker_enabled",
		 .function = guarded<luaEntitySetBehaviourEnabled<scene::component::HudMarker>>,
		 .description = "Show or hide the entity's `HudMarker`, the HUD icon anchored to it; false when it has none.",
		 .params = {entityId, {"enabled", Boolean}}, .returns = {{"done", Boolean}}},
		{.table = "entity", .name = "is_hud_marker_enabled",
		 .function = guarded<luaEntityIsBehaviourEnabled<scene::component::HudMarker>>,
		 .description = "Whether the entity's `HudMarker` is drawn; nothing when it has none.",
		 .params = {entityId}, .returns = {{"enabled", Boolean}}},
		{.table = "entity", .name = "set_hud_marker_label", .function = guarded<luaEntitySetHudMarkerLabel>,
		 .description = "Set the label of the entity's `HudMarker`; false when it has none.",
		 .params = {entityId, {"label", String}}, .returns = {{"done", Boolean}}},
		{.table = "entity", .name = "set_follow_target_enabled",
		 .function = guarded<luaEntitySetBehaviourEnabled<scene::component::FollowTarget>>,
		 .description = "Switch the entity's `FollowTarget` on or off (it stays where it is while off); false when it "
						"has none.",
		 .params = {entityId, {"enabled", Boolean}}, .returns = {{"done", Boolean}}},
		{.table = "entity", .name = "is_follow_target_enabled",
		 .function = guarded<luaEntityIsBehaviourEnabled<scene::component::FollowTarget>>,
		 .description = "Whether the entity's `FollowTarget` places it; nothing when it has none.",
		 .params = {entityId}, .returns = {{"enabled", Boolean}}},
		{.table = "entity", .name = "get_time_of_day", .function = guarded<luaEntityGetTimeOfDay>,
		 .description = "Time of day of the entity's `DayNightCycle`, in hours from 0 to 24; nothing when it has none.",
		 .params = {entityId}, .returns = {{"hours", Number}}},
		{.table = "entity", .name = "set_time_of_day", .function = guarded<luaEntitySetTimeOfDay>,
		 .description = "Set the time of day of the entity's `DayNightCycle`, in hours (wrapped to 0 to 24); false when "
						"it has none.",
		 .params = {entityId, {"hours", Number}}, .returns = {{"done", Boolean}}},
		{.table = "entity", .name = "set_day_night_enabled",
		 .function = guarded<luaEntitySetBehaviourEnabled<scene::component::DayNightCycle>>,
		 .description = "Let the hours of the entity's `DayNightCycle` advance or freeze them; false when it has none.",
		 .params = {entityId, {"enabled", Boolean}}, .returns = {{"done", Boolean}}},
		{.table = "entity", .name = "is_day_night_enabled",
		 .function = guarded<luaEntityIsBehaviourEnabled<scene::component::DayNightCycle>>,
		 .description = "Whether the hours of the entity's `DayNightCycle` advance; nothing when it has none.",
		 .params = {entityId}, .returns = {{"enabled", Boolean}}},
		{.table = "entity", .name = "set_first_person_enabled",
		 .function = guarded<luaEntitySetBehaviourEnabled<scene::component::FirstPersonController>>,
		 .description = "Switch the entity's `FirstPersonController` on or off (scripts move it while off); false "
						"when it has none.",
		 .params = {entityId, {"enabled", Boolean}}, .returns = {{"done", Boolean}}},
		{.table = "entity", .name = "is_first_person_enabled",
		 .function = guarded<luaEntityIsBehaviourEnabled<scene::component::FirstPersonController>>,
		 .description = "Whether the entity's `FirstPersonController` walks it; nothing when it has none.",
		 .params = {entityId}, .returns = {{"enabled", Boolean}}},
		{.table = "entity", .name = "set_ship_enabled",
		 .function = guarded<luaEntitySetBehaviourEnabled<scene::component::ShipController>>,
		 .description = "Switch the entity's `ShipController` on or off (scripts move it while off); false when it has "
						"none.",
		 .params = {entityId, {"enabled", Boolean}}, .returns = {{"done", Boolean}}},
		{.table = "entity", .name = "is_ship_enabled",
		 .function = guarded<luaEntityIsBehaviourEnabled<scene::component::ShipController>>,
		 .description = "Whether the entity's `ShipController` flies it; nothing when it has none.",
		 .params = {entityId}, .returns = {{"enabled", Boolean}}},
		{.table = "entity", .name = "get_ship_velocity", .function = guarded<luaEntityGetShipVelocity>,
		 .description = "Velocity of the entity's `ShipController` in world space, in metres per second; nothing when "
						"it has none.",
		 .params = {entityId}, .returns = {{"x", Number}, {"y", Number}, {"z", Number}}},
		{.table = "entity", .name = "get_ship_speed", .function = guarded<luaEntityGetShipSpeed>,
		 .description = "Speed of the entity's `ShipController`, in metres per second; nothing when it has none.",
		 .params = {entityId}, .returns = {{"speed", Number}}},
		{.table = "entity", .name = "get_ship_throttle", .function = guarded<luaEntityGetShipThrottle>,
		 .description = "Thrust input of the entity's `ShipController` this frame, from -1 (reverse) to 1 (full ahead); "
						"nothing when it has none.",
		 .params = {entityId}, .returns = {{"throttle", Number}}},
		{.table = "entity", .name = "set_ship_assist", .function = guarded<luaEntitySetShipAssist>,
		 .description = "Switch the flight assist of the entity's `ShipController` on or off; false when it has none.",
		 .params = {entityId, {"assist", Boolean}}, .returns = {{"done", Boolean}}},
		{.table = "entity", .name = "is_ship_assist_on", .function = guarded<luaEntityIsShipAssistOn>,
		 .description = "Whether the flight assist of the entity's `ShipController` stabilises it; nothing when it has "
						"none.",
		 .params = {entityId}, .returns = {{"assist", Boolean}}},
		{.table = "entity", .name = "set_render_target_enabled",
		 .function = guarded<luaEntitySetBehaviourEnabled<scene::component::RenderTarget>>,
		 .description = "Switch the entity's `RenderTarget` on or off (its image keeps the last frame while off); "
						"false when it has none.",
		 .params = {entityId, {"enabled", Boolean}}, .returns = {{"done", Boolean}}},
		{.table = "entity", .name = "is_render_target_enabled",
		 .function = guarded<luaEntityIsBehaviourEnabled<scene::component::RenderTarget>>,
		 .description = "Whether the entity's `RenderTarget` draws its image each frame; nothing when it has none.",
		 .params = {entityId}, .returns = {{"enabled", Boolean}}},
		{.table = "ui", .name = "find", .function = guarded<luaUiFind>,
		 .description = "First widget with this tag, searched in the scene's HUD asset then in the scene (0 when none); "
						"cache the result.",
		 .params = {{"name", String}}, .returns = {{"entity_id", Entity}}},
		{.table = "ui", .name = "set_text", .function = guarded<luaUiSetText>,
		 .description = "Set the text of a `UiText`, or of a `TextRenderer` of the world (a label, a price).",
		 .params = {entityId, {"text", String}}},
		{.table = "ui", .name = "get_text", .function = guarded<luaUiGetText>,
		 .description = "Text of a `UiText` or of a `TextRenderer`.", .params = {entityId},
		 .returns = {{"text", String}}},
		{.table = "ui", .name = "set_visible", .function = guarded<luaUiSetVisible>,
		 .description = "Show or hide the entity in the game.", .params = {entityId, {"visible", Boolean}}},
		{.table = "ui", .name = "set_size", .function = guarded<luaUiSetSize>,
		 .description = "Resize the `UiRect` of a widget, in the units of its size (screen heights, shares of its parent, "
						"pixels): a card fitted to its lines; false when the entity has no `UiRect`.",
		 .params = {entityId, {"width", Number}, {"height", Number}}, .returns = {{"done", Boolean}}},
		{.table = "ui", .name = "get_rect", .function = guarded<luaUiGetRect>,
		 .description = "Rectangle of a widget as the HUD lays it out, in pixels of the viewport from its top left (an "
						"arrow aimed at a button at any interface size); nil when it is hidden or has no `UiRect`.",
		 .params = {entityId},
		 .returns = {{"x", Number}, {"y", Number}, {"width", Number}, {"height", Number}}},
		{.table = "ui", .name = "set_image", .function = blocking<luaUiSetImage>,
		 .description = "Show a texture asset in a `UiImage` (an empty path clears it); false when the entity has no "
						"`UiImage` or the texture cannot be loaded.",
		 .params = {entityId, {"texture_path", String}}, .returns = {{"done", Boolean}}},
		{.table = "ui", .name = "set_image_frame", .function = guarded<luaUiSetImageFrame>,
		 .description = "Show one frame of the sprite sheet of a `UiImage` (its columns x rows grid, row-major from the "
						"top left, 0-based, clamped).",
		 .params = {entityId, {"frame", Integer}}, .returns = {{"done", Boolean}}},
		{.table = "ui", .name = "get_image_frame", .function = guarded<luaUiGetImageFrame>,
		 .description = "Frame a `UiImage` shows (0 when it has none).", .params = {entityId},
		 .returns = {{"frame", Integer}}},
		{.table = "ui", .name = "is_world_blocked", .function = guarded<luaUiIsWorldBlocked>,
		 .description = "Whether a shown `UiWindow` (pause menu, inventory) holds the world interactions back this "
						"frame: a script ignores its world clicks while it is true.",
		 .returns = {{"blocked", Boolean}}},
		{.table = "ui", .name = "set_progress", .function = guarded<luaUiSetProgress>,
		 .description = "Set the value of a `UiProgressBar` (0 to 1).", .params = {entityId, {"value", Number}}},
		{.table = "ui", .name = "raise_window", .function = guarded<luaUiRaiseWindow>,
		 .description = "Draw a `UiWindow` over its siblings (a click on it does the same).", .params = {entityId}},
		{.table = "ui", .name = "set_window_title", .function = guarded<luaUiSetWindowTitle>,
		 .description = "Set the title of a `UiWindow`.", .params = {entityId, {"title", String}}},
		{.table = "ui", .name = "set_style", .function = guarded<luaUiSetStyle>,
		 .description = "Make every widget of an entity follow a style of the HUD theme (empty: none); false when the "
						"entity has no widget. The style applies from the next frame, its overrides kept.",
		 .params = {entityId, {"style", String}}, .returns = {{"styled", Boolean}}},
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
		{.table = "ui", .name = "show_subtitle", .function = guarded<luaUiShowSubtitle>,
		 .description = "Show a subtitle at the bottom of the screen for `seconds` (3 by default), when the player keeps the "
						"`subtitles` setting on; the same text on screen is renewed.",
		 .params = {{"text", String}, {"seconds", Number, true}}},
		{.table = "ui", .name = "clear_subtitles", .function = guarded<luaUiClearSubtitles>,
		 .description = "Remove every subtitle on screen."},
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
		{.table = "save", .name = "has_save", .function = blocking<luaSaveHasSave>,
		 .description = "Whether a slot holds a save.", .params = {{"slot", Integer}}, .returns = {{"exists", Boolean}}},
		{.table = "save", .name = "delete_save", .function = blocking<luaSaveDeleteSave>,
		 .description = "Delete the save of a slot.", .params = {{"slot", Integer}}},
		{.table = "save", .name = "list_saves", .function = blocking<luaSaveListSaves>,
		 .description = "Every save, as a list of `{slot, timestamp, scene}`.", .returns = {{"saves", Table}}},
		{.table = "locale", .name = "tr", .function = guarded<luaLocaleTr>,
		 .description = "Text of a key in the language of the game, else in the fallback language, else the key; "
						"also the global `tr`.",
		 .params = {{"key", String}}, .returns = {{"text", String}}},
		{.table = "locale", .name = "get_language", .function = guarded<luaLocaleGetLanguage>,
		 .description = "Code of the language of the texts (`en`), empty when the game has none.",
		 .returns = {{"code", String}}},
		{.table = "locale", .name = "set_language", .function = guarded<luaLocaleSetLanguage>,
		 .description = "Show the texts in a language (an unknown one falls back) and keep it in the `language` setting.",
		 .params = {{"code", String}}},
		{.table = "locale", .name = "get_languages", .function = guarded<luaLocaleGetLanguages>,
		 .description = "Languages of the game, as a list of `{code, name}`.", .returns = {{"languages", Table}}},
		{.table = "settings", .name = "get", .function = guarded<luaSettingsGet>,
		 .description = "Setting value: user override, else game default, else `default` (nil when absent).",
		 .params = {{"key", String}, {"default", Any, true}}, .returns = {{"value", Any}}},
		{.table = "settings", .name = "set", .function = guarded<luaSettingsSet>,
		 .description = "Set a user override (integer, number, string or boolean).",
		 .params = {{"key", String}, {"value", Any}}},
		{.table = "settings", .name = "save", .function = blocking<luaSettingsSave>,
		 .description = "Write the user overrides to `settings.yml`.", .returns = {{"ok", Boolean}}},
		{.table = "settings", .name = "load", .function = blocking<luaSettingsLoad>,
		 .description = "Reload the user overrides from `settings.yml`."},
		{.table = "settings", .name = "reset", .function = guarded<luaSettingsReset>,
		 .description = "Remove a user override (back to the game default).", .params = {{"key", String}}},
		{.table = "settings", .name = "reset_all", .function = guarded<luaSettingsResetAll>,
		 .description = "Remove every user override."},
		{.table = "settings", .name = "apply", .function = blocking<luaSettingsApply>,
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
		 .description = "Set the terrain and overlay tile of a cell of a layer (-1 for none), without undo; false outside the grid or when the cell already held them.",
		 .params = {entityId, {"layer", Integer}, {"cell_x", Integer}, {"cell_y", Integer}, {"terrain", Integer}, {"overlay", Integer}},
		 .returns = {{"done", Boolean}}},
		{.table = "isometric", .name = "get_size", .function = guarded<luaIsometricGetSize>,
		 .description = "Grid size of the tilemap of an entity, in cells, and the world size of a cell; nothing when the entity has no tilemap.",
		 .params = {entityId}, .returns = {{"width", Integer}, {"height", Integer}, {"cell_size", Number}}},
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
		 .params = {entityId, {"from_x", Integer}, {"from_y", Integer}, {"to_x", Integer}, {"to_y", Integer}, {"mode", Any, true}, {"options", Table, true}},
		 .returns = {{"request", Integer}}},
		{.table = "path", .name = "find_now", .function = guarded<luaPathFindNow>,
		 .description = "Search a shortest path at once (a long query on a big map costs frame time, prefer `path.find`): the cells as `{x1, y1, x2, y2, ...}` and the cost in cells, nothing when no path joins the cells. `options` as in `path.find`.",
		 .params = {entityId, {"from_x", Integer}, {"from_y", Integer}, {"to_x", Integer}, {"to_y", Integer}, {"mode", Any, true}, {"options", Table, true}},
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
		{.table = "walk", .name = "find_path", .function = guarded<luaWalkFindPath>,
		 .description = "Shortest path across the `WalkArea` of an entity between two world points (each moved onto the floor when outside): the corners as `{x1, y1, x2, y2, ...}`, start and goal included; nothing when they lie on floors that do not touch.",
		 .params = {entityId, {"from_x", Number}, {"from_y", Number}, {"to_x", Number}, {"to_y", Number}},
		 .returns = {{"points", Table}}},
		{.table = "walk", .name = "is_inside", .function = guarded<luaWalkIsInside>,
		 .description = "Whether a world point is on the floor of the `WalkArea` of an entity (outside its holes).",
		 .params = {entityId, {"x", Number}, {"y", Number}}, .returns = {{"inside", Boolean}}},
		{.table = "walk", .name = "get_closest_point", .function = guarded<luaWalkGetClosestPoint>,
		 .description = "Point of the floor of the `WalkArea` nearest to a world point (the point itself when on the floor); nothing when the area has no floor.",
		 .params = {entityId, {"x", Number}, {"y", Number}}, .returns = {{"x", Number}, {"y", Number}}},
		{.table = "walk", .name = "get_scale_at", .function = guarded<luaWalkGetScaleAt>,
		 .description = "Scale by depth the `WalkArea` gives to an actor standing at a world point (1 when the area does not scale).",
		 .params = {entityId, {"x", Number}, {"y", Number}}, .returns = {{"scale", Number}}},
		{.table = "walk", .name = "go_to", .function = guarded<luaWalkGoTo>,
		 .description = "Send a `WalkAgent` to a world point of its area (moved onto the floor when outside): the path is found at once and walked over the next frames; false when no path joins them, the agent then stands still.",
		 .params = {entityId, {"x", Number}, {"y", Number}}, .returns = {{"found", Boolean}}},
		{.table = "walk", .name = "stop", .function = guarded<luaWalkStop>,
		 .description = "Stop a `WalkAgent` where it stands.", .params = {entityId}},
		{.table = "walk", .name = "is_moving", .function = guarded<luaWalkIsMoving>,
		 .description = "Whether a `WalkAgent` still has a path to walk.",
		 .params = {entityId}, .returns = {{"moving", Boolean}}},
		{.table = "walk", .name = "get_target", .function = guarded<luaWalkGetTarget>,
		 .description = "World point a walking `WalkAgent` goes to; nothing when it stands.",
		 .params = {entityId}, .returns = {{"x", Number}, {"y", Number}}},
		{.table = "walk", .name = "set_speed", .function = guarded<luaWalkSetSpeed>,
		 .description = "Walking speed of a `WalkAgent`, in world units per second at a scale of 1.",
		 .params = {entityId, {"speed", Number}}},
		{.table = "walk", .name = "set_area", .function = guarded<luaWalkSetArea>,
		 .description = "Put a `WalkAgent` on the `WalkArea` of another entity (it stops); false when that entity has no area.",
		 .params = {entityId, {"area", Integer}}, .returns = {{"done", Boolean}}},
		{.table = "nav", .name = "find_path", .function = guarded<luaNavFindPath>,
		 .description = "Shortest path on the `NavMeshVolume` of an entity between two world points (each moved onto the mesh), links included: the corners as `{x1, y1, z1, x2, y2, z2, ...}`, start and goal included; nothing when a point is off the mesh or no path joins them.",
		 .params = {entityId, {"from_x", Number}, {"from_y", Number}, {"from_z", Number}, {"to_x", Number}, {"to_y", Number}, {"to_z", Number}},
		 .returns = {{"points", Table}}},
		{.table = "nav", .name = "get_closest_point", .function = guarded<luaNavGetClosestPoint>,
		 .description = "Point of the navigation mesh of a `NavMeshVolume` nearest to a world point, within 2 units; nothing when the mesh is farther.",
		 .params = {entityId, {"x", Number}, {"y", Number}, {"z", Number}},
		 .returns = {{"x", Number}, {"y", Number}, {"z", Number}}},
		{.table = "nav", .name = "bake", .function = guarded<luaNavBake>,
		 .description = "Bake the navigation mesh of a `NavMeshVolume` again from the static geometry inside its box (a level built at run time); false when no floor was found, the previous mesh is then kept.",
		 .params = {entityId}, .returns = {{"done", Boolean}}},
		{.table = "nav", .name = "go_to", .function = guarded<luaNavGoTo>,
		 .description = "Send a `NavAgent` to a world point: the path is found at the next tick and walked around the walls and the other agents; false when the entity has no agent.",
		 .params = {entityId, {"x", Number}, {"y", Number}, {"z", Number}}, .returns = {{"sent", Boolean}}},
		{.table = "nav", .name = "stop", .function = guarded<luaNavStop>,
		 .description = "Stop a `NavAgent` where it stands.", .params = {entityId}},
		{.table = "nav", .name = "is_moving", .function = guarded<luaNavIsMoving>,
		 .description = "Whether a `NavAgent` still has a path to walk, or a goal waiting for its path.",
		 .params = {entityId}, .returns = {{"moving", Boolean}}},
		{.table = "nav", .name = "get_target", .function = guarded<luaNavGetTarget>,
		 .description = "World point a walking `NavAgent` goes to; nothing when it stands.",
		 .params = {entityId}, .returns = {{"x", Number}, {"y", Number}, {"z", Number}}},
		{.table = "nav", .name = "set_speed", .function = guarded<luaNavSetSpeed>,
		 .description = "Highest speed of a `NavAgent`, in world units per second.",
		 .params = {entityId, {"speed", Number}}},
		{.table = "animator", .name = "set_float", .function = guarded<luaAnimatorSetFloat>,
		 .description = "Set a number parameter of the entity's `Animator` state machine; false when it has no such parameter.",
		 .params = {entityId, {"name", String}, {"value", Number}}, .returns = {{"done", Boolean}}},
		{.table = "animator", .name = "set_bool", .function = guarded<luaAnimatorSetBool>,
		 .description = "Set a flag parameter of the entity's `Animator` state machine; false when it has no such parameter.",
		 .params = {entityId, {"name", String}, {"value", Boolean}}, .returns = {{"done", Boolean}}},
		{.table = "animator", .name = "trigger", .function = guarded<luaAnimatorTrigger>,
		 .description = "Fire a trigger parameter of the entity's `Animator`: it stays set until a transition uses it.",
		 .params = {entityId, {"name", String}}, .returns = {{"done", Boolean}}},
		{.table = "animator", .name = "get_parameter", .function = guarded<luaAnimatorGetParameter>,
		 .description = "Value of a parameter of the entity's `Animator` (1 or 0 for a flag); nothing when it has none.",
		 .params = {entityId, {"name", String}}, .returns = {{"value", Number}}},
		{.table = "animator", .name = "get_state", .function = guarded<luaAnimatorGetState>,
		 .description = "Name of the current state of the entity's `Animator`; nothing before its first frame.",
		 .params = {entityId}, .returns = {{"state", String}}},
		{.table = "animator", .name = "play", .function = guarded<luaAnimatorPlay>,
		 .description = "Cross-fade the entity's `Animator` to a state, whatever the transitions, in `fade` seconds (0.2 by default).",
		 .params = {entityId, {"state", String}, {"fade", Number, true}}, .returns = {{"done", Boolean}}},
		{.table = "animator", .name = "set_playing", .function = guarded<luaAnimatorSetPlaying>,
		 .description = "Run or hold the entity's `Animator` (held, the mesh keeps its pose); false when it has none.",
		 .params = {entityId, {"playing", Boolean}}, .returns = {{"done", Boolean}}},
		{.table = "socket", .name = "attach", .function = guarded<luaSocketAttach>,
		 .description = "Snap the entity on a socket of `owner`: it becomes its child and follows the socket (and its bone); false when the owner has no such socket.",
		 .params = {entityId, {"owner", Entity}, {"socket", String}}, .returns = {{"done", Boolean}}},
		{.table = "socket", .name = "detach", .function = guarded<luaSocketDetach>,
		 .description = "Release the entity from its socket: it keeps its parent and stays where it is; false when it was attached to none.",
		 .params = {entityId}, .returns = {{"done", Boolean}}},
		{.table = "socket", .name = "get_position", .function = guarded<luaSocketGetPosition>,
		 .description = "World position of a socket of the entity, following its bone; nothing when it has no such socket.",
		 .params = {entityId, {"socket", String}}, .returns = {{"x", Number}, {"y", Number}, {"z", Number}}},
		{.table = "spline", .name = "get_length", .function = guarded<luaSplineGetLength>,
		 .description = "Length of the entity's `Spline`, in the units of the entity; nothing when it has none.",
		 .params = {entityId}, .returns = {{"length", Number}}},
		{.table = "spline", .name = "sample", .function = guarded<luaSplineSample>,
		 .description = "World position at a distance along the entity's `Spline` (clamped when open, wrapped when closed), in a lane when named; nothing when it has none.",
		 .params = {entityId, {"distance", Number}, {"lane", String, true}}, .returns = {{"x", Number}, {"y", Number}, {"z", Number}}},
		{.table = "spline", .name = "get_tangent", .function = guarded<luaSplineGetTangent>,
		 .description = "World unit direction of the entity's `Spline` at a distance along it; nothing when it has none.",
		 .params = {entityId, {"distance", Number}}, .returns = {{"x", Number}, {"y", Number}, {"z", Number}}},
		{.table = "spline", .name = "get_nearest", .function = guarded<luaSplineGetNearest>,
		 .description = "Distance along the entity's `Spline` of its point nearest to a world position; nothing when it has none.",
		 .params = {entityId, {"x", Number}, {"y", Number}, {"z", Number}}, .returns = {{"distance", Number}}},
		{.table = "spline", .name = "get_point_count", .function = guarded<luaSplineGetPointCount>,
		 .description = "Number of control points of the entity's `Spline`; nothing when it has none.",
		 .params = {entityId}, .returns = {{"count", Integer}}},
		{.table = "spline", .name = "get_point", .function = guarded<luaSplineGetPoint>,
		 .description = "World position of a control point of the entity's `Spline`, from 1; nothing when it has no such point.",
		 .params = {entityId, {"index", Integer}}, .returns = {{"x", Number}, {"y", Number}, {"z", Number}}},
		{.table = "spline", .name = "set_point", .function = guarded<luaSplineSetPoint>,
		 .description = "Move a control point of the entity's `Spline` (from 1) to a world position; false when it has no such point.",
		 .params = {entityId, {"index", Integer}, {"x", Number}, {"y", Number}, {"z", Number}}, .returns = {{"done", Boolean}}},
		{.table = "spline", .name = "get_distance", .function = guarded<luaSplineGetDistance>,
		 .description = "Distance along its spline of the entity's `SplineFollower`; nothing when it has none.",
		 .params = {entityId}, .returns = {{"distance", Number}}},
		{.table = "spline", .name = "set_distance", .function = guarded<luaSplineSetDistance>,
		 .description = "Put the entity's `SplineFollower` at a distance along its spline (placed at the next frame); false when it has none.",
		 .params = {entityId, {"distance", Number}}, .returns = {{"done", Boolean}}},
		{.table = "spline", .name = "get_speed", .function = guarded<luaSplineGetSpeed>,
		 .description = "Speed of the entity's `SplineFollower`, in units of the spline per second; nothing when it has none.",
		 .params = {entityId}, .returns = {{"speed", Number}}},
		{.table = "spline", .name = "set_speed", .function = guarded<luaSplineSetSpeed>,
		 .description = "Set the speed of the entity's `SplineFollower` (negative goes backwards); false when it has none.",
		 .params = {entityId, {"speed", Number}}, .returns = {{"done", Boolean}}},
		{.table = "spline", .name = "is_waiting", .function = guarded<luaSplineIsWaiting>,
		 .description = "Whether the entity's `SplineFollower` waits at one of its stops; nothing when it has none.",
		 .params = {entityId}, .returns = {{"waiting", Boolean}}},
		{.table = "spline", .name = "set_lane", .function = guarded<luaSplineSetLane>,
		 .description = "Move the entity's `SplineFollower` to a lane of its spline (empty: the curve itself); false when it has none.",
		 .params = {entityId, {"lane", String}}, .returns = {{"done", Boolean}}},
		{.table = "entity", .name = "set_spline_follower_enabled",
		 .function = guarded<luaEntitySetBehaviourEnabled<scene::component::SplineFollower>>,
		 .description = "Switch the entity's `SplineFollower` on or off (it stays where it is while off); false when it has none.",
		 .params = {entityId, {"enabled", Boolean}}, .returns = {{"done", Boolean}}},
		{.table = "entity", .name = "is_spline_follower_enabled",
		 .function = guarded<luaEntityIsBehaviourEnabled<scene::component::SplineFollower>>,
		 .description = "Whether the entity's `SplineFollower` moves it; nothing when it has none.",
		 .params = {entityId}, .returns = {{"enabled", Boolean}}},
		{.table = "datatable", .name = "get", .function = guarded<luaDataTableGet>,
		 .description = "Cell of a `.owltable` by row key and column name: an integer, a number, a boolean or a string (an asset path, the row key of a reference); nothing when absent.",
		 .params = {{"table", String}, {"row", String}, {"column", String}}, .returns = {{"value", Any}}},
		{.table = "datatable", .name = "get_row", .function = guarded<luaDataTableGetRow>,
		 .description = "Every cell of a row of a `.owltable`, by column name; nothing when it has no such row.",
		 .params = {{"table", String}, {"row", String}}, .returns = {{"row", Table}}},
		{.table = "datatable", .name = "get_keys", .function = guarded<luaDataTableGetKeys>,
		 .description = "Keys of the rows of a `.owltable`, in order; nothing when it cannot be read.",
		 .params = {{"table", String}}, .returns = {{"keys", Table}}},
		{.table = "datatable", .name = "get_columns", .function = guarded<luaDataTableGetColumns>,
		 .description = "Names of the columns of a `.owltable`, in order; nothing when it cannot be read.",
		 .params = {{"table", String}}, .returns = {{"columns", Table}}},
		{.table = "datatable", .name = "get_count", .function = guarded<luaDataTableGetCount>,
		 .description = "Number of rows of a `.owltable`; nothing when it cannot be read.",
		 .params = {{"table", String}}, .returns = {{"count", Integer}}},
		{.table = "datatable", .name = "has_row", .function = guarded<luaDataTableHasRow>,
		 .description = "Whether a `.owltable` has a row of this key.",
		 .params = {{"table", String}, {"row", String}}, .returns = {{"found", Boolean}}},
		{.table = "datatable", .name = "resolve", .function = guarded<luaDataTableResolve>,
		 .description = "Follow a reference cell: the table it points into and the key of the row; nothing when the column is not a reference.",
		 .params = {{"table", String}, {"row", String}, {"column", String}}, .returns = {{"table", String}, {"key", String}}},
		{.table = "datatable", .name = "find", .function = guarded<luaDataTableFind>,
		 .description = "Key of the first row of a `.owltable` whose cell in a column equals a value; nothing when none does.",
		 .params = {{"table", String}, {"column", String}, {"value", Any}}, .returns = {{"key", String}}},
		{.table = "dialogue", .name = "start", .function = guarded<luaDialogueStart>,
		 .description = "Start a conversation over a `.owldlg` dialogue and go to its first line; its conditions and actions are global functions of the calling script. Nothing when the dialogue cannot be read or ends at once.",
		 .params = {{"dialogue", String}}, .returns = {{"conversation", Integer}}},
		{.table = "dialogue", .name = "get_line", .function = guarded<luaDialogueGetLine>,
		 .description = "What a conversation shows: `{kind, speaker, text, sound, choices}`, the texts in the language of the game, `choices` the offered answers of a Choice (empty on a Line); nothing once it ended.",
		 .params = {{"conversation", Integer}}, .returns = {{"line", Table}}},
		{.table = "dialogue", .name = "next", .function = guarded<luaDialogueNext>,
		 .description = "Leave the current line for the next one; does nothing on a Choice. False once the conversation ended.",
		 .params = {{"conversation", Integer}}, .returns = {{"running", Boolean}}},
		{.table = "dialogue", .name = "choose", .function = guarded<luaDialogueChoose>,
		 .description = "Pick an offered answer of the current Choice, from 1. False once the conversation ended.",
		 .params = {{"conversation", Integer}, {"choice", Integer}}, .returns = {{"running", Boolean}}},
		{.table = "dialogue", .name = "is_running", .function = guarded<luaDialogueIsRunning>,
		 .description = "Whether a conversation still shows a line.",
		 .params = {{"conversation", Integer}}, .returns = {{"running", Boolean}}},
		{.table = "dialogue", .name = "stop", .function = guarded<luaDialogueStop>,
		 .description = "End a conversation where it is.", .params = {{"conversation", Integer}}, .returns = {}},
		{.table = "sequence", .name = "play", .function = guarded<luaSequencePlay>,
		 .description = "Play the `.owlseq` of the entity's `SequencePlayer` from a time (0 by default), its camera made primary; false when it has none or the sequence cannot be read.",
		 .params = {entityId, {"from", Number, true}}, .returns = {{"done", Boolean}}},
		{.table = "sequence", .name = "stop", .function = guarded<luaSequenceStop>,
		 .description = "Stop the sequence of the entity's `SequencePlayer`: the cameras it replaced are primary again and its `onFinished` function is called; false when it does not play.",
		 .params = {entityId}, .returns = {{"done", Boolean}}},
		{.table = "sequence", .name = "seek", .function = guarded<luaSequenceSeek>,
		 .description = "Move the playing sequence of the entity to a time, in seconds, without calling the events in between; false when it does not play.",
		 .params = {entityId, {"time", Number}}, .returns = {{"done", Boolean}}},
		{.table = "sequence", .name = "is_playing", .function = guarded<luaSequenceIsPlaying>,
		 .description = "Whether the entity's `SequencePlayer` plays; nothing when it has none.",
		 .params = {entityId}, .returns = {{"playing", Boolean}}},
		{.table = "sequence", .name = "get_time", .function = guarded<luaSequenceGetTime>,
		 .description = "Playback time of the entity's `SequencePlayer`, in seconds; nothing when it has none.",
		 .params = {entityId}, .returns = {{"time", Number}}},
		{.table = "sequence", .name = "get_duration", .function = guarded<luaSequenceGetDuration>,
		 .description = "Length of the sequence of the entity's `SequencePlayer`, in seconds; nothing when it has none or it cannot be read.",
		 .params = {entityId}, .returns = {{"duration", Number}}},
		{.table = "sequence", .name = "set_speed", .function = guarded<luaSequenceSetSpeed>,
		 .description = "Set the playback speed of the entity's `SequencePlayer` (1: real time, 0: paused); false when it has none.",
		 .params = {entityId, {"speed", Number}}, .returns = {{"done", Boolean}}},
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
			{.name = "math",
			 .description = "Random values drawn from the scene seed and elementary functions with the same bits on "
							"every target, over the Lua standard `math` table."},
			{.name = "table", .description = "Sorted iteration over the Lua standard `table` table."},
			{.name = "physics", .description = "Physics body of an entity."},
			{.name = "input",
			 .description = "Actions of the game's input map, then the raw keyboard, mouse and gamepad state."},
			{.name = "sound", .description = "Sound playback by handle, the mixer buses and the music."},
			{.name = "scene", .description = "Entities of the running level and level changes."},
			{.name = "engine", .description = "The engine running the game."},
			{.name = "time", .description = "Frame and fixed-tick time."},
			{.name = "log", .description = "Engine log."},
			{.name = "entity", .description = "Entity queries."},
			{.name = "ui", .description = "HUD widgets and screen transitions."},
			{.name = "gamestate", .description = "Values kept across levels and stored in saves."},
			{.name = "save", .description = "Save slots (applied after the frame)."},
			{.name = "settings", .description = "Game settings: defaults from `game_settings.yml`, user overrides."},
			{.name = "locale", .description = "Texts of the game in its languages (`localisation.yml`)."},
			{.name = "trigger", .description = "Timer triggers."},
			{.name = "door", .description = "Raycast doors."},
			{.name = "pushwall", .description = "Raycast push-walls."},
			{.name = "isometric", .description = "Terrain picking and tile editing of an isometric tilemap."},
			{.name = "animator",
			 .description = "State machine of the `Animator` of a skinned mesh: parameters and states."},
			{.name = "socket", .description = "Entities snapped on the sockets of a mesh or of an entity."},
			{.name = "spline", .description = "Splines sampled by distance, and the entities following them."},
			{.name = "datatable",
			 .description = "Typed records of the `.owltable` data tables, by row key and column."},
			{.name = "dialogue",
			 .description = "Conversations over the `.owldlg` dialogues: lines, choices, Lua conditions and actions."},
			{.name = "sequence",
			 .description = "Cutscenes of the `.owlseq` sequences: play, stop and seek a `SequencePlayer`."},
			{.name = "path",
			 .description = "Shortest paths over the graph a tilemap's `navigation` rules describe (roads, tracks), "
							"searched over several frames."},
			{.name = "walk",
			 .description = "Free walking on the 2D floors of the `WalkArea`s: paths around the holes, the agents "
							"walking them, the scale by depth."},
			{.name = "nav",
			 .description = "Navigation meshes of the 3D scenes: paths over the floors and the links, the agents "
							"walking them around each other."},
	};
	return s_tables;
}

auto getLuaReferencePages() -> const std::vector<LuaReferencePage>& {
	static const std::vector<LuaReferencePage> s_pages{
			{.file = "lua-api-entities.md", .title = "Entities and transforms", .tables = {"entity", "transform"}},
			{.file = "lua-api-scene.md",
			 .title = "Scene, engine and log",
			 .tables = {"scene", "engine", "log", "math", "table"}},
			{.file = "lua-api-input.md", .title = "Input", .tables = {"input"}},
			{.file = "lua-api-physics.md", .title = "Physics", .tables = {"physics"}},
			{.file = "lua-api-sound.md", .title = "Sound", .tables = {"sound"}},
			{.file = "lua-api-time.md", .title = "Time and timers", .tables = {"time", "trigger"}},
			{.file = "lua-api-ui.md", .title = "HUD", .tables = {"ui"}},
			{.file = "lua-api-saves.md",
			 .title = "Game state, saves and settings",
			 .tables = {"gamestate", "save", "settings"}},
			{.file = "lua-api-locale.md", .title = "Localisation", .tables = {"locale"}},
			{.file = "lua-api-raycast.md", .title = "Raycast doors and push-walls", .tables = {"door", "pushwall"}},
			{.file = "lua-api-isometric.md",
			 .title = "Isometric terrain, paths and walkable areas",
			 .tables = {"isometric", "path", "walk"}},
			{.file = "lua-api-navigation.md", .title = "Navigation meshes", .tables = {"nav"}},
			{.file = "lua-api-animation.md", .title = "Animation and sockets", .tables = {"animator", "socket"}},
			{.file = "lua-api-data.md", .title = "Splines and data tables", .tables = {"spline", "datatable"}},
			{.file = "lua-api-dialogue.md", .title = "Dialogues", .tables = {"dialogue"}},
			{.file = "lua-api-sequence.md", .title = "Cutscene sequences", .tables = {"sequence"}},
	};
	return s_pages;
}

auto generateLuaReference() -> std::vector<LuaReferenceFile> {
	const auto pageId = [](const std::string_view iFile) -> std::string {
		return "page-" + std::string{iFile.substr(0, iFile.find('.'))};
	};
	const auto findTable = [](const std::string_view iName) -> const LuaTable* {
		const auto found = std::ranges::find(getLuaTables(), iName, &LuaTable::name);
		return found == getLuaTables().end() ? nullptr : &*found;
	};
	std::vector<LuaReferenceFile> files;
	const std::string index =
			"# Lua API reference {#page-lua-api}\n\n[TOC]\n\n"
			"**Level:** Confirmed. **Read first:** [Scripting Games in Lua](scripting.md).\n\n"
			"Every table and function a script can call, one page per theme. These pages are generated from "
			"the binding registry (`getLuaBindings()` in `source/owl/private/script/LuaBindings.cpp`): do "
			"not edit them by hand. When a binding changes, `owl_script_tests_unit_test` fails and writes "
			"the new pages to `lua-api/` in its working directory (the failure names the folder): copy "
			"them over `doc/pages/`. See [Lua scripting](scripting.md) for the callbacks, the properties "
			"and the sandbox.\n\n"
			"An `entity` is the integer UUID of an entity (`entity_id` in its own script, "
			"`scene.find_entity` for the others); `[name: type]` is optional.\n\n## Pages\n\n";
	std::vector<std::array<std::string, 3>> indexRows{{"Page", "Table", "Content"}};
	files.push_back({.name = "lua-api.md", .content = {}});
	for (const auto& page: getLuaReferencePages()) {
		std::string content = std::format("# Lua API: {} {{#{}}}\n\n[TOC]\n\n", page.title, pageId(page.file));
		content += "Generated from the binding registry, do not edit by hand: see the [Lua API reference](lua-api.md) "
				   "for the conventions and the other pages.\n";
		bool firstRow = true;
		for (const auto tableName: page.tables) {
			const auto* table = findTable(tableName);
			if (table == nullptr)
				continue;
			indexRows.push_back({firstRow ? std::format("[{}]({})", page.title, page.file) : std::string{},
								 std::format("`{}`", table->name), std::string{table->description}});
			firstRow = false;
			content += std::format("\n## `{}`\n\n{}\n\n", table->name, table->description);
			std::vector<std::array<std::string, 3>> rows{{"Function", "Returns", "Description"}};
			for (const auto& binding: getLuaBindings()) {
				if (binding.table == table->name)
					rows.push_back(
							{describeSignature(binding), describeReturns(binding), std::string{binding.description}});
			}
			content += markdownTable(rows);
		}
		files.push_back({.name = std::string{page.file}, .content = std::move(content)});
	}
	files.front().content = index + markdownTable(indexRows);
	return files;
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
	// `tr` is the one global binding: translated texts appear in most lines of a game's scripts.
	lua_pushcfunction(iState, guarded<luaLocaleTr>);
	lua_setglobal(iState, "tr");
}

}// namespace owl::script
