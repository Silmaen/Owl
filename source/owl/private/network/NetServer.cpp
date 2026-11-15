/**
 * @file NetServer.cpp
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "network/NetServer.h"

#include "SessionProtocol.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

namespace owl::network {

namespace {

// Peer slots beyond the client limit, so a full server can still answer `ServerFull`.
constexpr uint32_t g_SpareSlots = 4;

// A non-zero random reconnection token.
auto makeToken() -> uint64_t {
	static std::mt19937_64 generator{std::random_device{}()};
	uint64_t token = 0;
	while (token == 0) token = generator();
	return token;
}

}// namespace

NetServer::NetServer(uniq<Transport> iTransport, SessionConfig iConfig)
	: mp_transport{std::move(iTransport)}, m_config{std::move(iConfig)} {}

NetServer::~NetServer() { stop(); }

auto NetServer::start(const uint16_t iPort) -> expected<void, NetworkError> {
	if (m_running) {
		OWL_CORE_WARN("NetServer: already running on port {}.", getPort())
		return unexpected{NetworkError::AlreadyOpen};
	}
	if (auto listening = mp_transport->listen(iPort, m_config.maxClients + g_SpareSlots); !listening) {
		OWL_CORE_WARN("NetServer: cannot listen on port {}.", iPort)
		return listening;
	}
	m_running = true;
	OWL_CORE_INFO("NetServer: '{}' listening on port {}.", m_config.gameId, getPort())
	return {};
}

void NetServer::stop() {
	if (!m_running)
		return;
	mp_transport->close();
	m_clients.clear();
	m_clientOfPeer.clear();
	m_pending.clear();
	m_running = false;
}

void NetServer::onUpdate(const core::Timestep& iTimeStep) {
	// Events raised between two updates (a kick) belong to the next one.
	m_events.erase(m_events.begin(), m_events.begin() + static_cast<std::ptrdiff_t>(m_reported));
	m_reported = 0;
	if (!m_running)
		return;
	m_transportEvents.clear();
	mp_transport->poll(m_transportEvents);
	for (auto& event: m_transportEvents) handle(event);

	const float elapsed = iTimeStep.getSeconds();
	std::vector<PeerId> silent;
	for (auto& [peer, pending]: m_pending) {
		pending.waitedFor += elapsed;
		if (pending.waitedFor >= m_config.handshakeTimeout)
			silent.push_back(peer);
	}
	for (const PeerId peer: silent) {
		OWL_CORE_INFO("NetServer: peer {} never greeted, closing.", peer)
		m_pending.erase(peer);
		mp_transport->disconnect(peer);
	}
	std::vector<ClientId> expired;
	for (auto& [id, client]: m_clients) {
		if (client.peer)
			continue;
		client.lostFor += elapsed;
		if (client.lostFor >= m_config.reconnectGrace)
			expired.push_back(id);
	}
	std::ranges::sort(expired);
	for (const ClientId id: expired) removeClient(id, LeaveReason::TimedOut);
	m_reported = m_events.size();
}

void NetServer::handle(TransportEvent& iEvent) {
	switch (iEvent.kind) {
		case TransportEvent::Kind::Connected:
			m_pending.emplace(iEvent.peer, Pending{});
			return;
		case TransportEvent::Kind::Disconnected:
			{
				if (m_pending.erase(iEvent.peer) > 0)
					return;
				const auto bound = m_clientOfPeer.find(iEvent.peer);
				if (bound == m_clientOfPeer.end())
					return;
				const ClientId id = bound->second;
				m_clientOfPeer.erase(bound);
				if (iEvent.reason == DisconnectReason::Timeout) {
					Client& client = m_clients.at(id);
					client.peer.reset();
					client.lostFor = 0.f;
					m_events.push_back({.kind = ServerEvent::Kind::ClientLost,
										.client = id,
										.channel = 0,
										.reason = LeaveReason::TimedOut,
										.payload = {}});
				} else {
					removeClient(id, LeaveReason::Left);
				}
				return;
			}
		case TransportEvent::Kind::Received:
			{
				if (m_pending.contains(iEvent.peer)) {
					handleHello(iEvent.peer, iEvent.payload);
					return;
				}
				const auto bound = m_clientOfPeer.find(iEvent.peer);
				if (bound == m_clientOfPeer.end() || iEvent.payload.empty() ||
					iEvent.payload.front() != static_cast<uint8_t>(protocol::MessageKind::Data))
					return;
				iEvent.payload.erase(iEvent.payload.begin());
				m_events.push_back({.kind = ServerEvent::Kind::Message,
									.client = bound->second,
									.channel = iEvent.channel,
									.reason = LeaveReason::Left,
									.payload = std::move(iEvent.payload)});
				return;
			}
	}
}

void NetServer::handleHello(const PeerId iPeer, const std::span<const uint8_t> iPayload) {
	const auto hello = protocol::decodeHello(iPayload);
	if (!hello || hello->protocol != g_SessionProtocolVersion) {
		reject(iPeer, RejectReason::WrongProtocol);
		return;
	}
	if (hello->gameId != m_config.gameId || hello->gameVersion != m_config.gameVersion) {
		reject(iPeer, RejectReason::WrongGame);
		return;
	}
	ClientId id = 0;
	bool resumed = false;
	if (hello->token != 0) {
		const auto known = std::ranges::find_if(
				m_clients, [&hello](const auto& iEntry) -> bool { return iEntry.second.token == hello->token; });
		if (known == m_clients.end()) {
			reject(iPeer, RejectReason::UnknownSession);
			return;
		}
		id = known->first;
		Client& client = known->second;
		if (client.peer) {
			// The client noticed the loss before the server: the old link is dead, drop it.
			m_clientOfPeer.erase(*client.peer);
			mp_transport->disconnect(*client.peer);
			m_events.push_back({.kind = ServerEvent::Kind::ClientLost,
								.client = id,
								.channel = 0,
								.reason = LeaveReason::TimedOut,
								.payload = {}});
		}
		client.peer = iPeer;
		client.lostFor = 0.f;
		resumed = true;
	} else {
		if (m_clients.size() >= m_config.maxClients) {
			reject(iPeer, RejectReason::ServerFull);
			return;
		}
		id = m_nextClient++;
		m_clients.emplace(id, Client{.token = makeToken(), .peer = iPeer, .lostFor = 0.f});
	}
	m_pending.erase(iPeer);
	m_clientOfPeer[iPeer] = id;
	const auto welcome = protocol::encodeWelcome(id, m_clients.at(id).token);
	if (!mp_transport->send(iPeer, 0, Delivery::Reliable, welcome))
		OWL_CORE_WARN("NetServer: cannot welcome client {}.", id)
	m_events.push_back({.kind = resumed ? ServerEvent::Kind::ClientResumed : ServerEvent::Kind::ClientJoined,
						.client = id,
						.channel = 0,
						.reason = LeaveReason::Left,
						.payload = {}});
}

void NetServer::reject(const PeerId iPeer, const RejectReason iReason) {
	OWL_CORE_INFO("NetServer: refusing peer {} ({}).", iPeer, static_cast<int>(iReason))
	m_pending.erase(iPeer);
	const auto message = protocol::encodeReason(protocol::MessageKind::Reject, static_cast<uint8_t>(iReason));
	if (!mp_transport->send(iPeer, 0, Delivery::Reliable, message))
		OWL_CORE_WARN("NetServer: cannot tell peer {} why it is refused.", iPeer)
	mp_transport->disconnect(iPeer);
}

void NetServer::removeClient(const ClientId iClient, const LeaveReason iReason) {
	if (m_clients.erase(iClient) == 0)
		return;
	m_events.push_back(
			{.kind = ServerEvent::Kind::ClientLeft, .client = iClient, .channel = 0, .reason = iReason, .payload = {}});
}

auto NetServer::send(const ClientId iClient, const uint8_t iChannel, const Delivery iDelivery,
					 const std::span<const uint8_t> iPayload) -> expected<void, NetworkError> {
	const auto found = m_clients.find(iClient);
	if (found == m_clients.end() || !found->second.peer) {
		OWL_CORE_WARN("NetServer: client {} is not connected.", iClient)
		return unexpected{NetworkError::UnknownPeer};
	}
	protocol::encodeData(iPayload, m_frame);
	return mp_transport->send(*found->second.peer, iChannel, iDelivery, m_frame);
}

void NetServer::broadcast(const uint8_t iChannel, const Delivery iDelivery, const std::span<const uint8_t> iPayload) {
	protocol::encodeData(iPayload, m_frame);
	for (const auto& client: m_clients | std::views::values)
		if (client.peer && !mp_transport->send(*client.peer, iChannel, iDelivery, m_frame))
			OWL_CORE_WARN("NetServer: broadcast missed peer {}.", *client.peer)
}

void NetServer::kick(const ClientId iClient) {
	const auto found = m_clients.find(iClient);
	if (found == m_clients.end())
		return;
	if (const auto peer = found->second.peer; peer) {
		const auto goodbye =
				protocol::encodeReason(protocol::MessageKind::Goodbye, static_cast<uint8_t>(LeaveReason::Kicked));
		if (!mp_transport->send(*peer, 0, Delivery::Reliable, goodbye))
			OWL_CORE_WARN("NetServer: cannot tell client {} it is kicked.", iClient)
		m_clientOfPeer.erase(*peer);
		mp_transport->disconnect(*peer);
	}
	removeClient(iClient, LeaveReason::Kicked);
}

auto NetServer::getClients() const -> std::vector<ClientId> {
	std::vector<ClientId> clients;
	for (const auto& [id, client]: m_clients)
		if (client.peer)
			clients.push_back(id);
	std::ranges::sort(clients);
	return clients;
}

auto NetServer::isConnected(const ClientId iClient) const -> bool {
	const auto found = m_clients.find(iClient);
	return found != m_clients.end() && found->second.peer.has_value();
}

auto NetServer::getStats(const ClientId iClient) const -> expected<PeerStats, NetworkError> {
	const auto found = m_clients.find(iClient);
	if (found == m_clients.end() || !found->second.peer) {
		OWL_CORE_WARN("NetServer: client {} is not connected.", iClient)
		return unexpected{NetworkError::UnknownPeer};
	}
	return mp_transport->getStats(*found->second.peer);
}

}// namespace owl::network
