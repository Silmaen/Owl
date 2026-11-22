/**
 * @file MixedTransport.h
 * @author Silmaen
 * @date 23/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Transport.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace owl::network {

/**
 * @brief
 *  Several transports seen as one: a server listening on each of them on the same port number, so that desktop
 *  clients (ENet, UDP) and Web clients (WebSocket, TCP) join one match.
 *
 * Each peer of a backend gets an identifier of its own here; `connect` goes through the first backend.
 */
class OWL_API MixedTransport final : public Transport {
public:
	/**
	 * @brief
	 *  Gather transports; none of them may be open yet.
	 * @param[in] iTransports The backends, the first one used by `connect` and choosing the port.
	 */
	explicit MixedTransport(std::vector<uniq<Transport>> iTransports);

	MixedTransport(const MixedTransport&) = delete;

	MixedTransport(MixedTransport&&) = delete;

	auto operator=(const MixedTransport&) -> MixedTransport& = delete;

	auto operator=(MixedTransport&&) -> MixedTransport& = delete;

	/**
	 * @brief
	 *  Destructor, closes every backend.
	 */
	~MixedTransport() override;

	/**
	 * @brief
	 *  Get the number of backends.
	 * @return The number of backends.
	 */
	[[nodiscard]] auto getTransportCount() const -> size_t { return m_transports.size(); }

	/**
	 * @brief
	 *  Get the backend a peer came through.
	 * @param[in] iPeer The peer.
	 * @return The type of its backend, or an error when the peer is unknown.
	 */
	[[nodiscard]] auto getPeerType(PeerId iPeer) const -> expected<Type, NetworkError>;

	/**
	 * @brief
	 *  Listen on the port with the first backend, then on the port it got with the others.
	 * @param[in] iPort The port, 0 for one chosen by the first backend.
	 * @param[in] iMaxPeers The maximum number of peers of each backend.
	 * @return Nothing, or why a backend could not listen (every backend closed then).
	 */
	auto listen(uint16_t iPort, uint32_t iMaxPeers) -> expected<void, NetworkError> override;

	/**
	 * @brief
	 *  Start connecting to a listener through the first backend.
	 * @param[in] iHost The host name or address of the listener.
	 * @param[in] iPort The port of the listener.
	 * @return The peer standing for the listener, or why the connection could not start.
	 */
	auto connect(const std::string& iHost, uint16_t iPort) -> expected<PeerId, NetworkError> override;

	/**
	 * @brief
	 *  Close the link with a peer; it is forgotten with its `Disconnected` event.
	 * @param[in] iPeer The peer.
	 */
	void disconnect(PeerId iPeer) override;

	/**
	 * @brief
	 *  Close every backend and forget the peers.
	 */
	void close() override;

	/**
	 * @brief
	 *  Send a message to a peer through its backend.
	 * @param[in] iPeer The peer.
	 * @param[in] iChannel The channel, below `g_ChannelCount`.
	 * @param[in] iDelivery How the message travels.
	 * @param[in] iPayload The bytes of the message.
	 * @return Nothing, or why the message was not accepted.
	 */
	auto send(PeerId iPeer, uint8_t iChannel, Delivery iDelivery, std::span<const uint8_t> iPayload)
			-> expected<void, NetworkError> override;

	/**
	 * @brief
	 *  Collect the events of every backend, their peers renamed.
	 * @param[out] oEvents Receives the events, appended backend after backend.
	 */
	void poll(std::vector<TransportEvent>& oEvents) override;

	/**
	 * @brief
	 *  Check whether a backend listens or connects.
	 * @return True when one is open.
	 */
	[[nodiscard]] auto isOpen() const -> bool override;

	/**
	 * @brief
	 *  Get the local port every backend listens on.
	 * @return The port, 0 when not listening.
	 */
	[[nodiscard]] auto getPort() const -> uint16_t override;

	/**
	 * @brief
	 *  Get the identifiers of the connected peers of every backend.
	 * @return The peers, in no particular order.
	 */
	[[nodiscard]] auto getPeers() const -> std::vector<PeerId> override;

	/**
	 * @brief
	 *  Get the statistics of a peer from its backend.
	 * @param[in] iPeer The peer.
	 * @return The statistics, or an error when the peer is unknown.
	 */
	[[nodiscard]] auto getStats(PeerId iPeer) const -> expected<PeerStats, NetworkError> override;

private:
	/// Where a peer lives.
	struct Route {
		/// Index of its backend.
		size_t transport = 0;
		/// Its identifier in the backend.
		PeerId inner = 0;
	};

	/**
	 * @brief
	 *  Get the identifier of a backend's peer, given one when it has none yet.
	 * @param[in] iTransport Index of the backend.
	 * @param[in] iInner The identifier in the backend.
	 * @return The identifier here.
	 */
	auto outerPeer(size_t iTransport, PeerId iInner) -> PeerId;

	/**
	 * @brief
	 *  Forget a peer.
	 * @param[in] iPeer The identifier here.
	 */
	void forget(PeerId iPeer);

	/// The backends.
	std::vector<uniq<Transport>> m_transports;
	/// Backend and inner identifier of each peer.
	std::unordered_map<PeerId, Route> m_routes;
	/// Per backend, the identifier here of each of its peers.
	std::vector<std::unordered_map<PeerId, PeerId>> m_outer;
	/// Next identifier handed out.
	PeerId m_nextPeer = 1;
	/// Events of one backend during a poll.
	std::vector<TransportEvent> m_backendEvents;
};

}// namespace owl::network
