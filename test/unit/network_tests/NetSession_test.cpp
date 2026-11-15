/**
 * @file NetSession_test.cpp
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <network/LoopbackTransport.h>
#include <network/NetClient.h>
#include <network/NetServer.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <vector>

using namespace owl;
using namespace owl::network;

namespace {

auto config() -> SessionConfig {
	return {.gameId = "test.game",
			.gameVersion = 3,
			.maxClients = 2,
			.handshakeTimeout = 1.f,
			.reconnectGrace = 2.f,
			.reconnectInterval = 0.5f};
}

auto step(const std::chrono::milliseconds iDelta) -> core::Timestep {
	core::Timestep timestep;
	timestep.forceUpdate(iDelta);
	return timestep;
}

/// Server, clients and the events each saw over the updates.
struct Session {
	NetServer server{mkUniq<LoopbackTransport>(), config()};
	std::vector<ServerEvent> serverLog;
	std::vector<std::vector<ClientEvent>> clientLogs;
	std::vector<uniq<NetClient>> clients;

	Session() { EXPECT_TRUE(server.start(0)); }

	auto addClient(SessionConfig iConfig = config()) -> NetClient& {
		clients.push_back(mkUniq<NetClient>(mkUniq<LoopbackTransport>(), std::move(iConfig)));
		clientLogs.emplace_back();
		EXPECT_TRUE(clients.back()->connect("localhost", server.getPort()));
		return *clients.back();
	}

	void pump(const int iSteps = 4, const std::chrono::milliseconds iDelta = std::chrono::milliseconds{10}) {
		const auto timestep = step(iDelta);
		for (int i = 0; i < iSteps; ++i) {
			for (size_t c = 0; c < clients.size(); ++c) {
				clients[c]->onUpdate(timestep);
				for (const auto& event: clients[c]->getEvents()) clientLogs[c].push_back(event);
			}
			server.onUpdate(timestep);
			for (const auto& event: server.getEvents()) serverLog.push_back(event);
		}
	}

	[[nodiscard]] auto serverSaw(const ServerEvent::Kind iKind) const -> size_t {
		return static_cast<size_t>(std::ranges::count_if(
				serverLog, [iKind](const ServerEvent& iEvent) -> bool { return iEvent.kind == iKind; }));
	}

	[[nodiscard]] auto clientSaw(const size_t iClient, const ClientEvent::Kind iKind) const -> size_t {
		return static_cast<size_t>(std::ranges::count_if(
				clientLogs[iClient], [iKind](const ClientEvent& iEvent) -> bool { return iEvent.kind == iKind; }));
	}

	[[nodiscard]] auto lastLeft(const size_t iClient) const -> const ClientEvent* {
		for (const auto& event: clientLogs[iClient] | std::views::reverse)
			if (event.kind == ClientEvent::Kind::Left)
				return &event;
		return nullptr;
	}
};

}// namespace

TEST(NetSession, JoinAndExchangeMessages) {
	Session session;
	NetClient& client = session.addClient();
	EXPECT_EQ(client.getState(), NetClient::State::Connecting);
	session.pump();
	ASSERT_EQ(client.getState(), NetClient::State::Connected);
	ASSERT_TRUE(client.getClientId());
	EXPECT_EQ(session.serverSaw(ServerEvent::Kind::ClientJoined), 1u);
	EXPECT_EQ(session.clientSaw(0, ClientEvent::Kind::Joined), 1u);
	const ClientId id = *client.getClientId();
	EXPECT_EQ(session.server.getClients(), std::vector<ClientId>{id});
	EXPECT_TRUE(session.server.isConnected(id));
	EXPECT_TRUE(session.server.getStats(id));
	EXPECT_TRUE(client.getStats());

	const std::array<uint8_t, 2> ping{1, 2};
	ASSERT_TRUE(client.send(1, Delivery::Reliable, ping));
	const std::array<uint8_t, 1> pong{9};
	ASSERT_TRUE(session.server.send(id, 3, Delivery::Unreliable, pong));
	session.pump(1);
	const auto message = std::ranges::find_if(session.serverLog, [](const ServerEvent& iEvent) -> bool {
		return iEvent.kind == ServerEvent::Kind::Message;
	});
	ASSERT_NE(message, session.serverLog.end());
	EXPECT_EQ(message->client, id);
	EXPECT_EQ(message->channel, 1);
	EXPECT_EQ(message->payload, (std::vector<uint8_t>{1, 2}));
	const auto reply = std::ranges::find_if(session.clientLogs[0], [](const ClientEvent& iEvent) -> bool {
		return iEvent.kind == ClientEvent::Kind::Message;
	});
	ASSERT_NE(reply, session.clientLogs[0].end());
	EXPECT_EQ(reply->channel, 3);
	EXPECT_EQ(reply->payload, (std::vector<uint8_t>{9}));
}

TEST(NetSession, BroadcastReachesEveryClient) {
	Session session;
	session.addClient();
	session.addClient();
	session.pump();
	const std::array<uint8_t, 1> tick{5};
	session.server.broadcast(0, Delivery::Reliable, tick);
	session.pump(1);
	EXPECT_EQ(session.clientSaw(0, ClientEvent::Kind::Message), 1u);
	EXPECT_EQ(session.clientSaw(1, ClientEvent::Kind::Message), 1u);
	EXPECT_NE(session.clients[0]->getClientId(), session.clients[1]->getClientId());
}

TEST(NetSession, RejectsAnotherGameOrVersion) {
	Session session;
	auto other = config();
	other.gameId = "other.game";
	session.addClient(other);
	auto older = config();
	older.gameVersion = 2;
	session.addClient(older);
	session.pump();
	for (size_t c = 0; c < 2; ++c) {
		const auto* left = session.lastLeft(c);
		ASSERT_NE(left, nullptr);
		EXPECT_EQ(left->reason, LeaveReason::Rejected);
		EXPECT_EQ(left->rejectReason, RejectReason::WrongGame);
		EXPECT_EQ(session.clients[c]->getState(), NetClient::State::Disconnected);
	}
	EXPECT_EQ(session.serverSaw(ServerEvent::Kind::ClientJoined), 0u);
}

TEST(NetSession, RejectsWhenFull) {
	Session session;
	session.addClient();
	session.addClient();
	session.pump();
	session.addClient();
	session.pump();
	const auto* left = session.lastLeft(2);
	ASSERT_NE(left, nullptr);
	EXPECT_EQ(left->rejectReason, RejectReason::ServerFull);
	EXPECT_EQ(session.server.getClients().size(), 2u);
}

TEST(NetSession, ReconnectsWithinGraceAndKeepsItsId) {
	Session session;
	NetClient& client = session.addClient();
	session.pump();
	const auto id = client.getClientId();
	ASSERT_TRUE(id);

	auto& transport = dynamic_cast<LoopbackTransport&>(client.getTransport());
	transport.severLink(transport.getPeers().front());
	session.pump(1);
	EXPECT_EQ(client.getState(), NetClient::State::Reconnecting);
	EXPECT_EQ(session.clientSaw(0, ClientEvent::Kind::Lost), 1u);
	EXPECT_EQ(session.serverSaw(ServerEvent::Kind::ClientLost), 1u);
	EXPECT_FALSE(session.server.isConnected(*id));
	EXPECT_EQ(client.send(0, Delivery::Reliable, std::array<uint8_t, 1>{0}).error(), NetworkError::NotOpen);

	session.pump(10, std::chrono::milliseconds{100});
	EXPECT_EQ(client.getState(), NetClient::State::Connected);
	EXPECT_EQ(client.getClientId(), id);
	EXPECT_EQ(session.clientSaw(0, ClientEvent::Kind::Resumed), 1u);
	EXPECT_EQ(session.serverSaw(ServerEvent::Kind::ClientResumed), 1u);
	EXPECT_EQ(session.serverSaw(ServerEvent::Kind::ClientLeft), 0u);
	EXPECT_TRUE(session.server.isConnected(*id));
}

TEST(NetSession, LostClientTimesOutAfterGrace) {
	Session session;
	NetClient& client = session.addClient();
	session.pump();
	// The server goes away for good: the client retries, then gives up after the grace time.
	session.server.stop();
	auto& transport = dynamic_cast<LoopbackTransport&>(client.getTransport());
	EXPECT_TRUE(transport.getPeers().empty());
	session.pump(1);
	const auto* left = session.lastLeft(0);
	ASSERT_NE(left, nullptr);
	EXPECT_EQ(left->reason, LeaveReason::Left);
}

TEST(NetSession, ServerDropsALostClientAfterGrace) {
	Session session;
	NetClient& client = session.addClient();
	session.pump();
	const auto id = client.getClientId();
	auto& transport = dynamic_cast<LoopbackTransport&>(client.getTransport());
	transport.severLink(transport.getPeers().front());
	// The client stops trying at once: only the server's grace timer runs.
	client.disconnect();
	session.pump(30, std::chrono::milliseconds{100});
	const auto left = std::ranges::find_if(session.serverLog, [](const ServerEvent& iEvent) -> bool {
		return iEvent.kind == ServerEvent::Kind::ClientLeft;
	});
	ASSERT_NE(left, session.serverLog.end());
	EXPECT_EQ(left->client, *id);
	EXPECT_EQ(left->reason, LeaveReason::TimedOut);
}

TEST(NetSession, ClientGivesUpReconnectingAfterGrace) {
	Session session;
	NetClient& client = session.addClient();
	session.pump();
	auto& transport = dynamic_cast<LoopbackTransport&>(client.getTransport());
	transport.severLink(transport.getPeers().front());
	session.server.stop();
	session.pump(30, std::chrono::milliseconds{100});
	const auto* left = session.lastLeft(0);
	ASSERT_NE(left, nullptr);
	EXPECT_EQ(left->reason, LeaveReason::TimedOut);
	EXPECT_EQ(client.getState(), NetClient::State::Disconnected);
}

TEST(NetSession, KickAndLeave) {
	Session session;
	const NetClient& first = session.addClient();
	NetClient& second = session.addClient();
	session.pump();
	session.server.kick(*first.getClientId());
	second.disconnect();
	session.pump();
	const auto* kicked = session.lastLeft(0);
	ASSERT_NE(kicked, nullptr);
	EXPECT_EQ(kicked->reason, LeaveReason::Kicked);
	EXPECT_EQ(second.getState(), NetClient::State::Disconnected);
	EXPECT_EQ(session.serverSaw(ServerEvent::Kind::ClientLeft), 2u);
	EXPECT_TRUE(session.server.getClients().empty());
}

TEST(NetSession, SilentPeerIsClosedAfterHandshakeTimeout) {
	Session session;
	LoopbackTransport raw;
	ASSERT_TRUE(raw.connect("", session.server.getPort()));
	session.pump(1);
	EXPECT_EQ(session.server.getTransport().getPeers().size(), 1u);
	session.pump(12, std::chrono::milliseconds{100});
	EXPECT_TRUE(session.server.getTransport().getPeers().empty());
	EXPECT_EQ(session.serverSaw(ServerEvent::Kind::ClientJoined), 0u);
}

TEST(NetSession, GarbageGreetingIsRejected) {
	Session session;
	LoopbackTransport raw;
	const auto peer = raw.connect("", session.server.getPort());
	ASSERT_TRUE(peer);
	session.pump(1);
	ASSERT_TRUE(raw.send(*peer, 0, Delivery::Reliable, std::array<uint8_t, 3>{1, 2, 3}));
	session.pump(1);
	std::vector<TransportEvent> events;
	raw.poll(events);
	ASSERT_GE(events.size(), 2u);
	EXPECT_EQ(events[events.size() - 2].payload.at(1), static_cast<uint8_t>(RejectReason::WrongProtocol));
	EXPECT_EQ(events.back().kind, TransportEvent::Kind::Disconnected);
}

TEST(NetSession, ServerErrors) {
	Session session;
	EXPECT_TRUE(session.server.isRunning());
	EXPECT_EQ(session.server.start(0).error(), NetworkError::AlreadyOpen);
	EXPECT_EQ(session.server.send(99, 0, Delivery::Reliable, std::array<uint8_t, 1>{0}).error(),
			  NetworkError::UnknownPeer);
	EXPECT_EQ(session.server.getStats(99).error(), NetworkError::UnknownPeer);
	EXPECT_FALSE(session.server.isConnected(99));
	session.server.kick(99);
	session.server.stop();
	EXPECT_FALSE(session.server.isRunning());
	EXPECT_EQ(session.server.getPort(), 0);
}

TEST(NetSession, ClientErrors) {
	NetClient client{mkUniq<LoopbackTransport>(), config()};
	EXPECT_EQ(client.getStats().error(), NetworkError::NotOpen);
	ASSERT_TRUE(client.connect("", 1));
	EXPECT_EQ(client.connect("", 1).error(), NetworkError::AlreadyOpen);
	client.onUpdate(step(std::chrono::milliseconds{10}));
	ASSERT_EQ(client.getEvents().size(), 1u);
	EXPECT_EQ(client.getEvents()[0].reason, LeaveReason::Unreachable);
}
