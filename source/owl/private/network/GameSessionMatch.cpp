/**
 * @file GameSessionMatch.cpp
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "GameProtocol.h"
#include "network/GameSession.h"
#include "physics/PhysicCommand.h"
#include "scene/Entity.h"
#include "scene/Scene.h"
#include "scene/SceneSerializer.h"
#include "scene/WorldHash.h"
#include "scene/component/Hierarchy.h"
#include "scene/component/PhysicBody.h"
#include "scene/component/Replicated.h"
#include "scene/component/Transform.h"

#include <magic_enum/magic_enum.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iterator>
#include <numbers>
#include <optional>
#include <span>
#include <string>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

namespace owl::network {

namespace {

using protocol::Message;
using scene::component::Replicated;

// Most transforms a client keeps per entity.
constexpr size_t g_MaxSamples = 32;
// Ticks of world hashes the host keeps waiting for the clients' ones, in hash intervals.
constexpr uint64_t g_HashHistory = 8;

void writeKind(ByteWriter& ioWriter, const Message iKind) {
	ioWriter.clear();
	ioWriter.writeU8(static_cast<uint8_t>(iKind));
}

void writeVec3(ByteWriter& ioWriter, const math::vec3& iValue) {
	ioWriter.writeF32(iValue.x());
	ioWriter.writeF32(iValue.y());
	ioWriter.writeF32(iValue.z());
}

auto readVec3(ByteReader& ioReader) -> std::optional<math::vec3> {
	const auto x = ioReader.readF32();
	const auto y = ioReader.readF32();
	const auto z = ioReader.readF32();
	if (!x || !y || !z)
		return std::nullopt;
	return math::vec3{*x, *y, *z};
}

auto uuidOf(const scene::Entity& iEntity) -> uint64_t { return static_cast<uint64_t>(iEntity.getUUID()); }

// The shortest turn from one angle to another, so that an interpolation never spins the long way.
auto lerpAngle(const float iFrom, const float iTo, const float iShare) -> float {
	constexpr float turn = 2.f * std::numbers::pi_v<float>;
	float delta = std::fmod(iTo - iFrom, turn);
	if (delta > std::numbers::pi_v<float>)
		delta -= turn;
	else if (delta < -std::numbers::pi_v<float>)
		delta += turn;
	return iFrom + delta * iShare;
}

void writeEntry(ByteWriter& ioWriter, const scene::Entity& iEntity) {
	const auto& transform = iEntity.getComponent<scene::component::Transform>().transform;
	ioWriter.writeU64(uuidOf(iEntity));
	writeVec3(ioWriter, transform.translation());
	writeVec3(ioWriter, transform.rotation());
	writeVec3(ioWriter, transform.scale());
}

// Whether a client decides the entity itself: it owns it and decides it, or predicts it.
auto isDecidedBy(const Replicated& iReplicated, const PlayerId iPlayer) -> bool {
	return iReplicated.owner == iPlayer &&
		   (iReplicated.authority == Replicated::Authority::Owner || iReplicated.predict);
}

// Every entity under a root, each parent before its children.
void collectSubtree(const scene::Scene& iScene, const scene::Entity& iRoot, std::vector<scene::Entity>& oEntities) {
	oEntities.push_back(iRoot);
	for (size_t index = 0; index < oEntities.size(); ++index) {
		for (const auto child: oEntities[index].getComponent<scene::component::Hierarchy>().childrenIds) {
			if (const auto entity = iScene.findEntityByUUID(child); entity)
				oEntities.push_back(entity);
		}
	}
}

}// namespace

void GameSession::clearMatch() {
	m_currentTick.reset();
	m_playableEnd = 0;
	m_hostHashes.clear();
	m_clientHashes.clear();
	m_interest.clear();
	m_hits.clear();
	m_views.clear();
	m_sceneEntities.clear();
	m_snapshotTimer = 0.f;
	m_tracks.clear();
	m_serverTime = 0.0;
	m_sinceSnapshot = 0.0;
	m_heldBodies.clear();
}

void GameSession::onRuntimeStart(scene::Scene& ioScene) {
	clearMatch();
	ioScene.getSimulationClock().setTickLimit(std::nullopt);
	for (const auto view = ioScene.registry.view<Replicated>(); const auto handle: view)
		m_sceneEntities.insert(uuidOf(scene::Entity{handle, &ioScene}));
	if (m_matchStarting) {
		m_matchStarting = false;
		m_state = MatchState::Playing;
		m_stats.desyncTick.reset();
		m_stats.stalledFrames = 0;
		if (m_matchSeed != 0)
			ioScene.setRandomSeed(m_matchSeed);
		if (m_role == NetRole::Host)
			broadcastRoster();
		queueHook("on_match_start");
		OWL_CORE_INFO("GameSession: Match started ({}).", magic_enum::enum_name(m_rules.model))
	}
	if (m_state == MatchState::Playing && m_rules.model == NetModel::Lockstep) {
		std::vector<PlayerId> players;
		players.reserve(m_players.size() + 1);
		for (const auto& player: m_players) players.push_back(player.id);
		if (players.empty())
			players.push_back(m_localPlayer);
		m_lockstep.reset(players, m_rules.inputDelay);
	}
}

void GameSession::limitTicks(scene::Scene& ioScene) {
	auto& clock = ioScene.getSimulationClock();
	if (m_state != MatchState::Playing || m_rules.model != NetModel::Lockstep) {
		if (clock.getTickLimit().has_value())
			clock.setTickLimit(std::nullopt);
		return;
	}
	m_playableEnd = m_lockstep.getPlayableEnd(m_playableEnd);
	if (clock.getTotalTickCount() >= m_playableEnd)
		++m_stats.stalledFrames;
	clock.setTickLimit(m_playableEnd);
}

void GameSession::onTick(scene::Scene& ioScene, const uint64_t iTick) {
	if (m_state != MatchState::Playing || m_rules.model != NetModel::Lockstep)
		return;
	m_currentTick = iTick;
	if (iTick > 0 && iTick % m_rules.hashInterval == 0) {
		const uint64_t hash = scene::computeWorldHash(ioScene);
		if (m_role == NetRole::Host) {
			m_hostHashes[iTick] = hash;
			compareHashes(iTick);
		} else if (m_role == NetRole::Client) {
			writeKind(m_writer, Message::Hash);
			m_writer.writeU64(iTick);
			m_writer.writeU64(hash);
			std::ignore = mp_client->send(protocol::g_ControlChannel, Delivery::Reliable, m_writer.getBytes());
		}
	}
	const uint64_t target = iTick + m_rules.inputDelay;
	const PlayerInput& input = ioScene.getActions().getStates();
	writeKind(m_writer, Message::Input);
	m_writer.writeU64(target);
	m_writer.writeU32(m_localPlayer);
	writePlayerInput(m_writer, input);
	sendToServerOrAll(protocol::g_LockstepChannel, Delivery::Reliable, m_writer.getBytes());
	m_lockstep.put(target, m_localPlayer, input);
	m_lockstep.discardBefore(iTick);
}

auto GameSession::getPlayerInput(const PlayerId iPlayer) const -> const PlayerInput* {
	if (!m_currentTick.has_value())
		return nullptr;
	return m_lockstep.get(*m_currentTick, iPlayer);
}

void GameSession::compareHashes(const uint64_t iTick) {
	const auto host = m_hostHashes.find(iTick);
	const auto clients = m_clientHashes.find(iTick);
	if (host == m_hostHashes.end() || clients == m_clientHashes.end())
		return;
	for (const auto& [player, hash]: clients->second) {
		if (hash == host->second)
			continue;
		OWL_CORE_WARN("GameSession: Player {} desynchronised at tick {} (world hash {:#018x}, host {:#018x}).", player,
					  iTick, hash, host->second)
		if (!m_stats.desyncTick.has_value()) {
			writeKind(m_writer, Message::Desync);
			m_writer.writeU64(iTick);
			mp_server->broadcast(protocol::g_ControlChannel, Delivery::Reliable, m_writer.getBytes());
		}
		reportDesync(iTick);
		break;
	}
	const uint64_t keep =
			iTick > g_HashHistory * m_rules.hashInterval ? iTick - g_HashHistory * m_rules.hashInterval : 0;
	m_hostHashes.erase(m_hostHashes.begin(), m_hostHashes.lower_bound(keep));
	m_clientHashes.erase(m_clientHashes.begin(), m_clientHashes.lower_bound(keep));
}

void GameSession::reportDesync(const uint64_t iTick) {
	if (m_stats.desyncTick.has_value())
		return;
	m_stats.desyncTick = iTick;
	OWL_CORE_WARN("GameSession: The simulation diverged at tick {}.", iTick)
	queueHook("on_desync", {static_cast<int64_t>(iTick)});
}

auto GameSession::setOwner(const scene::Entity& iEntity, const PlayerId iPlayer) -> bool {
	if (!iEntity || !iEntity.hasComponent<Replicated>()) {
		OWL_CORE_WARN("GameSession: Only a replicated entity has an owner.")
		return false;
	}
	if (m_role == NetRole::Client) {
		OWL_CORE_WARN("GameSession: Only the host gives an entity to a player.")
		return false;
	}
	iEntity.getComponent<Replicated>().owner = iPlayer;
	if (mp_server != nullptr) {
		writeKind(m_writer, Message::Owner);
		m_writer.writeU64(uuidOf(iEntity));
		m_writer.writeU32(iPlayer);
		mp_server->broadcast(protocol::g_ControlChannel, Delivery::Reliable, m_writer.getBytes());
	}
	return true;
}

auto GameSession::hasAuthority(const scene::Entity& iEntity) const -> bool {
	if (m_role == NetRole::Offline || !iEntity || !iEntity.hasComponent<Replicated>())
		return true;
	const auto& replicated = iEntity.getComponent<Replicated>();
	if (m_role == NetRole::Host)
		return replicated.authority == Replicated::Authority::Server || replicated.owner == g_HostPlayer;
	return isDecidedBy(replicated, m_localPlayer);
}

void GameSession::setViewpoint(const PlayerId iPlayer, const std::optional<math::vec3>& iPosition) {
	if (iPosition.has_value())
		m_viewpoints[iPlayer] = *iPosition;
	else
		m_viewpoints.erase(iPlayer);
}

auto GameSession::viewpointsOf(scene::Scene& ioScene, const PlayerId iPlayer) const -> std::vector<math::vec3> {
	std::vector<math::vec3> viewpoints;
	if (const auto found = m_viewpoints.find(iPlayer); found != m_viewpoints.end())
		viewpoints.push_back(found->second);
	for (const auto view = ioScene.registry.view<Replicated>(); const auto handle: view) {
		if (view.get<Replicated>(handle).owner != iPlayer)
			continue;
		viewpoints.push_back(ioScene.getWorldTransform(scene::Entity{handle, &ioScene}).translation());
	}
	return viewpoints;
}

void GameSession::placeEntity(scene::Scene& ioScene, const scene::Entity& iEntity, const TransformSample& iSample) {
	auto& transform = iEntity.getComponent<scene::component::Transform>().transform;
	transform.translation() = iSample.translation;
	transform.rotation() = iSample.rotation;
	transform.scale() = iSample.scale;
	if (!iEntity.hasComponent<scene::component::PhysicBody>() || !physics::PhysicCommand::isInitialized(ioScene))
		return;
	const uint64_t uuid = uuidOf(iEntity);
	if (!m_heldBodies.contains(uuid)) {
		if (iEntity.getComponent<scene::component::PhysicBody>().body.type == scene::SceneBody::BodyType::Dynamic)
			physics::PhysicCommand::setSimulated(iEntity, false);
		m_heldBodies.insert(uuid);
	}
	const auto world = ioScene.getWorldTransform(iEntity);
	physics::PhysicCommand::setTransform3D(iEntity, world.translation(), world.rotation());
}

void GameSession::recordHits(scene::Scene& ioScene) {
	const auto& clock = ioScene.getSimulationClock();
	const double now = static_cast<double>(clock.getTotalTickCount()) * clock.getStepSeconds();
	for (const auto view = ioScene.registry.view<Replicated>(); const auto handle: view) {
		const auto& replicated = view.get<Replicated>(handle);
		if (replicated.hitRadius <= 0.f)
			continue;
		const scene::Entity entity{handle, &ioScene};
		m_hits.record(now, uuidOf(entity), ioScene.getWorldTransform(entity).translation(), replicated.hitRadius);
	}
	m_hits.prune(now - static_cast<double>(m_rules.lagCompensationWindow));
	m_hostTime = now;
}

auto GameSession::rewindRaycast(const PlayerId iShooter, const math::vec3& iOrigin, const math::vec3& iDirection,
								const float iMaxDistance, const uint64_t iIgnored) const -> std::optional<RewindHit> {
	if (m_role == NetRole::Client)
		return std::nullopt;
	double delay = 0.0;
	if (iShooter != g_HostPlayer && mp_server != nullptr) {
		if (const auto stats = mp_server->getStats(iShooter); stats.has_value())
			delay = static_cast<double>(stats->roundTripMs) / 2000.0;
		delay += static_cast<double>(m_rules.interpolationDelay);
	}
	delay = std::min(delay, static_cast<double>(m_rules.lagCompensationWindow));
	return m_hits.raycast(m_hostTime - delay, iOrigin, iDirection, iMaxDistance, iIgnored);
}

void GameSession::sendSnapshots(scene::Scene& ioScene) {
	m_snapshotTimer += m_frameSeconds;
	const float period = 1.f / m_rules.snapshotRate;
	if (m_snapshotTimer < period)
		return;
	m_snapshotTimer = std::fmod(m_snapshotTimer, period);
	std::vector<InterestCandidate> candidates;
	for (const auto view = ioScene.registry.view<Replicated>(); const auto handle: view) {
		const scene::Entity entity{handle, &ioScene};
		if (ioScene.isPendingDestructionInTree(entity))
			continue;
		const auto& replicated = view.get<Replicated>(handle);
		candidates.push_back({.entity = uuidOf(entity),
							  .position = ioScene.getWorldTransform(entity).translation(),
							  .relevance = replicated.relevance,
							  .priority = replicated.priority});
	}
	for (const auto& player: m_players) {
		if (player.id != g_HostPlayer && player.connected)
			sendSnapshot(ioScene, player.id, candidates);
	}
}

void GameSession::sendSnapshot(scene::Scene& ioScene, const PlayerId iPlayer,
							   const std::span<const InterestCandidate> iCandidates) {
	const auto viewpoints = viewpointsOf(ioScene, iPlayer);
	const auto chosen = m_interest.select(iPlayer, iCandidates, viewpoints, m_rules.snapshotBudget);
	const auto relevantList = m_interest.getRelevant(iPlayer);
	const std::unordered_set<uint64_t> relevant{relevantList.begin(), relevantList.end()};
	auto& view = m_views[iPlayer];
	const auto sendReliable = [this, iPlayer]() -> void {
		std::ignore = mp_server->send(iPlayer, protocol::g_ControlChannel, Delivery::Reliable, m_writer.getBytes());
	};
	for (const auto uuid: std::vector<uint64_t>{view.spawned.begin(), view.spawned.end()}) {
		// A child spawned with its replicated root goes with it.
		if (const auto entity = ioScene.findEntityByUUID(core::UUID{uuid});
			entity && (relevant.contains(uuid) || !entity.hasComponent<Replicated>()))
			continue;
		writeKind(m_writer, Message::Despawn);
		m_writer.writeU64(uuid);
		sendReliable();
		view.spawned.erase(uuid);
		view.components.erase(uuid);
	}
	std::vector<scene::Entity> entries;
	for (const auto uuid: chosen) {
		const auto entity = ioScene.findEntityByUUID(core::UUID{uuid});
		if (!entity)
			continue;
		const auto& replicated = entity.getComponent<Replicated>();
		if (!m_sceneEntities.contains(uuid) && !view.spawned.contains(uuid)) {
			std::vector<scene::Entity> subtree;
			collectSubtree(ioScene, entity, subtree);
			for (const auto& member: subtree) {
				writeKind(m_writer, Message::Spawn);
				m_writer.writeU64(uuidOf(member));
				m_writer.writeU64(static_cast<uint64_t>(member.getComponent<scene::component::Hierarchy>().parentId));
				m_writer.writeString(scene::SceneSerializer::serializeEntityToString(member));
				sendReliable();
				view.spawned.insert(uuidOf(member));
			}
		}
		for (const auto& key: replicated.components) {
			const auto state = scene::SceneSerializer::serializeComponentToString(entity, key);
			const uint64_t hash = std::hash<std::string>{}(state);
			auto& sentHash = view.components[uuid][key];
			if (state.empty() || sentHash == hash)
				continue;
			sentHash = hash;
			writeKind(m_writer, Message::ComponentState);
			m_writer.writeU64(uuid);
			m_writer.writeString(key);
			m_writer.writeString(state);
			sendReliable();
		}
		if (replicated.transform &&
			(replicated.authority != Replicated::Authority::Owner || replicated.owner != iPlayer))
			entries.push_back(entity);
	}
	const auto& clock = ioScene.getSimulationClock();
	writeKind(m_writer, Message::Snapshot);
	protocol::writeF64(m_writer, static_cast<double>(clock.getTotalTickCount()) * clock.getStepSeconds());
	m_writer.writeU16(static_cast<uint16_t>(entries.size()));
	for (const auto& entity: entries) writeEntry(m_writer, entity);
	std::ignore = mp_server->send(iPlayer, protocol::g_StateChannel, Delivery::Unreliable, m_writer.getBytes());
}

void GameSession::sendOwnedStates(scene::Scene& ioScene) {
	m_snapshotTimer += m_frameSeconds;
	const float period = 1.f / m_rules.snapshotRate;
	if (m_snapshotTimer < period)
		return;
	m_snapshotTimer = std::fmod(m_snapshotTimer, period);
	std::vector<scene::Entity> owned;
	for (const auto view = ioScene.registry.view<Replicated>(); const auto handle: view) {
		const auto& replicated = view.get<Replicated>(handle);
		if (replicated.transform && replicated.authority == Replicated::Authority::Owner &&
			replicated.owner == m_localPlayer)
			owned.emplace_back(handle, &ioScene);
	}
	if (owned.empty())
		return;
	writeKind(m_writer, Message::OwnerState);
	m_writer.writeU16(static_cast<uint16_t>(owned.size()));
	for (const auto& entity: owned) writeEntry(m_writer, entity);
	std::ignore = mp_client->send(protocol::g_StateChannel, Delivery::Unreliable, m_writer.getBytes());
}

void GameSession::readOwnedStates(scene::Scene& ioScene, const PlayerId iFrom, ByteReader& ioReader) {
	const auto count = ioReader.readU16();
	if (!count.has_value())
		return;
	for (uint16_t index = 0; index < *count; ++index) {
		const auto uuid = ioReader.readU64();
		const auto translation = readVec3(ioReader);
		const auto rotation = readVec3(ioReader);
		const auto scale = readVec3(ioReader);
		if (!uuid || !translation || !rotation || !scale)
			return;
		const auto entity = ioScene.findEntityByUUID(core::UUID{*uuid});
		if (!entity || !entity.hasComponent<Replicated>())
			continue;
		const auto& replicated = entity.getComponent<Replicated>();
		if (replicated.authority != Replicated::Authority::Owner || replicated.owner != iFrom)
			continue;
		placeEntity(ioScene, entity,
					{.time = 0.0, .translation = *translation, .rotation = *rotation, .scale = *scale});
	}
}

void GameSession::readSnapshot(scene::Scene& ioScene, ByteReader& ioReader) {
	const auto time = protocol::readF64(ioReader);
	const auto count = ioReader.readU16();
	if (!time || !count)
		return;
	if (*time > m_serverTime) {
		m_serverTime = *time;
		m_sinceSnapshot = 0.0;
	}
	for (uint16_t index = 0; index < *count; ++index) {
		const auto uuid = ioReader.readU64();
		const auto translation = readVec3(ioReader);
		const auto rotation = readVec3(ioReader);
		const auto scale = readVec3(ioReader);
		if (!uuid || !translation || !rotation || !scale)
			return;
		const auto entity = ioScene.findEntityByUUID(core::UUID{*uuid});
		if (!entity || !entity.hasComponent<Replicated>())
			continue;
		const auto& replicated = entity.getComponent<Replicated>();
		const TransformSample sample{.time = *time,
									 .translation = *translation,
									 .rotation = *rotation,
									 .scale = *scale};
		if (replicated.authority == Replicated::Authority::Owner && replicated.owner == m_localPlayer)
			continue;
		if (replicated.predict && replicated.owner == m_localPlayer) {
			const auto& current = entity.getComponent<scene::component::Transform>().transform.translation();
			if ((current - *translation).norm() > m_rules.correctionThreshold)
				placeEntity(ioScene, entity, sample);
			continue;
		}
		auto& track = m_tracks[*uuid];
		if (!track.empty() && track.back().time >= *time)
			continue;
		track.push_back(sample);
		if (track.size() > g_MaxSamples)
			track.pop_front();
	}
}

void GameSession::applyTracks(scene::Scene& ioScene) {
	m_sinceSnapshot += static_cast<double>(m_frameSeconds);
	const double renderTime = m_serverTime + m_sinceSnapshot - static_cast<double>(m_rules.interpolationDelay);
	for (auto it = m_tracks.begin(); it != m_tracks.end();) {
		const auto entity = ioScene.findEntityByUUID(core::UUID{it->first});
		auto& track = it->second;
		if (!entity || !entity.hasComponent<Replicated>() || track.empty()) {
			it = m_tracks.erase(it);
			continue;
		}
		if (isDecidedBy(entity.getComponent<Replicated>(), m_localPlayer)) {
			it = m_tracks.erase(it);
			continue;
		}
		if (!entity.getComponent<Replicated>().interpolate) {
			placeEntity(ioScene, entity, track.back());
			track.erase(track.begin(), std::prev(track.end()));
			++it;
			continue;
		}
		while (track.size() > 1 && track[1].time <= renderTime) track.pop_front();
		TransformSample shown = track.front();
		if (track.size() > 1 && renderTime > track[0].time) {
			const auto& next = track[1];
			const double span = next.time - track[0].time;
			const auto share = static_cast<float>(span > 0.0 ? (renderTime - track[0].time) / span : 1.0);
			shown.translation = track[0].translation + (next.translation - track[0].translation) * share;
			shown.scale = track[0].scale + (next.scale - track[0].scale) * share;
			shown.rotation = {lerpAngle(track[0].rotation.x(), next.rotation.x(), share),
							  lerpAngle(track[0].rotation.y(), next.rotation.y(), share),
							  lerpAngle(track[0].rotation.z(), next.rotation.z(), share)};
		}
		placeEntity(ioScene, entity, shown);
		++it;
	}
}

void GameSession::readOwner(scene::Scene& ioScene, ByteReader& ioReader) {
	const auto uuid = ioReader.readU64();
	const auto player = ioReader.readU32();
	if (!uuid || !player)
		return;
	if (const auto entity = ioScene.findEntityByUUID(core::UUID{*uuid}); entity && entity.hasComponent<Replicated>())
		entity.getComponent<Replicated>().owner = *player;
}

void GameSession::readSpawn(scene::Scene& ioScene, ByteReader& ioReader) {
	const auto uuid = ioReader.readU64();
	const auto parent = ioReader.readU64();
	const auto yaml = ioReader.readString();
	if (!uuid || !parent || !yaml || ioScene.findEntityByUUID(core::UUID{*uuid}))
		return;
	const auto entity = ioScene.createEntityWithUUID(core::UUID{*uuid});
	if (!scene::SceneSerializer::applyEntityFromString(entity, *yaml)) {
		OWL_CORE_WARN("GameSession: Entity {} spawned by the host does not load.", *uuid)
		ioScene.destroyEntityDeferred(entity);
		return;
	}
	const auto parentEntity = *parent != 0 ? ioScene.findEntityByUUID(core::UUID{*parent}) : scene::Entity{};
	if (parentEntity) {
		// The host sent the local transform: keep it under the new parent.
		const auto local = entity.getComponent<scene::component::Transform>().transform;
		ioScene.setParent(entity, parentEntity);
		entity.getComponent<scene::component::Transform>().transform = local;
	}
	if (!parentEntity || !parentEntity.hasComponent<Replicated>() ||
		m_sceneEntities.contains(static_cast<uint64_t>(parentEntity.getUUID())))
		ioScene.startSpawnedSubtree(entity);
}

void GameSession::readDespawn(scene::Scene& ioScene, ByteReader& ioReader) {
	const auto uuid = ioReader.readU64();
	if (!uuid)
		return;
	m_tracks.erase(*uuid);
	m_heldBodies.erase(*uuid);
	if (const auto entity = ioScene.findEntityByUUID(core::UUID{*uuid}); entity)
		ioScene.destroyEntityDeferred(entity);
}

void GameSession::readComponentState(scene::Scene& ioScene, ByteReader& ioReader) {
	const auto uuid = ioReader.readU64();
	const auto key = ioReader.readString();
	const auto state = ioReader.readString();
	if (!uuid || !key || !state)
		return;
	const auto entity = ioScene.findEntityByUUID(core::UUID{*uuid});
	if (!entity)
		return;
	const auto updated = scene::SceneSerializer::replaceComponentInString(
			scene::SceneSerializer::serializeEntityToString(entity), *key, *state);
	if (updated.empty() || !scene::SceneSerializer::applyEntityFromString(entity, updated))
		OWL_CORE_WARN_ONCE("GameSession: Component {} sent by the host does not apply.", *key)
}

}// namespace owl::network
