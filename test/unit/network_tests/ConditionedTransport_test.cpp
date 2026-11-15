/**
 * @file ConditionedTransport_test.cpp
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <network/ConditionedTransport.h>
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
using std::chrono::milliseconds;

namespace {

auto pollAll(Transport& ioTransport) -> std::vector<TransportEvent> {
	std::vector<TransportEvent> events;
	ioTransport.poll(events);
	return events;
}

auto received(const std::vector<TransportEvent>& iEvents) -> std::vector<uint8_t> {
	std::vector<uint8_t> bytes;
	for (const auto& event: iEvents)
		if (event.kind == TransportEvent::Kind::Received && !event.payload.empty())
			bytes.push_back(event.payload.front());
	return bytes;
}

/// A loopback server and a conditioned loopback client linked to it, on a clock the test moves.
struct Link {
	milliseconds now{0};
	LoopbackTransport server;
	ConditionedTransport client;
	PeerId toServer = 0;

	explicit Link(const LinkConditions& iConditions)
		: client{mkUniq<LoopbackTransport>(), iConditions, [this]() -> milliseconds { return now; }} {
		EXPECT_TRUE(server.listen(0, 4));
		toServer = client.connect("local", server.getPort()).value_or(0);
		static_cast<void>(pollAll(client));
		static_cast<void>(pollAll(server));
	}

	void send(const uint8_t iByte, const Delivery iDelivery = Delivery::Reliable, const uint8_t iChannel = 0) {
		const std::array bytes{iByte};
		EXPECT_TRUE(client.send(toServer, iChannel, iDelivery, bytes));
	}

	auto deliver(const milliseconds iAdvance) -> std::vector<TransportEvent> {
		now += iAdvance;
		static_cast<void>(pollAll(client));
		return pollAll(server);
	}
};

}// namespace

TEST(ConditionedTransport, NeutralConditionsPassThrough) {
	Link link{{}};
	EXPECT_EQ(link.client.getType(), Transport::Type::Loopback);
	EXPECT_TRUE(link.client.isOpen());
	link.send(7);
	EXPECT_EQ(link.client.getHeldCount(), 0u);
	EXPECT_EQ(received(pollAll(link.server)), std::vector<uint8_t>{7});
}

TEST(ConditionedTransport, LatencyHoldsUntilDue) {
	Link link{{.latencyMs = 100}};
	link.send(1);
	EXPECT_EQ(link.client.getHeldCount(), 1u);
	EXPECT_TRUE(received(link.deliver(milliseconds{60})).empty());
	EXPECT_EQ(received(link.deliver(milliseconds{40})), std::vector<uint8_t>{1});
	EXPECT_EQ(link.client.getHeldCount(), 0u);
}

TEST(ConditionedTransport, ReliableKeepsOrderUnderJitter) {
	Link link{{.latencyMs = 10, .jitterMs = 80, .seed = 5}};
	std::vector<uint8_t> expected;
	for (uint8_t i = 0; i < 30; ++i) {
		link.send(i);
		expected.push_back(i);
		link.now += milliseconds{1};
	}
	EXPECT_EQ(received(link.deliver(milliseconds{200})), expected);
}

TEST(ConditionedTransport, LossDropsOnlyUnreliable) {
	Link link{{.loss = 1.f}};
	link.send(1, Delivery::Unreliable);
	link.send(2, Delivery::Reliable);
	EXPECT_EQ(received(link.deliver(milliseconds{1})), std::vector<uint8_t>{2});
	const auto stats = link.client.getStats(link.toServer);
	ASSERT_TRUE(stats);
	EXPECT_FLOAT_EQ(stats->packetLoss, 0.5f);
}

TEST(ConditionedTransport, DuplicateSendsUnreliableTwice) {
	Link link{{.duplicate = 1.f}};
	link.send(1, Delivery::Unreliable);
	link.send(2, Delivery::Reliable);
	auto bytes = received(link.deliver(milliseconds{1}));
	std::ranges::sort(bytes);
	EXPECT_EQ(bytes, (std::vector<uint8_t>{1, 1, 2}));
}

TEST(ConditionedTransport, SameSeedSameOutcome) {
	const LinkConditions conditions{.latencyMs = 5, .jitterMs = 20, .loss = 0.5f, .seed = 42};
	auto run = [&conditions]() -> std::vector<uint8_t> {
		Link link{conditions};
		for (uint8_t i = 0; i < 64; ++i) link.send(i, Delivery::Unreliable);
		return received(link.deliver(milliseconds{100}));
	};
	const auto first = run();
	EXPECT_EQ(first, run());
	EXPECT_GT(first.size(), 0u);
	EXPECT_LT(first.size(), 64u);
}

TEST(ConditionedTransport, DisconnectWaitsForHeldMessages) {
	Link link{{.latencyMs = 50}};
	link.send(9);
	link.client.disconnect(link.toServer);
	EXPECT_TRUE(link.deliver(milliseconds{0}).empty());
	const auto events = link.deliver(milliseconds{50});
	ASSERT_EQ(events.size(), 2u);
	EXPECT_EQ(events[0].kind, TransportEvent::Kind::Received);
	EXPECT_EQ(events[1].kind, TransportEvent::Kind::Disconnected);
	EXPECT_EQ(events[1].reason, DisconnectReason::Graceful);
}

TEST(ConditionedTransport, LostPeerDropsItsHeldMessages) {
	Link link{{.latencyMs = 50}};
	link.send(3);
	link.server.disconnect(link.server.getPeers().front());
	static_cast<void>(pollAll(link.client));
	EXPECT_EQ(link.client.getHeldCount(), 0u);
}

TEST(ConditionedTransport, RefusesBadSends) {
	Link link{{.latencyMs = 10}};
	const std::array<uint8_t, 1> bytes{0};
	const auto badChannel = link.client.send(link.toServer, g_ChannelCount, Delivery::Reliable, bytes);
	ASSERT_FALSE(badChannel);
	EXPECT_EQ(badChannel.error(), NetworkError::InvalidChannel);
	const auto badPeer = link.client.send(link.toServer + 100, 0, Delivery::Reliable, bytes);
	ASSERT_FALSE(badPeer);
	EXPECT_EQ(badPeer.error(), NetworkError::UnknownPeer);
}

TEST(ConditionedTransport, StatsAndConditions) {
	Link link{{.latencyMs = 40, .jitterMs = 20}};
	const auto stats = link.client.getStats(link.toServer);
	ASSERT_TRUE(stats);
	EXPECT_EQ(stats->roundTripMs, 50u);
	EXPECT_FALSE(link.client.getStats(link.toServer + 100));
	link.client.setConditions({});
	EXPECT_TRUE(link.client.getConditions().isNeutral());
	EXPECT_EQ(link.client.getPeers().size(), 1u);
	EXPECT_EQ(link.client.getPort(), 0);
	EXPECT_EQ(link.client.getInner().getType(), Transport::Type::Loopback);
	link.client.close();
	EXPECT_FALSE(link.client.isOpen());
}

TEST(ConditionedTransport, SessionSurvivesABadLink) {
	milliseconds now{0};
	const SessionConfig config{.gameId = "test.game", .gameVersion = 1};
	const LinkConditions conditions{.latencyMs = 30, .jitterMs = 30, .loss = 0.3f, .duplicate = 0.2f, .seed = 9};
	auto clock = [&now]() -> milliseconds { return now; };
	NetServer server{mkUniq<ConditionedTransport>(mkUniq<LoopbackTransport>(), conditions, clock), config};
	ASSERT_TRUE(server.start(0));
	NetClient client{mkUniq<ConditionedTransport>(mkUniq<LoopbackTransport>(), conditions, clock), config};
	ASSERT_TRUE(client.connect("local", server.getPort()));

	core::Timestep timestep;
	timestep.forceUpdate(milliseconds{10});
	size_t reliable = 0;
	for (int i = 0; i < 100; ++i) {
		now += milliseconds{10};
		if (client.getState() == NetClient::State::Connected && i < 50) {
			const std::array bytes{static_cast<uint8_t>(i)};
			EXPECT_TRUE(client.send(0, Delivery::Reliable, bytes));
			EXPECT_TRUE(client.send(1, Delivery::Unreliable, bytes));
		}
		client.onUpdate(timestep);
		server.onUpdate(timestep);
		for (const auto& event: server.getEvents())
			if (event.kind == ServerEvent::Kind::Message && event.channel == 0)
				++reliable;
	}
	EXPECT_EQ(client.getState(), NetClient::State::Connected);
	EXPECT_GT(reliable, 0u);
}
