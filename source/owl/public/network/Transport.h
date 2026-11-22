/**
 * @file Transport.h
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/expected.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

/**
 * @brief
 *  Networking: transports, connection management and the messages they carry.
 */
namespace owl::network {

/// Identifier of a peer of a transport, unique while the transport is open.
using PeerId = uint32_t;

/// Number of channels of every transport: messages of one channel keep their order, channels do not wait for each other.
constexpr uint8_t g_ChannelCount = 4;

/// Failures of the network API.
enum struct NetworkError : uint8_t {
	BackendUnavailable,///< The backend is not built in, or failed to initialize.
	AlreadyOpen,///< The transport already listens or connects.
	NotOpen,///< The transport neither listens nor connects.
	BindFailed,///< The local port could not be bound.
	ResolveFailed,///< The host name did not resolve.
	ConnectFailed,///< The connection could not start.
	UnknownPeer,///< No peer has this identifier.
	InvalidChannel,///< The channel is not below `g_ChannelCount`.
};

/// How a message travels.
enum struct Delivery : uint8_t {
	Reliable,///< Delivered once, in order within its channel, resent until acknowledged.
	Unreliable,///< Sent once, may be lost; a late one is dropped instead of delivered out of order.
};

/// Why a peer went away.
enum struct DisconnectReason : uint8_t {
	Graceful,///< One side closed the connection.
	Timeout,///< The peer stopped answering.
	Refused,///< The connection never completed.
};

/// Something that happened on a transport since the last poll.
struct TransportEvent {
	/// Kinds of transport events.
	enum struct Kind : uint8_t {
		Connected,///< A peer connected (incoming on a listener, outgoing on a client).
		Disconnected,///< A peer went away, see `reason`.
		Received,///< A message arrived from a peer, see `channel` and `payload`.
	};
	/// What happened.
	Kind kind = Kind::Received;
	/// The peer concerned.
	PeerId peer = 0;
	/// Channel of a received message.
	uint8_t channel = 0;
	/// Why a peer went away, for `Kind::Disconnected`.
	DisconnectReason reason = DisconnectReason::Graceful;
	/// Bytes of a received message.
	std::vector<uint8_t> payload;
};

/// Link statistics of one peer.
struct PeerStats {
	/// Mean round-trip time, in milliseconds.
	uint32_t roundTripMs = 0;
	/// Share of the packets lost, between 0 and 1.
	float packetLoss = 0.f;
	/// Bytes sent to the peer.
	uint64_t bytesSent = 0;
	/// Bytes received from the peer.
	uint64_t bytesReceived = 0;
};

/**
 * @brief
 *  A message transport between this process and its peers: a listener accepts peers, a client connects to one.
 *
 * Every call happens on one thread, the one that polls. Nothing is sent or received outside `poll()`.
 */
class OWL_API Transport {
public:
	/// Transport backends.
	enum struct Type : uint8_t {
		Loopback,///< In-process links, for tests, local play and a host playing on its own server.
		Enet,///< Reliable UDP through ENet.
		WebSocket,///< WebSocket over TCP, the transport of a page; a desktop server accepts it too.
		Mixed,///< ENet and WebSocket at once (`MixedTransport`): a desktop server for desktop and Web clients.
	};

	/**
	 * @brief
	 *  Create a transport.
	 * @param[in] iType The backend type.
	 */
	explicit Transport(const Type iType) : m_type{iType} {}

	Transport(const Transport&) = delete;

	Transport(Transport&&) = delete;

	auto operator=(const Transport&) -> Transport& = delete;

	auto operator=(Transport&&) -> Transport& = delete;

	/**
	 * @brief
	 *  Destructor, closes the transport.
	 */
	virtual ~Transport();

	/**
	 * @brief
	 *  Create a transport of a backend.
	 * @param[in] iType The backend type.
	 * @return The transport, or an error when the backend is not built in.
	 */
	static auto create(Type iType) -> expected<uniq<Transport>, NetworkError>;

	/**
	 * @brief
	 *  Get the backend type.
	 * @return The backend type.
	 */
	[[nodiscard]] auto getType() const -> Type { return m_type; }

	/**
	 * @brief
	 *  Accept peers on a local port.
	 * @param[in] iPort The port, 0 for one chosen by the system (see `getPort()`).
	 * @param[in] iMaxPeers The maximum number of peers connected at once.
	 * @return Nothing, or why listening failed.
	 */
	virtual auto listen(uint16_t iPort, uint32_t iMaxPeers) -> expected<void, NetworkError> = 0;

	/**
	 * @brief
	 *  Start connecting to a listener; a `Connected` event follows when the link is up.
	 * @param[in] iHost The host name or address of the listener.
	 * @param[in] iPort The port of the listener.
	 * @return The peer standing for the listener, or why the connection could not start.
	 */
	virtual auto connect(const std::string& iHost, uint16_t iPort) -> expected<PeerId, NetworkError> = 0;

	/**
	 * @brief
	 *  Close the link with a peer once the messages already queued to it are sent; a `Disconnected` event follows on
	 *  both sides.
	 * @param[in] iPeer The peer.
	 */
	virtual void disconnect(PeerId iPeer) = 0;

	/**
	 * @brief
	 *  Close every link and stop listening; no event follows.
	 */
	virtual void close() = 0;

	/**
	 * @brief
	 *  Queue a message to a peer.
	 * @param[in] iPeer The peer.
	 * @param[in] iChannel The channel, below `g_ChannelCount`.
	 * @param[in] iDelivery How the message travels.
	 * @param[in] iPayload The bytes of the message.
	 * @return Nothing, or why the message was not queued.
	 */
	virtual auto send(PeerId iPeer, uint8_t iChannel, Delivery iDelivery, std::span<const uint8_t> iPayload)
			-> expected<void, NetworkError> = 0;

	/**
	 * @brief
	 *  Send the queued messages and collect what happened since the last poll.
	 * @param[out] oEvents Receives the events, appended in the order they happened.
	 */
	virtual void poll(std::vector<TransportEvent>& oEvents) = 0;

	/**
	 * @brief
	 *  Check whether the transport listens or connects.
	 * @return True when open.
	 */
	[[nodiscard]] virtual auto isOpen() const -> bool = 0;

	/**
	 * @brief
	 *  Get the local port a listener is bound to.
	 * @return The port, 0 when not listening.
	 */
	[[nodiscard]] virtual auto getPort() const -> uint16_t = 0;

	/**
	 * @brief
	 *  Get the identifiers of the connected peers.
	 * @return The peers, in no particular order.
	 */
	[[nodiscard]] virtual auto getPeers() const -> std::vector<PeerId> = 0;

	/**
	 * @brief
	 *  Get the link statistics of a peer.
	 * @param[in] iPeer The peer.
	 * @return The statistics, or an error when the peer is unknown.
	 */
	[[nodiscard]] virtual auto getStats(PeerId iPeer) const -> expected<PeerStats, NetworkError> = 0;

private:
	/// The backend type.
	Type m_type;
};

}// namespace owl::network
