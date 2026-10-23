/**
 * @file ProceduralSky.cpp
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "renderer/ProceduralSky.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>

namespace owl::renderer {

namespace {

constexpr float g_pi = std::numbers::pi_v<float>;
// Sun zenith angle of the reference sky (60 degrees high), whose zenith luminance is a pixel of 1.
constexpr float g_referenceZenithAngle = g_pi / 6.f;
// Lowest cosine of a view or sun zenith angle given to the Perez function, which diverges at the horizon.
constexpr float g_minCosine = 0.01f;
// Warm colour of the dusk glow, linear.
const math::vec3 g_glowColor{1.f, 0.38f, 0.12f};

// Perez distribution coefficients A to E of one channel for a turbidity.
using Perez = std::array<float, 5>;

auto perezLuminance(const float iT) -> Perez {
	return {0.1787f * iT - 1.4630f, -0.3554f * iT + 0.4275f, -0.0227f * iT + 5.3251f, 0.1206f * iT - 2.5771f,
			-0.0670f * iT + 0.3703f};
}

auto perezX(const float iT) -> Perez {
	return {-0.0193f * iT - 0.2592f, -0.0665f * iT + 0.0008f, -0.0004f * iT + 0.2125f, -0.0641f * iT - 0.8989f,
			-0.0033f * iT + 0.0452f};
}

auto perezY(const float iT) -> Perez {
	return {-0.0167f * iT - 0.2608f, -0.0950f * iT + 0.0092f, -0.0079f * iT + 0.2102f, -0.0441f * iT - 1.6537f,
			-0.0109f * iT + 0.0529f};
}

auto perez(const Perez& iC, const float iCosTheta, const float iGamma) -> float {
	const float cosGamma = std::cos(iGamma);
	return (1.f + iC[0] * std::exp(iC[1] / std::max(iCosTheta, g_minCosine))) *
		   (1.f + iC[2] * std::exp(iC[3] * iGamma) + iC[4] * cosGamma * cosGamma);
}

auto zenithLuminance(const float iT, const float iThetaSun) -> float {
	const float chi = (4.f / 9.f - iT / 120.f) * (g_pi - 2.f * iThetaSun);
	return std::max((4.0453f * iT - 4.9710f) * std::tan(chi) - 0.2155f * iT + 2.4192f, 0.f);
}

auto zenithChromaticity(const float iT, const float iThetaSun) -> math::vec2 {
	const float t2 = iThetaSun * iThetaSun;
	const float t3 = t2 * iThetaSun;
	const float tt = iT * iT;
	const float x = tt * (0.00166f * t3 - 0.00375f * t2 + 0.00209f * iThetaSun) +
					iT * (-0.02903f * t3 + 0.06377f * t2 - 0.03202f * iThetaSun + 0.00394f) +
					(0.11693f * t3 - 0.21196f * t2 + 0.06052f * iThetaSun + 0.25886f);
	const float y = tt * (0.00275f * t3 - 0.00610f * t2 + 0.00317f * iThetaSun) +
					iT * (-0.04214f * t3 + 0.08970f * t2 - 0.04153f * iThetaSun + 0.00516f) +
					(0.15346f * t3 - 0.26756f * t2 + 0.06670f * iThetaSun + 0.26688f);
	return {x, y};
}

auto xyYToLinearSrgb(const float iX, const float iY, const float iLuminance) -> math::vec3 {
	if (iY <= 0.f)
		return {0.f, 0.f, 0.f};
	const float bigX = iX / iY * iLuminance;
	const float bigZ = (1.f - iX - iY) / iY * iLuminance;
	return {std::max(3.2406f * bigX - 1.5372f * iLuminance - 0.4986f * bigZ, 0.f),
			std::max(-0.9689f * bigX + 1.8758f * iLuminance + 0.0415f * bigZ, 0.f),
			std::max(0.0557f * bigX - 0.2040f * iLuminance + 1.0570f * bigZ, 0.f)};
}

auto smoothstep(const float iEdge0, const float iEdge1, const float iX) -> float {
	const float t = std::clamp((iX - iEdge0) / (iEdge1 - iEdge0), 0.f, 1.f);
	return t * t * (3.f - 2.f * t);
}

auto flat(const math::vec3& iDirection) -> math::vec3 {
	const math::vec3 horizontal{iDirection.x(), 0.f, iDirection.z()};
	return horizontal.norm() > 1e-5f ? horizontal.normalized() : math::vec3{1.f, 0.f, 0.f};
}

// Everything of the sky that does not depend on the view direction.
struct SkyFrame {
	math::vec3 sun;
	float thetaSun = 0.f;
	Perez luminance{};
	Perez chromaX{};
	Perez chromaY{};
	float zenithY = 0.f;
	math::vec2 zenithXy{0.f, 0.f};
	float scale = 0.f;
	float day = 0.f;
	float glow = 0.f;
	float night = 0.f;
};

auto makeFrame(const SkyParameters& iParameters) -> SkyFrame {
	SkyFrame frame;
	frame.sun =
			iParameters.sunDirection.norm() > 1e-5f ? iParameters.sunDirection.normalized() : math::vec3{0.f, 1.f, 0.f};
	const float turbidity = std::clamp(iParameters.turbidity, 1.7f, 10.f);
	const float sunHeight = frame.sun.y();
	frame.thetaSun = std::acos(std::clamp(sunHeight, g_minCosine, 1.f));
	frame.luminance = perezLuminance(turbidity);
	frame.chromaX = perezX(turbidity);
	frame.chromaY = perezY(turbidity);
	frame.zenithY = zenithLuminance(turbidity, frame.thetaSun);
	frame.zenithXy = zenithChromaticity(turbidity, frame.thetaSun);
	frame.scale = 1.f / std::max(zenithLuminance(turbidity, g_referenceZenithAngle), 1e-4f);
	frame.day = smoothstep(-0.12f, 0.05f, sunHeight);
	frame.glow = std::exp(-(sunHeight * sunHeight) / (0.12f * 0.12f));
	frame.night = 1.f - smoothstep(-0.2f, 0.02f, sunHeight);
	return frame;
}

auto daySky(const SkyFrame& iFrame, const math::vec3& iDirection) -> math::vec3 {
	const float cosTheta = std::max(iDirection.y(), 0.f);
	const float gamma = std::acos(std::clamp(iDirection * iFrame.sun, -1.f, 1.f));
	const float zenithFactor = perez(iFrame.luminance, 1.f, iFrame.thetaSun);
	const float luminance =
			iFrame.zenithY * perez(iFrame.luminance, cosTheta, gamma) / std::max(zenithFactor, 1e-4f) * iFrame.scale;
	const float x = iFrame.zenithXy.x() * perez(iFrame.chromaX, cosTheta, gamma) /
					std::max(perez(iFrame.chromaX, 1.f, iFrame.thetaSun), 1e-4f);
	const float y = iFrame.zenithXy.y() * perez(iFrame.chromaY, cosTheta, gamma) /
					std::max(perez(iFrame.chromaY, 1.f, iFrame.thetaSun), 1e-4f);
	return xyYToLinearSrgb(x, y, luminance) * iFrame.day;
}

auto skyRadiance(const SkyParameters& iParameters, const SkyFrame& iFrame, const math::vec3& iDirection) -> math::vec3 {
	const float height = iDirection.y();
	const float above = smoothstep(-0.02f, 0.02f, height);
	const math::vec3 horizon = daySky(iFrame, flat(iDirection));
	math::vec3 color = height >= 0.f ? daySky(iFrame, iDirection) : horizon;
	const float towardSun = std::max(flat(iDirection) * flat(iFrame.sun), 0.f);
	const float band = std::exp(-std::abs(height) * 6.f);
	color = color + g_glowColor * (iFrame.glow * band * (0.15f + 0.85f * towardSun * towardSun) * 0.6f);
	const float zenithward = 0.35f + 0.65f * std::max(height, 0.f);
	color = color + iParameters.nightColor * (iParameters.nightBrightness * iFrame.night * zenithward);
	const math::vec3 ground = horizon * 0.45f + iParameters.groundColor * (std::max(horizon.y(), 0.f) * 0.6f +
																		   iParameters.nightBrightness * iFrame.night);
	return color * above + ground * (1.f - above);
}

}// namespace

auto renderProceduralSky(const SkyParameters& iParameters, const math::vec2ui& iSize) -> FloatImage {
	OWL_PROFILE_FUNCTION()

	FloatImage image;
	image.size = {std::max(iSize.x(), 2u), std::max(iSize.y(), 1u)};
	image.pixels.resize(static_cast<size_t>(image.size.x()) * image.size.y());
	const SkyFrame frame = makeFrame(iParameters);
	for (uint32_t y = 0; y < image.size.y(); ++y) {
		for (uint32_t x = 0; x < image.size.x(); ++x) {
			const math::vec2 uv{(static_cast<float>(x) + 0.5f) / static_cast<float>(image.size.x()),
								(static_cast<float>(y) + 0.5f) / static_cast<float>(image.size.y())};
			image.pixels[static_cast<size_t>(y) * image.size.x() + x] =
					skyRadiance(iParameters, frame, Equirectangular::toDirection(uv));
		}
	}
	return image;
}

auto skyBodiesOf(const SkyParameters& iParameters) -> SkyBodies {
	const math::vec3 sun =
			iParameters.sunDirection.norm() > 1e-5f ? iParameters.sunDirection.normalized() : math::vec3{0.f, 1.f, 0.f};
	const float day = smoothstep(-0.05f, 0.05f, sun.y());
	const float night = 1.f - smoothstep(-0.2f, 0.02f, sun.y());
	return {.sunDirection = sun,
			.discRadius = 0.5f * std::max(iParameters.discSize, 0.f) * g_pi / 180.f,
			.sunRadiance = 30.f * day,
			.moonRadiance = iParameters.nightBrightness * 20.f * night,
			.starRadiance = std::max(iParameters.stars, 0.f) * iParameters.nightBrightness * 30.f * night};
}

}// namespace owl::renderer
