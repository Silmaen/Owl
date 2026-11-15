/**
 * @file NetClient.h
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Session.h"
#include "Transport.h"

#include "core/Timestep.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace owl::network {

/// Something that happened on a client during its last update.
struct ClientEvent {
	/// Kinds of client events.
	enum struct Kind : uint8_t {
		Joined,///< The server accepted the client, see `NetClient::getClientId()`.
		Lost,///< The link was lost; the client tries to reconnect for the grace time.
		Resumed,///< The client reconnected and kept its place.
		Left,///< The session is over, see `reason`.
		Message,///< The server sent a message, see `channel` and `payload`.
	};
	/// What happened.
	Kind kind = Kind::Message;
	/// Channel of a message.
	uint8_t channel = 0;
	/// Why the session ended, for `Kind::Left`.
	LeaveReason reason = LeaveReason::Left;
	/// Why the server refused the client, for `Kind::Left` with the reason `Rejected`.
	RejectReason rejectReason = RejectReason::WrongProtocol;
	/// Bytes of a message.
	std::vector<uint8_t> payload;
};

/**
 * @brief
 *  A client of a `NetServer`: it connects, greets the server with its game and version, and reconnects on its own
 *  when the link is lost, keeping its `ClientId`.
 */
class OWL_API NetClient final {
public:
	/// Client states.
	enum struct State : uint8_t {
		Disconnected,///< No session.
		Connecting,///< Waiting for the link to the server.
		Handshaking,///< Link up, waiting for the server's answer.
		Connected,///< In session.
		Reconnecting,///< Link lost, trying to restore it within the grace time.
	};

	/**
	 * @brief
	 *  Create a disconnected client.
	 * @param[in] iTransport The transport it connects through.
	 * @param[in] iConfig What the server must match.
	 */
	NetClient(uniq<Transport> iTransport, SessionConfig iConfig);

	NetClient(const NetClient&) = delete;

	NetClient(NetClient&&) = delete;

	auto operator=(const NetClient&) -> NetClient& = delete;

	auto operator=(NetClient&&) -> NetClient& = delete;

	/**
	 * @brief
	 *  Destructor, leaves the session.
	 */
	~NetClient();

	/**
	 * @brief
	 *  Start joining a server; `Joined` or `Left` follows in an update.
	 * @param[in] iHost The host name or address of the server.
	 * @param[in] iPort The port of the server.
	 * @return Nothing, or why the connection could not start.
	 */
	auto connect(const std::string& iHost, uint16_t iPort) -> expected<void, NetworkError>;

	/**
	 * @brief
	 *  Leave the session; the server sees the client leave.
	 */
	void disconnect();

	/**
	 * @brief
	 *  Poll the transport, run the handshake and the reconnection; the events of this update replace the previous ones.
	 * @param[in] iTimeStep Time elapsed since the last update.
	 */
	void onUpdate(const core::Timestep& iTimeStep);

	/**
	 * @brief
	 *  Get the events of the last update.
	 * @return The events, in the order they happened.
	 */
	[[nodiscard]] auto getEvents() const -> std::span<const ClientEvent> { return m_events; }

	/**
	 * @brief
	 *  Send a message to the server.
	 * @param[in] iChannel The channel, below `g_ChannelCount`.
	 * @param[in] iDelivery How the message travels.
	 * @param[in] iPayload The bytes of the message.
	 * @return Nothing, or `NotOpen` outside a session.
	 */
	auto send(uint8_t iChannel, Delivery iDelivery, std::span<const uint8_t> iPayload) -> expected<void, NetworkError>;

	/**
	 * @brief
	 *  Get the state of the client.
	 * @return The state.
	 */
	[[nodiscard]] auto getState() const -> State { return m_state; }

	/**
	 * @brief
	 *  Get the identifier the server gave this client.
	 * @return The identifier, none before the first `Joined`.
	 */
	[[nodiscard]] auto getClientId() const -> std::optional<ClientId> { return m_clientId; }

	/**
	 * @brief
	 *  Get the link statistics with the server.
	 * @return The statistics, or an error outside a session.
	 */
	[[nodiscard]] auto getStats() const -> expected<PeerStats, NetworkError>;

	/**
	 * @brief
	 *  Get the transport, to inspect or simulate its link.
	 * @return The transport.
	 */
	[[nodiscard]] auto getTransport() -> Transport& { return *mp_transport; }

private:
	/**
	 * @brief
	 *  Handle one transport event.
	 * @param[in] iEvent The event.
	 */
	void handle(TransportEvent& iEvent);

	/**
	 * @brief
	 *  Handle a session message of the server.
	 * @param[in] iChannel The channel it came on.
	 * @param[in,out] ioPayload The bytes of the message, moved into a `Message` event.
	 */
	void handleServerMessage(uint8_t iChannel, std::vector<uint8_t>& ioPayload);

	/**
	 * @brief
	 *  Start one connection attempt to the server.
	 * @return True when the attempt started.
	 */
	auto attempt() -> bool;

	/**
	 * @brief
	 *  End the session and report it.
	 * @param[in] iReason Why.
	 * @param[in] iReject Why the server refused, for `LeaveReason::Rejected`.
	 */
	void end(LeaveReason iReason, RejectReason iReject = RejectReason::WrongProtocol);

	/// The transport.
	uniq<Transport> mp_transport;
	/// What the server must match.
	SessionConfig m_config;
	/// The state.
	State m_state = State::Disconnected;
	/// Host of the server.
	std::string m_host;
	/// Port of the server.
	uint16_t m_port = 0;
	/// Transport peer of the server, none between attempts.
	std::optional<PeerId> m_server;
	/// Identifier given by the server.
	std::optional<ClientId> m_clientId;
	/// Secret to reconnect.
	uint64_t m_token = 0;
	/// Time since the link was lost, in seconds.
	float m_lostFor = 0.f;
	/// Time since the last reconnection attempt, in seconds.
	float m_sinceAttempt = 0.f;
	/// Time spent in the handshake, in seconds.
	float m_handshakeFor = 0.f;
	/// Transport events of the current update, reused.
	std::vector<TransportEvent> m_transportEvents;
	/// Framed outgoing game message, reused.
	std::vector<uint8_t> m_frame;
	/// Events of the last update.
	std::vector<ClientEvent> m_events;
};

}// namespace owl::network
