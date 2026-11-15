/**
 * @file EnetTransport.cpp
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "EnetTransport.h"

#include "core/external/enet.h"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <ranges>
#include <span>
#include <string>
#include <vector>

namespace owl::network::enet {

namespace {

// Number of live ENet transports: ENet is initialized by the first and deinitialized by the last.
std::atomic<uint32_t> g_EnetUsers{0};

// Peer slots of a host that only connects.
constexpr uint32_t g_ClientSlots = 8;

// Disconnection data meaning "closed on purpose"; a timeout carries 0.
constexpr uint32_t g_GracefulData = 1;

// Timeout of a silent peer: ENet's retry limit, then at least 3 s, at most 10 s.
constexpr uint32_t g_TimeoutLimit = 32;
constexpr uint32_t g_TimeoutMinimumMs = 3000;
constexpr uint32_t g_TimeoutMaximumMs = 10000;

// Identifier stored in an ENet peer's user data.
auto peerIdOf(const ENetPeer* iPeer) -> PeerId { return static_cast<PeerId>(reinterpret_cast<uintptr_t>(iPeer->data)); }

}// namespace

EnetTransport::EnetTransport() : Transport{Type::Enet} {
	if (g_EnetUsers.fetch_add(1) == 0 && enet_initialize() != 0) {
		OWL_CORE_ERROR("EnetTransport: ENet failed to initialize.")
		g_EnetUsers.fetch_sub(1);
		return;
	}
	m_available = true;
}

EnetTransport::~EnetTransport() {
	close();
	if (m_available && g_EnetUsers.fetch_sub(1) == 1)
		enet_deinitialize();
}

auto EnetTransport::ensureHost(const uint32_t iMaxPeers) -> bool {
	if (mp_host != nullptr)
		return true;
	mp_host = enet_host_create(nullptr, iMaxPeers, g_ChannelCount, 0, 0);
	return mp_host != nullptr;
}

auto EnetTransport::track(ENetPeer* iPeer) -> PeerId {
	const PeerId id = m_nextPeer++;
	iPeer->data =
			reinterpret_cast<void*>(static_cast<uintptr_t>(id));// NOLINT(*-no-int-to-ptr): ENet's user data is a void*
	enet_peer_timeout(iPeer, g_TimeoutLimit, g_TimeoutMinimumMs, g_TimeoutMaximumMs);
	m_peers.emplace(id,
					PeerState{.peer = iPeer, .connected = false, .closing = false, .bytesSent = 0, .bytesReceived = 0});
	return id;
}

auto EnetTransport::listen(const uint16_t iPort, const uint32_t iMaxPeers) -> expected<void, NetworkError> {
	if (!m_available) {
		OWL_CORE_WARN("EnetTransport: ENet is not initialized.")
		return unexpected{NetworkError::BackendUnavailable};
	}
	if (mp_host != nullptr) {
		OWL_CORE_WARN("EnetTransport: already open.")
		return unexpected{NetworkError::AlreadyOpen};
	}
	ENetAddress address{};
	address.host = ENET_HOST_ANY;
	address.port = iPort;
	mp_host = enet_host_create(&address, iMaxPeers, g_ChannelCount, 0, 0);
	if (mp_host == nullptr) {
		OWL_CORE_WARN("EnetTransport: cannot bind port {}.", iPort)
		return unexpected{NetworkError::BindFailed};
	}
	m_port = mp_host->address.port;
	return {};
}

auto EnetTransport::connect(const std::string& iHost, const uint16_t iPort) -> expected<PeerId, NetworkError> {
	if (!m_available) {
		OWL_CORE_WARN("EnetTransport: ENet is not initialized.")
		return unexpected{NetworkError::BackendUnavailable};
	}
	ENetAddress address{};
	if (enet_address_set_host(&address, iHost.c_str()) != 0) {
		OWL_CORE_WARN("EnetTransport: cannot resolve host '{}'.", iHost)
		return unexpected{NetworkError::ResolveFailed};
	}
	address.port = iPort;
	if (!ensureHost(g_ClientSlots)) {
		OWL_CORE_WARN("EnetTransport: cannot create the client host.")
		return unexpected{NetworkError::BindFailed};
	}
	ENetPeer* peer = enet_host_connect(mp_host, &address, g_ChannelCount, 0);
	if (peer == nullptr) {
		OWL_CORE_WARN("EnetTransport: no free peer slot to reach {}:{}.", iHost, iPort)
		return unexpected{NetworkError::ConnectFailed};
	}
	return track(peer);
}

void EnetTransport::disconnect(const PeerId iPeer) {
	const auto found = m_peers.find(iPeer);
	if (found == m_peers.end() || found->second.closing)
		return;
	found->second.closing = true;
	// "later": the messages already queued (a rejection, a last state) leave before the disconnection.
	enet_peer_disconnect_later(found->second.peer, g_GracefulData);
}

void EnetTransport::close() {
	if (mp_host == nullptr)
		return;
	for (const auto& state: m_peers | std::views::values) enet_peer_disconnect_now(state.peer, g_GracefulData);
	enet_host_flush(mp_host);
	enet_host_destroy(mp_host);
	mp_host = nullptr;
	m_port = 0;
	m_peers.clear();
}

auto EnetTransport::send(const PeerId iPeer, const uint8_t iChannel, const Delivery iDelivery,
						 const std::span<const uint8_t> iPayload) -> expected<void, NetworkError> {
	if (iChannel >= g_ChannelCount) {
		OWL_CORE_WARN("EnetTransport: channel {} out of range.", iChannel)
		return unexpected{NetworkError::InvalidChannel};
	}
	const auto found = m_peers.find(iPeer);
	if (found == m_peers.end() || !found->second.connected) {
		OWL_CORE_WARN("EnetTransport: no connected peer {}.", iPeer)
		return unexpected{NetworkError::UnknownPeer};
	}
	const uint32_t flags = iDelivery == Delivery::Reliable ? ENET_PACKET_FLAG_RELIABLE : 0;
	ENetPacket* packet = enet_packet_create(iPayload.data(), iPayload.size(), flags);
	if (packet == nullptr || enet_peer_send(found->second.peer, iChannel, packet) != 0) {
		if (packet != nullptr)
			enet_packet_destroy(packet);
		OWL_CORE_WARN("EnetTransport: cannot queue a message to peer {}.", iPeer)
		return unexpected{NetworkError::UnknownPeer};
	}
	found->second.bytesSent += iPayload.size();
	return {};
}

void EnetTransport::poll(std::vector<TransportEvent>& oEvents) {
	if (mp_host == nullptr)
		return;
	ENetEvent event{};
	while (enet_host_service(mp_host, &event, 0) > 0) {
		switch (event.type) {
			case ENET_EVENT_TYPE_CONNECT:
				{
					// An incoming peer has no identifier yet; an outgoing one got it from connect().
					const PeerId id = event.peer->data == nullptr ? track(event.peer) : peerIdOf(event.peer);
					m_peers[id].connected = true;
					oEvents.push_back({.kind = TransportEvent::Kind::Connected,
									   .peer = id,
									   .channel = 0,
									   .reason = DisconnectReason::Graceful,
									   .payload = {}});
					break;
				}
			case ENET_EVENT_TYPE_RECEIVE:
				{
					const PeerId id = peerIdOf(event.peer);
					const std::span<const uint8_t> bytes{event.packet->data, event.packet->dataLength};
					if (const auto found = m_peers.find(id); found != m_peers.end())
						found->second.bytesReceived += bytes.size();
					oEvents.push_back({.kind = TransportEvent::Kind::Received,
									   .peer = id,
									   .channel = event.channelID,
									   .reason = DisconnectReason::Graceful,
									   .payload = {bytes.begin(), bytes.end()}});
					enet_packet_destroy(event.packet);
					break;
				}
			case ENET_EVENT_TYPE_DISCONNECT:
				{
					const PeerId id = peerIdOf(event.peer);
					const auto found = m_peers.find(id);
					if (found == m_peers.end())
						break;
					DisconnectReason reason = DisconnectReason::Timeout;
					if (!found->second.connected)
						reason = DisconnectReason::Refused;
					else if (found->second.closing || event.data == g_GracefulData)
						reason = DisconnectReason::Graceful;
					m_peers.erase(found);
					event.peer->data = nullptr;
					oEvents.push_back({.kind = TransportEvent::Kind::Disconnected,
									   .peer = id,
									   .channel = 0,
									   .reason = reason,
									   .payload = {}});
					break;
				}
			case ENET_EVENT_TYPE_NONE:
				break;
		}
	}
}

auto EnetTransport::getPeers() const -> std::vector<PeerId> {
	std::vector<PeerId> peers;
	for (const auto& [id, state]: m_peers)
		if (state.connected)
			peers.push_back(id);
	return peers;
}

auto EnetTransport::getStats(const PeerId iPeer) const -> expected<PeerStats, NetworkError> {
	const auto found = m_peers.find(iPeer);
	if (found == m_peers.end()) {
		OWL_CORE_WARN("EnetTransport: no peer {}.", iPeer)
		return unexpected{NetworkError::UnknownPeer};
	}
	const ENetPeer* peer = found->second.peer;
	return PeerStats{.roundTripMs = peer->roundTripTime,
					 .packetLoss =
							 static_cast<float>(peer->packetLoss) / static_cast<float>(ENET_PEER_PACKET_LOSS_SCALE),
					 .bytesSent = found->second.bytesSent,
					 .bytesReceived = found->second.bytesReceived};
}

}// namespace owl::network::enet
