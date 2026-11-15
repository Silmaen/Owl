/**
 * @file EnetTransport_test.cpp
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <network/NetClient.h>
#include <network/NetServer.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <thread>
#include <utility>
#include <vector>

using namespace owl;
using namespace owl::network;

namespace {

auto makeEnet() -> uniq<Transport> {
	auto transport = Transport::create(Transport::Type::Enet);
	return transport ? std::move(*transport) : nullptr;
}

}// namespace

// A real UDP round trip on the local host: connect, handshake, a reliable message each way, leave.
TEST(EnetTransport, SessionOverLocalUdp) {
	auto serverTransport = makeEnet();
	auto clientTransport = makeEnet();
	if (!serverTransport || !clientTransport)
		GTEST_SKIP() << "ENet is not built in";
	const SessionConfig config{.gameId = "enet.test",
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
	EXPECT_EQ(client.getTransport().getType(), Transport::Type::Enet);
}

TEST(EnetTransport, Errors) {
	auto transport = makeEnet();
	if (!transport)
		GTEST_SKIP() << "ENet is not built in";
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
