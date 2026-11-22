/**
 * @file MixedTransport.cpp
 * @author Silmaen
 * @date 23/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "network/MixedTransport.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace owl::network {

MixedTransport::MixedTransport(std::vector<uniq<Transport>> iTransports)
	: Transport{Type::Mixed}, m_transports{std::move(iTransports)}, m_outer(m_transports.size()) {}

MixedTransport::~MixedTransport() { close(); }

auto MixedTransport::getPeerType(const PeerId iPeer) const -> expected<Type, NetworkError> {
	const auto found = m_routes.find(iPeer);
	if (found == m_routes.end()) {
		OWL_CORE_WARN("MixedTransport: no peer {}.", iPeer)
		return unexpected{NetworkError::UnknownPeer};
	}
	return m_transports[found->second.transport]->getType();
}

auto MixedTransport::listen(const uint16_t iPort, const uint32_t iMaxPeers) -> expected<void, NetworkError> {
	if (m_transports.empty()) {
		OWL_CORE_WARN("MixedTransport: no backend to listen with.")
		return unexpected{NetworkError::BackendUnavailable};
	}
	if (isOpen()) {
		OWL_CORE_WARN("MixedTransport: already open.")
		return unexpected{NetworkError::AlreadyOpen};
	}
	uint16_t port = iPort;
	for (const auto& transport: m_transports) {
		if (auto listening = transport->listen(port, iMaxPeers); !listening) {
			OWL_CORE_WARN("MixedTransport: {} cannot listen on port {}.", magic_enum::enum_name(transport->getType()),
						  port)
			close();
			return unexpected{listening.error()};
		}
		port = transport->getPort();
	}
	return {};
}

auto MixedTransport::connect(const std::string& iHost, const uint16_t iPort) -> expected<PeerId, NetworkError> {
	if (m_transports.empty()) {
		OWL_CORE_WARN("MixedTransport: no backend to connect with.")
		return unexpected{NetworkError::BackendUnavailable};
	}
	const auto inner = m_transports.front()->connect(iHost, iPort);
	if (!inner)
		return unexpected{inner.error()};
	return outerPeer(0, *inner);
}

void MixedTransport::disconnect(const PeerId iPeer) {
	const auto found = m_routes.find(iPeer);
	if (found != m_routes.end())
		m_transports[found->second.transport]->disconnect(found->second.inner);
}

void MixedTransport::close() {
	for (const auto& transport: m_transports) transport->close();
	m_routes.clear();
	for (auto& peers: m_outer) peers.clear();
}

auto MixedTransport::send(const PeerId iPeer, const uint8_t iChannel, const Delivery iDelivery,
						  const std::span<const uint8_t> iPayload) -> expected<void, NetworkError> {
	const auto found = m_routes.find(iPeer);
	if (found == m_routes.end()) {
		OWL_CORE_WARN("MixedTransport: no peer {}.", iPeer)
		return unexpected{NetworkError::UnknownPeer};
	}
	return m_transports[found->second.transport]->send(found->second.inner, iChannel, iDelivery, iPayload);
}

void MixedTransport::poll(std::vector<TransportEvent>& oEvents) {
	for (size_t index = 0; index < m_transports.size(); ++index) {
		m_backendEvents.clear();
		m_transports[index]->poll(m_backendEvents);
		for (auto& event: m_backendEvents) {
			event.peer = outerPeer(index, event.peer);
			if (event.kind == TransportEvent::Kind::Disconnected)
				forget(event.peer);
			oEvents.push_back(std::move(event));
		}
	}
}

auto MixedTransport::isOpen() const -> bool {
	return std::ranges::any_of(m_transports, [](const auto& iTransport) -> bool { return iTransport->isOpen(); });
}

auto MixedTransport::getPort() const -> uint16_t {
	return m_transports.empty() ? uint16_t{0} : m_transports.front()->getPort();
}

auto MixedTransport::getPeers() const -> std::vector<PeerId> {
	std::vector<PeerId> peers;
	for (size_t index = 0; index < m_transports.size(); ++index) {
		for (const PeerId inner: m_transports[index]->getPeers()) {
			if (const auto found = m_outer[index].find(inner); found != m_outer[index].end())
				peers.push_back(found->second);
		}
	}
	return peers;
}

auto MixedTransport::getStats(const PeerId iPeer) const -> expected<PeerStats, NetworkError> {
	const auto found = m_routes.find(iPeer);
	if (found == m_routes.end()) {
		OWL_CORE_WARN("MixedTransport: no peer {}.", iPeer)
		return unexpected{NetworkError::UnknownPeer};
	}
	return m_transports[found->second.transport]->getStats(found->second.inner);
}

auto MixedTransport::outerPeer(const size_t iTransport, const PeerId iInner) -> PeerId {
	auto& peers = m_outer[iTransport];
	if (const auto found = peers.find(iInner); found != peers.end())
		return found->second;
	const PeerId peer = m_nextPeer++;
	peers.emplace(iInner, peer);
	m_routes.emplace(peer, Route{.transport = iTransport, .inner = iInner});
	return peer;
}

void MixedTransport::forget(const PeerId iPeer) {
	const auto found = m_routes.find(iPeer);
	if (found == m_routes.end())
		return;
	m_outer[found->second.transport].erase(found->second.inner);
	m_routes.erase(found);
}

}// namespace owl::network
