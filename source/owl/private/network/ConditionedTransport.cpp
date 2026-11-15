/**
 * @file ConditionedTransport.cpp
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "network/ConditionedTransport.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace owl::network {

namespace {

// Constants of the splitmix64 generator.
constexpr uint64_t g_MixIncrement = 0x9e3779b97f4a7c15ULL;
constexpr uint64_t g_MixFirst = 0xbf58476d1ce4e5b9ULL;
constexpr uint64_t g_MixSecond = 0x94d049bb133111ebULL;
constexpr uint32_t g_ShiftFirst = 30;
constexpr uint32_t g_ShiftSecond = 27;
constexpr uint32_t g_ShiftLast = 31;
// The top 53 bits of a draw make the mantissa of a double in [0, 1).
constexpr uint32_t g_MantissaShift = 11;
constexpr double g_MantissaScale = 0x1.0p-53;

auto steadyClock() -> ConditionedTransport::Clock {
	const auto start = std::chrono::steady_clock::now();
	return [start]() -> std::chrono::milliseconds {
		return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
	};
}

}// namespace

ConditionedTransport::ConditionedTransport(uniq<Transport> iInner, const LinkConditions& iConditions, Clock iClock)
	: Transport{iInner->getType()}, m_inner{std::move(iInner)}, m_conditions{iConditions},
	  m_clock{iClock ? std::move(iClock) : steadyClock()}, m_random{iConditions.seed} {}

ConditionedTransport::~ConditionedTransport() { close(); }

void ConditionedTransport::setConditions(const LinkConditions& iConditions) {
	m_conditions = iConditions;
	m_random = iConditions.seed;
}

auto ConditionedTransport::listen(const uint16_t iPort, const uint32_t iMaxPeers) -> expected<void, NetworkError> {
	return m_inner->listen(iPort, iMaxPeers);
}

auto ConditionedTransport::connect(const std::string& iHost, const uint16_t iPort) -> expected<PeerId, NetworkError> {
	return m_inner->connect(iHost, iPort);
}

void ConditionedTransport::disconnect(const PeerId iPeer) {
	if (!holdsFor(iPeer)) {
		m_inner->disconnect(iPeer);
		return;
	}
	if (std::ranges::find(m_closing, iPeer) == m_closing.end())
		m_closing.push_back(iPeer);
}

void ConditionedTransport::close() {
	m_held.clear();
	m_closing.clear();
	m_inner->close();
}

auto ConditionedTransport::send(const PeerId iPeer, const uint8_t iChannel, const Delivery iDelivery,
								const std::span<const uint8_t> iPayload) -> expected<void, NetworkError> {
	if (iChannel >= g_ChannelCount) {
		OWL_CORE_WARN("ConditionedTransport: channel {} out of range.", iChannel)
		return unexpected{NetworkError::InvalidChannel};
	}
	if (!m_inner->getStats(iPeer)) {
		OWL_CORE_WARN("ConditionedTransport: no peer {}.", iPeer)
		return unexpected{NetworkError::UnknownPeer};
	}
	++m_sent;
	if (m_conditions.isNeutral() && !holdsFor(iPeer))
		return m_inner->send(iPeer, iChannel, iDelivery, iPayload);
	if (iDelivery == Delivery::Unreliable) {
		if (draw() < static_cast<double>(m_conditions.loss)) {
			++m_dropped;
			return {};
		}
		if (draw() < static_cast<double>(m_conditions.duplicate))
			hold(iPeer, iChannel, iDelivery, iPayload);
	}
	hold(iPeer, iChannel, iDelivery, iPayload);
	return {};
}

void ConditionedTransport::poll(std::vector<TransportEvent>& oEvents) {
	const auto now = m_clock();
	const auto due = std::ranges::find_if(m_held, [now](const Held& iHeld) -> bool { return iHeld.due > now; });
	for (const Held& held: std::ranges::subrange(m_held.begin(), due))
		if (!m_inner->send(held.peer, held.channel, held.delivery, held.payload))
			OWL_CORE_TRACE("ConditionedTransport: held message to peer {} dropped.", held.peer)
	m_held.erase(m_held.begin(), due);
	std::erase_if(m_closing, [this](const PeerId iPeer) -> bool {
		if (holdsFor(iPeer))
			return false;
		m_inner->disconnect(iPeer);
		return true;
	});
	const size_t first = oEvents.size();
	m_inner->poll(oEvents);
	for (size_t i = first; i < oEvents.size(); ++i)
		if (oEvents[i].kind == TransportEvent::Kind::Disconnected)
			forget(oEvents[i].peer);
}

auto ConditionedTransport::isOpen() const -> bool { return m_inner->isOpen(); }

auto ConditionedTransport::getPort() const -> uint16_t { return m_inner->getPort(); }

auto ConditionedTransport::getPeers() const -> std::vector<PeerId> { return m_inner->getPeers(); }

auto ConditionedTransport::getStats(const PeerId iPeer) const -> expected<PeerStats, NetworkError> {
	auto stats = m_inner->getStats(iPeer);
	if (!stats)
		return stats;
	stats->roundTripMs += m_conditions.latencyMs + m_conditions.jitterMs / 2;
	if (m_sent > 0) {
		const double ownLoss = static_cast<double>(m_dropped) / static_cast<double>(m_sent);
		stats->packetLoss = static_cast<float>(1.0 - (1.0 - static_cast<double>(stats->packetLoss)) * (1.0 - ownLoss));
	}
	return stats;
}

void ConditionedTransport::hold(const PeerId iPeer, const uint8_t iChannel, const Delivery iDelivery,
								const std::span<const uint8_t> iPayload) {
	const uint32_t jitter =
			m_conditions.jitterMs == 0
					? 0
					: static_cast<uint32_t>(std::floor(draw() * static_cast<double>(m_conditions.jitterMs + 1)));
	auto due = m_clock() + std::chrono::milliseconds{m_conditions.latencyMs + jitter};
	if (iDelivery == Delivery::Reliable) {
		for (const Held& held: m_held)
			if (held.peer == iPeer && held.channel == iChannel && held.delivery == Delivery::Reliable)
				due = std::max(due, held.due);
	}
	const auto place = std::ranges::upper_bound(m_held, due, {}, &Held::due);
	m_held.insert(place, Held{.due = due,
							  .peer = iPeer,
							  .channel = iChannel,
							  .delivery = iDelivery,
							  .payload = {iPayload.begin(), iPayload.end()}});
}

auto ConditionedTransport::draw() -> double {
	m_random += g_MixIncrement;
	uint64_t mixed = m_random;
	mixed = (mixed ^ (mixed >> g_ShiftFirst)) * g_MixFirst;
	mixed = (mixed ^ (mixed >> g_ShiftSecond)) * g_MixSecond;
	mixed ^= mixed >> g_ShiftLast;
	return static_cast<double>(mixed >> g_MantissaShift) * g_MantissaScale;
}

auto ConditionedTransport::holdsFor(const PeerId iPeer) const -> bool {
	return std::ranges::any_of(m_held, [iPeer](const Held& iHeld) -> bool { return iHeld.peer == iPeer; });
}

void ConditionedTransport::forget(const PeerId iPeer) {
	std::erase_if(m_held, [iPeer](const Held& iHeld) -> bool { return iHeld.peer == iPeer; });
	std::erase(m_closing, iPeer);
}

}// namespace owl::network
