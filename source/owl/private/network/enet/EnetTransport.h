/**
 * @file EnetTransport.h
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "network/Transport.h"

#include "core/external/enet.h"

#include <cstdint>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace owl::network::enet {

/**
 * @brief
 *  Reliable UDP transport on ENet: one ENet channel per transport channel, reliable or
 *  unreliable sequenced packets.
 */
class EnetTransport final : public Transport {
public:
	/**
	 * @brief
	 *  Create a closed ENet transport; ENet itself is initialized by the first one.
	 */
	EnetTransport();

	EnetTransport(const EnetTransport&) = delete;

	EnetTransport(EnetTransport&&) = delete;

	auto operator=(const EnetTransport&) -> EnetTransport& = delete;

	auto operator=(EnetTransport&&) -> EnetTransport& = delete;

	/**
	 * @brief
	 *  Destructor, closes the transport; the last one deinitializes ENet.
	 */
	~EnetTransport() override;

	/**
	 * @brief
	 *  Check whether ENet initialized.
	 * @return True when usable.
	 */
	[[nodiscard]] auto isAvailable() const -> bool { return m_available; }

	auto listen(uint16_t iPort, uint32_t iMaxPeers) -> expected<void, NetworkError> override;

	auto connect(const std::string& iHost, uint16_t iPort) -> expected<PeerId, NetworkError> override;

	void disconnect(PeerId iPeer) override;

	void close() override;

	auto send(PeerId iPeer, uint8_t iChannel, Delivery iDelivery, std::span<const uint8_t> iPayload)
			-> expected<void, NetworkError> override;

	void poll(std::vector<TransportEvent>& oEvents) override;

	[[nodiscard]] auto isOpen() const -> bool override { return mp_host != nullptr; }

	[[nodiscard]] auto getPort() const -> uint16_t override { return m_port; }

	[[nodiscard]] auto getPeers() const -> std::vector<PeerId> override;

	[[nodiscard]] auto getStats(PeerId iPeer) const -> expected<PeerStats, NetworkError> override;

private:
	/// What the transport knows of one ENet peer.
	struct PeerState {
		/// The ENet peer.
		ENetPeer* peer = nullptr;
		/// Whether the connection completed.
		bool connected = false;
		/// Whether this side asked for the disconnection.
		bool closing = false;
		/// Bytes sent.
		uint64_t bytesSent = 0;
		/// Bytes received.
		uint64_t bytesReceived = 0;
	};

	/**
	 * @brief
	 *  Create the ENet host when missing.
	 * @param[in] iMaxPeers Number of peer slots.
	 * @return True when the host exists.
	 */
	auto ensureHost(uint32_t iMaxPeers) -> bool;

	/**
	 * @brief
	 *  Register an ENet peer under a new identifier.
	 * @param[in] iPeer The ENet peer.
	 * @return Its identifier.
	 */
	auto track(ENetPeer* iPeer) -> PeerId;

	/// Whether ENet initialized for this transport.
	bool m_available = false;
	/// The ENet host, null when closed.
	ENetHost* mp_host = nullptr;
	/// Port listened on, 0 when not listening.
	uint16_t m_port = 0;
	/// Next peer identifier.
	PeerId m_nextPeer = 1;
	/// Peers by identifier.
	std::unordered_map<PeerId, PeerState> m_peers;
};

}// namespace owl::network::enet
