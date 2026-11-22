/**
 * @file MixedTransport_test.cpp
 * @author Silmaen
 * @date 23/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <network/LoopbackTransport.h>
#include <network/MixedTransport.h>
#include <network/NetClient.h>
#include <network/NetServer.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <set>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

using namespace owl;
using namespace owl::network;

namespace {

auto makeMixed(const size_t iLoopbacks) -> uniq<MixedTransport> {
	std::vector<uniq<Transport>> transports;
	for (size_t index = 0; index < iLoopbacks; ++index) transports.push_back(mkUniq<LoopbackTransport>());
	return mkUniq<MixedTransport>(std::move(transports));
}

auto pollOnce(Transport& ioTransport) -> std::vector<TransportEvent> {
	std::vector<TransportEvent> events;
	ioTransport.poll(events);
	return events;
}

// A transport of the desktop, null when not built in.
auto makeTransport(const Transport::Type iType) -> uniq<Transport> {
#ifdef OWL_PLATFORM_WEB
	std::ignore = iType;
	return nullptr;
#else
	auto transport = Transport::create(iType);
	return transport ? std::move(*transport) : nullptr;
#endif
}

}// namespace

TEST(MixedTransport, WithoutBackendNothingOpens) {
	const auto mixed = makeMixed(0);
	EXPECT_EQ(mixed->getType(), Transport::Type::Mixed);
	EXPECT_EQ(mixed->listen(0, 2).error(), NetworkError::BackendUnavailable);
	EXPECT_EQ(mixed->connect("127.0.0.1", 1).error(), NetworkError::BackendUnavailable);
	EXPECT_FALSE(mixed->isOpen());
	EXPECT_EQ(mixed->getPort(), 0);
}

// Two loopbacks cannot share a port: the second fails and the first is closed again.
TEST(MixedTransport, ABackendThatCannotListenClosesTheOthers) {
	const auto mixed = makeMixed(2);
	EXPECT_EQ(mixed->listen(0, 2).error(), NetworkError::BindFailed);
	EXPECT_FALSE(mixed->isOpen());
	EXPECT_EQ(mixed->getPort(), 0);
}

TEST(MixedTransport, AListenerRenamesThePeersOfItsBackend) {
	const auto mixed = makeMixed(1);
	ASSERT_TRUE(mixed->listen(0, 4));
	EXPECT_EQ(mixed->listen(0, 4).error(), NetworkError::AlreadyOpen);
	LoopbackTransport first;
	LoopbackTransport second;
	ASSERT_TRUE(first.connect("", mixed->getPort()));
	ASSERT_TRUE(second.connect("", mixed->getPort()));

	std::set<PeerId> peers;
	for (const auto& event: pollOnce(*mixed)) {
		EXPECT_EQ(event.kind, TransportEvent::Kind::Connected);
		peers.insert(event.peer);
	}
	ASSERT_EQ(peers.size(), 2u);
	EXPECT_EQ(mixed->getPeers().size(), 2u);
	const PeerId peer = *peers.begin();
	EXPECT_EQ(mixed->getPeerType(peer).value(), Transport::Type::Loopback);
	EXPECT_TRUE(mixed->getStats(peer));

	ASSERT_TRUE(mixed->send(peer, 1, Delivery::Reliable, std::array<uint8_t, 2>{7, 8}));
	size_t received = 0;
	for (auto* client: {&first, &second}) {
		for (const auto& event: pollOnce(*client)) {
			if (event.kind == TransportEvent::Kind::Received) {
				EXPECT_EQ(event.payload, (std::vector<uint8_t>{7, 8}));
				++received;
			}
		}
	}
	EXPECT_EQ(received, 1u);

	mixed->disconnect(peer);
	const auto events = pollOnce(*mixed);
	ASSERT_EQ(events.size(), 1u);
	EXPECT_EQ(events.front().kind, TransportEvent::Kind::Disconnected);
	EXPECT_EQ(events.front().peer, peer);
	EXPECT_EQ(mixed->getPeerType(peer).error(), NetworkError::UnknownPeer);
	EXPECT_EQ(mixed->send(peer, 0, Delivery::Reliable, std::array<uint8_t, 1>{1}).error(), NetworkError::UnknownPeer);
	EXPECT_EQ(mixed->getStats(peer).error(), NetworkError::UnknownPeer);
	EXPECT_EQ(mixed->getPeers().size(), 1u);
}

TEST(MixedTransport, AClientConnectsThroughTheFirstBackend) {
	LoopbackTransport listener;
	ASSERT_TRUE(listener.listen(0, 1));
	const auto mixed = makeMixed(1);
	const auto peer = mixed->connect("", listener.getPort());
	ASSERT_TRUE(peer);
	const auto events = pollOnce(*mixed);
	ASSERT_EQ(events.size(), 1u);
	EXPECT_EQ(events.front().kind, TransportEvent::Kind::Connected);
	EXPECT_EQ(events.front().peer, *peer);
	mixed->close();
	EXPECT_FALSE(mixed->isOpen());
	EXPECT_TRUE(mixed->getPeers().empty());
}

// One server on ENet and WebSocket: a desktop client and a Web client join it on the same port number.
TEST(MixedTransport, DesktopAndWebClientsJoinOneServer) {
	auto serverTransport = makeTransport(Transport::Type::Mixed);
	auto enetClient = makeTransport(Transport::Type::Enet);
	auto webClient = makeTransport(Transport::Type::WebSocket);
	if (!serverTransport || !enetClient || !webClient)
		GTEST_SKIP() << "ENet and the WebSocket server are not built in";
	const SessionConfig config{.gameId = "mixed.test",
							   .gameVersion = 1,
							   .maxClients = 4,
							   .handshakeTimeout = 5.f,
							   .reconnectGrace = 5.f,
							   .reconnectInterval = 1.f};
	NetServer server{std::move(serverTransport), config};
	ASSERT_TRUE(server.start(0));
	ASSERT_NE(server.getPort(), 0);
	NetClient desktop{std::move(enetClient), config};
	NetClient web{std::move(webClient), config};
	ASSERT_TRUE(desktop.connect("127.0.0.1", server.getPort()));
	ASSERT_TRUE(web.connect("127.0.0.1", server.getPort()));

	core::Timestep timestep;
	std::set<ClientId> senders;
	size_t answers = 0;
	const std::array<uint8_t, 1> ping{1};
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
	while (std::chrono::steady_clock::now() < deadline && (senders.size() < 2 || answers < 2)) {
		timestep.update();
		for (auto* client: {&desktop, &web}) {
			client->onUpdate(timestep);
			for (const auto& event: client->getEvents()) {
				if (event.kind == ClientEvent::Kind::Joined) {
					EXPECT_TRUE(client->send(0, Delivery::Reliable, ping));
				}
				if (event.kind == ClientEvent::Kind::Message && event.payload == std::vector<uint8_t>{2})
					++answers;
			}
		}
		server.onUpdate(timestep);
		for (const auto& event: server.getEvents()) {
			if (event.kind != ServerEvent::Kind::Message)
				continue;
			senders.insert(event.client);
			EXPECT_TRUE(server.send(event.client, 1, Delivery::Reliable, std::array<uint8_t, 1>{2}));
		}
		std::this_thread::sleep_for(std::chrono::milliseconds{1});
	}
	EXPECT_EQ(senders.size(), 2u);
	EXPECT_EQ(answers, 2u);
	EXPECT_EQ(server.getTransport().getType(), Transport::Type::Mixed);
}
