/**
 * @file NetServer.h
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Session.h"
#include "Transport.h"

#include "core/Timestep.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

namespace owl::network {

/// Something that happened on a server during its last update.
struct ServerEvent {
	/// Kinds of server events.
	enum struct Kind : uint8_t {
		ClientJoined,///< A client completed its handshake.
		ClientLost,///< The link of a client was lost; its place is kept for the grace time.
		ClientResumed,///< A lost client reconnected within the grace time.
		ClientLeft,///< A client is gone for good, see `reason`.
		Message,///< A client sent a message, see `channel` and `payload`.
	};
	/// What happened.
	Kind kind = Kind::Message;
	/// The client concerned.
	ClientId client = 0;
	/// Channel of a message.
	uint8_t channel = 0;
	/// Why the client left, for `Kind::ClientLeft`.
	LeaveReason reason = LeaveReason::Left;
	/// Bytes of a message.
	std::vector<uint8_t> payload;
};

/**
 * @brief
 *  An authoritative server: it accepts the clients of its game on a transport, names each one by a `ClientId` that
 *  survives a reconnection, and exchanges messages with them. Clients only talk to the server.
 */
class OWL_API NetServer final {
public:
	/**
	 * @brief
	 *  Create a stopped server.
	 * @param[in] iTransport The transport it listens on.
	 * @param[in] iConfig What its clients must match.
	 */
	NetServer(uniq<Transport> iTransport, SessionConfig iConfig);

	NetServer(const NetServer&) = delete;

	NetServer(NetServer&&) = delete;

	auto operator=(const NetServer&) -> NetServer& = delete;

	auto operator=(NetServer&&) -> NetServer& = delete;

	/**
	 * @brief
	 *  Destructor, stops the server.
	 */
	~NetServer();

	/**
	 * @brief
	 *  Start accepting clients.
	 * @param[in] iPort The port, 0 for one chosen by the system (see `getPort()`).
	 * @return Nothing, or why the transport could not listen.
	 */
	auto start(uint16_t iPort) -> expected<void, NetworkError>;

	/**
	 * @brief
	 *  Close every session and stop listening.
	 */
	void stop();

	/**
	 * @brief
	 *  Poll the transport, run the handshakes and the grace timers; the events of this update replace the previous ones.
	 * @param[in] iTimeStep Time elapsed since the last update.
	 */
	void onUpdate(const core::Timestep& iTimeStep);

	/**
	 * @brief
	 *  Get the events of the last update.
	 * @return The events, in the order they happened.
	 */
	[[nodiscard]] auto getEvents() const -> std::span<const ServerEvent> {
		return std::span<const ServerEvent>{m_events}.first(m_reported);
	}

	/**
	 * @brief
	 *  Send a message to a connected client.
	 * @param[in] iClient The client.
	 * @param[in] iChannel The channel, below `g_ChannelCount`.
	 * @param[in] iDelivery How the message travels.
	 * @param[in] iPayload The bytes of the message.
	 * @return Nothing, or `UnknownPeer` when the client is unknown or lost.
	 */
	auto send(ClientId iClient, uint8_t iChannel, Delivery iDelivery, std::span<const uint8_t> iPayload)
			-> expected<void, NetworkError>;

	/**
	 * @brief
	 *  Send a message to every connected client.
	 * @param[in] iChannel The channel, below `g_ChannelCount`.
	 * @param[in] iDelivery How the message travels.
	 * @param[in] iPayload The bytes of the message.
	 */
	void broadcast(uint8_t iChannel, Delivery iDelivery, std::span<const uint8_t> iPayload);

	/**
	 * @brief
	 *  Remove a client; the next update reports a `ClientLeft` event with the reason `Kicked`.
	 * @param[in] iClient The client.
	 */
	void kick(ClientId iClient);

	/**
	 * @brief
	 *  Check whether the server accepts clients.
	 * @return True once started.
	 */
	[[nodiscard]] auto isRunning() const -> bool { return m_running; }

	/**
	 * @brief
	 *  Get the port the server listens on.
	 * @return The port, 0 when stopped.
	 */
	[[nodiscard]] auto getPort() const -> uint16_t { return mp_transport->getPort(); }

	/**
	 * @brief
	 *  Get the clients whose link is up.
	 * @return The clients, by increasing identifier.
	 */
	[[nodiscard]] auto getClients() const -> std::vector<ClientId>;

	/**
	 * @brief
	 *  Check whether a client's link is up.
	 * @param[in] iClient The client.
	 * @return True when connected, false when unknown or lost.
	 */
	[[nodiscard]] auto isConnected(ClientId iClient) const -> bool;

	/**
	 * @brief
	 *  Get the link statistics of a connected client.
	 * @param[in] iClient The client.
	 * @return The statistics, or an error when the client is unknown or lost.
	 */
	[[nodiscard]] auto getStats(ClientId iClient) const -> expected<PeerStats, NetworkError>;

	/**
	 * @brief
	 *  Get the transport, to inspect or simulate its links.
	 * @return The transport.
	 */
	[[nodiscard]] auto getTransport() -> Transport& { return *mp_transport; }

private:
	/// A client known to the server.
	struct Client {
		/// Secret the client presents to reconnect.
		uint64_t token = 0;
		/// Its transport peer, none while lost.
		std::optional<PeerId> peer;
		/// Time since the link was lost, in seconds.
		float lostFor = 0.f;
	};

	/// A transport peer that has not greeted the server yet.
	struct Pending {
		/// Time since it connected, in seconds.
		float waitedFor = 0.f;
	};

	/**
	 * @brief
	 *  Handle one transport event.
	 * @param[in] iEvent The event.
	 */
	void handle(TransportEvent& iEvent);

	/**
	 * @brief
	 *  Handle the greeting of a pending peer.
	 * @param[in] iPeer The peer.
	 * @param[in] iPayload The bytes of the greeting.
	 */
	void handleHello(PeerId iPeer, std::span<const uint8_t> iPayload);

	/**
	 * @brief
	 *  Refuse a peer and close its link.
	 * @param[in] iPeer The peer.
	 * @param[in] iReason Why.
	 */
	void reject(PeerId iPeer, RejectReason iReason);

	/**
	 * @brief
	 *  Forget a client and report it gone.
	 * @param[in] iClient The client.
	 * @param[in] iReason Why it left.
	 */
	void removeClient(ClientId iClient, LeaveReason iReason);

	/// The transport.
	uniq<Transport> mp_transport;
	/// What clients must match.
	SessionConfig m_config;
	/// Whether the server accepts clients.
	bool m_running = false;
	/// Next client identifier.
	ClientId m_nextClient = 1;
	/// Clients by identifier.
	std::unordered_map<ClientId, Client> m_clients;
	/// Client of each greeted transport peer.
	std::unordered_map<PeerId, ClientId> m_clientOfPeer;
	/// Peers that have not greeted yet.
	std::unordered_map<PeerId, Pending> m_pending;
	/// Transport events of the current update, reused.
	std::vector<TransportEvent> m_transportEvents;
	/// Framed outgoing game message, reused.
	std::vector<uint8_t> m_frame;
	/// Events of the last update, then the ones raised since (a kick).
	std::vector<ServerEvent> m_events;
	/// Number of events already reported by the last update.
	size_t m_reported = 0;
};

}// namespace owl::network
