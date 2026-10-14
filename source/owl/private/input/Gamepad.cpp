/**
 * @file Gamepad.cpp
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "input/Gamepad.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <string_view>

namespace owl::input {

namespace {

constexpr float g_minExponent = 0.1f;
constexpr float g_maxExponent = 10.f;
constexpr float g_maxDeadZone = 0.99f;
constexpr std::size_t g_guidLength = 32;

auto isMappingLine(const std::string_view iLine) -> bool {
	const auto firstComma = iLine.find(',');
	if (firstComma != g_guidLength)
		return false;
	const auto guid = iLine.substr(0, g_guidLength);
	if (!std::ranges::all_of(
				guid, [](const char iChar) -> bool { return std::isxdigit(static_cast<unsigned char>(iChar)) != 0; }))
		return false;
	const auto secondComma = iLine.find(',', firstComma + 1);
	return secondComma != std::string_view::npos && secondComma > firstComma + 1;
}

auto shapeMagnitude(const float iMagnitude, const AxisResponse& iResponse) -> float {
	const float deadZone = std::clamp(iResponse.deadZone, 0.f, g_maxDeadZone);
	const float magnitude = std::min(iMagnitude, 1.f);
	if (magnitude <= deadZone)
		return 0.f;
	const float scaled = (magnitude - deadZone) / (1.f - deadZone);
	return std::pow(scaled, iResponse.getExponent());
}

}// namespace

auto AxisResponse::getExponent() const -> float {
	switch (curve) {
		case ResponseCurve::Linear:
			return 1.f;
		case ResponseCurve::Quadratic:
			return 2.f;
		case ResponseCurve::Cubic:
			return 3.f;
		case ResponseCurve::Custom:
			return std::isfinite(exponent) ? std::clamp(exponent, g_minExponent, g_maxExponent) : 1.f;
	}
	return 1.f;
}

auto applyAxisResponse(const float iValue, const AxisResponse& iResponse) -> float {
	if (!std::isfinite(iValue))
		return 0.f;
	return std::copysign(shapeMagnitude(std::abs(iValue), iResponse), iValue);
}

auto applyStickResponse(const math::vec2& iStick, const AxisResponse& iResponse) -> math::vec2 {
	if (!std::isfinite(iStick.x()) || !std::isfinite(iStick.y()))
		return {0.f, 0.f};
	const float magnitude = iStick.norm();
	const float shaped = shapeMagnitude(magnitude, iResponse);
	if (shaped <= 0.f)
		return {0.f, 0.f};
	return iStick * (shaped / magnitude);
}

auto countGamepadMappings(const std::string_view iMappings) -> std::size_t {
	std::size_t count = 0;
	std::size_t start = 0;
	while (start < iMappings.size()) {
		auto end = iMappings.find('\n', start);
		if (end == std::string_view::npos)
			end = iMappings.size();
		auto line = iMappings.substr(start, end - start);
		while (!line.empty() && std::isspace(static_cast<unsigned char>(line.front())) != 0) line.remove_prefix(1);
		while (!line.empty() && std::isspace(static_cast<unsigned char>(line.back())) != 0) line.remove_suffix(1);
		if (!line.empty() && line.front() != '#' && isMappingLine(line))
			++count;
		start = end + 1;
	}
	return count;
}

}// namespace owl::input
