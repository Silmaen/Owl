/**
 * @file net_session_fuzzer.cpp
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include <core/Log.h>
#include <core/Timestep.h>
#include <network/LoopbackTransport.h>
#include <network/NetClient.h>
#include <network/NetServer.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

extern "C" auto LLVMFuzzerTestOneInput(const uint8_t* iData, size_t iSize) -> int;

namespace {

using namespace owl;
using namespace owl::network;

// Cut the input into messages, each one prefixed by its length byte.
auto splitMessages(const std::span<const uint8_t> iBytes) -> std::vector<std::span<const uint8_t>> {
	std::vector<std::span<const uint8_t>> messages;
	size_t offset = 0;
	while (offset < iBytes.size()) {
		const size_t length = std::min<size_t>(iBytes[offset], iBytes.size() - offset - 1);
		messages.push_back(iBytes.subspan(offset + 1, length));
		offset += length + 1;
	}
	return messages;
}

auto step() -> core::Timestep {
	core::Timestep timestep;
	timestep.forceUpdate(std::chrono::milliseconds{10});
	return timestep;
}

// A peer sends raw messages to a server: they are either understood or refused, never a crash.
void fuzzServer(const std::vector<std::span<const uint8_t>>& iMessages, const SessionConfig& iConfig) {
	NetServer server{mkUniq<LoopbackTransport>(), iConfig};
	if (!server.start(0))
		return;
	LoopbackTransport raw;
	const auto peer = raw.connect("local", server.getPort());
	if (!peer)
		return;
	std::vector<TransportEvent> events;
	const auto timestep = step();
	for (const auto& message: iMessages) {
		static_cast<void>(raw.send(*peer, 0, Delivery::Reliable, message));
		raw.poll(events);
		server.onUpdate(timestep);
	}
}

// A raw listener answers a client with arbitrary messages.
void fuzzClient(const std::vector<std::span<const uint8_t>>& iMessages, const SessionConfig& iConfig) {
	LoopbackTransport raw;
	if (!raw.listen(0, 1))
		return;
	NetClient client{mkUniq<LoopbackTransport>(), iConfig};
	if (!client.connect("local", raw.getPort()))
		return;
	std::vector<TransportEvent> events;
	const auto timestep = step();
	client.onUpdate(timestep);
	raw.poll(events);
	const auto peers = raw.getPeers();
	if (peers.empty())
		return;
	for (const auto& message: iMessages) {
		static_cast<void>(raw.send(peers.front(), 0, Delivery::Reliable, message));
		raw.poll(events);
		client.onUpdate(timestep);
	}
}

}// namespace

extern "C" auto LLVMFuzzerTestOneInput(const uint8_t* iData, const size_t iSize) -> int {
	static const bool s_logReady = []() -> bool {
		core::Log::init(core::Log::Level::Off);
		return true;
	}();
	static_cast<void>(s_logReady);
	const SessionConfig config{.gameId = "fuzz.game", .gameVersion = 1};
	const auto messages = splitMessages(std::span{iData, iSize});
	fuzzServer(messages, config);
	fuzzClient(messages, config);
	return 0;
}
