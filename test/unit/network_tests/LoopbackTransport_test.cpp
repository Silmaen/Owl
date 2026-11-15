/**
 * @file LoopbackTransport_test.cpp
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <network/LoopbackTransport.h>

#include <array>
#include <cstdint>
#include <vector>

using namespace owl::network;

namespace {

auto pollAll(Transport& ioTransport) -> std::vector<TransportEvent> {
	std::vector<TransportEvent> events;
	ioTransport.poll(events);
	return events;
}

}// namespace

TEST(LoopbackTransport, ConnectSendDisconnect) {
	LoopbackTransport server;
	LoopbackTransport client;
	ASSERT_TRUE(server.listen(0, 4));
	EXPECT_NE(server.getPort(), 0);
	EXPECT_TRUE(server.isOpen());
	EXPECT_FALSE(client.isOpen());

	const auto peer = client.connect("ignored", server.getPort());
	ASSERT_TRUE(peer);
	auto serverEvents = pollAll(server);
	ASSERT_EQ(serverEvents.size(), 1u);
	EXPECT_EQ(serverEvents[0].kind, TransportEvent::Kind::Connected);
	const PeerId clientOnServer = serverEvents[0].peer;
	auto clientEvents = pollAll(client);
	ASSERT_EQ(clientEvents.size(), 1u);
	EXPECT_EQ(clientEvents[0].peer, *peer);

	const std::array<uint8_t, 3> first{1, 2, 3};
	const std::array<uint8_t, 1> second{4};
	ASSERT_TRUE(client.send(*peer, 2, Delivery::Reliable, first));
	ASSERT_TRUE(client.send(*peer, 2, Delivery::Unreliable, second));
	serverEvents = pollAll(server);
	ASSERT_EQ(serverEvents.size(), 2u);
	EXPECT_EQ(serverEvents[0].kind, TransportEvent::Kind::Received);
	EXPECT_EQ(serverEvents[0].channel, 2);
	EXPECT_EQ(serverEvents[0].payload, (std::vector<uint8_t>{1, 2, 3}));
	EXPECT_EQ(serverEvents[1].payload, (std::vector<uint8_t>{4}));

	const auto stats = client.getStats(*peer);
	ASSERT_TRUE(stats);
	EXPECT_EQ(stats->bytesSent, 4u);
	EXPECT_EQ(server.getStats(clientOnServer)->bytesReceived, 4u);
	EXPECT_EQ(server.getPeers(), std::vector<PeerId>{clientOnServer});

	server.disconnect(clientOnServer);
	serverEvents = pollAll(server);
	ASSERT_EQ(serverEvents.size(), 1u);
	EXPECT_EQ(serverEvents[0].kind, TransportEvent::Kind::Disconnected);
	EXPECT_EQ(serverEvents[0].reason, DisconnectReason::Graceful);
	clientEvents = pollAll(client);
	ASSERT_EQ(clientEvents.size(), 1u);
	EXPECT_EQ(clientEvents[0].reason, DisconnectReason::Graceful);
	EXPECT_TRUE(server.getPeers().empty());
}

TEST(LoopbackTransport, Errors) {
	LoopbackTransport server;
	ASSERT_TRUE(server.listen(0, 1));
	EXPECT_EQ(server.listen(0, 1).error(), NetworkError::AlreadyOpen);
	LoopbackTransport other;
	EXPECT_EQ(other.listen(server.getPort(), 1).error(), NetworkError::BindFailed);

	const std::array<uint8_t, 1> byte{0};
	EXPECT_EQ(other.send(42, 0, Delivery::Reliable, byte).error(), NetworkError::UnknownPeer);
	EXPECT_EQ(other.getStats(42).error(), NetworkError::UnknownPeer);
	const auto peer = other.connect("", server.getPort());
	ASSERT_TRUE(peer);
	EXPECT_EQ(other.send(*peer, g_ChannelCount, Delivery::Reliable, byte).error(), NetworkError::InvalidChannel);
}

TEST(LoopbackTransport, RefusedWhenNothingListensOrFull) {
	LoopbackTransport client;
	const auto nobody = client.connect("", 1);
	ASSERT_TRUE(nobody);
	auto events = pollAll(client);
	ASSERT_EQ(events.size(), 1u);
	EXPECT_EQ(events[0].kind, TransportEvent::Kind::Disconnected);
	EXPECT_EQ(events[0].reason, DisconnectReason::Refused);

	LoopbackTransport server;
	ASSERT_TRUE(server.listen(0, 1));
	LoopbackTransport first;
	ASSERT_TRUE(first.connect("", server.getPort()));
	ASSERT_TRUE(client.connect("", server.getPort()));
	events = pollAll(client);
	ASSERT_EQ(events.size(), 1u);
	EXPECT_EQ(events[0].reason, DisconnectReason::Refused);
}

TEST(LoopbackTransport, SeverLinkLooksLikeATimeout) {
	LoopbackTransport server;
	LoopbackTransport client;
	ASSERT_TRUE(server.listen(0, 2));
	const auto peer = client.connect("", server.getPort());
	ASSERT_TRUE(peer);
	static_cast<void>(pollAll(server));
	static_cast<void>(pollAll(client));
	client.severLink(*peer);
	const auto clientEvents = pollAll(client);
	ASSERT_EQ(clientEvents.size(), 1u);
	EXPECT_EQ(clientEvents[0].reason, DisconnectReason::Timeout);
	const auto serverEvents = pollAll(server);
	ASSERT_EQ(serverEvents.size(), 1u);
	EXPECT_EQ(serverEvents[0].reason, DisconnectReason::Timeout);
}

TEST(LoopbackTransport, CloseNotifiesPeersAndFreesThePort) {
	auto server = owl::mkUniq<LoopbackTransport>();
	LoopbackTransport client;
	ASSERT_TRUE(server->listen(0, 2));
	const uint16_t port = server->getPort();
	ASSERT_TRUE(client.connect("", port));
	static_cast<void>(pollAll(client));
	server.reset();
	const auto events = pollAll(client);
	ASSERT_EQ(events.size(), 1u);
	EXPECT_EQ(events[0].kind, TransportEvent::Kind::Disconnected);
	EXPECT_FALSE(client.isOpen());
	LoopbackTransport again;
	EXPECT_TRUE(again.listen(port, 1));
}

TEST(Transport, CreateLoopback) {
	const auto transport = Transport::create(Transport::Type::Loopback);
	ASSERT_TRUE(transport);
	EXPECT_EQ((*transport)->getType(), Transport::Type::Loopback);
}
