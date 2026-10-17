/**
 * @file NavProfile.cpp
 * @author Silmaen
 * @date 16/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "scene/nav/NavProfile.h"

#include <algorithm>
#include <cstdint>

namespace owl::scene::nav {

auto parseDirection(const std::string_view iName) -> std::optional<Direction> {
	for (const auto direction: directions) {
		if (iName == directionNames[static_cast<size_t>(direction)])
			return direction;
	}
	return std::nullopt;
}

auto NavCellOverride::getSide(const Direction iSide) const -> SideState {
	const bool noLeave = (noExit & directionBit(iSide)) != 0;
	const bool noCome = (noEnter & directionBit(iSide)) != 0;
	if (noLeave && noCome)
		return SideState::Closed;
	if (noCome)
		return SideState::ExitOnly;
	if (noLeave)
		return SideState::EnterOnly;
	return SideState::TwoWay;
}

void NavCellOverride::setSide(const Direction iSide, const SideState iState) {
	const uint8_t bit = directionBit(iSide);
	const bool noLeave = iState == SideState::EnterOnly || iState == SideState::Closed;
	const bool noCome = iState == SideState::ExitOnly || iState == SideState::Closed;
	noExit = noLeave ? static_cast<uint8_t>(noExit | bit) : static_cast<uint8_t>(noExit & ~bit);
	noEnter = noCome ? static_cast<uint8_t>(noEnter | bit) : static_cast<uint8_t>(noEnter & ~bit);
}

auto NavProfile::findRule(const int32_t iTile) const -> const NavTileRule* {
	const auto found =
			std::ranges::find_if(rules, [iTile](const NavTileRule& iRule) -> bool { return iRule.tile == iTile; });
	return found == rules.end() ? nullptr : &*found;
}

auto NavProfile::findOverride(const uint32_t iX, const uint32_t iY) const -> const NavCellOverride* {
	const auto found = std::ranges::find_if(overrides, [iX, iY](const NavCellOverride& iOverride) -> bool {
		return iOverride.x == iX && iOverride.y == iY;
	});
	return found == overrides.end() ? nullptr : &*found;
}

auto NavProfile::getSideState(const uint32_t iX, const uint32_t iY, const Direction iSide) const -> SideState {
	const auto* entry = findOverride(iX, iY);
	return entry != nullptr ? entry->getSide(iSide) : SideState::TwoWay;
}

namespace {

// The exception of a cell, created when absent.
auto touchOverride(std::vector<NavCellOverride>& ioOverrides, const uint32_t iX, const uint32_t iY)
		-> NavCellOverride& {
	const auto found = std::ranges::find_if(ioOverrides, [iX, iY](const NavCellOverride& iOverride) -> bool {
		return iOverride.x == iX && iOverride.y == iY;
	});
	if (found != ioOverrides.end())
		return *found;
	ioOverrides.push_back({.x = iX, .y = iY});
	return ioOverrides.back();
}

// Drop the neutral exceptions.
void pruneOverrides(std::vector<NavCellOverride>& ioOverrides) {
	std::erase_if(ioOverrides, [](const NavCellOverride& iOverride) -> bool { return iOverride.isNeutral(); });
}

// The state seen from the other end of an edge.
constexpr auto mirrored(const SideState iState) -> SideState {
	switch (iState) {
		case SideState::ExitOnly:
			return SideState::EnterOnly;
		case SideState::EnterOnly:
			return SideState::ExitOnly;
		case SideState::TwoWay:
		case SideState::Closed:
			break;
	}
	return iState;
}

}// namespace

void NavProfile::setEdgeState(const uint32_t iX, const uint32_t iY, const Direction iSide, const SideState iState) {
	touchOverride(overrides, iX, iY).setSide(iSide, iState);
	const auto nx = static_cast<int64_t>(iX) + offsetX(iSide);
	const auto ny = static_cast<int64_t>(iY) + offsetY(iSide);
	if (nx >= 0 && ny >= 0) {
		touchOverride(overrides, static_cast<uint32_t>(nx), static_cast<uint32_t>(ny))
				.setSide(opposite(iSide), mirrored(iState));
	}
	pruneOverrides(overrides);
}

void NavProfile::setCellCost(const uint32_t iX, const uint32_t iY, const float iCost) {
	touchOverride(overrides, iX, iY).cost = std::clamp(iCost, 0.f, 64.f);
	pruneOverrides(overrides);
}

}// namespace owl::scene::nav
