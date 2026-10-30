/**
 * @file Bus.h
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace owl::sound {

/**
 * @brief
 *  Mixer bus a sound plays on: every bus feeds the master one.
 */
enum struct Bus : uint8_t {
	Master,///< The master bus, which every other bus feeds.
	Music,///< Background music, dry: no reverb zone nor zone filter.
	SFX,///< Sound effects.
	Ambient,///< Ambient beds and environment loops.
	Voice,///< Dialogue and barks.
};

/// Number of buses.
constexpr size_t g_busCount = 5;

/// Every bus, in the order of the enum.
constexpr std::array<Bus, g_busCount> g_allBuses{Bus::Master, Bus::Music, Bus::SFX, Bus::Ambient, Bus::Voice};

/**
 * @brief
 *  Index of a bus in the arrays of the mixer.
 * @param[in] iBus The bus.
 * @return Its index, below g_busCount.
 */
[[nodiscard]] constexpr auto getBusIndex(const Bus iBus) noexcept -> size_t { return static_cast<size_t>(iBus); }

/**
 * @brief
 *  Whether the sounds of a bus are heard through the reverb zones and their filter (all but Music and Master).
 * @param[in] iBus The bus.
 * @return True when the bus sends to the environment.
 */
[[nodiscard]] constexpr auto isEnvironmentBus(const Bus iBus) noexcept -> bool {
	return iBus != Bus::Music && iBus != Bus::Master;
}

/**
 * @brief
 *  Name of a bus, as written in files and shown by the editor (`Master`, `Music`, `SFX`, `Ambient`, `Voice`).
 * @param[in] iBus The bus.
 * @return The name.
 */
[[nodiscard]] OWL_API auto getBusName(Bus iBus) -> std::string_view;

/**
 * @brief
 *  Bus of a name, the case ignored (`music`, `SFX`...).
 * @param[in] iName The name.
 * @return The bus, none for an unknown name.
 */
[[nodiscard]] OWL_API auto parseBus(std::string_view iName) -> std::optional<Bus>;

}// namespace owl::sound
