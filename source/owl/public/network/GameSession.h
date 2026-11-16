/**
 * @file GameSession.h
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/expected.h"
#include "math/vectors.h"
#include "network/ConditionedTransport.h"
#include "network/InterestManager.h"
#include "network/LagCompensation.h"
#include "network/LockstepBuffer.h"
#include "network/Match.h"
#include "network/NetClient.h"
#include "network/NetServer.h"
#include "network/Transport.h"
#include "scene/NetworkHook.h"
#include "script/CallValue.h"

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace owl::scene {
class Entity;
}// namespace owl::scene

namespace owl::network {

/// What this machine is in a session.
enum struct NetRole : uint8_t {
	Offline,///< No session.
	Host,///< The server, with a local player.
	Client,///< A client of a server.
};

/// Where the session stands.
enum struct MatchState : uint8_t {
	Idle,///< No session, or a client still connecting.
	Lobby,///< The players gather; the host starts the match.
	Playing,///< The match runs.
};

/// Where a remote procedure call runs.
enum struct RpcTarget : uint8_t {
	Server,///< On the server.
	Client,///< On one player's machine.
	All,///< On every machine of the session, the caller's included.
};

/**
 * @brief
 *  Figures of a session, for the stats overlay and the Network panel.
 */
struct OWL_API NetStats {
	/// What this machine is.
	NetRole role = NetRole::Offline;
	/// Where the session stands.
	MatchState state = MatchState::Idle;
	/// The local player.
	PlayerId localPlayer = g_HostPlayer;
	/// Players of the session.
	uint32_t players = 0;
	/// Round trip to the server, or the mean one to the clients, in milliseconds.
	uint32_t roundTripMs = 0;
	/// Share of the packets lost, between 0 and 1.
	float packetLoss = 0.f;
	/// Bytes sent per second.
	float sentPerSecond = 0.f;
	/// Bytes received per second.
	float receivedPerSecond = 0.f;
	/// Replicated entities of the scene.
	uint32_t replicatedEntities = 0;
	/// Ticks the scene played since the match started.
	uint64_t tick = 0;
	/// Lockstep: frames that waited for an input of another player.
	uint64_t stalledFrames = 0;
	/// Lockstep: the first tick whose world hash differed between two machines.
	std::optional<uint64_t> desyncTick;
	/// Remote procedure calls sent.
	uint64_t rpcSent = 0;
	/// Remote procedure calls received.
	uint64_t rpcReceived = 0;
};

/**
 * @brief
 *  A networked game: the lobby, the remote procedure calls, the lockstep or authoritative match, its replication and
 *  its fairness, over a `NetServer` or a `NetClient`.
 *
 * One session per game (`app::EngineContext::getNetworkSession`), going on across levels; the scenes report to it
 * through `scene::NetworkHook`. Everything runs on the thread of the scene's update.
 */
class OWL_API GameSession final : public scene::NetworkHook {
public:
	/**
	 * @brief
	 *  Create an offline session.
	 * @param[in] iGameId Name of the game: a server refuses clients of another game.
	 * @param[in] iGameVersion Version of the game's messages.
	 * @param[in] iRules Rules of the matches.
	 */
	GameSession(std::string iGameId, uint32_t iGameVersion, const MatchRules& iRules);

	/**
	 * @brief
	 *  Destructor: leaves the session.
	 */
	~GameSession() override;

	/**
	 * @brief
	 *  Copy constructor (deleted).
	 */
	GameSession(const GameSession&) = delete;

	/**
	 * @brief
	 *  Move constructor (deleted).
	 */
	GameSession(GameSession&&) = delete;

	/**
	 * @brief
	 *  Copy assignment (deleted).
	 * @return This session.
	 */
	auto operator=(const GameSession&) -> GameSession& = delete;

	/**
	 * @brief
	 *  Move assignment (deleted).
	 * @return This session.
	 */
	auto operator=(GameSession&&) -> GameSession& = delete;

	/**
	 * @brief
	 *  Rules of the next matches.
	 * @param[in] iRules The rules.
	 */
	void setRules(const MatchRules& iRules) { m_rules = iRules.clamped(); }

	/**
	 * @brief
	 *  Rules of the matches (a client plays by the host's model and input delay).
	 * @return The rules.
	 */
	[[nodiscard]] auto getRules() const -> const MatchRules& { return m_rules; }

	/**
	 * @brief
	 *  Transport of the next `host()` or `join()`: ENet by default, the loopback for a session within one process.
	 * @param[in] iType The transport type.
	 */
	void setTransportType(const Transport::Type iType) { m_transportType = iType; }

	/**
	 * @brief
	 *  Simulated network conditions of the outgoing messages (the Network panel of Owl Nest).
	 * @param[in] iConditions The conditions, the default ones for a perfect link.
	 */
	void setLinkConditions(const LinkConditions& iConditions);

	/**
	 * @brief
	 *  Simulated network conditions of the outgoing messages.
	 * @return The conditions.
	 */
	[[nodiscard]] auto getLinkConditions() const -> const LinkConditions& { return m_conditions; }

	/**
	 * @brief
	 *  Name of the local player, shown in the lobby.
	 * @param[in] iName The name.
	 */
	void setLocalName(const std::string& iName);

	/**
	 * @brief
	 *  How the local player plays, known to the lobby.
	 * @param[in] iType The input type.
	 */
	void setLocalInputType(InputType iType);

	/**
	 * @brief
	 *  Open a lobby as the host.
	 * @param[in] iPort Port to listen on, 0 for one chosen by the system.
	 * @return Nothing, or why the server did not start.
	 */
	auto host(uint16_t iPort) -> expected<void, NetworkError>;

	/**
	 * @brief
	 *  Join the lobby of a host.
	 * @param[in] iHost Address of the host.
	 * @param[in] iPort Port of the host.
	 * @return Nothing, or why the connection did not start.
	 */
	auto join(const std::string& iHost, uint16_t iPort) -> expected<void, NetworkError>;

	/**
	 * @brief
	 *  Leave the session: the host closes it, a client leaves it.
	 */
	void leave();

	/**
	 * @brief
	 *  What this machine is.
	 * @return The role.
	 */
	[[nodiscard]] auto getRole() const -> NetRole { return m_role; }

	/**
	 * @brief
	 *  Where the session stands.
	 * @return The state.
	 */
	[[nodiscard]] auto getMatchState() const -> MatchState { return m_state; }

	/**
	 * @brief
	 *  The local player.
	 * @return The player number (the host's until a client is welcomed).
	 */
	[[nodiscard]] auto getLocalPlayer() const -> PlayerId { return m_localPlayer; }

	/**
	 * @brief
	 *  Port the host listens on.
	 * @return The port, 0 when not hosting.
	 */
	[[nodiscard]] auto getPort() const -> uint16_t;

	/**
	 * @brief
	 *  Players of the lobby or of the match, by number.
	 * @return The players.
	 */
	[[nodiscard]] auto getPlayers() const -> const std::vector<PlayerInfo>& { return m_players; }

	/**
	 * @brief
	 *  A player of the session.
	 * @param[in] iPlayer The player number.
	 * @return The player, nullptr when unknown.
	 */
	[[nodiscard]] auto findPlayer(PlayerId iPlayer) const -> const PlayerInfo*;

	/**
	 * @brief
	 *  Tell the lobby whether the local player is ready.
	 * @param[in] iReady Whether ready.
	 */
	void setReady(bool iReady);

	/**
	 * @brief
	 *  Whether every player of the lobby is ready.
	 * @return True with at least one player, all ready.
	 */
	[[nodiscard]] auto isEveryoneReady() const -> bool;

	/**
	 * @brief
	 *  Start the match (host): every machine loads the scene and plays it.
	 * @param[in] iScene Asset path of the scene of the match.
	 * @param[in] iSeed Seed of the match, 0 to keep the scene's.
	 * @return Nothing, or why the match did not start.
	 */
	auto startMatch(const std::string& iScene, uint64_t iSeed = 0) -> expected<void, StartError>;

	/**
	 * @brief
	 *  Remove a player from the session (host).
	 * @param[in] iPlayer The player.
	 */
	void kick(PlayerId iPlayer);

	/**
	 * @brief
	 *  Call a function of an entity's script on other machines.
	 * @param[in] iTarget Where it runs.
	 * @param[in] iTo The player of `RpcTarget::Client`.
	 * @param[in] iEntity UUID of the entity whose script runs it, 0 for the scene script.
	 * @param[in] iFunction Name of the Lua function.
	 * @param[in] iArgs Its arguments.
	 * @param[in] iDelivery Reliable, or sent once (a late one is dropped).
	 * @return True when the call was sent or queued.
	 */
	auto sendRpc(RpcTarget iTarget, PlayerId iTo, uint64_t iEntity, const std::string& iFunction,
				 std::span<const script::CallValue> iArgs, Delivery iDelivery) -> bool;

	/**
	 * @brief
	 *  Player who sent the remote procedure call running now.
	 * @return The sender, the local player outside a call.
	 */
	[[nodiscard]] auto getRpcSender() const -> PlayerId { return m_rpcSender.value_or(m_localPlayer); }

	/**
	 * @brief
	 *  Give an entity to a player (host): it predicts it, or decides it with `Replicated::Authority::Owner`.
	 * @param[in] iEntity The entity with a `Replicated`.
	 * @param[in] iPlayer The new owner.
	 * @return True when the owner changed.
	 */
	auto setOwner(const scene::Entity& iEntity, PlayerId iPlayer) -> bool;

	/**
	 * @brief
	 *  Whether this machine decides an entity: the server for a server one or one it owns, the owner otherwise.
	 * @param[in] iEntity The entity.
	 * @return True offline, without a `Replicated`, or when this machine decides it.
	 */
	[[nodiscard]] auto hasAuthority(const scene::Entity& iEntity) const -> bool;

	/**
	 * @brief
	 *  Place a player's point of view for the interest management (host), besides the entities it owns.
	 * @param[in] iPlayer The player.
	 * @param[in] iPosition The position, none to remove it.
	 */
	void setViewpoint(PlayerId iPlayer, const std::optional<math::vec3>& iPosition);

	/**
	 * @brief
	 *  Lockstep: the actions of a player in the tick playing now.
	 * @param[in] iPlayer The player.
	 * @return The actions in the order of the action map, nullptr outside a lockstep tick or for an unknown player.
	 */
	[[nodiscard]] auto getPlayerInput(PlayerId iPlayer) const -> const PlayerInput*;

	/**
	 * @brief
	 *  Lag compensation (host): the nearest hit sphere a shot crosses, every sphere where the shooter saw it.
	 * @param[in] iShooter The player who shot: the spheres are rewound by its latency and the interpolation delay.
	 * @param[in] iOrigin Origin of the shot.
	 * @param[in] iDirection Direction of the shot.
	 * @param[in] iMaxDistance Range of the shot.
	 * @param[in] iIgnored UUID of an entity the shot ignores (the shooter's), 0 for none.
	 * @return The hit, none for a miss or off the host.
	 */
	[[nodiscard]] auto rewindRaycast(PlayerId iShooter, const math::vec3& iOrigin, const math::vec3& iDirection,
									 float iMaxDistance, uint64_t iIgnored) const -> std::optional<RewindHit>;

	/**
	 * @brief
	 *  Figures of the session.
	 * @return The figures of the last frame.
	 */
	[[nodiscard]] auto getStats() const -> const NetStats& { return m_stats; }

	/**
	 * @brief
	 *  The hit sphere history of the host (tests, Network panel).
	 * @return The history.
	 */
	[[nodiscard]] auto getHitHistory() const -> const HitHistory& { return m_hits; }

	/**
	 * @brief
	 *  Measure the local machine for the lobby (the runner and the editor call it each frame).
	 * @param[in] iFrameSeconds Duration of the last frame.
	 */
	void observeFrame(float iFrameSeconds);

	void onRuntimeStart(scene::Scene& ioScene) override;

	void onFrameBegin(scene::Scene& ioScene, const core::Timestep& iTimeStep) override;

	void onTick(scene::Scene& ioScene, uint64_t iTick) override;

	void onFrameEnd(scene::Scene& ioScene) override;

private:
	/// A script function to call at the next frame.
	struct ScriptCall {
		/// Player who asked for it.
		PlayerId sender = g_HostPlayer;
		/// UUID of the entity whose script runs it, 0 for the scene script; none for every script.
		std::optional<uint64_t> entity;
		/// Name of the function.
		std::string function;
		/// Its arguments.
		std::vector<script::CallValue> args;
	};
	/// A received transform, at a time of the server.
	struct TransformSample {
		/// Time of the server, in seconds.
		double time = 0.0;
		/// Translation.
		math::vec3 translation{0.f, 0.f, 0.f};
		/// Rotation (Euler angles, radians).
		math::vec3 rotation{0.f, 0.f, 0.f};
		/// Scale.
		math::vec3 scale{1.f, 1.f, 1.f};
	};
	/// What the host sent a client.
	struct ClientView {
		/// Entities spawned at run time the client has.
		std::unordered_set<uint64_t> spawned;
		/// Hash of the last state sent, per entity and component key.
		std::unordered_map<uint64_t, std::unordered_map<std::string, uint64_t>> components;
	};

	/**
	 * @brief
	 *  Make the transport of a new server or client, wrapped in the simulated conditions.
	 * @return The transport, or why the backend is missing.
	 */
	[[nodiscard]] auto makeTransport() -> expected<uniq<Transport>, NetworkError>;
	/**
	 * @brief
	 *  Session configuration for the server and the client.
	 * @return The configuration.
	 */
	[[nodiscard]] auto makeConfig() const -> SessionConfig;
	/**
	 * @brief
	 *  Send a message to the server, or to every client from the host.
	 */
	void sendToServerOrAll(uint8_t iChannel, Delivery iDelivery, std::span<const uint8_t> iBytes);
	/**
	 * @brief
	 *  Send the lobby to every client and tell the local scripts.
	 */
	void broadcastRoster();
	/**
	 * @brief
	 *  Read a lobby sent by the host.
	 */
	void readRoster(ByteReader& ioReader);
	/**
	 * @brief
	 *  Add or update a player of the host's lobby.
	 * @return The player.
	 */
	auto upsertPlayer(PlayerId iPlayer) -> PlayerInfo&;
	/**
	 * @brief
	 *  Queue a script function for every script.
	 */
	void queueHook(const std::string& iName, std::vector<script::CallValue> iArgs = {});
	/**
	 * @brief
	 *  Run the queued script calls on a scene.
	 */
	void dispatchCalls(scene::Scene& ioScene);
	/**
	 * @brief
	 *  Host: handle the events of the server.
	 */
	void updateServer(scene::Scene& ioScene, const core::Timestep& iTimeStep);
	/**
	 * @brief
	 *  Client: handle the events of the client.
	 */
	void updateClient(scene::Scene& ioScene, const core::Timestep& iTimeStep);
	/**
	 * @brief
	 *  Host: handle a message from a client.
	 */
	void onClientMessage(scene::Scene& ioScene, PlayerId iFrom, uint8_t iChannel, std::span<const uint8_t> iBytes);
	/**
	 * @brief
	 *  Client: handle a message from the host.
	 */
	void onServerMessage(scene::Scene& ioScene, uint8_t iChannel, std::span<const uint8_t> iBytes);
	/**
	 * @brief
	 *  Route a remote procedure call read on a machine.
	 */
	void routeRpc(PlayerId iFrom, ByteReader& ioReader);
	/**
	 * @brief
	 *  Host: tell a player why it is refused, then kick it.
	 */
	void refuse(PlayerId iPlayer, RefuseReason iReason);
	/**
	 * @brief
	 *  Client: send the local player to the lobby.
	 */
	void sendHello();
	/**
	 * @brief
	 *  Host: read the entities a client decides.
	 */
	void readOwnedStates(scene::Scene& ioScene, PlayerId iFrom, ByteReader& ioReader);
	/**
	 * @brief
	 *  Client: read the new owner of an entity.
	 */
	void readOwner(scene::Scene& ioScene, ByteReader& ioReader);
	/**
	 * @brief
	 *  Client: create an entity the host spawned.
	 */
	void readSpawn(scene::Scene& ioScene, ByteReader& ioReader);
	/**
	 * @brief
	 *  Client: remove an entity the host despawned.
	 */
	void readDespawn(scene::Scene& ioScene, ByteReader& ioReader);
	/**
	 * @brief
	 *  Client: apply a replicated component.
	 */
	void readComponentState(scene::Scene& ioScene, ByteReader& ioReader);
	/**
	 * @brief
	 *  Write an entity's transform and move its physics body along.
	 */
	void placeEntity(scene::Scene& ioScene, const scene::Entity& iEntity, const TransformSample& iSample);
	/**
	 * @brief
	 *  Start the match on this machine: load its scene.
	 */
	void beginMatch(scene::Scene& ioScene);
	/**
	 * @brief
	 *  Host: compare the world hashes of a tick.
	 */
	void compareHashes(uint64_t iTick);
	/**
	 * @brief
	 *  Mark a desync and tell the scripts.
	 */
	void reportDesync(uint64_t iTick);
	/**
	 * @brief
	 *  Lockstep: hold the clock at the last tick every player's input is known for.
	 */
	void limitTicks(scene::Scene& ioScene);
	/**
	 * @brief
	 *  Host: send the snapshots that are due.
	 */
	void sendSnapshots(scene::Scene& ioScene);
	/**
	 * @brief
	 *  Host: send one client its snapshot, spawns and component states.
	 */
	void sendSnapshot(scene::Scene& ioScene, PlayerId iPlayer, std::span<const InterestCandidate> iCandidates);
	/**
	 * @brief
	 *  Client: send the entities it decides.
	 */
	void sendOwnedStates(scene::Scene& ioScene);
	/**
	 * @brief
	 *  Client: read a snapshot.
	 */
	void readSnapshot(scene::Scene& ioScene, ByteReader& ioReader);
	/**
	 * @brief
	 *  Client: show the received entities at the interpolated time.
	 */
	void applyTracks(scene::Scene& ioScene);
	/**
	 * @brief
	 *  Host: record the hit spheres of the frame.
	 */
	void recordHits(scene::Scene& ioScene);
	/**
	 * @brief
	 *  Host: viewpoints of a client (its owned entities and its explicit viewpoint).
	 * @return The world positions.
	 */
	[[nodiscard]] auto viewpointsOf(scene::Scene& ioScene, PlayerId iPlayer) const -> std::vector<math::vec3>;
	/**
	 * @brief
	 *  Update the figures.
	 */
	void updateStats(scene::Scene& ioScene, float iSeconds);
	/**
	 * @brief
	 *  Forget the state of a match.
	 */
	void clearMatch();

	/// Name of the game.
	std::string m_gameId;
	/// Version of the game's messages.
	uint32_t m_gameVersion;
	/// Rules of the matches.
	MatchRules m_rules;
	/// Transport of the next server or client.
	Transport::Type m_transportType = Transport::Type::Enet;
	/// Simulated conditions of the outgoing messages.
	LinkConditions m_conditions;
	/// The conditioned transport of the server or client, owned by it.
	ConditionedTransport* mp_conditioned = nullptr;
	/// The server while hosting.
	uniq<NetServer> mp_server;
	/// The client while joined.
	uniq<NetClient> mp_client;
	/// What this machine is.
	NetRole m_role = NetRole::Offline;
	/// Where the session stands.
	MatchState m_state = MatchState::Idle;
	/// The local player.
	PlayerId m_localPlayer = g_HostPlayer;
	/// The local player as the lobby shows it.
	PlayerInfo m_local;
	/// Players of the session, by number.
	std::vector<PlayerInfo> m_players;
	/// Scene the match plays, waiting for it to start on this machine.
	std::optional<std::string> m_matchScene;
	/// Seed of the match, 0 to keep the scene's.
	uint64_t m_matchSeed = 0;
	/// Whether the scene of the match must be loaded at the next frame.
	bool m_loadPending = false;
	/// Whether the next scene to start is the match's.
	bool m_matchStarting = false;
	/// Client: the session ended in this update, left after its events.
	bool m_leavePending = false;
	/// Client: why the host refused it, before the kick.
	std::optional<RefuseReason> m_refusal;
	/// Client: time since the local player was last sent to the lobby, in seconds.
	float m_helloTimer = 0.f;
	/// Host: players to kick after the events of the update.
	std::vector<PlayerId> m_pendingKicks;
	/// Script calls of the next frame.
	std::vector<ScriptCall> m_calls;
	/// Sender of the remote procedure call running now.
	std::optional<PlayerId> m_rpcSender;
	/// Lockstep inputs.
	LockstepBuffer m_lockstep;
	/// Lockstep: tick playing now, none outside a tick.
	std::optional<uint64_t> m_currentTick;
	/// Lockstep: first tick whose input is not known yet.
	uint64_t m_playableEnd = 0;
	/// Lockstep: the host's world hash of each tick waiting for the clients' ones.
	std::map<uint64_t, uint64_t> m_hostHashes;
	/// Lockstep: the clients' world hashes of each tick.
	std::map<uint64_t, std::map<PlayerId, uint64_t>> m_clientHashes;
	/// Interest management of the host.
	InterestManager m_interest;
	/// Hit spheres of the host.
	HitHistory m_hits;
	/// Host: time of the server at the last recorded hit spheres, in seconds.
	double m_hostTime = 0.0;
	/// What the host sent each client.
	std::unordered_map<PlayerId, ClientView> m_views;
	/// Viewpoints placed by the scripts of the host.
	std::unordered_map<PlayerId, math::vec3> m_viewpoints;
	/// Replicated entities present when the scene started (on every machine).
	std::unordered_set<uint64_t> m_sceneEntities;
	/// Time since the last snapshot, in seconds.
	float m_snapshotTimer = 0.f;
	/// Client: received transforms per entity, oldest first.
	std::unordered_map<uint64_t, std::deque<TransformSample>> m_tracks;
	/// Client: newest time of the server received.
	double m_serverTime = 0.0;
	/// Client: seconds since the newest snapshot arrived.
	double m_sinceSnapshot = 0.0;
	/// Client: entities whose physics body is held out of the local simulation.
	std::unordered_set<uint64_t> m_heldBodies;
	/// Duration of the frame being updated, in seconds.
	float m_frameSeconds = 0.f;
	/// Mean frame duration of the local machine, in milliseconds.
	float m_meanFrameMs = 0.f;
	/// Byte counts at the last figures update.
	uint64_t m_lastSent = 0;
	/// Byte counts at the last figures update.
	uint64_t m_lastReceived = 0;
	/// Time since the rates were measured, in seconds.
	float m_rateTimer = 0.f;
	/// Figures of the last frame.
	NetStats m_stats;
	/// Reused message buffer.
	ByteWriter m_writer;
};

}// namespace owl::network
