/**
 * @file GameSession.cpp
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "GameProtocol.h"
#include "network/GameSession.h"
#include "scene/Entity.h"
#include "scene/Scene.h"
#include "scene/component/LuaScript.h"
#include "scene/component/Replicated.h"
#include "script/ScriptInstance.h"

#include <magic_enum/magic_enum.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

namespace owl::network {

namespace {

using protocol::Message;

// Seconds between two lobby updates of a client (its machine measure goes on).
constexpr float g_HelloPeriod = 2.f;
// Weight of the last frame in the mean frame duration.
constexpr float g_FrameWeight = 0.05f;

void writeKind(ByteWriter& ioWriter, const Message iKind) {
	ioWriter.clear();
	ioWriter.writeU8(static_cast<uint8_t>(iKind));
}

void writePlayer(ByteWriter& ioWriter, const PlayerInfo& iPlayer) {
	ioWriter.writeU32(iPlayer.id);
	ioWriter.writeString(iPlayer.name);
	ioWriter.writeU8(iPlayer.ready ? 1 : 0);
	ioWriter.writeU8(static_cast<uint8_t>(iPlayer.inputType));
	ioWriter.writeU32(iPlayer.profile.cpuThreads);
	ioWriter.writeF32(iPlayer.profile.frameMs);
	ioWriter.writeU8(iPlayer.connected ? 1 : 0);
}

auto readPlayer(ByteReader& ioReader) -> std::optional<PlayerInfo> {
	const auto id = ioReader.readU32();
	auto name = ioReader.readString();
	const auto ready = ioReader.readU8();
	const auto input = ioReader.readU8();
	const auto threads = ioReader.readU32();
	const auto frameMs = ioReader.readF32();
	const auto connected = ioReader.readU8();
	if (!id || !name || !ready || !input || !threads || !frameMs || !connected)
		return std::nullopt;
	const auto inputType = magic_enum::enum_cast<InputType>(*input);
	return PlayerInfo{.id = *id,
					  .name = std::move(*name),
					  .ready = *ready != 0,
					  .inputType = inputType.value_or(InputType::KeyboardMouse),
					  .profile = {.cpuThreads = *threads, .frameMs = *frameMs},
					  .connected = *connected != 0};
}

// Calls a script function when the script defines it.
void callIfDefined(const scene::component::LuaScript& iScript, const std::string& iName,
				   const std::span<const script::CallValue> iArgs) {
	if (iScript.instance && iScript.instance->isValid() && iScript.instance->hasFunction(iName))
		std::ignore = iScript.instance->callFunction(iName, iArgs);
}

auto describeRefusal(const RefuseReason iReason) -> std::string {
	switch (iReason) {
		case RefuseReason::MatchFull:
			return "match_full";
		case RefuseReason::InputType:
			return "input_type";
		case RefuseReason::MatchStarted:
			return "match_started";
	}
	return "refused";
}

auto describeLeave(const LeaveReason iReason) -> std::string {
	switch (iReason) {
		case LeaveReason::Left:
			return "left";
		case LeaveReason::TimedOut:
			return "timed_out";
		case LeaveReason::Kicked:
			return "kicked";
		case LeaveReason::Rejected:
			return "rejected";
		case LeaveReason::Unreachable:
			return "unreachable";
	}
	return "left";
}

}// namespace

GameSession::GameSession(std::string iGameId, const uint32_t iGameVersion, const MatchRules& iRules)
	: m_gameId{std::move(iGameId)}, m_gameVersion{iGameVersion}, m_rules{iRules.clamped()} {
	m_local.name = "Player";
	m_local.profile.cpuThreads = std::thread::hardware_concurrency();
}

GameSession::~GameSession() { leave(); }

void GameSession::setLinkConditions(const LinkConditions& iConditions) {
	m_conditions = iConditions;
	if (mp_conditioned != nullptr)
		mp_conditioned->setConditions(iConditions);
}

void GameSession::setLocalName(const std::string& iName) {
	m_local.name = iName;
	if (m_role == NetRole::Host)
		broadcastRoster();
	else if (m_role == NetRole::Client && m_state != MatchState::Idle)
		m_helloTimer = g_HelloPeriod;
}

void GameSession::setLocalInputType(const InputType iType) {
	m_local.inputType = iType;
	if (m_role == NetRole::Host)
		broadcastRoster();
	else if (m_role == NetRole::Client && m_state != MatchState::Idle)
		m_helloTimer = g_HelloPeriod;
}

auto GameSession::makeTransport() -> expected<uniq<Transport>, NetworkError> {
	auto inner = Transport::create(m_transportType);
	if (!inner.has_value()) {
		OWL_CORE_WARN("GameSession: Transport {} unavailable.", magic_enum::enum_name(m_transportType))
		return unexpected{inner.error()};
	}
	auto conditioned = mkUniq<ConditionedTransport>(std::move(*inner), m_conditions);
	mp_conditioned = conditioned.get();
	return uniq<Transport>{std::move(conditioned)};
}

auto GameSession::makeConfig() const -> SessionConfig {
	return {.gameId = m_gameId,
			.gameVersion = m_gameVersion,
			.maxClients = std::max(1U, m_rules.maxPlayers - 1),
			.handshakeTimeout = 5.f,
			.reconnectGrace = 10.f,
			.reconnectInterval = 1.f};
}

auto GameSession::host(const uint16_t iPort) -> expected<void, NetworkError> {
	leave();
	auto transport = makeTransport();
	if (!transport.has_value())
		return unexpected{transport.error()};
	mp_server = mkUniq<NetServer>(std::move(*transport), makeConfig());
	if (auto started = mp_server->start(iPort); !started.has_value()) {
		OWL_CORE_WARN("GameSession: Server did not start on port {}.", iPort)
		mp_server.reset();
		mp_conditioned = nullptr;
		return unexpected{started.error()};
	}
	m_role = NetRole::Host;
	m_state = MatchState::Lobby;
	m_localPlayer = g_HostPlayer;
	m_local.id = g_HostPlayer;
	m_local.connected = true;
	m_players = {m_local};
	OWL_CORE_INFO("GameSession: Hosting {} on port {}.", m_gameId, getPort())
	queueHook("on_lobby_changed");
	return {};
}

auto GameSession::join(const std::string& iHost, const uint16_t iPort) -> expected<void, NetworkError> {
	leave();
	auto transport = makeTransport();
	if (!transport.has_value())
		return unexpected{transport.error()};
	mp_client = mkUniq<NetClient>(std::move(*transport), makeConfig());
	if (auto connecting = mp_client->connect(iHost, iPort); !connecting.has_value()) {
		OWL_CORE_WARN("GameSession: Cannot join {}:{}.", iHost, iPort)
		mp_client.reset();
		mp_conditioned = nullptr;
		return unexpected{connecting.error()};
	}
	m_role = NetRole::Client;
	m_state = MatchState::Idle;
	OWL_CORE_INFO("GameSession: Joining {}:{}.", iHost, iPort)
	return {};
}

void GameSession::leave() {
	if (mp_server != nullptr)
		mp_server->stop();
	if (mp_client != nullptr)
		mp_client->disconnect();
	if (m_role != NetRole::Offline)
		OWL_CORE_INFO("GameSession: Session closed.")
	mp_server.reset();
	mp_client.reset();
	mp_conditioned = nullptr;
	m_role = NetRole::Offline;
	m_state = MatchState::Idle;
	m_localPlayer = g_HostPlayer;
	m_local.id = g_HostPlayer;
	m_local.ready = false;
	m_players.clear();
	m_matchScene.reset();
	m_loadPending = false;
	m_matchStarting = false;
	m_leavePending = false;
	m_refusal.reset();
	m_stats = {};
	clearMatch();
}

auto GameSession::getPort() const -> uint16_t { return mp_server != nullptr ? mp_server->getPort() : 0; }

auto GameSession::findPlayer(const PlayerId iPlayer) const -> const PlayerInfo* {
	const auto found = std::ranges::find(m_players, iPlayer, &PlayerInfo::id);
	return found == m_players.end() ? nullptr : &*found;
}

void GameSession::setReady(const bool iReady) {
	m_local.ready = iReady;
	if (m_role == NetRole::Host)
		broadcastRoster();
	else if (m_role == NetRole::Client && m_state != MatchState::Idle)
		m_helloTimer = g_HelloPeriod;
}

auto GameSession::isEveryoneReady() const -> bool {
	return !m_players.empty() && std::ranges::all_of(m_players, &PlayerInfo::ready);
}

auto GameSession::startMatch(const std::string& iScene, const uint64_t iSeed) -> expected<void, StartError> {
	if (m_role != NetRole::Host) {
		OWL_CORE_WARN("GameSession: Only the host starts a match.")
		return unexpected{StartError::NotHost};
	}
	if (m_state != MatchState::Lobby) {
		OWL_CORE_WARN("GameSession: The match has started already.")
		return unexpected{StartError::NotInLobby};
	}
	if (iScene.empty()) {
		OWL_CORE_WARN("GameSession: No scene to start the match in.")
		return unexpected{StartError::NoScene};
	}
	if (!isEveryoneReady()) {
		OWL_CORE_WARN("GameSession: A player is not ready.")
		return unexpected{StartError::NotReady};
	}
	for (const auto& player: m_players) {
		if (!player.profile.meets(m_rules.minimum)) {
			OWL_CORE_WARN("GameSession: The machine of {} ({} threads, {:.1f} ms a frame) is below the minimum.",
						  player.name, player.profile.cpuThreads, player.profile.frameMs)
			return unexpected{StartError::BelowMinimum};
		}
	}
	m_matchScene = iScene;
	m_matchSeed = iSeed;
	writeKind(m_writer, Message::Start);
	m_writer.writeString(iScene);
	m_writer.writeU64(iSeed);
	m_writer.writeU8(static_cast<uint8_t>(m_rules.model));
	m_writer.writeU32(m_rules.inputDelay);
	m_writer.writeU32(m_rules.hashInterval);
	mp_server->broadcast(protocol::g_ControlChannel, Delivery::Reliable, m_writer.getBytes());
	m_loadPending = true;
	OWL_CORE_INFO("GameSession: Match starts in {} with {} players.", iScene, m_players.size())
	return {};
}

void GameSession::kick(const PlayerId iPlayer) {
	if (m_role == NetRole::Host && iPlayer != g_HostPlayer)
		m_pendingKicks.push_back(iPlayer);
}

void GameSession::refuse(const PlayerId iPlayer, const RefuseReason iReason) {
	OWL_CORE_INFO("GameSession: Player {} refused ({}).", iPlayer, describeRefusal(iReason))
	writeKind(m_writer, Message::Refused);
	m_writer.writeU8(static_cast<uint8_t>(iReason));
	std::ignore = mp_server->send(iPlayer, protocol::g_ControlChannel, Delivery::Reliable, m_writer.getBytes());
	m_pendingKicks.push_back(iPlayer);
}

void GameSession::sendToServerOrAll(const uint8_t iChannel, const Delivery iDelivery,
									const std::span<const uint8_t> iBytes) {
	if (mp_server != nullptr)
		mp_server->broadcast(iChannel, iDelivery, iBytes);
	else if (mp_client != nullptr)
		std::ignore = mp_client->send(iChannel, iDelivery, iBytes);
}

void GameSession::sendHello() {
	m_helloTimer = 0.f;
	if (mp_client == nullptr)
		return;
	writeKind(m_writer, Message::Hello);
	writePlayer(m_writer, m_local);
	std::ignore = mp_client->send(protocol::g_ControlChannel, Delivery::Reliable, m_writer.getBytes());
}

auto GameSession::upsertPlayer(const PlayerId iPlayer) -> PlayerInfo& {
	if (const auto found = std::ranges::find(m_players, iPlayer, &PlayerInfo::id); found != m_players.end())
		return *found;
	PlayerInfo added;
	added.id = iPlayer;
	added.name = std::format("Player {}", iPlayer);
	m_players.push_back(std::move(added));
	std::ranges::sort(m_players, {}, &PlayerInfo::id);
	return *std::ranges::find(m_players, iPlayer, &PlayerInfo::id);
}

void GameSession::broadcastRoster() {
	if (mp_server == nullptr)
		return;
	if (auto found = std::ranges::find(m_players, g_HostPlayer, &PlayerInfo::id); found != m_players.end())
		*found = m_local;
	writeKind(m_writer, Message::Roster);
	m_writer.writeU8(static_cast<uint8_t>(m_state));
	m_writer.writeU16(static_cast<uint16_t>(m_players.size()));
	for (const auto& player: m_players) writePlayer(m_writer, player);
	mp_server->broadcast(protocol::g_ControlChannel, Delivery::Reliable, m_writer.getBytes());
	queueHook("on_lobby_changed");
}

void GameSession::readRoster(ByteReader& ioReader) {
	const auto state = ioReader.readU8();
	const auto count = ioReader.readU16();
	if (!state || !count)
		return;
	std::vector<PlayerInfo> players;
	for (uint16_t index = 0; index < *count; ++index) {
		auto player = readPlayer(ioReader);
		if (!player.has_value())
			return;
		players.push_back(std::move(*player));
	}
	for (const auto& player: players) {
		if (findPlayer(player.id) == nullptr && player.id != m_localPlayer && !m_players.empty())
			queueHook("on_player_join", {int64_t{player.id}});
	}
	for (const auto& player: m_players) {
		if (std::ranges::find(players, player.id, &PlayerInfo::id) == players.end())
			queueHook("on_player_leave", {int64_t{player.id}});
	}
	m_players = std::move(players);
	if (m_state == MatchState::Idle)
		m_state = magic_enum::enum_cast<MatchState>(*state).value_or(MatchState::Lobby);
	queueHook("on_lobby_changed");
}

void GameSession::queueHook(const std::string& iName, std::vector<script::CallValue> iArgs) {
	m_calls.push_back({.sender = m_localPlayer, .entity = std::nullopt, .function = iName, .args = std::move(iArgs)});
}

void GameSession::dispatchCalls(scene::Scene& ioScene) {
	if (m_calls.empty())
		return;
	// A call may queue others: they run at the next frame.
	const auto calls = std::exchange(m_calls, {});
	for (const auto& call: calls) {
		m_rpcSender = call.sender;
		if (!call.entity.has_value()) {
			callIfDefined(ioScene.getSceneScript(), call.function, call.args);
			callIfDefined(ioScene.getHudScript(), call.function, call.args);
			for (const auto view = ioScene.registry.view<scene::component::LuaScript>(); const auto handle: view) {
				if (const scene::Entity entity{handle, &ioScene}; !ioScene.isPendingDestructionInTree(entity))
					callIfDefined(view.get<scene::component::LuaScript>(handle), call.function, call.args);
			}
		} else if (*call.entity == 0) {
			callIfDefined(ioScene.getSceneScript(), call.function, call.args);
		} else if (const auto entity = ioScene.findEntityByUUID(core::UUID{*call.entity});
				   entity && entity.hasComponent<scene::component::LuaScript>()) {
			callIfDefined(entity.getComponent<scene::component::LuaScript>(), call.function, call.args);
		}
	}
	m_rpcSender.reset();
}

auto GameSession::sendRpc(const RpcTarget iTarget, const PlayerId iTo, const uint64_t iEntity,
						  const std::string& iFunction, const std::span<const script::CallValue> iArgs,
						  const Delivery iDelivery) -> bool {
	if (iArgs.size() > protocol::g_MaxRpcArgs) {
		OWL_CORE_WARN("GameSession: Call of {} with {} arguments, {} at most.", iFunction, iArgs.size(),
					  protocol::g_MaxRpcArgs)
		return false;
	}
	const auto queueLocal = [&]() -> void {
		m_calls.push_back({.sender = m_localPlayer,
						   .entity = iEntity,
						   .function = iFunction,
						   .args = {iArgs.begin(), iArgs.end()}});
	};
	const auto encode = [&]() -> std::span<const uint8_t> {
		writeKind(m_writer, Message::Rpc);
		m_writer.writeU8(static_cast<uint8_t>(iTarget));
		m_writer.writeU32(iTo);
		m_writer.writeU32(m_localPlayer);
		m_writer.writeU64(iEntity);
		m_writer.writeString(iFunction);
		m_writer.writeU8(static_cast<uint8_t>(iDelivery));
		m_writer.writeU8(static_cast<uint8_t>(iArgs.size()));
		for (const auto& arg: iArgs) protocol::writeValue(m_writer, arg);
		return m_writer.getBytes();
	};
	++m_stats.rpcSent;
	const bool toSelf = iTarget == RpcTarget::Client && iTo == m_localPlayer;
	if (m_role == NetRole::Offline || toSelf || (m_role == NetRole::Host && iTarget == RpcTarget::Server)) {
		queueLocal();
		return true;
	}
	if (iTarget == RpcTarget::All)
		queueLocal();
	const auto bytes = encode();
	if (m_role == NetRole::Host) {
		if (iTarget == RpcTarget::All)
			mp_server->broadcast(protocol::g_RpcChannel, iDelivery, bytes);
		else if (!mp_server->send(iTo, protocol::g_RpcChannel, iDelivery, bytes).has_value()) {
			OWL_CORE_WARN("GameSession: No player {} to call {} on.", iTo, iFunction)
			return false;
		}
		return true;
	}
	return mp_client->send(protocol::g_RpcChannel, iDelivery, bytes).has_value();
}

void GameSession::routeRpc(const PlayerId iFrom, ByteReader& ioReader) {
	const auto target = ioReader.readU8();
	const auto to = ioReader.readU32();
	const auto sender = ioReader.readU32();
	const auto entity = ioReader.readU64();
	auto function = ioReader.readString();
	const auto delivery = ioReader.readU8();
	const auto argc = ioReader.readU8();
	if (!target || !to || !sender || !entity || !function || !delivery || !argc || *argc > protocol::g_MaxRpcArgs ||
		*target > static_cast<uint8_t>(RpcTarget::All))
		return;
	std::vector<script::CallValue> args;
	args.reserve(*argc);
	for (uint8_t index = 0; index < *argc; ++index) {
		auto value = protocol::readValue(ioReader);
		if (!value.has_value())
			return;
		args.push_back(std::move(*value));
	}
	++m_stats.rpcReceived;
	const auto rpcTarget = static_cast<RpcTarget>(*target);
	// The host trusts the link a call came on, not the sender it claims.
	const PlayerId caller = m_role == NetRole::Host ? iFrom : *sender;
	const auto mode = *delivery == 0 ? Delivery::Reliable : Delivery::Unreliable;
	if (m_role == NetRole::Host && (rpcTarget == RpcTarget::All || (rpcTarget == RpcTarget::Client && *to != 0))) {
		writeKind(m_writer, Message::Rpc);
		m_writer.writeU8(*target);
		m_writer.writeU32(*to);
		m_writer.writeU32(caller);
		m_writer.writeU64(*entity);
		m_writer.writeString(*function);
		m_writer.writeU8(*delivery);
		m_writer.writeU8(*argc);
		for (const auto& arg: args) protocol::writeValue(m_writer, arg);
		if (rpcTarget == RpcTarget::Client) {
			std::ignore = mp_server->send(*to, protocol::g_RpcChannel, mode, m_writer.getBytes());
			return;
		}
		for (const auto client: mp_server->getClients()) {
			if (client != caller)
				std::ignore = mp_server->send(client, protocol::g_RpcChannel, mode, m_writer.getBytes());
		}
	}
	m_calls.push_back({.sender = caller, .entity = entity, .function = std::move(*function), .args = std::move(args)});
}

void GameSession::observeFrame(const float iFrameSeconds) {
	if (!std::isfinite(iFrameSeconds) || iFrameSeconds <= 0.f)
		return;
	const float frameMs = iFrameSeconds * 1000.f;
	m_meanFrameMs = m_meanFrameMs <= 0.f ? frameMs : m_meanFrameMs + (frameMs - m_meanFrameMs) * g_FrameWeight;
	m_local.profile.frameMs = m_meanFrameMs;
}

void GameSession::updateServer(scene::Scene& ioScene, const core::Timestep& iTimeStep) {
	mp_server->onUpdate(iTimeStep);
	bool rosterChanged = false;
	for (const auto& event: mp_server->getEvents()) {
		switch (event.kind) {
			case ServerEvent::Kind::ClientJoined:
				if (m_state == MatchState::Playing) {
					refuse(event.client, RefuseReason::MatchStarted);
				} else if (m_players.size() >= m_rules.maxPlayers) {
					refuse(event.client, RefuseReason::MatchFull);
				} else {
					upsertPlayer(event.client);
					queueHook("on_player_join", {int64_t{event.client}});
					rosterChanged = true;
				}
				break;
			case ServerEvent::Kind::ClientLost:
			case ServerEvent::Kind::ClientResumed:
				if (const auto found = std::ranges::find(m_players, event.client, &PlayerInfo::id);
					found != m_players.end()) {
					found->connected = event.kind == ServerEvent::Kind::ClientResumed;
					rosterChanged = true;
				}
				break;
			case ServerEvent::Kind::ClientLeft:
				if (std::erase_if(m_players, [&event](const PlayerInfo& iPlayer) -> bool {
						return iPlayer.id == event.client;
					}) > 0) {
					m_lockstep.removePlayer(event.client);
					m_interest.forget(event.client);
					m_views.erase(event.client);
					m_viewpoints.erase(event.client);
					queueHook("on_player_leave", {int64_t{event.client}});
					rosterChanged = true;
				}
				break;
			case ServerEvent::Kind::Message:
				onClientMessage(ioScene, event.client, event.channel, event.payload);
				if (event.channel == protocol::g_ControlChannel && !event.payload.empty() &&
					event.payload.front() == static_cast<uint8_t>(Message::Hello))
					rosterChanged = true;
				break;
		}
	}
	for (const auto player: std::exchange(m_pendingKicks, {})) mp_server->kick(player);
	if (rosterChanged)
		broadcastRoster();
}

void GameSession::onClientMessage(scene::Scene& ioScene, const PlayerId iFrom, const uint8_t iChannel,
								  const std::span<const uint8_t> iBytes) {
	ByteReader reader{iBytes};
	const auto kind = reader.readU8();
	if (!kind.has_value())
		return;
	switch (static_cast<Message>(*kind)) {
		case Message::Hello:
			if (auto player = readPlayer(reader); player.has_value()) {
				if (!m_rules.crossplay && player->inputType != m_local.inputType) {
					refuse(iFrom, RefuseReason::InputType);
					return;
				}
				auto& known = upsertPlayer(iFrom);
				player->id = iFrom;
				player->connected = known.connected;
				known = std::move(*player);
			}
			return;
		case Message::Hash:
			if (const auto tick = reader.readU64(), hash = reader.readU64(); tick && hash) {
				m_clientHashes[*tick][iFrom] = *hash;
				compareHashes(*tick);
			}
			return;
		case Message::Rpc:
			if (iChannel == protocol::g_RpcChannel)
				routeRpc(iFrom, reader);
			return;
		case Message::OwnerState:
			readOwnedStates(ioScene, iFrom, reader);
			return;
		case Message::Input:
			{
				const auto tick = reader.readU64();
				if (const auto player = reader.readU32(); tick && player) {
					auto input = readPlayerInput(reader);
					if (!input.has_value())
						return;
					writeKind(m_writer, Message::Input);
					m_writer.writeU64(*tick);
					m_writer.writeU32(iFrom);
					writePlayerInput(m_writer, *input);
					for (const auto client: mp_server->getClients()) {
						if (client != iFrom)
							std::ignore = mp_server->send(client, protocol::g_LockstepChannel, Delivery::Reliable,
														  m_writer.getBytes());
					}
					m_lockstep.put(*tick, iFrom, *input);
				}
				return;
			}
		case Message::Roster:
		case Message::Refused:
		case Message::Start:
		case Message::Owner:
		case Message::Spawn:
		case Message::Despawn:
		case Message::ComponentState:
		case Message::Desync:
		case Message::Snapshot:
			OWL_CORE_WARN_ONCE("GameSession: Client {} sent a host message.", iFrom)
			return;
	}
}

void GameSession::updateClient(scene::Scene& ioScene, const core::Timestep& iTimeStep) {
	mp_client->onUpdate(iTimeStep);
	for (const auto& event: mp_client->getEvents()) {
		switch (event.kind) {
			case ClientEvent::Kind::Joined:
				m_localPlayer = mp_client->getClientId().value_or(g_HostPlayer);
				m_local.id = m_localPlayer;
				if (m_state == MatchState::Idle)
					m_state = MatchState::Lobby;
				OWL_CORE_INFO("GameSession: Joined as player {}.", m_localPlayer)
				sendHello();
				break;
			case ClientEvent::Kind::Resumed:
				sendHello();
				break;
			case ClientEvent::Kind::Lost:
				break;
			case ClientEvent::Kind::Left:
				OWL_CORE_INFO("GameSession: Left the session ({}).", describeLeave(event.reason))
				queueHook("on_disconnected",
						  {m_refusal.has_value() ? describeRefusal(*m_refusal) : describeLeave(event.reason)});
				m_leavePending = true;
				break;
			case ClientEvent::Kind::Message:
				onServerMessage(ioScene, event.channel, event.payload);
				break;
		}
	}
	if (m_leavePending) {
		// The hook queued above survives the session.
		auto calls = std::exchange(m_calls, {});
		leave();
		m_calls = std::move(calls);
		return;
	}
	m_helloTimer += m_frameSeconds;
	if (m_state == MatchState::Lobby && m_helloTimer >= g_HelloPeriod)
		sendHello();
}

void GameSession::onServerMessage(scene::Scene& ioScene, [[maybe_unused]] const uint8_t iChannel,
								  const std::span<const uint8_t> iBytes) {
	ByteReader reader{iBytes};
	const auto kind = reader.readU8();
	if (!kind.has_value())
		return;
	switch (static_cast<Message>(*kind)) {
		case Message::Roster:
			readRoster(reader);
			return;
		case Message::Refused:
			if (const auto reason = reader.readU8(); reason.has_value())
				m_refusal = magic_enum::enum_cast<RefuseReason>(*reason);
			return;
		case Message::Start:
			if (auto scenePath = reader.readString(); scenePath.has_value()) {
				const auto seed = reader.readU64();
				const auto model = reader.readU8();
				const auto delay = reader.readU32();
				const auto interval = reader.readU32();
				if (!seed || !model || !delay || !interval)
					return;
				m_rules.model = magic_enum::enum_cast<NetModel>(*model).value_or(NetModel::Authoritative);
				m_rules.inputDelay = *delay;
				m_rules.hashInterval = *interval;
				m_rules = m_rules.clamped();
				m_matchScene = std::move(*scenePath);
				m_matchSeed = *seed;
				m_loadPending = true;
			}
			return;
		case Message::Owner:
			readOwner(ioScene, reader);
			return;
		case Message::Spawn:
			readSpawn(ioScene, reader);
			return;
		case Message::Despawn:
			readDespawn(ioScene, reader);
			return;
		case Message::ComponentState:
			readComponentState(ioScene, reader);
			return;
		case Message::Desync:
			if (const auto tick = reader.readU64(); tick.has_value())
				reportDesync(*tick);
			return;
		case Message::Rpc:
			routeRpc(g_HostPlayer, reader);
			return;
		case Message::Snapshot:
			readSnapshot(ioScene, reader);
			return;
		case Message::Input:
			{
				const auto tick = reader.readU64();
				if (const auto player = reader.readU32(); tick && player) {
					if (auto input = readPlayerInput(reader); input.has_value())
						m_lockstep.put(*tick, *player, *input);
				}
				return;
			}
		case Message::Hello:
		case Message::Hash:
		case Message::OwnerState:
			OWL_CORE_WARN_ONCE("GameSession: The host sent a client message.")
			return;
	}
}

void GameSession::beginMatch(scene::Scene& ioScene) {
	m_loadPending = false;
	if (!m_matchScene.has_value())
		return;
	m_matchStarting = true;
	ioScene.teleportRequest.pending = true;
	ioScene.teleportRequest.levelName = *m_matchScene;
	ioScene.teleportRequest.targetName.clear();
	ioScene.teleportRequest.initialVelocity = {0.f, 0.f};
	ioScene.teleportRequest.rotationDelta = 0.f;
}

void GameSession::onFrameBegin(scene::Scene& ioScene, const core::Timestep& iTimeStep) {
	OWL_PROFILE_FUNCTION()

	m_frameSeconds = iTimeStep.getSeconds();
	observeFrame(m_frameSeconds);
	if (m_role == NetRole::Host)
		updateServer(ioScene, iTimeStep);
	else if (m_role == NetRole::Client)
		updateClient(ioScene, iTimeStep);
	if (m_loadPending)
		beginMatch(ioScene);
	if (m_role == NetRole::Client && m_state == MatchState::Playing && m_rules.model == NetModel::Authoritative)
		applyTracks(ioScene);
	limitTicks(ioScene);
	dispatchCalls(ioScene);
}

void GameSession::onFrameEnd(scene::Scene& ioScene) {
	OWL_PROFILE_FUNCTION()

	if (m_state == MatchState::Playing && m_rules.model == NetModel::Authoritative) {
		if (m_role == NetRole::Host) {
			recordHits(ioScene);
			sendSnapshots(ioScene);
		} else if (m_role == NetRole::Client) {
			sendOwnedStates(ioScene);
		}
	}
	updateStats(ioScene, m_frameSeconds);
}

void GameSession::updateStats(scene::Scene& ioScene, const float iSeconds) {
	m_stats.role = m_role;
	m_stats.state = m_state;
	m_stats.localPlayer = m_localPlayer;
	m_stats.players = static_cast<uint32_t>(m_players.size());
	m_stats.tick = ioScene.getSimulationClock().getTotalTickCount();
	m_stats.replicatedEntities = static_cast<uint32_t>(ioScene.registry.storage<scene::component::Replicated>().size());
	uint64_t sent = 0;
	uint64_t received = 0;
	uint64_t roundTrip = 0;
	float loss = 0.f;
	uint32_t peers = 0;
	const auto add = [&](const PeerStats& iStats) -> void {
		sent += iStats.bytesSent;
		received += iStats.bytesReceived;
		roundTrip += iStats.roundTripMs;
		loss += iStats.packetLoss;
		++peers;
	};
	if (mp_server != nullptr) {
		for (const auto client: mp_server->getClients()) {
			if (const auto stats = mp_server->getStats(client); stats.has_value())
				add(*stats);
		}
	} else if (mp_client != nullptr) {
		if (const auto stats = mp_client->getStats(); stats.has_value())
			add(*stats);
	}
	m_stats.roundTripMs = peers == 0 ? 0 : static_cast<uint32_t>(roundTrip / peers);
	m_stats.packetLoss = peers == 0 ? 0.f : loss / static_cast<float>(peers);
	m_rateTimer += iSeconds;
	if (m_rateTimer < 0.5f)
		return;
	m_stats.sentPerSecond = static_cast<float>(sent >= m_lastSent ? sent - m_lastSent : 0) / m_rateTimer;
	m_stats.receivedPerSecond =
			static_cast<float>(received >= m_lastReceived ? received - m_lastReceived : 0) / m_rateTimer;
	m_lastSent = sent;
	m_lastReceived = received;
	m_rateTimer = 0.f;
}

}// namespace owl::network
