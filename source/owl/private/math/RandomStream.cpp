/**
 * @file RandomStream.cpp
 * @author Silmaen
 * @date 03/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "math/RandomStream.h"

#include <bit>
#include <cstdint>
#include <string_view>
#include <utility>

namespace owl::math {

RandomStream::RandomStream() { reseed(0); }

RandomStream::RandomStream(const uint64_t iSeed) { reseed(iSeed); }

RandomStream::RandomStream(const uint64_t iSeed, const std::string_view iName) { reseed(iSeed ^ mix(hashName(iName))); }

void RandomStream::reseed(const uint64_t iSeed) {
	uint64_t value = iSeed;
	for (auto& word: m_state) {
		word = mix(value);
		value += 0x9e3779b97f4a7c15ull;
	}
}

auto RandomStream::nextU64() -> uint64_t {
	const uint64_t result = std::rotl(m_state[1] * 5u, 7) * 9u;
	const uint64_t shifted = m_state[1] << 17u;
	m_state[2] ^= m_state[0];
	m_state[3] ^= m_state[1];
	m_state[1] ^= m_state[2];
	m_state[0] ^= m_state[3];
	m_state[2] ^= shifted;
	m_state[3] = std::rotl(m_state[3], 45);
	return result;
}

auto RandomStream::nextFloat() -> float { return static_cast<float>(nextU64() >> 40u) * 0x1.0p-24f; }

auto RandomStream::nextDouble() -> double { return static_cast<double>(nextU64() >> 11u) * 0x1.0p-53; }

auto RandomStream::nextInt(int64_t iMin, int64_t iMax) -> int64_t {
	if (iMax < iMin)
		std::swap(iMin, iMax);
	const uint64_t span = static_cast<uint64_t>(iMax) - static_cast<uint64_t>(iMin);
	if (span == UINT64_MAX)
		return static_cast<int64_t>(nextU64());
	const uint64_t count = span + 1u;
	// Reject the top values that would make the low residues more likely.
	const uint64_t limit = UINT64_MAX - (UINT64_MAX % count + 1u) % count;
	uint64_t value = nextU64();
	while (value > limit) value = nextU64();
	return static_cast<int64_t>(static_cast<uint64_t>(iMin) + value % count);
}

auto RandomStream::nextRange(const float iMin, const float iMax) -> float { return iMin + (iMax - iMin) * nextFloat(); }

void RandomStream::setState(const State& iState) {
	if (iState == State{}) {
		reseed(0);
		return;
	}
	m_state = iState;
}

}// namespace owl::math
