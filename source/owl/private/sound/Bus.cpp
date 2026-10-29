/**
 * @file Bus.cpp
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "sound/Bus.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <string_view>

namespace owl::sound {

auto getBusName(const Bus iBus) -> std::string_view {
	switch (iBus) {
		case Bus::Master:
			return "Master";
		case Bus::Music:
			return "Music";
		case Bus::SFX:
			return "SFX";
		case Bus::Ambient:
			return "Ambient";
		case Bus::Voice:
			return "Voice";
	}
	return "SFX";
}

auto parseBus(const std::string_view iName) -> std::optional<Bus> {
	const auto equalsIgnoringCase = [iName](const std::string_view iOther) -> bool {
		return std::ranges::equal(iName, iOther, [](const char iLeft, const char iRight) -> bool {
			return std::tolower(static_cast<unsigned char>(iLeft)) == std::tolower(static_cast<unsigned char>(iRight));
		});
	};
	for (const Bus bus: g_allBuses)
		if (equalsIgnoringCase(getBusName(bus)))
			return bus;
	return std::nullopt;
}

}// namespace owl::sound
