/**
 * @file RandomStream.h
 * @author Silmaen
 * @date 03/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"

#include <array>
#include <cstdint>
#include <string_view>

namespace owl::math {

/**
 * @brief
 *  Seeded pseudo-random stream (xoshiro256**), the same sequence on every target.
 *
 * Integer arithmetic only: a seed and a stream name give the same values, bit for bit, on every compiler and CPU.
 * The simulation draws from one stream per system (`scene::Scene::getRandomStream`) so that a system drawing one
 * more value leaves the sequences of the others unchanged.
 */
class OWL_API RandomStream final {
public:
	/// Internal state of the generator.
	using State = std::array<uint64_t, 4>;

	/**
	 * @brief
	 *  Copy constructor.
	 */
	RandomStream(const RandomStream&) = default;

	/**
	 * @brief
	 *  Move constructor.
	 */
	RandomStream(RandomStream&&) = default;

	/**
	 * @brief
	 *  Copy assignment operator.
	 * @return A reference to this object.
	 */
	auto operator=(const RandomStream&) -> RandomStream& = default;

	/**
	 * @brief
	 *  Move assignment operator.
	 * @return A reference to this object.
	 */
	auto operator=(RandomStream&&) -> RandomStream& = default;

	/**
	 * @brief
	 *  Destructor.
	 */
	~RandomStream() = default;

	/**
	 * @brief
	 *  Stream of seed 0.
	 */
	RandomStream();

	/**
	 * @brief
	 *  Stream of a seed.
	 * @param[in] iSeed The seed.
	 */
	explicit RandomStream(uint64_t iSeed);

	/**
	 * @brief
	 *  Named stream of a seed: two names give two independent sequences.
	 * @param[in] iSeed The seed.
	 * @param[in] iName The stream name.
	 */
	RandomStream(uint64_t iSeed, std::string_view iName);

	/**
	 * @brief
	 *  Restart the stream from a seed.
	 * @param[in] iSeed The seed.
	 */
	void reseed(uint64_t iSeed);

	/**
	 * @brief
	 *  Next raw value.
	 * @return 64 random bits.
	 */
	auto nextU64() -> uint64_t;

	/**
	 * @brief
	 *  Next value in [0, 1), on 24 bits.
	 * @return The value.
	 */
	auto nextFloat() -> float;

	/**
	 * @brief
	 *  Next value in [0, 1), on 53 bits.
	 * @return The value.
	 */
	auto nextDouble() -> double;

	/**
	 * @brief
	 *  Next integer in [iMin, iMax], without modulo bias.
	 * @param[in] iMin Lowest value.
	 * @param[in] iMax Highest value (swapped with iMin when lower).
	 * @return The value.
	 */
	auto nextInt(int64_t iMin, int64_t iMax) -> int64_t;

	/**
	 * @brief
	 *  Next value in [iMin, iMax).
	 * @param[in] iMin Lowest value.
	 * @param[in] iMax Upper bound.
	 * @return The value.
	 */
	auto nextRange(float iMin, float iMax) -> float;

	/**
	 * @brief
	 *  The generator state, to snapshot the stream.
	 * @return The state.
	 */
	[[nodiscard]] auto getState() const -> const State& { return m_state; }

	/**
	 * @brief
	 *  Restore a snapshot of the stream.
	 * @param[in] iState The state (an all-zero state, which the generator cannot leave, restarts seed 0).
	 */
	void setState(const State& iState);

	/**
	 * @brief
	 *  Compare the states.
	 * @param[in] iOther The other stream.
	 * @return True when both streams give the same sequence.
	 */
	auto operator==(const RandomStream& iOther) const -> bool = default;

	/**
	 * @brief
	 *  SplitMix64 finaliser, the seed expansion of the streams.
	 * @param[in] iValue The value to mix.
	 * @return The mixed value.
	 */
	[[nodiscard]] static constexpr auto mix(uint64_t iValue) -> uint64_t {
		iValue += 0x9e3779b97f4a7c15ull;
		iValue = (iValue ^ (iValue >> 30u)) * 0xbf58476d1ce4e5b9ull;
		iValue = (iValue ^ (iValue >> 27u)) * 0x94d049bb133111ebull;
		return iValue ^ (iValue >> 31u);
	}

	/**
	 * @brief
	 *  FNV-1a hash of a stream name, the same on every target.
	 * @param[in] iName The name.
	 * @return The hash.
	 */
	[[nodiscard]] static constexpr auto hashName(const std::string_view iName) -> uint64_t {
		uint64_t hash = 0xcbf29ce484222325ull;
		for (const char character: iName) hash = (hash ^ static_cast<uint8_t>(character)) * 0x100000001b3ull;
		return hash;
	}

private:
	/// Generator state, never all zero.
	State m_state{};
};

}// namespace owl::math
