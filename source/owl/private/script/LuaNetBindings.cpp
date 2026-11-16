/**
 * @file LuaNetBindings.cpp
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "LuaNetBindings.h"

#include "EntityHandle.h"
#include "app/EngineContext.h"
#include "core/Macros.h"
#include "core/external/lua.h"
#include "network/GameSession.h"
#include "scene/Entity.h"
#include "scene/Scene.h"
#include "scene/component/Replicated.h"
#include "script/LuaEngine.h"
#include "script/LuaValues.h"

#include <magic_enum/magic_enum.hpp>

#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace owl::script {

namespace {

template<LuaFunction Function>
auto guarded(lua_State* iState) -> int {
	return LuaEngine::callGuarded(iState, Function);
}

auto contextOf(lua_State* iState) -> app::EngineContext* {
	const auto* boundScene = getBoundScene(iState);
	return boundScene != nullptr ? boundScene->getEngineContext() : nullptr;
}

// The game's session, created on first use; none for a script bound to no scene of a game.
auto sessionOf(lua_State* iState) -> network::GameSession* {
	auto* context = contextOf(iState);
	return context != nullptr ? &context->getNetworkSession() : nullptr;
}

// The session when the game has one already: reading it does not open one.
auto existingSessionOf(lua_State* iState) -> network::GameSession* {
	auto* context = contextOf(iState);
	return context != nullptr && context->hasNetworkSession() ? &context->getNetworkSession() : nullptr;
}

auto entityAt(lua_State* iState, const int iIndex) -> std::optional<scene::Entity> {
	auto* activeScene = getBoundScene(iState);
	if (activeScene == nullptr)
		return std::nullopt;
	const auto handle = luaL_checknumber(iState, iIndex);
	if (handle < 1 || handle != std::floor(handle))
		return std::nullopt;
	return resolveEntityHandle(*activeScene, static_cast<uint64_t>(handle));
}

// The UUID of the entity a call targets: 0 for the scene script.
auto targetAt(lua_State* iState, const int iIndex) -> std::optional<uint64_t> {
	if (isInteger(iState, iIndex) && toInteger(iState, iIndex) == 0)
		return 0;
	const auto entity = entityAt(iState, iIndex);
	if (!entity.has_value())
		return std::nullopt;
	return static_cast<uint64_t>(entity->getUUID());
}

auto toLower(const std::string_view iName) -> std::string {
	std::string out;
	for (const char character: iName) {
		if (character >= 'A' && character <= 'Z' && !out.empty())
			out += '_';
		out += static_cast<char>(character >= 'A' && character <= 'Z' ? character - 'A' + 'a' : character);
	}
	return out;
}

template<typename Enum>
void pushEnumName(lua_State* iState, const Enum iValue) {
	const auto name = toLower(magic_enum::enum_name(iValue));
	lua_pushlstring(iState, name.data(), name.size());
}

auto readArgs(lua_State* iState, const int iFirst) -> std::vector<CallValue> {
	std::vector<CallValue> args;
	for (int index = iFirst; index <= lua_gettop(iState); ++index) {
		switch (lua_type(iState, index)) {
			case LUA_TBOOLEAN:
				args.emplace_back(lua_toboolean(iState, index) != 0);
				break;
			case LUA_TNUMBER:
				if (isInteger(iState, index))
					args.emplace_back(toInteger(iState, index));
				else
					args.emplace_back(static_cast<double>(lua_tonumber(iState, index)));
				break;
			case LUA_TSTRING:
				{
					size_t length = 0;
					const char* text = lua_tolstring(iState, index, &length);
					args.emplace_back(std::string{text, length});
					break;
				}
			case LUA_TNIL:
				args.emplace_back();
				break;
			default:
				luaL_error(iState,
						   "net: argument %d of a call is a %s; send nil, booleans, numbers and strings "
						   "(net.uuid for an entity)",
						   index - iFirst + 1, luaL_typename(iState, index));
		}
	}
	return args;
}

auto luaNetHost(lua_State* iState) -> int {
	auto* session = sessionOf(iState);
	const auto port = static_cast<uint16_t>(optInteger(iState, 1, 0));
	lua_pushboolean(iState, session != nullptr && session->host(port).has_value() ? 1 : 0);
	return 1;
}

auto luaNetJoin(lua_State* iState) -> int {
	auto* session = sessionOf(iState);
	const char* address = luaL_checkstring(iState, 1);
	const auto port = static_cast<uint16_t>(checkInteger(iState, 2));
	lua_pushboolean(iState, session != nullptr && session->join(address, port).has_value() ? 1 : 0);
	return 1;
}

auto luaNetLeave(lua_State* iState) -> int {
	if (auto* session = existingSessionOf(iState); session != nullptr)
		session->leave();
	return 0;
}

auto luaNetGetRole(lua_State* iState) -> int {
	const auto* session = existingSessionOf(iState);
	pushEnumName(iState, session != nullptr ? session->getRole() : network::NetRole::Offline);
	return 1;
}

auto luaNetGetState(lua_State* iState) -> int {
	const auto* session = existingSessionOf(iState);
	pushEnumName(iState, session != nullptr ? session->getMatchState() : network::MatchState::Idle);
	return 1;
}

auto luaNetIsServer(lua_State* iState) -> int {
	const auto* session = existingSessionOf(iState);
	lua_pushboolean(iState, session == nullptr || session->getRole() != network::NetRole::Client ? 1 : 0);
	return 1;
}

auto luaNetGetLocalPlayer(lua_State* iState) -> int {
	const auto* session = existingSessionOf(iState);
	pushInteger(iState, session != nullptr ? session->getLocalPlayer() : network::g_HostPlayer);
	return 1;
}

auto luaNetGetPort(lua_State* iState) -> int {
	const auto* session = existingSessionOf(iState);
	pushInteger(iState, session != nullptr ? session->getPort() : 0);
	return 1;
}

auto luaNetSetPlayerName(lua_State* iState) -> int {
	if (auto* session = sessionOf(iState); session != nullptr)
		session->setLocalName(luaL_checkstring(iState, 1));
	return 0;
}

auto luaNetSetInputType(lua_State* iState) -> int {
	const std::string_view name = luaL_checkstring(iState, 1);
	std::optional<network::InputType> type;
	for (const auto value: magic_enum::enum_values<network::InputType>()) {
		if (toLower(magic_enum::enum_name(value)) == name)
			type = value;
	}
	if (!type.has_value())
		luaL_error(iState, "net.set_input_type: unknown input type '%s' (keyboard_mouse, gamepad, touch)",
				   std::string{name}.c_str());
	if (auto* session = sessionOf(iState); session != nullptr)
		session->setLocalInputType(*type);
	return 0;
}

auto luaNetSetReady(lua_State* iState) -> int {
	if (auto* session = sessionOf(iState); session != nullptr)
		session->setReady(lua_toboolean(iState, 1) != 0);
	return 0;
}

auto luaNetGetPlayers(lua_State* iState) -> int {
	const auto* session = existingSessionOf(iState);
	lua_newtable(iState);
	if (session == nullptr)
		return 1;
	int index = 1;
	for (const auto& player: session->getPlayers()) {
		lua_createtable(iState, 0, 6);
		pushInteger(iState, player.id);
		lua_setfield(iState, -2, "id");
		lua_pushlstring(iState, player.name.data(), player.name.size());
		lua_setfield(iState, -2, "name");
		lua_pushboolean(iState, player.ready ? 1 : 0);
		lua_setfield(iState, -2, "ready");
		pushEnumName(iState, player.inputType);
		lua_setfield(iState, -2, "input");
		lua_pushboolean(iState, player.connected ? 1 : 0);
		lua_setfield(iState, -2, "connected");
		lua_pushboolean(iState, player.profile.meets(session->getRules().minimum) ? 1 : 0);
		lua_setfield(iState, -2, "meets_minimum");
		lua_rawseti(iState, -2, index++);
	}
	return 1;
}

auto luaNetIsEveryoneReady(lua_State* iState) -> int {
	const auto* session = existingSessionOf(iState);
	lua_pushboolean(iState, session != nullptr && session->isEveryoneReady() ? 1 : 0);
	return 1;
}

auto luaNetStartMatch(lua_State* iState) -> int {
	auto* session = existingSessionOf(iState);
	const char* scenePath = luaL_checkstring(iState, 1);
	const auto seed = static_cast<uint64_t>(optInteger(iState, 2, 0));
	if (session == nullptr) {
		lua_pushboolean(iState, 0);
		pushEnumName(iState, network::StartError::NotHost);
		return 2;
	}
	const auto started = session->startMatch(scenePath, seed);
	lua_pushboolean(iState, started.has_value() ? 1 : 0);
	if (started.has_value())
		lua_pushnil(iState);
	else
		pushEnumName(iState, started.error());
	return 2;
}

auto luaNetKick(lua_State* iState) -> int {
	if (auto* session = existingSessionOf(iState); session != nullptr)
		session->kick(static_cast<network::PlayerId>(checkInteger(iState, 1)));
	return 0;
}

template<network::RpcTarget Target, network::Delivery Mode>
auto luaNetRpc(lua_State* iState) -> int {
	constexpr int first = Target == network::RpcTarget::Client ? 2 : 1;
	const network::PlayerId to =
			Target == network::RpcTarget::Client ? static_cast<network::PlayerId>(checkInteger(iState, 1)) : 0;
	const auto target = targetAt(iState, first);
	const char* function = luaL_checkstring(iState, first + 1);
	const auto args = readArgs(iState, first + 2);
	auto* session = sessionOf(iState);
	lua_pushboolean(iState, session != nullptr && target.has_value() &&
											session->sendRpc(Target, to, *target, function, args, Mode)
									? 1
									: 0);
	return 1;
}

auto luaNetGetSender(lua_State* iState) -> int {
	const auto* session = existingSessionOf(iState);
	pushInteger(iState, session != nullptr ? session->getRpcSender() : network::g_HostPlayer);
	return 1;
}

auto luaNetUuid(lua_State* iState) -> int {
	const auto entity = entityAt(iState, 1);
	const auto text = entity.has_value() ? std::to_string(static_cast<uint64_t>(entity->getUUID())) : std::string{"0"};
	lua_pushlstring(iState, text.data(), text.size());
	return 1;
}

auto luaNetSetOwner(lua_State* iState) -> int {
	const auto entity = entityAt(iState, 1);
	const auto player = static_cast<network::PlayerId>(checkInteger(iState, 2));
	auto* session = sessionOf(iState);
	lua_pushboolean(iState, session != nullptr && entity.has_value() && session->setOwner(*entity, player) ? 1 : 0);
	return 1;
}

auto luaNetGetOwner(lua_State* iState) -> int {
	const auto entity = entityAt(iState, 1);
	pushInteger(iState, entity.has_value() && entity->hasComponent<scene::component::Replicated>()
								? entity->getComponent<scene::component::Replicated>().owner
								: network::g_HostPlayer);
	return 1;
}

auto luaNetHasAuthority(lua_State* iState) -> int {
	const auto entity = entityAt(iState, 1);
	const auto* session = existingSessionOf(iState);
	lua_pushboolean(iState, entity.has_value() && (session == nullptr || session->hasAuthority(*entity)) ? 1 : 0);
	return 1;
}

auto luaNetSetViewpoint(lua_State* iState) -> int {
	const auto player = static_cast<network::PlayerId>(checkInteger(iState, 1));
	const math::vec3 position{static_cast<float>(luaL_checknumber(iState, 2)),
							  static_cast<float>(luaL_checknumber(iState, 3)),
							  static_cast<float>(luaL_optnumber(iState, 4, 0.0))};
	if (auto* session = sessionOf(iState); session != nullptr)
		session->setViewpoint(player, position);
	return 0;
}

auto luaNetClearViewpoint(lua_State* iState) -> int {
	if (auto* session = existingSessionOf(iState); session != nullptr)
		session->setViewpoint(static_cast<network::PlayerId>(checkInteger(iState, 1)), std::nullopt);
	return 0;
}

// The state of an action of a player in the lockstep tick playing, none when unknown.
auto actionOf(lua_State* iState) -> std::optional<input::ActionState> {
	const auto player = static_cast<network::PlayerId>(checkInteger(iState, 1));
	const char* name = luaL_checkstring(iState, 2);
	const auto* session = existingSessionOf(iState);
	const auto* boundScene = getBoundScene(iState);
	if (session == nullptr || boundScene == nullptr)
		return std::nullopt;
	const auto* input = session->getPlayerInput(player);
	const auto& map = boundScene->getActions().getMap();
	if (input == nullptr || map == nullptr)
		return std::nullopt;
	const auto index = map->indexOf(name);
	if (!index.has_value() || *index >= input->size())
		return std::nullopt;
	return (*input)[*index];
}

auto luaNetIsHeld(lua_State* iState) -> int {
	const auto action = actionOf(iState);
	lua_pushboolean(iState, action.has_value() && action->held ? 1 : 0);
	return 1;
}

auto luaNetIsPressed(lua_State* iState) -> int {
	const auto action = actionOf(iState);
	lua_pushboolean(iState, action.has_value() && action->pressed ? 1 : 0);
	return 1;
}

auto luaNetGetValue(lua_State* iState) -> int {
	const auto action = actionOf(iState);
	lua_pushnumber(iState, action.has_value() ? static_cast<lua_Number>(action->value) : 0.0);
	return 1;
}

auto luaNetGetVector(lua_State* iState) -> int {
	const auto action = actionOf(iState);
	lua_pushnumber(iState, action.has_value() ? static_cast<lua_Number>(action->vector.x()) : 0.0);
	lua_pushnumber(iState, action.has_value() ? static_cast<lua_Number>(action->vector.y()) : 0.0);
	return 2;
}

auto luaNetGetTick(lua_State* iState) -> int {
	const auto* boundScene = getBoundScene(iState);
	pushInteger(iState,
				boundScene != nullptr ? static_cast<int64_t>(boundScene->getSimulationClock().getTotalTickCount()) : 0);
	return 1;
}

auto luaNetRewindRaycast(lua_State* iState) -> int {
	const auto shooter = static_cast<network::PlayerId>(checkInteger(iState, 1));
	const auto arg = [iState](const int iIndex) -> float {
		return static_cast<float>(luaL_checknumber(iState, iIndex));
	};
	const math::vec3 origin{arg(2), arg(3), arg(4)};
	const math::vec3 direction{arg(5), arg(6), arg(7)};
	const float range = arg(8);
	uint64_t ignored = 0;
	if (lua_gettop(iState) >= 9 && !lua_isnil(iState, 9)) {
		if (const auto entity = entityAt(iState, 9); entity.has_value())
			ignored = static_cast<uint64_t>(entity->getUUID());
	}
	const auto* session = existingSessionOf(iState);
	const auto* boundScene = getBoundScene(iState);
	const auto hit =
			session != nullptr ? session->rewindRaycast(shooter, origin, direction, range, ignored) : std::nullopt;
	if (!hit.has_value() || boundScene == nullptr) {
		lua_pushnil(iState);
		return 1;
	}
	pushInteger(iState, static_cast<int64_t>(getEntityHandle(*boundScene, core::UUID{hit->entity})));
	lua_pushnumber(iState, static_cast<lua_Number>(hit->distance));
	return 2;
}

auto luaNetGetStats(lua_State* iState) -> int {
	const auto* session = existingSessionOf(iState);
	const network::NetStats stats = session != nullptr ? session->getStats() : network::NetStats{};
	lua_createtable(iState, 0, 12);
	const auto field = [iState](const char* iName, const double iValue) -> void {
		lua_pushnumber(iState, iValue);
		lua_setfield(iState, -2, iName);
	};
	pushEnumName(iState, stats.role);
	lua_setfield(iState, -2, "role");
	field("players", stats.players);
	field("ping", stats.roundTripMs);
	field("loss", static_cast<double>(stats.packetLoss));
	field("sent", static_cast<double>(stats.sentPerSecond));
	field("received", static_cast<double>(stats.receivedPerSecond));
	field("entities", stats.replicatedEntities);
	field("tick", static_cast<double>(stats.tick));
	field("stalled", static_cast<double>(stats.stalledFrames));
	field("rpc_sent", static_cast<double>(stats.rpcSent));
	field("rpc_received", static_cast<double>(stats.rpcReceived));
	if (stats.desyncTick.has_value())
		field("desync_tick", static_cast<double>(*stats.desyncTick));
	return 1;
}

}// namespace

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wmissing-designated-field-initializers")
OWL_DIAG_DISABLE_GCC("-Wmissing-field-initializers")
void appendNetworkBindings(std::vector<LuaBinding>& ioBindings) {
	using enum LuaType;
	const LuaValue entityId{.name = "entity_id", .type = Entity};
	const LuaValue player{.name = "player", .type = Integer};
	const LuaValue target{.name = "entity_id", .type = Entity};
	const LuaValue function{.name = "name", .type = String};
	const LuaValue args{.name = "...", .type = Any, .optional = true};
	const LuaValue sent{.name = "sent", .type = Boolean};
	const LuaValue action{.name = "action", .type = String};
	// clang-format off
	const std::vector<LuaBinding> bindings{
		{.table = "net", .name = "host", .function = guarded<luaNetHost>,
		 .description = "Open a lobby as the host on a port (0 or none: one the system chooses); false when the server "
						"does not start.",
		 .params = {{"port", Integer, true}}, .returns = {{"ok", Boolean}}},
		{.table = "net", .name = "join", .function = guarded<luaNetJoin>,
		 .description = "Join the lobby of a host; `on_lobby_changed` follows once joined, `on_disconnected` when "
						"refused or unreachable.",
		 .params = {{"address", String}, {"port", Integer}}, .returns = {{"ok", Boolean}}},
		{.table = "net", .name = "leave", .function = guarded<luaNetLeave>,
		 .description = "Leave the session: the host closes it for every player."},
		{.table = "net", .name = "get_role", .function = guarded<luaNetGetRole>,
		 .description = "What this machine is: `offline`, `host` or `client`.", .returns = {{"role", String}}},
		{.table = "net", .name = "get_state", .function = guarded<luaNetGetState>,
		 .description = "Where the session stands: `idle`, `lobby` or `playing`.", .returns = {{"state", String}}},
		{.table = "net", .name = "is_server", .function = guarded<luaNetIsServer>,
		 .description = "Whether this machine decides the game: the host, or a game played offline.",
		 .returns = {{"server", Boolean}}},
		{.table = "net", .name = "get_local_player", .function = guarded<luaNetGetLocalPlayer>,
		 .description = "Number of the local player (0 for the host and offline).", .returns = {player}},
		{.table = "net", .name = "get_port", .function = guarded<luaNetGetPort>,
		 .description = "Port the host listens on, 0 when not hosting.", .returns = {{"port", Integer}}},
		{.table = "net", .name = "set_player_name", .function = guarded<luaNetSetPlayerName>,
		 .description = "Name of the local player in the lobby.", .params = {{"name", String}}},
		{.table = "net", .name = "set_input_type", .function = guarded<luaNetSetInputType>,
		 .description = "How the local player plays (`keyboard_mouse`, `gamepad`, `touch`): without crossplay, the "
						"host refuses players not playing like it.",
		 .params = {{"type", String}}},
		{.table = "net", .name = "set_ready", .function = guarded<luaNetSetReady>,
		 .description = "Tell the lobby whether the local player is ready.", .params = {{"ready", Boolean}}},
		{.table = "net", .name = "get_players", .function = guarded<luaNetGetPlayers>,
		 .description = "Players of the lobby or match, as `{id, name, ready, input, connected, meets_minimum}`.",
		 .returns = {{"players", Table}}},
		{.table = "net", .name = "is_everyone_ready", .function = guarded<luaNetIsEveryoneReady>,
		 .description = "Whether every player of the lobby is ready.", .returns = {{"ready", Boolean}}},
		{.table = "net", .name = "start_match", .function = guarded<luaNetStartMatch>,
		 .description = "Host: every machine loads the scene and plays it (a `seed` other than 0 replaces the "
						"scene's); else false and why (`not_ready`, `below_minimum`...).",
		 .params = {{"scene", String}, {"seed", Integer, true}}, .returns = {{"ok", Boolean}, {"error", String}}},
		{.table = "net", .name = "kick", .function = guarded<luaNetKick>,
		 .description = "Host: remove a player from the session.", .params = {player}},
		{.table = "net", .name = "rpc_server", .function = guarded<luaNetRpc<network::RpcTarget::Server, network::Delivery::Reliable>>,
		 .description = "Run the function `name` of the entity's script (0: the scene script) on the server, with the "
						"arguments (nil, booleans, numbers, strings).",
		 .params = {target, function, args}, .returns = {sent}},
		{.table = "net", .name = "rpc_client", .function = guarded<luaNetRpc<network::RpcTarget::Client, network::Delivery::Reliable>>,
		 .description = "Run the function `name` of the entity's script on one player's machine.",
		 .params = {player, target, function, args}, .returns = {sent}},
		{.table = "net", .name = "rpc_all", .function = guarded<luaNetRpc<network::RpcTarget::All, network::Delivery::Reliable>>,
		 .description = "Run the function `name` of the entity's script on every machine, this one included.",
		 .params = {target, function, args}, .returns = {sent}},
		{.table = "net", .name = "rpc_server_unreliable", .function = guarded<luaNetRpc<network::RpcTarget::Server, network::Delivery::Unreliable>>,
		 .description = "As `rpc_server`, sent once: a late or lost call is dropped (inputs sent every tick).",
		 .params = {target, function, args}, .returns = {sent}},
		{.table = "net", .name = "rpc_client_unreliable", .function = guarded<luaNetRpc<network::RpcTarget::Client, network::Delivery::Unreliable>>,
		 .description = "As `rpc_client`, sent once.", .params = {player, target, function, args}, .returns = {sent}},
		{.table = "net", .name = "rpc_all_unreliable", .function = guarded<luaNetRpc<network::RpcTarget::All, network::Delivery::Unreliable>>,
		 .description = "As `rpc_all`, sent once.", .params = {target, function, args}, .returns = {sent}},
		{.table = "net", .name = "get_sender", .function = guarded<luaNetGetSender>,
		 .description = "Player who sent the remote call running now (the local player outside one).",
		 .returns = {player}},
		{.table = "net", .name = "uuid", .function = guarded<luaNetUuid>,
		 .description = "UUID of an entity as text, the same on every machine: pass it in a call, "
						"`scene.find_by_uuid` finds the entity.",
		 .params = {entityId}, .returns = {{"uuid", String}}},
		{.table = "net", .name = "set_owner", .function = guarded<luaNetSetOwner>,
		 .description = "Host: give a replicated entity to a player, who predicts it or decides it.",
		 .params = {entityId, player}, .returns = {{"ok", Boolean}}},
		{.table = "net", .name = "get_owner", .function = guarded<luaNetGetOwner>,
		 .description = "Player owning a replicated entity (0 for the host).", .params = {entityId},
		 .returns = {player}},
		{.table = "net", .name = "has_authority", .function = guarded<luaNetHasAuthority>,
		 .description = "Whether this machine moves the entity: offline, an entity it decides or predicts, "
						"any entity without `Replicated`.",
		 .params = {entityId}, .returns = {{"authority", Boolean}}},
		{.table = "net", .name = "set_viewpoint", .function = guarded<luaNetSetViewpoint>,
		 .description = "Host: a point of view of a player for the interest management, besides the entities it owns.",
		 .params = {player, {"x", Number}, {"y", Number}, {"z", Number, true}}},
		{.table = "net", .name = "clear_viewpoint", .function = guarded<luaNetClearViewpoint>,
		 .description = "Host: remove the point of view of a player.", .params = {player}},
		{.table = "net", .name = "is_held", .function = guarded<luaNetIsHeld>,
		 .description = "Lockstep: whether a player holds an action in the tick playing.", .params = {player, action},
		 .returns = {{"held", Boolean}}},
		{.table = "net", .name = "is_pressed", .function = guarded<luaNetIsPressed>,
		 .description = "Lockstep: whether a player pressed an action in the tick playing.", .params = {player, action},
		 .returns = {{"pressed", Boolean}}},
		{.table = "net", .name = "get_value", .function = guarded<luaNetGetValue>,
		 .description = "Lockstep: reading of a player's axis or button action in the tick playing.",
		 .params = {player, action}, .returns = {{"value", Number}}},
		{.table = "net", .name = "get_vector", .function = guarded<luaNetGetVector>,
		 .description = "Lockstep: reading of a player's vector action in the tick playing.", .params = {player, action},
		 .returns = {{"x", Number}, {"y", Number}}},
		{.table = "net", .name = "get_tick", .function = guarded<luaNetGetTick>,
		 .description = "Ticks the scene played since it started (the same on every machine of a lockstep match).",
		 .returns = {{"tick", Integer}}},
		{.table = "net", .name = "rewind_raycast", .function = guarded<luaNetRewindRaycast>,
		 .description = "Host: the nearest hit sphere (`Replicated` hit radius) a shot crosses, every sphere where the "
						"shooting player saw it; the entity and the distance, nil for a miss.",
		 .params = {player, {"ox", Number}, {"oy", Number}, {"oz", Number}, {"dx", Number}, {"dy", Number},
					{"dz", Number}, {"range", Number}, {"ignored", Entity, true}},
		 .returns = {{"entity_id", Entity}, {"distance", Number}}},
		{.table = "net", .name = "get_stats", .function = guarded<luaNetGetStats>,
		 .description = "Figures of the session: `role`, `players`, `ping`, `loss`, `sent` and `received` (bytes per "
						"second), `entities`, `tick`, `stalled`, `rpc_sent`, `rpc_received`, `desync_tick`.",
		 .returns = {{"stats", Table}}},
	};
	// clang-format on
	ioBindings.insert(ioBindings.end(), bindings.begin(), bindings.end());
}
OWL_DIAG_POP

}// namespace owl::script
