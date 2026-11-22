/**
 * @file WebSocketTransport_test.cpp
 * @author Silmaen
 * @date 19/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <network/NetClient.h>
#include <network/NetServer.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace owl;
using namespace owl::network;

namespace {

// A page only connects: the tests that listen run on the desktop.
auto makeWebSocket() -> uniq<Transport> {
#ifdef OWL_PLATFORM_WEB
	return nullptr;
#else
	auto transport = Transport::create(Transport::Type::WebSocket);
	return transport ? std::move(*transport) : nullptr;
#endif
}

}// namespace

// A real WebSocket round trip on the local host: connect, handshake, a reliable message each way, leave.
TEST(WebSocketTransport, SessionOverLocalWebSocket) {
	auto serverTransport = makeWebSocket();
	auto clientTransport = makeWebSocket();
	if (!serverTransport || !clientTransport)
		GTEST_SKIP() << "WebSocket server not built in";
	const SessionConfig config{.gameId = "websocket.test",
							   .gameVersion = 1,
							   .maxClients = 4,
							   .handshakeTimeout = 5.f,
							   .reconnectGrace = 5.f,
							   .reconnectInterval = 1.f};
	NetServer server{std::move(serverTransport), config};
	ASSERT_TRUE(server.start(0));
	ASSERT_NE(server.getPort(), 0);
	NetClient client{std::move(clientTransport), config};
	ASSERT_TRUE(client.connect("127.0.0.1", server.getPort()));

	core::Timestep timestep;
	bool serverGotMessage = false;
	bool clientGotMessage = false;
	const std::array<uint8_t, 3> ping{1, 2, 3};
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
	while (std::chrono::steady_clock::now() < deadline && !(serverGotMessage && clientGotMessage)) {
		timestep.update();
		client.onUpdate(timestep);
		for (const auto& event: client.getEvents()) {
			if (event.kind == ClientEvent::Kind::Joined) {
				EXPECT_TRUE(client.send(0, Delivery::Reliable, ping));
			}
			if (event.kind == ClientEvent::Kind::Message)
				clientGotMessage = event.payload == std::vector<uint8_t>{4};
		}
		server.onUpdate(timestep);
		for (const auto& event: server.getEvents()) {
			if (event.kind != ServerEvent::Kind::Message)
				continue;
			serverGotMessage = event.payload == std::vector<uint8_t>{1, 2, 3};
			EXPECT_TRUE(server.send(event.client, 1, Delivery::Reliable, std::array<uint8_t, 1>{4}));
		}
		std::this_thread::sleep_for(std::chrono::milliseconds{1});
	}
	EXPECT_TRUE(serverGotMessage);
	EXPECT_TRUE(clientGotMessage);
	ASSERT_TRUE(client.getClientId());
	EXPECT_TRUE(server.getStats(*client.getClientId()));
	EXPECT_EQ(client.getTransport().getType(), Transport::Type::WebSocket);
}

TEST(WebSocketTransport, Errors) {
	auto transport = makeWebSocket();
	if (!transport)
		GTEST_SKIP() << "WebSocket server not built in";
	EXPECT_FALSE(transport->isOpen());
	EXPECT_EQ(transport->send(1, 0, Delivery::Reliable, std::array<uint8_t, 1>{0}).error(), NetworkError::UnknownPeer);
	EXPECT_EQ(transport->getStats(1).error(), NetworkError::UnknownPeer);
	ASSERT_TRUE(transport->listen(0, 2));
	EXPECT_EQ(transport->listen(0, 2).error(), NetworkError::AlreadyOpen);
	EXPECT_EQ(transport->send(1, g_ChannelCount, Delivery::Reliable, std::array<uint8_t, 1>{0}).error(),
			  NetworkError::InvalidChannel);
	EXPECT_TRUE(transport->getPeers().empty());
	transport->close();
	EXPECT_FALSE(transport->isOpen());
	EXPECT_EQ(transport->getPort(), 0);
}

TEST(WebSocketTransport, RefusedWithoutServer) {
	auto listener = makeWebSocket();
	auto transport = makeWebSocket();
	if (!listener || !transport)
		GTEST_SKIP() << "WebSocket server not built in";
	// A port that was just free: nothing listens there any more.
	ASSERT_TRUE(listener->listen(0, 1));
	const auto port = listener->getPort();
	listener->close();
	const auto peer = transport->connect("127.0.0.1", port);
	ASSERT_TRUE(peer);
	EXPECT_TRUE(transport->isOpen());
	EXPECT_EQ(transport->listen(0, 1).error(), NetworkError::AlreadyOpen);
	std::vector<TransportEvent> events;
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
	while (std::chrono::steady_clock::now() < deadline && events.empty()) {
		transport->poll(events);
		std::this_thread::sleep_for(std::chrono::milliseconds{1});
	}
	ASSERT_EQ(events.size(), 1u);
	EXPECT_EQ(events.front().kind, TransportEvent::Kind::Disconnected);
	EXPECT_EQ(events.front().peer, *peer);
	EXPECT_EQ(events.front().reason, DisconnectReason::Refused);
	EXPECT_TRUE(transport->getPeers().empty());
}

TEST(WebSocketTransport, FullServerClosesTheExtraPeer) {
	auto server = makeWebSocket();
	auto first = makeWebSocket();
	auto second = makeWebSocket();
	if (!server || !first || !second)
		GTEST_SKIP() << "WebSocket server not built in";
	ASSERT_TRUE(server->listen(0, 1));
	ASSERT_TRUE(first->connect("127.0.0.1", server->getPort()));
	std::vector<TransportEvent> serverEvents;
	auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
	while (std::chrono::steady_clock::now() < deadline && serverEvents.empty()) {
		server->poll(serverEvents);
		std::this_thread::sleep_for(std::chrono::milliseconds{1});
	}
	ASSERT_EQ(serverEvents.size(), 1u);
	EXPECT_EQ(serverEvents.front().kind, TransportEvent::Kind::Connected);
	ASSERT_TRUE(second->connect("127.0.0.1", server->getPort()));
	std::vector<TransportEvent> secondEvents;
	deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
	bool closed = false;
	while (std::chrono::steady_clock::now() < deadline && !closed) {
		second->poll(secondEvents);
		for (const auto& event: secondEvents) closed = closed || event.kind == TransportEvent::Kind::Disconnected;
		std::this_thread::sleep_for(std::chrono::milliseconds{1});
	}
	EXPECT_TRUE(closed);
	EXPECT_EQ(server->getPeers().size(), 1u);
}

#ifdef OWL_PLATFORM_WEB
TEST(WebSocketTransport, PageCannotListen) {
	auto transport = Transport::create(Transport::Type::WebSocket);
	ASSERT_TRUE(transport);
	EXPECT_EQ((*transport)->listen(0, 1).error(), NetworkError::BackendUnavailable);
	EXPECT_FALSE((*transport)->isOpen());
	EXPECT_FALSE(Transport::create(Transport::Type::Enet));
}
#endif
