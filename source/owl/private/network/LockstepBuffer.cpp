/**
 * @file LockstepBuffer.cpp
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "network/LockstepBuffer.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>

namespace owl::network {

namespace {

constexpr uint8_t g_HeldBit = 1U;
constexpr uint8_t g_PressedBit = 2U;
constexpr uint8_t g_ReleasedBit = 4U;
// Most actions of an input: a longer list comes from a broken or hostile message.
constexpr uint16_t g_MaxActions = 1024;

}// namespace

void writePlayerInput(ByteWriter& ioWriter, const PlayerInput& iInput) {
	const auto count = static_cast<uint16_t>(std::min<size_t>(iInput.size(), g_MaxActions));
	ioWriter.writeU16(count);
	for (const auto& action: std::span{iInput}.first(count)) {
		uint8_t flags = 0;
		if (action.held)
			flags |= g_HeldBit;
		if (action.pressed)
			flags |= g_PressedBit;
		if (action.released)
			flags |= g_ReleasedBit;
		ioWriter.writeU8(flags);
		ioWriter.writeF32(action.value);
		ioWriter.writeF32(action.vector.x());
		ioWriter.writeF32(action.vector.y());
	}
}

auto readPlayerInput(ByteReader& ioReader) -> std::optional<PlayerInput> {
	const auto count = ioReader.readU16();
	if (!count.has_value() || *count > g_MaxActions)
		return std::nullopt;
	PlayerInput input(*count);
	for (auto& action: input) {
		const auto flags = ioReader.readU8();
		const auto value = ioReader.readF32();
		const auto x = ioReader.readF32();
		const auto y = ioReader.readF32();
		if (!flags || !value || !x || !y)
			return std::nullopt;
		action.held = (*flags & g_HeldBit) != 0;
		action.pressed = (*flags & g_PressedBit) != 0;
		action.released = (*flags & g_ReleasedBit) != 0;
		action.value = *value;
		action.vector = {*x, *y};
	}
	return input;
}

void LockstepBuffer::reset(const std::span<const PlayerId> iPlayers, const uint32_t iDelay) {
	m_players.assign(iPlayers.begin(), iPlayers.end());
	std::ranges::sort(m_players);
	m_inputs.clear();
	m_firstKept = 0;
	for (uint64_t tick = 0; tick < iDelay; ++tick) {
		for (const auto player: m_players) m_inputs[tick][player] = {};
	}
}

void LockstepBuffer::removePlayer(const PlayerId iPlayer) { std::erase(m_players, iPlayer); }

void LockstepBuffer::put(const uint64_t iTick, const PlayerId iPlayer, const PlayerInput& iInput) {
	if (iTick < m_firstKept)
		return;
	m_inputs[iTick].try_emplace(iPlayer, iInput);
}

auto LockstepBuffer::isComplete(const uint64_t iTick) const -> bool {
	const auto found = m_inputs.find(iTick);
	if (found == m_inputs.end())
		return m_players.empty();
	return std::ranges::all_of(m_players,
							   [&found](const PlayerId iPlayer) -> bool { return found->second.contains(iPlayer); });
}

auto LockstepBuffer::getPlayableEnd(const uint64_t iFrom) const -> uint64_t {
	uint64_t tick = std::max(iFrom, m_firstKept);
	while (isComplete(tick) && m_inputs.contains(tick)) ++tick;
	return tick;
}

auto LockstepBuffer::get(const uint64_t iTick, const PlayerId iPlayer) const -> const PlayerInput* {
	const auto found = m_inputs.find(iTick);
	if (found == m_inputs.end())
		return nullptr;
	const auto input = found->second.find(iPlayer);
	return input == found->second.end() ? nullptr : &input->second;
}

void LockstepBuffer::discardBefore(const uint64_t iTick) {
	m_firstKept = std::max(m_firstKept, iTick);
	m_inputs.erase(m_inputs.begin(), m_inputs.lower_bound(m_firstKept));
}

}// namespace owl::network
