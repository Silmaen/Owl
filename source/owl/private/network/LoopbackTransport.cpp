/**
 * @file LoopbackTransport.cpp
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "network/LoopbackTransport.h"

#include <cstdint>
#include <iterator>
#include <ranges>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace owl::network {

// One end of a link: the transport at the other end and the identifier it gives this one.
struct LoopbackLink {
	// The transport at the other end.
	LoopbackTransport::Impl* remote = nullptr;
	// How the other end names this one.
	PeerId remoteId = 0;
	// Statistics of the link.
	PeerStats stats;
};

struct LoopbackTransport::Impl {
	// Links by peer identifier.
	std::unordered_map<PeerId, LoopbackLink> links;
	// Events waiting for the next poll.
	std::vector<TransportEvent> pending;
	// Port listened on, 0 when not listening.
	uint16_t port = 0;
	// Maximum number of incoming links.
	uint32_t maxPeers = 0;
	// Next peer identifier to hand out.
	PeerId nextPeer = 1;

	// Create the local end of a link.
	auto addLink(Impl* iRemote, const PeerId iRemoteId) -> PeerId {
		const PeerId id = nextPeer++;
		links.emplace(id, LoopbackLink{.remote = iRemote, .remoteId = iRemoteId, .stats = {}});
		return id;
	}

	// Queue a disconnection event.
	void pushDisconnected(const PeerId iPeer, const DisconnectReason iReason) {
		pending.push_back({.kind = TransportEvent::Kind::Disconnected,
						   .peer = iPeer,
						   .channel = 0,
						   .reason = iReason,
						   .payload = {}});
	}

	// Remove a link on both ends, each getting a disconnection event.
	void cut(const PeerId iPeer, const DisconnectReason iReason, const bool iNotifyLocal) {
		const auto found = links.find(iPeer);
		if (found == links.end())
			return;
		const LoopbackLink link = found->second;
		links.erase(found);
		if (link.remote != nullptr && link.remote->links.erase(link.remoteId) > 0)
			link.remote->pushDisconnected(link.remoteId, iReason);
		if (iNotifyLocal)
			pushDisconnected(iPeer, iReason);
	}
};

namespace {

// First port handed out to a loopback listener asking for any port.
constexpr uint16_t g_FirstDynamicPort = 49152;

// The loopback listeners of the process, by port. Transports live on one thread, like every transport call.
auto listeners() -> std::unordered_map<uint16_t, LoopbackTransport::Impl*>& {
	static std::unordered_map<uint16_t, LoopbackTransport::Impl*> registry;
	return registry;
}

}// namespace

LoopbackTransport::LoopbackTransport() : Transport{Type::Loopback}, mp_impl{mkUniq<Impl>()} {}

LoopbackTransport::~LoopbackTransport() { close(); }

auto LoopbackTransport::listen(const uint16_t iPort, const uint32_t iMaxPeers) -> expected<void, NetworkError> {
	if (mp_impl->port != 0) {
		OWL_CORE_WARN("LoopbackTransport: already listening on port {}.", mp_impl->port)
		return unexpected{NetworkError::AlreadyOpen};
	}
	auto& registry = listeners();
	uint16_t port = iPort;
	if (port == 0) {
		port = g_FirstDynamicPort;
		while (registry.contains(port) && port < UINT16_MAX) ++port;
	}
	if (registry.contains(port)) {
		OWL_CORE_WARN("LoopbackTransport: port {} already has a loopback listener.", port)
		return unexpected{NetworkError::BindFailed};
	}
	registry.emplace(port, mp_impl.get());
	mp_impl->port = port;
	mp_impl->maxPeers = iMaxPeers;
	return {};
}

auto LoopbackTransport::connect([[maybe_unused]] const std::string& iHost, const uint16_t iPort)
		-> expected<PeerId, NetworkError> {
	const auto& registry = listeners();
	const auto found = registry.find(iPort);
	if (found == registry.end() || found->second == mp_impl.get() ||
		found->second->links.size() >= found->second->maxPeers) {
		// Like a socket, the refusal comes as an event: the caller handles one path for every failure after the start.
		const PeerId id = mp_impl->nextPeer++;
		mp_impl->pushDisconnected(id, DisconnectReason::Refused);
		return id;
	}
	Impl* listener = found->second;
	const PeerId localId = mp_impl->nextPeer;
	const PeerId remoteId = listener->addLink(mp_impl.get(), localId);
	mp_impl->addLink(listener, remoteId);
	listener->pending.push_back({.kind = TransportEvent::Kind::Connected,
								 .peer = remoteId,
								 .channel = 0,
								 .reason = DisconnectReason::Graceful,
								 .payload = {}});
	mp_impl->pending.push_back({.kind = TransportEvent::Kind::Connected,
								.peer = localId,
								.channel = 0,
								.reason = DisconnectReason::Graceful,
								.payload = {}});
	return localId;
}

void LoopbackTransport::disconnect(const PeerId iPeer) { mp_impl->cut(iPeer, DisconnectReason::Graceful, true); }

void LoopbackTransport::severLink(const PeerId iPeer) { mp_impl->cut(iPeer, DisconnectReason::Timeout, true); }

void LoopbackTransport::close() {
	while (!mp_impl->links.empty()) mp_impl->cut(mp_impl->links.begin()->first, DisconnectReason::Graceful, false);
	if (mp_impl->port != 0) {
		listeners().erase(mp_impl->port);
		mp_impl->port = 0;
	}
	mp_impl->pending.clear();
}

auto LoopbackTransport::send(const PeerId iPeer, const uint8_t iChannel, [[maybe_unused]] const Delivery iDelivery,
							 const std::span<const uint8_t> iPayload) -> expected<void, NetworkError> {
	if (iChannel >= g_ChannelCount) {
		OWL_CORE_WARN("LoopbackTransport: channel {} out of range.", iChannel)
		return unexpected{NetworkError::InvalidChannel};
	}
	const auto found = mp_impl->links.find(iPeer);
	if (found == mp_impl->links.end()) {
		OWL_CORE_WARN("LoopbackTransport: no peer {}.", iPeer)
		return unexpected{NetworkError::UnknownPeer};
	}
	LoopbackLink& link = found->second;
	link.stats.bytesSent += iPayload.size();
	if (const auto back = link.remote->links.find(link.remoteId); back != link.remote->links.end())
		back->second.stats.bytesReceived += iPayload.size();
	link.remote->pending.push_back({.kind = TransportEvent::Kind::Received,
									.peer = link.remoteId,
									.channel = iChannel,
									.reason = DisconnectReason::Graceful,
									.payload = {iPayload.begin(), iPayload.end()}});
	return {};
}

void LoopbackTransport::poll(std::vector<TransportEvent>& oEvents) {
	oEvents.insert(oEvents.end(), std::make_move_iterator(mp_impl->pending.begin()),
				   std::make_move_iterator(mp_impl->pending.end()));
	mp_impl->pending.clear();
}

auto LoopbackTransport::isOpen() const -> bool { return mp_impl->port != 0 || !mp_impl->links.empty(); }

auto LoopbackTransport::getPort() const -> uint16_t { return mp_impl->port; }

auto LoopbackTransport::getPeers() const -> std::vector<PeerId> {
	std::vector<PeerId> peers;
	peers.reserve(mp_impl->links.size());
	for (const auto& id: mp_impl->links | std::views::keys) peers.push_back(id);
	return peers;
}

auto LoopbackTransport::getStats(const PeerId iPeer) const -> expected<PeerStats, NetworkError> {
	const auto found = mp_impl->links.find(iPeer);
	if (found == mp_impl->links.end()) {
		OWL_CORE_WARN("LoopbackTransport: no peer {}.", iPeer)
		return unexpected{NetworkError::UnknownPeer};
	}
	return found->second.stats;
}

}// namespace owl::network
