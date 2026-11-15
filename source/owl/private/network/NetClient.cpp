/**
 * @file NetClient.cpp
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "network/NetClient.h"

#include "SessionProtocol.h"

#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace owl::network {

NetClient::NetClient(uniq<Transport> iTransport, SessionConfig iConfig)
	: mp_transport{std::move(iTransport)}, m_config{std::move(iConfig)} {}

NetClient::~NetClient() {
	disconnect();
	mp_transport->close();
}

auto NetClient::connect(const std::string& iHost, const uint16_t iPort) -> expected<void, NetworkError> {
	if (m_state != State::Disconnected) {
		OWL_CORE_WARN("NetClient: already in a session.")
		return unexpected{NetworkError::AlreadyOpen};
	}
	m_host = iHost;
	m_port = iPort;
	m_token = 0;
	m_clientId.reset();
	auto server = mp_transport->connect(m_host, m_port);
	if (!server) {
		OWL_CORE_WARN("NetClient: cannot reach {}:{}.", iHost, iPort)
		return unexpected{server.error()};
	}
	m_server = *server;
	m_state = State::Connecting;
	m_handshakeFor = 0.f;
	return {};
}

void NetClient::disconnect() {
	if (m_state == State::Disconnected)
		return;
	if (m_server)
		mp_transport->disconnect(*m_server);
	m_server.reset();
	m_state = State::Disconnected;
	m_clientId.reset();
	m_token = 0;
}

auto NetClient::attempt() -> bool {
	auto server = mp_transport->connect(m_host, m_port);
	m_sinceAttempt = 0.f;
	if (!server) {
		OWL_CORE_WARN("NetClient: reconnection to {}:{} could not start.", m_host, m_port)
		return false;
	}
	m_server = *server;
	m_handshakeFor = 0.f;
	return true;
}

void NetClient::end(const LeaveReason iReason, const RejectReason iReject) {
	if (m_server)
		mp_transport->disconnect(*m_server);
	m_server.reset();
	m_state = State::Disconnected;
	m_clientId.reset();
	m_token = 0;
	m_events.push_back(
			{.kind = ClientEvent::Kind::Left, .channel = 0, .reason = iReason, .rejectReason = iReject, .payload = {}});
}

void NetClient::onUpdate(const core::Timestep& iTimeStep) {
	m_events.clear();
	if (m_state == State::Disconnected)
		return;
	m_transportEvents.clear();
	mp_transport->poll(m_transportEvents);
	for (auto& event: m_transportEvents) {
		handle(event);
		if (m_state == State::Disconnected)
			return;
	}

	const float elapsed = iTimeStep.getSeconds();
	if (m_state == State::Handshaking) {
		m_handshakeFor += elapsed;
		if (m_handshakeFor >= m_config.handshakeTimeout) {
			OWL_CORE_WARN("NetClient: the server did not answer the greeting.")
			end(LeaveReason::Unreachable);
		}
		return;
	}
	if (m_state != State::Reconnecting)
		return;
	m_lostFor += elapsed;
	m_sinceAttempt += elapsed;
	if (m_lostFor >= m_config.reconnectGrace) {
		OWL_CORE_WARN("NetClient: could not reconnect to {}:{} in time.", m_host, m_port)
		end(LeaveReason::TimedOut);
		return;
	}
	if (!m_server && m_sinceAttempt >= m_config.reconnectInterval)
		static_cast<void>(attempt());
}

void NetClient::handle(TransportEvent& iEvent) {
	if (!m_server || iEvent.peer != *m_server)
		return;
	switch (iEvent.kind) {
		case TransportEvent::Kind::Connected:
			{
				const auto hello = protocol::encodeHello({.protocol = g_SessionProtocolVersion,
														  .gameId = m_config.gameId,
														  .gameVersion = m_config.gameVersion,
														  .token = m_token});
				if (!mp_transport->send(*m_server, 0, Delivery::Reliable, hello)) {
					end(LeaveReason::Unreachable);
					return;
				}
				if (m_state == State::Connecting)
					m_state = State::Handshaking;
				m_handshakeFor = 0.f;
				return;
			}
		case TransportEvent::Kind::Disconnected:
			m_server.reset();
			if (m_state == State::Reconnecting)
				return;// The attempt failed; the next one comes after the interval.
			if (m_state == State::Connected && iEvent.reason == DisconnectReason::Timeout) {
				m_state = State::Reconnecting;
				m_lostFor = 0.f;
				m_sinceAttempt = 0.f;
				m_events.push_back({.kind = ClientEvent::Kind::Lost,
									.channel = 0,
									.reason = LeaveReason::TimedOut,
									.rejectReason = RejectReason::WrongProtocol,
									.payload = {}});
				return;
			}
			end(m_state == State::Connected ? LeaveReason::Left : LeaveReason::Unreachable);
			return;
		case TransportEvent::Kind::Received:
			handleServerMessage(iEvent.channel, iEvent.payload);
			return;
	}
}

void NetClient::handleServerMessage(const uint8_t iChannel, std::vector<uint8_t>& ioPayload) {
	ByteReader reader{ioPayload};
	const auto kind = reader.readU8();
	if (!kind)
		return;
	switch (static_cast<protocol::MessageKind>(*kind)) {
		case protocol::MessageKind::Welcome:
			{
				const auto id = reader.readU32();
				const auto token = reader.readU64();
				if (!id || !token)
					return;
				const bool resumed = m_state == State::Reconnecting && m_clientId == *id;
				m_clientId = id;
				m_token = *token;
				m_state = State::Connected;
				m_events.push_back({.kind = resumed ? ClientEvent::Kind::Resumed : ClientEvent::Kind::Joined,
									.channel = 0,
									.reason = LeaveReason::Left,
									.rejectReason = RejectReason::WrongProtocol,
									.payload = {}});
				return;
			}
		case protocol::MessageKind::Reject:
			{
				const auto reason = reader.readU8().value_or(0);
				OWL_CORE_WARN("NetClient: refused by the server ({}).", reason)
				end(LeaveReason::Rejected, static_cast<RejectReason>(reason));
				return;
			}
		case protocol::MessageKind::Goodbye:
			end(static_cast<LeaveReason>(reader.readU8().value_or(0)));
			return;
		case protocol::MessageKind::Data:
			if (m_state != State::Connected)
				return;
			ioPayload.erase(ioPayload.begin());
			m_events.push_back({.kind = ClientEvent::Kind::Message,
								.channel = iChannel,
								.reason = LeaveReason::Left,
								.rejectReason = RejectReason::WrongProtocol,
								.payload = std::move(ioPayload)});
			return;
		case protocol::MessageKind::Hello:
			return;
	}
}

auto NetClient::send(const uint8_t iChannel, const Delivery iDelivery, const std::span<const uint8_t> iPayload)
		-> expected<void, NetworkError> {
	if (m_state != State::Connected || !m_server) {
		OWL_CORE_WARN("NetClient: not in a session.")
		return unexpected{NetworkError::NotOpen};
	}
	protocol::encodeData(iPayload, m_frame);
	return mp_transport->send(*m_server, iChannel, iDelivery, m_frame);
}

auto NetClient::getStats() const -> expected<PeerStats, NetworkError> {
	if (!m_server) {
		OWL_CORE_WARN("NetClient: not linked to a server.")
		return unexpected{NetworkError::NotOpen};
	}
	return mp_transport->getStats(*m_server);
}

}// namespace owl::network
