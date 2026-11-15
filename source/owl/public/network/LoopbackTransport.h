/**
 * @file LoopbackTransport.h
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Transport.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace owl::network {

/**
 * @brief
 *  A transport linking the transports of one process, without sockets: what one sends, its peer receives at its next
 *  poll. Ports name the in-process listeners; every message arrives, in order.
 */
class OWL_API LoopbackTransport final : public Transport {
public:
	/**
	 * @brief
	 *  Create a closed loopback transport.
	 */
	LoopbackTransport();

	LoopbackTransport(const LoopbackTransport&) = delete;

	LoopbackTransport(LoopbackTransport&&) = delete;

	auto operator=(const LoopbackTransport&) -> LoopbackTransport& = delete;

	auto operator=(LoopbackTransport&&) -> LoopbackTransport& = delete;

	/**
	 * @brief
	 *  Destructor, closes the transport.
	 */
	~LoopbackTransport() override;

	/**
	 * @brief
	 *  Accept the loopback peers that connect to a port of this process.
	 * @param[in] iPort The port, 0 for a free one (see `getPort()`).
	 * @param[in] iMaxPeers The maximum number of peers connected at once.
	 * @return Nothing, or `BindFailed` when another loopback listener holds the port.
	 */
	auto listen(uint16_t iPort, uint32_t iMaxPeers) -> expected<void, NetworkError> override;

	/**
	 * @brief
	 *  Connect to the loopback listener of a port; the host name is ignored. A `Disconnected` event with the reason
	 *  `Refused` follows when nothing listens there or the listener is full.
	 * @param[in] iHost Ignored.
	 * @param[in] iPort The port of the listener.
	 * @return The peer standing for the listener.
	 */
	auto connect(const std::string& iHost, uint16_t iPort) -> expected<PeerId, NetworkError> override;

	/**
	 * @brief
	 *  Close the link with a peer; both sides get a `Disconnected` event with the reason `Graceful`.
	 * @param[in] iPeer The peer.
	 */
	void disconnect(PeerId iPeer) override;

	/**
	 * @brief
	 *  Close every link and stop listening; the peers get a `Disconnected` event with the reason `Graceful`.
	 */
	void close() override;

	/**
	 * @brief
	 *  Hand a message to a peer.
	 * @param[in] iPeer The peer.
	 * @param[in] iChannel The channel, below `g_ChannelCount`.
	 * @param[in] iDelivery Ignored: loopback messages always arrive.
	 * @param[in] iPayload The bytes of the message.
	 * @return Nothing, or why the message was not handed over.
	 */
	auto send(PeerId iPeer, uint8_t iChannel, Delivery iDelivery, std::span<const uint8_t> iPayload)
			-> expected<void, NetworkError> override;

	/**
	 * @brief
	 *  Collect what happened since the last poll.
	 * @param[out] oEvents Receives the events, appended in the order they happened.
	 */
	void poll(std::vector<TransportEvent>& oEvents) override;

	/**
	 * @brief
	 *  Check whether the transport listens or has a link.
	 * @return True when open.
	 */
	[[nodiscard]] auto isOpen() const -> bool override;

	/**
	 * @brief
	 *  Get the port this transport listens on.
	 * @return The port, 0 when not listening.
	 */
	[[nodiscard]] auto getPort() const -> uint16_t override;

	/**
	 * @brief
	 *  Get the identifiers of the linked peers.
	 * @return The peers, in no particular order.
	 */
	[[nodiscard]] auto getPeers() const -> std::vector<PeerId> override;

	/**
	 * @brief
	 *  Get the byte counts of a peer; round trip and loss stay 0.
	 * @param[in] iPeer The peer.
	 * @return The statistics, or `UnknownPeer`.
	 */
	[[nodiscard]] auto getStats(PeerId iPeer) const -> expected<PeerStats, NetworkError> override;

	/**
	 * @brief
	 *  Cut the link with a peer as a lost connection would: both sides get a `Disconnected` event with the reason
	 *  `Timeout`. Simulates a network failure.
	 * @param[in] iPeer The peer.
	 */
	void severLink(PeerId iPeer);

	/// Internal state, shared with the in-process listener registry.
	struct Impl;

private:
	/// The links, the pending events and the listening port.
	uniq<Impl> mp_impl;
};

}// namespace owl::network
