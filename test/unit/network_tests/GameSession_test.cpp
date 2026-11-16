/**
 * @file GameSession_test.cpp
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <app/EngineContext.h>
#include <core/Log.h>
#include <network/GameSession.h>
#include <scene/Entity.h>
#include <scene/GameState.h>
#include <scene/Scene.h>
#include <scene/WorldHash.h>
#include <scene/component/components.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

using namespace owl;
using namespace owl::network;

namespace {

// Keeps what the session tells the scripts, and answers the remote calls.
constexpr auto g_Script = R"(
function on_player_join(player)
	gamestate.set('joined', player)
end
function on_player_leave(player)
	gamestate.set('left', player)
end
function on_match_start()
	gamestate.set('started', true)
	net.rpc_server(entity_id, 'hello', net.get_local_player(), 'hi')
end
function hello(player, word)
	gamestate.set('hello_from', net.get_sender())
	gamestate.set('hello_word', word)
end
function add(a, b)
	gamestate.set('sum', a + b)
	gamestate.set('sum_sender', net.get_sender())
end
function on_desync(tick)
	gamestate.set('desync', tick)
end
function on_disconnected(reason)
	gamestate.set('disconnected', reason)
end
)";

constexpr uint64_t g_Scripted = 50;
constexpr uint64_t g_Moving = 70;

auto frame() -> core::Timestep {
	core::Timestep step;
	step.forceUpdate(std::chrono::milliseconds{16});
	return step;
}

/// One machine: its engine context, its session and the scene it plays.
struct Peer {
	app::EngineContext context;
	shared<scene::Scene> scene;

	auto session() -> GameSession& { return context.getNetworkSession(); }

	[[nodiscard]] auto state(const std::string& iKey) const -> std::optional<scene::GameState::Value> {
		return scene->getGameState().get(iKey);
	}
};

class GameSessionTest : public testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_dir = std::filesystem::temp_directory_path() /
				("owl_game_session_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
		std::filesystem::create_directories(m_dir);
		std::ofstream{m_dir / "peer.lua"} << g_Script;
	}

	void TearDown() override {
		m_peers.clear();
		std::error_code error;
		std::filesystem::remove_all(m_dir, error);
		core::Log::invalidate();
	}

	// A scene with a scripted entity and a replicated one, started on the peer.
	void play(Peer& ioPeer, const std::function<void(scene::Scene&)>& iExtra = {}) const {
		ioPeer.scene = mkShared<scene::Scene>();
		ioPeer.scene->setEngineContext(&ioPeer.context);
		auto scripted = ioPeer.scene->createEntityWithUUID(core::UUID{g_Scripted}, "Scripted");
		if (m_scripted)
			scripted.addComponent<scene::component::LuaScript>().scriptPath = (m_dir / "peer.lua").generic_string();
		auto moving = ioPeer.scene->createEntityWithUUID(core::UUID{g_Moving}, "Moving");
		auto& replicated = moving.addComponent<scene::component::Replicated>();
		replicated.interpolate = false;
		replicated.components = {"SpriteRenderer"};
		moving.addComponent<scene::component::SpriteRenderer>();
		if (iExtra)
			iExtra(*ioPeer.scene);
		ioPeer.scene->onStartRuntime();
	}

	auto addPeer(const MatchRules& iRules = {}) -> Peer& {
		auto& peer = *m_peers.emplace_back(mkUniq<Peer>());
		peer.context.setNetworkRules(iRules);
		peer.context.setNetworkGame("test.session", 1);
		peer.session().setTransportType(Transport::Type::Loopback);
		play(peer);
		return peer;
	}

	void pump(const int iFrames = 6) const {
		for (int index = 0; index < iFrames; ++index) {
			for (const auto& peer: m_peers) peer->scene->onUpdateRuntime(frame(), false);
		}
	}

	// A host and a client in its lobby.
	auto lobby(const MatchRules& iRules = {}) -> std::pair<Peer&, Peer&> {
		auto& host = addPeer(iRules);
		auto& client = addPeer(iRules);
		EXPECT_TRUE(host.session().host(0).has_value());
		EXPECT_TRUE(client.session().join("localhost", host.session().getPort()).has_value());
		pump();
		return {host, client};
	}

	// Both ready, the match started, the scene of the match loaded on each machine.
	void startMatch(Peer& ioHost, Peer& ioClient, const std::function<void(scene::Scene&)>& iExtra = {}) const {
		ioHost.session().setReady(true);
		ioClient.session().setReady(true);
		pump();
		ASSERT_TRUE(ioHost.session().startMatch("match.owl").has_value());
		pump(2);
		for (auto* peer: {&ioHost, &ioClient}) {
			ASSERT_TRUE(peer->scene->teleportRequest.pending);
			EXPECT_EQ(peer->scene->teleportRequest.levelName, "match.owl");
			play(*peer, iExtra);
		}
		pump();
	}

	std::filesystem::path m_dir;
	std::vector<uniq<Peer>> m_peers;
	// The script keeps what each machine saw in its game state, which a lockstep world hash compares.
	bool m_scripted = true;
};

}// namespace

TEST_F(GameSessionTest, AClientJoinsTheLobby) {
	OWL_REQUIRE_MODULE(SCRIPT);
	auto [host, client] = lobby();
	host.session().setLocalName("Alice");
	client.session().setLocalName("Bob");
	pump(4);
	EXPECT_EQ(host.session().getRole(), NetRole::Host);
	EXPECT_EQ(client.session().getRole(), NetRole::Client);
	EXPECT_EQ(client.session().getMatchState(), MatchState::Lobby);
	EXPECT_NE(client.session().getLocalPlayer(), g_HostPlayer);
	ASSERT_EQ(host.session().getPlayers().size(), 2U);
	ASSERT_EQ(client.session().getPlayers().size(), 2U);
	const auto* bob = host.session().findPlayer(client.session().getLocalPlayer());
	ASSERT_NE(bob, nullptr);
	EXPECT_EQ(bob->name, "Bob");
	EXPECT_EQ(client.session().findPlayer(g_HostPlayer)->name, "Alice");
	EXPECT_EQ(host.state("joined"), scene::GameState::Value{int64_t{client.session().getLocalPlayer()}});
}

TEST_F(GameSessionTest, TheMatchStartsWhenEveryoneIsReady) {
	OWL_REQUIRE_MODULE(SCRIPT);
	auto [host, client] = lobby();
	EXPECT_EQ(client.session().startMatch("match.owl").error(), StartError::NotHost);
	host.session().setReady(true);
	pump();
	EXPECT_FALSE(host.session().isEveryoneReady());
	EXPECT_EQ(host.session().startMatch("match.owl").error(), StartError::NotReady);
	EXPECT_EQ(host.session().startMatch("").error(), StartError::NoScene);
	startMatch(host, client);
	EXPECT_EQ(host.session().getMatchState(), MatchState::Playing);
	EXPECT_EQ(client.session().getMatchState(), MatchState::Playing);
	EXPECT_EQ(host.state("started"), scene::GameState::Value{true});
	EXPECT_EQ(client.state("started"), scene::GameState::Value{true});
	EXPECT_EQ(host.session().startMatch("match.owl").error(), StartError::NotInLobby);
	pump();
	EXPECT_EQ(host.state("hello_from"), scene::GameState::Value{int64_t{client.session().getLocalPlayer()}});
	EXPECT_EQ(host.state("hello_word"), scene::GameState::Value{std::string{"hi"}});
}

TEST_F(GameSessionTest, AMachineBelowTheMinimumHoldsTheStart) {
	OWL_REQUIRE_MODULE(SCRIPT);
	MatchRules rules;
	rules.minimum.cpuThreads = 100000;
	auto [host, client] = lobby(rules);
	host.session().setReady(true);
	client.session().setReady(true);
	pump();
	EXPECT_EQ(host.session().startMatch("match.owl").error(), StartError::BelowMinimum);
}

TEST_F(GameSessionTest, WithoutCrossplayAnotherInputTypeIsRefused) {
	OWL_REQUIRE_MODULE(SCRIPT);
	MatchRules rules;
	rules.crossplay = false;
	auto& host = addPeer(rules);
	auto& client = addPeer(rules);
	client.session().setLocalInputType(InputType::Gamepad);
	ASSERT_TRUE(host.session().host(0).has_value());
	ASSERT_TRUE(client.session().join("localhost", host.session().getPort()).has_value());
	pump(12);
	EXPECT_EQ(client.session().getRole(), NetRole::Offline);
	EXPECT_EQ(client.state("disconnected"), scene::GameState::Value{std::string{"input_type"}});
	EXPECT_EQ(host.session().getPlayers().size(), 1U);
}

TEST_F(GameSessionTest, AFullMatchRefusesAPlayer) {
	OWL_REQUIRE_MODULE(SCRIPT);
	MatchRules rules;
	rules.maxPlayers = 2;
	auto [host, client] = lobby(rules);
	auto& third = addPeer(rules);
	ASSERT_TRUE(third.session().join("localhost", host.session().getPort()).has_value());
	pump(12);
	EXPECT_EQ(third.session().getRole(), NetRole::Offline);
	EXPECT_EQ(host.session().getPlayers().size(), 2U);
	client.session().leave();
	pump(4);
	EXPECT_EQ(host.session().getPlayers().size(), 1U);
	EXPECT_TRUE(host.state("left").has_value());
}

TEST_F(GameSessionTest, RemoteCallsRunWhereTheyAreSent) {
	OWL_REQUIRE_MODULE(SCRIPT);
	auto [host, client] = lobby();
	const std::vector<script::CallValue> args{int64_t{2}, 3.5};
	EXPECT_TRUE(host.session().sendRpc(RpcTarget::All, 0, g_Scripted, "add", args, Delivery::Reliable));
	pump();
	EXPECT_EQ(host.state("sum"), scene::GameState::Value{5.5f});
	EXPECT_EQ(client.state("sum"), scene::GameState::Value{5.5f});
	EXPECT_EQ(client.state("sum_sender"), scene::GameState::Value{int64_t{0}});
	const std::vector<script::CallValue> more{int64_t{10}, int64_t{1}};
	EXPECT_TRUE(client.session().sendRpc(RpcTarget::Server, 0, g_Scripted, "add", more, Delivery::Unreliable));
	pump();
	EXPECT_EQ(host.state("sum"), scene::GameState::Value{int64_t{11}});
	EXPECT_EQ(host.state("sum_sender"), scene::GameState::Value{int64_t{client.session().getLocalPlayer()}});
	EXPECT_EQ(client.state("sum"), scene::GameState::Value{5.5f});
	EXPECT_GE(host.session().getStats().rpcSent, 1U);
	EXPECT_GE(host.session().getStats().rpcReceived, 1U);
}

TEST_F(GameSessionTest, AnOfflineCallRunsLocally) {
	OWL_REQUIRE_MODULE(SCRIPT);
	auto& alone = addPeer();
	const std::vector<script::CallValue> args{int64_t{1}, int64_t{1}};
	EXPECT_TRUE(alone.session().sendRpc(RpcTarget::Server, 0, g_Scripted, "add", args, Delivery::Reliable));
	pump(1);
	EXPECT_EQ(alone.state("sum"), scene::GameState::Value{int64_t{2}});
	EXPECT_TRUE(alone.session().hasAuthority(alone.scene->findEntityByUUID(core::UUID{g_Moving})));
}

TEST_F(GameSessionTest, TheHostSendsTheEntitiesItDecides) {
	OWL_REQUIRE_MODULE(SCRIPT);
	auto [host, client] = lobby();
	startMatch(host, client);
	const auto hostEntity = host.scene->findEntityByUUID(core::UUID{g_Moving});
	hostEntity.getComponent<scene::component::Transform>().transform.translation() = {3.f, 4.f, 0.f};
	hostEntity.getComponent<scene::component::SpriteRenderer>().color = {1.f, 0.f, 0.f, 1.f};
	EXPECT_TRUE(host.session().hasAuthority(hostEntity));
	EXPECT_FALSE(client.session().hasAuthority(client.scene->findEntityByUUID(core::UUID{g_Moving})));
	pump(12);
	const auto clientEntity = client.scene->findEntityByUUID(core::UUID{g_Moving});
	EXPECT_FLOAT_EQ(clientEntity.getComponent<scene::component::Transform>().transform.translation().x(), 3.f);
	EXPECT_FLOAT_EQ(clientEntity.getComponent<scene::component::SpriteRenderer>().color.y(), 0.f);
	EXPECT_EQ(client.session().getStats().replicatedEntities, 1U);
}

TEST_F(GameSessionTest, AnEntitySpawnedOnTheHostAppearsOnTheClient) {
	OWL_REQUIRE_MODULE(SCRIPT);
	auto [host, client] = lobby();
	startMatch(host, client);
	auto spawned = host.scene->createEntityWithUUID(core::UUID{80}, "Spawned");
	spawned.addComponent<scene::component::Replicated>();
	spawned.getComponent<scene::component::Transform>().transform.translation() = {1.f, 2.f, 3.f};
	auto child = host.scene->createEntityWithUUID(core::UUID{81}, "Child");
	host.scene->setParent(child, spawned);
	pump(12);
	const auto copy = client.scene->findEntityByUUID(core::UUID{80});
	ASSERT_TRUE(copy);
	EXPECT_EQ(copy.getComponent<scene::component::Tag>().tag, "Spawned");
	EXPECT_FLOAT_EQ(copy.getComponent<scene::component::Transform>().transform.translation().z(), 3.f);
	const auto childCopy = client.scene->findEntityByUUID(core::UUID{81});
	ASSERT_TRUE(childCopy);
	EXPECT_EQ(childCopy.getComponent<scene::component::Hierarchy>().parentId, core::UUID{80});
	host.scene->destroyEntityDeferred(spawned);
	pump(12);
	EXPECT_FALSE(client.scene->findEntityByUUID(core::UUID{80}));
}

TEST_F(GameSessionTest, AFarEntityIsNotSent) {
	OWL_REQUIRE_MODULE(SCRIPT);
	auto [host, client] = lobby();
	startMatch(host, client);
	host.session().setViewpoint(client.session().getLocalPlayer(), math::vec3{0.f, 0.f, 0.f});
	auto near = host.scene->createEntityWithUUID(core::UUID{90}, "Near");
	near.addComponent<scene::component::Replicated>().relevance = 10.f;
	auto far = host.scene->createEntityWithUUID(core::UUID{91}, "Far");
	far.addComponent<scene::component::Replicated>().relevance = 10.f;
	far.getComponent<scene::component::Transform>().transform.translation() = {500.f, 0.f, 0.f};
	pump(12);
	EXPECT_TRUE(client.scene->findEntityByUUID(core::UUID{90}));
	EXPECT_FALSE(client.scene->findEntityByUUID(core::UUID{91}));
	far.getComponent<scene::component::Transform>().transform.translation() = {5.f, 0.f, 0.f};
	pump(12);
	EXPECT_TRUE(client.scene->findEntityByUUID(core::UUID{91}));
	host.session().setViewpoint(client.session().getLocalPlayer(), std::nullopt);
}

TEST_F(GameSessionTest, TheOwnerDecidesItsEntity) {
	OWL_REQUIRE_MODULE(SCRIPT);
	auto [host, client] = lobby();
	startMatch(host, client, [](scene::Scene& ioScene) -> void {
		auto owned = ioScene.createEntityWithUUID(core::UUID{95}, "Owned");
		owned.addComponent<scene::component::Replicated>().authority = scene::component::Replicated::Authority::Owner;
	});
	const PlayerId player = client.session().getLocalPlayer();
	EXPECT_TRUE(host.session().setOwner(host.scene->findEntityByUUID(core::UUID{95}), player));
	EXPECT_FALSE(client.session().setOwner(client.scene->findEntityByUUID(core::UUID{95}), player));
	pump(4);
	const auto mine = client.scene->findEntityByUUID(core::UUID{95});
	EXPECT_EQ(mine.getComponent<scene::component::Replicated>().owner, player);
	EXPECT_TRUE(client.session().hasAuthority(mine));
	EXPECT_FALSE(host.session().hasAuthority(host.scene->findEntityByUUID(core::UUID{95})));
	mine.getComponent<scene::component::Transform>().transform.translation() = {-7.f, 0.f, 0.f};
	pump(12);
	EXPECT_FLOAT_EQ(host.scene->findEntityByUUID(core::UUID{95})
							.getComponent<scene::component::Transform>()
							.transform.translation()
							.x(),
					-7.f);
}

TEST_F(GameSessionTest, APredictedEntityIsCorrectedWhenItDrifts) {
	OWL_REQUIRE_MODULE(SCRIPT);
	auto [host, client] = lobby();
	startMatch(host, client, [](scene::Scene& ioScene) -> void {
		auto predicted = ioScene.createEntityWithUUID(core::UUID{96}, "Predicted");
		predicted.addComponent<scene::component::Replicated>().predict = true;
	});
	const PlayerId player = client.session().getLocalPlayer();
	ASSERT_TRUE(host.session().setOwner(host.scene->findEntityByUUID(core::UUID{96}), player));
	pump(4);
	const auto mine = client.scene->findEntityByUUID(core::UUID{96});
	EXPECT_TRUE(client.session().hasAuthority(mine));
	auto& translation = mine.getComponent<scene::component::Transform>().transform.translation();
	translation = {0.1f, 0.f, 0.f};
	pump(12);
	EXPECT_FLOAT_EQ(translation.x(), 0.1f);
	translation = {9.f, 0.f, 0.f};
	pump(12);
	EXPECT_FLOAT_EQ(translation.x(), 0.f);
}

TEST_F(GameSessionTest, TheHostRewindsTheHitSpheres) {
	OWL_REQUIRE_MODULE(SCRIPT);
	auto [host, client] = lobby();
	startMatch(host, client);
	auto target = host.scene->findEntityByUUID(core::UUID{g_Moving});
	target.getComponent<scene::component::Replicated>().hitRadius = 1.f;
	pump(4);
	const auto hit = host.session().rewindRaycast(g_HostPlayer, {0.f, -10.f, 0.f}, {0.f, 1.f, 0.f}, 50.f, 0);
	ASSERT_TRUE(hit.has_value());
	EXPECT_EQ(hit->entity, g_Moving);
	EXPECT_FALSE(client.session().rewindRaycast(g_HostPlayer, {0.f, -10.f, 0.f}, {0.f, 1.f, 0.f}, 50.f, 0));
	EXPECT_EQ(host.session().getHitHistory().getEntityCount(), 1U);
}

TEST_F(GameSessionTest, LockstepPlaysTheSameTicksEverywhere) {
	OWL_REQUIRE_MODULE(SCRIPT);
	MatchRules rules;
	rules.model = NetModel::Lockstep;
	rules.inputDelay = 2;
	rules.hashInterval = 10;
	auto [host, client] = lobby(rules);
	m_scripted = false;
	startMatch(host, client);
	pump(40);
	const uint64_t hostTicks = host.scene->getSimulationClock().getTotalTickCount();
	const uint64_t clientTicks = client.scene->getSimulationClock().getTotalTickCount();
	EXPECT_GT(hostTicks, 20U);
	EXPECT_LE(hostTicks > clientTicks ? hostTicks - clientTicks : clientTicks - hostTicks, rules.inputDelay + 1);
	EXPECT_FALSE(host.session().getStats().desyncTick.has_value());
	EXPECT_NE(host.session().getPlayerInput(client.session().getLocalPlayer()), nullptr);
	// The host alone cannot run past the inputs it has.
	for (int index = 0; index < 30; ++index) host.scene->onUpdateRuntime(frame(), false);
	EXPECT_LE(host.scene->getSimulationClock().getTotalTickCount(), clientTicks + rules.inputDelay + 1);
	EXPECT_GT(host.session().getStats().stalledFrames, 0U);
}

TEST_F(GameSessionTest, ADesyncIsReported) {
	OWL_REQUIRE_MODULE(SCRIPT);
	MatchRules rules;
	rules.model = NetModel::Lockstep;
	rules.inputDelay = 1;
	rules.hashInterval = 5;
	auto [host, client] = lobby(rules);
	startMatch(host, client);
	client.scene->findEntityByUUID(core::UUID{g_Moving})
			.getComponent<scene::component::Transform>()
			.transform.translation() = {42.f, 0.f, 0.f};
	pump(40);
	ASSERT_TRUE(host.session().getStats().desyncTick.has_value());
	EXPECT_TRUE(client.session().getStats().desyncTick.has_value());
	EXPECT_TRUE(host.state("desync").has_value());
}

TEST_F(GameSessionTest, SimulatedConditionsDelayTheMessages) {
	OWL_REQUIRE_MODULE(SCRIPT);
	auto [host, client] = lobby();
	client.session().setLinkConditions({.latencyMs = 50});
	EXPECT_EQ(client.session().getLinkConditions().latencyMs, 50U);
	const auto stats = host.session().getStats();
	EXPECT_EQ(stats.role, NetRole::Host);
	EXPECT_EQ(stats.players, 2U);
	host.session().leave();
	pump(12);
	EXPECT_EQ(client.session().getRole(), NetRole::Offline);
}
