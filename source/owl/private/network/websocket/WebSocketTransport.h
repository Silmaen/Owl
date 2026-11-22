/**
 * @file WebSocketTransport.h
 * @author Silmaen
 * @date 19/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "network/Transport.h"

#include <cstdint>
#include <deque>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

namespace owl::network::websocket {

/**
 * @brief
 *  Transport over WebSocket, the only one a browser has: binary messages on `ws://<host>:<port>/owl`, each one its
 *  channel byte then its payload. TCP delivers every message once and in order, the unreliable ones too.
 *
 *  A desktop listens (cpp-httplib's server, a thread per peer) and connects (its client); a page only connects
 *  (Emscripten's WebSocket API). The events of the threads queue up until `poll`.
 */
class WebSocketTransport final : public Transport {
public:
	/**
	 * @brief
	 *  Constructor.
	 */
	WebSocketTransport();

	WebSocketTransport(const WebSocketTransport&) = delete;
	WebSocketTransport(WebSocketTransport&&) = delete;
	auto operator=(const WebSocketTransport&) -> WebSocketTransport& = delete;
	auto operator=(WebSocketTransport&&) -> WebSocketTransport& = delete;

	/**
	 * @brief
	 *  Destructor: closes every connection.
	 */
	~WebSocketTransport() override;

	/**
	 * @brief
	 *  Accepts connections on a port (desktop only).
	 * @param[in] iPort The port, 0 for any.
	 * @param[in] iMaxPeers Peers accepted at the same time.
	 * @return Nothing, or the error.
	 */
	auto listen(uint16_t iPort, uint32_t iMaxPeers) -> expected<void, NetworkError> override;

	/**
	 * @brief
	 *  Starts a connection to a server; `Connected` follows once it opens.
	 * @param[in] iHost Host name or address.
	 * @param[in] iPort Its port.
	 * @return The peer of the server, or the error.
	 */
	auto connect(const std::string& iHost, uint16_t iPort) -> expected<PeerId, NetworkError> override;

	/**
	 * @brief
	 *  Closes the connection of a peer.
	 * @param[in] iPeer The peer.
	 */
	void disconnect(PeerId iPeer) override;

	/**
	 * @brief
	 *  Closes every connection and stops listening.
	 */
	void close() override;

	/**
	 * @brief
	 *  Sends a message to a peer.
	 * @param[in] iPeer The peer.
	 * @param[in] iChannel Its channel.
	 * @param[in] iDelivery Unused: the stream is reliable.
	 * @param[in] iPayload The bytes.
	 * @return Nothing, or the error.
	 */
	auto send(PeerId iPeer, uint8_t iChannel, Delivery iDelivery, std::span<const uint8_t> iPayload)
			-> expected<void, NetworkError> override;

	/**
	 * @brief
	 *  Moves the events queued since the last poll.
	 * @param[out] oEvents The events, appended.
	 */
	void poll(std::vector<TransportEvent>& oEvents) override;

	/**
	 * @brief
	 *  Whether the transport listens or connects.
	 * @return True when open.
	 */
	[[nodiscard]] auto isOpen() const -> bool override;

	/**
	 * @brief
	 *  Port listened on.
	 * @return The port, 0 when not listening.
	 */
	[[nodiscard]] auto getPort() const -> uint16_t override;

	/**
	 * @brief
	 *  The connected peers.
	 * @return Their identifiers.
	 */
	[[nodiscard]] auto getPeers() const -> std::vector<PeerId> override;

	/**
	 * @brief
	 *  Link statistics of a peer: the bytes only, TCP hides the round trips and the losses.
	 * @param[in] iPeer The peer.
	 * @return The statistics, or the error.
	 */
	[[nodiscard]] auto getStats(PeerId iPeer) const -> expected<PeerStats, NetworkError> override;

	/// Path of the WebSocket endpoint.
	static constexpr std::string_view g_path = "/owl";

	/// What the backend keeps of one connection (`WebSocketTransport.cpp`).
	struct Link;
	/// The listening server of the backend (`WebSocketTransport.cpp`).
	struct Listener;

	/**
	 * @brief
	 *  Queues an event of a connection, from any thread.
	 * @param[in] iEvent The event.
	 */
	void push(TransportEvent iEvent);

	/**
	 * @brief
	 *  Counts the bytes received from a peer, from any thread.
	 * @param[in] iPeer The peer.
	 * @param[in] iBytes The bytes.
	 */
	void countReceived(PeerId iPeer, uint64_t iBytes);

	/**
	 * @brief
	 *  Forgets a peer whose connection ended, from any thread.
	 * @param[in] iPeer The peer.
	 * @param[in] iReason Why it ended.
	 */
	void drop(PeerId iPeer, DisconnectReason iReason);

	/**
	 * @brief
	 *  Registers a connection.
	 * @param[in] iLink The connection.
	 * @return Its peer identifier.
	 */
	auto track(shared<Link> iLink) -> PeerId;

private:
	/// Guards the queue, the peers and the counters.
	mutable std::mutex m_mutex;
	/// Events not polled yet.
	std::deque<TransportEvent> m_events;
	/// Connections by peer.
	std::unordered_map<PeerId, shared<Link>> m_links;
	/// Next peer identifier.
	PeerId m_nextPeer = 1;
	/// The client connections, joined by close() (desktop).
	std::vector<shared<Link>> m_clients;
	/// The server, null when not listening.
	uniq<Listener> m_listener;
	/// Port listened on.
	uint16_t m_port = 0;
	/// Whether the transport connected out.
	bool m_connecting = false;
};

}// namespace owl::network::websocket
