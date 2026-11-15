/**
 * @file ConditionedTransport.h
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Transport.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace owl::network {

/// Network conditions a `ConditionedTransport` applies to the messages it sends.
struct LinkConditions {
	/// Delay added to every message, in milliseconds.
	uint32_t latencyMs = 0;
	/// Upper bound of a random delay added on top of the latency, in milliseconds.
	uint32_t jitterMs = 0;
	/// Share of the unreliable messages dropped, between 0 and 1.
	float loss = 0.f;
	/// Share of the unreliable messages sent twice, between 0 and 1.
	float duplicate = 0.f;
	/// Seed of the random draws: one seed and one sequence of sends give the same outcome.
	uint64_t seed = 0;

	/**
	 * @brief
	 *  Check whether these conditions change nothing.
	 * @return True when no delay, loss nor duplication applies.
	 */
	[[nodiscard]] auto isNeutral() const -> bool {
		return latencyMs == 0 && jitterMs == 0 && loss <= 0.f && duplicate <= 0.f;
	}
};

/**
 * @brief
 *  A transport wrapping another one to send its messages through a simulated bad network: delayed, jittered, lost or
 *  duplicated. Only the sending side is conditioned: wrap both ends for a delay both ways.
 *
 * Reliable messages are only delayed, never lost nor duplicated, and keep their order within a channel; a disconnection
 * waits for the messages already held for the peer. The wrapped transport's own events come back unchanged.
 */
class OWL_API ConditionedTransport final : public Transport {
public:
	/// Source of the current time, in milliseconds from any fixed origin.
	using Clock = std::function<std::chrono::milliseconds()>;

	/**
	 * @brief
	 *  Wrap a transport.
	 * @param[in] iInner The transport that really sends the messages.
	 * @param[in] iConditions The conditions to apply.
	 * @param[in] iClock The time source, the steady clock when empty.
	 */
	explicit ConditionedTransport(uniq<Transport> iInner, const LinkConditions& iConditions = {}, Clock iClock = {});

	ConditionedTransport(const ConditionedTransport&) = delete;

	ConditionedTransport(ConditionedTransport&&) = delete;

	auto operator=(const ConditionedTransport&) -> ConditionedTransport& = delete;

	auto operator=(ConditionedTransport&&) -> ConditionedTransport& = delete;

	/**
	 * @brief
	 *  Destructor, closes the wrapped transport.
	 */
	~ConditionedTransport() override;

	/**
	 * @brief
	 *  Change the conditions; the messages already held keep their send time. The random draws restart from the seed.
	 * @param[in] iConditions The new conditions.
	 */
	void setConditions(const LinkConditions& iConditions);

	/**
	 * @brief
	 *  Get the conditions applied.
	 * @return The conditions.
	 */
	[[nodiscard]] auto getConditions() const -> const LinkConditions& { return m_conditions; }

	/**
	 * @brief
	 *  Get the number of messages held until their send time.
	 * @return The number of held messages.
	 */
	[[nodiscard]] auto getHeldCount() const -> size_t { return m_held.size(); }

	/**
	 * @brief
	 *  Get the wrapped transport.
	 * @return The wrapped transport.
	 */
	[[nodiscard]] auto getInner() -> Transport& { return *m_inner; }

	/**
	 * @brief
	 *  Accept peers on a local port of the wrapped transport.
	 * @param[in] iPort The port, 0 for one chosen by the backend.
	 * @param[in] iMaxPeers The maximum number of peers connected at once.
	 * @return Nothing, or why listening failed.
	 */
	auto listen(uint16_t iPort, uint32_t iMaxPeers) -> expected<void, NetworkError> override;

	/**
	 * @brief
	 *  Start connecting to a listener through the wrapped transport.
	 * @param[in] iHost The host name or address of the listener.
	 * @param[in] iPort The port of the listener.
	 * @return The peer standing for the listener, or why the connection could not start.
	 */
	auto connect(const std::string& iHost, uint16_t iPort) -> expected<PeerId, NetworkError> override;

	/**
	 * @brief
	 *  Close the link with a peer once the messages held for it are sent.
	 * @param[in] iPeer The peer.
	 */
	void disconnect(PeerId iPeer) override;

	/**
	 * @brief
	 *  Drop the held messages, close every link and stop listening.
	 */
	void close() override;

	/**
	 * @brief
	 *  Hold a message until its send time, or drop it when an unreliable message is drawn as lost.
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
	 *  Hand the messages due to the wrapped transport, then collect its events.
	 * @param[out] oEvents Receives the events, appended in the order they happened.
	 */
	void poll(std::vector<TransportEvent>& oEvents) override;

	/**
	 * @brief
	 *  Check whether the wrapped transport listens or connects.
	 * @return True when open.
	 */
	[[nodiscard]] auto isOpen() const -> bool override;

	/**
	 * @brief
	 *  Get the local port the wrapped transport listens on.
	 * @return The port, 0 when not listening.
	 */
	[[nodiscard]] auto getPort() const -> uint16_t override;

	/**
	 * @brief
	 *  Get the identifiers of the connected peers.
	 * @return The peers, in no particular order.
	 */
	[[nodiscard]] auto getPeers() const -> std::vector<PeerId> override;

	/**
	 * @brief
	 *  Get the statistics of the wrapped transport, with the simulated delay added to the round trip and the simulated
	 *  loss to the packet loss.
	 * @param[in] iPeer The peer.
	 * @return The statistics, or an error when the peer is unknown.
	 */
	[[nodiscard]] auto getStats(PeerId iPeer) const -> expected<PeerStats, NetworkError> override;

private:
	/// A message waiting for its send time.
	struct Held {
		/// Send time, in milliseconds of the clock.
		std::chrono::milliseconds due{0};
		/// Destination.
		PeerId peer = 0;
		/// Channel.
		uint8_t channel = 0;
		/// How it travels.
		Delivery delivery = Delivery::Reliable;
		/// Bytes of the message.
		std::vector<uint8_t> payload;
	};

	/**
	 * @brief
	 *  Hold one copy of a message, keeping reliable messages of a channel in order.
	 * @param[in] iPeer The peer.
	 * @param[in] iChannel The channel.
	 * @param[in] iDelivery How the message travels.
	 * @param[in] iPayload The bytes of the message.
	 */
	void hold(PeerId iPeer, uint8_t iChannel, Delivery iDelivery, std::span<const uint8_t> iPayload);

	/**
	 * @brief
	 *  Draw the next random number.
	 * @return A number in [0, 1).
	 */
	auto draw() -> double;

	/**
	 * @brief
	 *  Check whether a message is held for a peer.
	 * @param[in] iPeer The peer.
	 * @return True when one is held.
	 */
	[[nodiscard]] auto holdsFor(PeerId iPeer) const -> bool;

	/**
	 * @brief
	 *  Forget the held messages and the pending disconnection of a peer.
	 * @param[in] iPeer The peer.
	 */
	void forget(PeerId iPeer);

	/// The transport that really sends.
	uniq<Transport> m_inner;
	/// The conditions applied.
	LinkConditions m_conditions;
	/// The time source.
	Clock m_clock;
	/// State of the random draws.
	uint64_t m_random = 0;
	/// Messages waiting for their send time, sorted by due time, the first sent first among equals.
	std::vector<Held> m_held;
	/// Peers to disconnect once their held messages are sent.
	std::vector<PeerId> m_closing;
	/// Messages handed to `send()`, for the loss statistics.
	uint64_t m_sent = 0;
	/// Unreliable messages dropped.
	uint64_t m_dropped = 0;
};

}// namespace owl::network
