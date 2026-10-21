/**
 * @file EnvironmentMap.cpp
 * @author Silmaen
 * @date 21/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/EnvironmentMap.h"

#include "app/Application.h"

#include <stb_image.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <memory>
#include <numbers>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace owl::renderer {

namespace {

constexpr float g_pi = std::numbers::pi_v<float>;
constexpr uint32_t g_minLevelWidth = 8;
constexpr uint32_t g_minLevelHeight = 4;
constexpr uint32_t g_maxLutSize = 256;

template<typename Fn>
void parallelRows(const uint32_t iCount, Fn&& iFunction) {
	const auto workers = std::min<uint32_t>(std::max(1u, std::thread::hardware_concurrency()), iCount);
	if (workers <= 1) {
		for (uint32_t row = 0; row < iCount; ++row) { iFunction(row); }
		return;
	}
	std::atomic<uint32_t> next{0};
	auto work = [&]() -> void {
		for (;;) {
			const uint32_t row = next.fetch_add(1);
			if (row >= iCount) {
				return;
			}
			iFunction(row);
		}
	};
	std::vector<std::thread> threads;
	threads.reserve(workers - 1);
	for (uint32_t i = 1; i < workers; ++i) { threads.emplace_back(work); }
	work();
	for (auto& thread: threads) { thread.join(); }
}

auto radicalInverse(uint32_t iBits) -> float {
	iBits = (iBits << 16u) | (iBits >> 16u);
	iBits = ((iBits & 0x55555555u) << 1u) | ((iBits & 0xAAAAAAAAu) >> 1u);
	iBits = ((iBits & 0x33333333u) << 2u) | ((iBits & 0xCCCCCCCCu) >> 2u);
	iBits = ((iBits & 0x0F0F0F0Fu) << 4u) | ((iBits & 0xF0F0F0F0u) >> 4u);
	iBits = ((iBits & 0x00FF00FFu) << 8u) | ((iBits & 0xFF00FF00u) >> 8u);
	return static_cast<float>(iBits) * 2.3283064365386963e-10f;
}

auto hammersley(const uint32_t iIndex, const uint32_t iCount) -> math::vec2 {
	return {static_cast<float>(iIndex) / static_cast<float>(iCount), radicalInverse(iIndex)};
}

auto distributionGgx(const float iNoH, const float iAlpha) -> float {
	const float a2 = iAlpha * iAlpha;
	const float d = iNoH * iNoH * (a2 - 1.f) + 1.f;
	return a2 / (g_pi * d * d);
}

// Half vector of the GGX distribution in tangent space (z = normal).
auto sampleGgxHalf(const math::vec2& iXi, const float iAlpha) -> math::vec3 {
	const float phi = 2.f * g_pi * iXi.x();
	const float a2 = iAlpha * iAlpha;
	const float cosTheta = std::sqrt((1.f - iXi.y()) / (1.f + (a2 - 1.f) * iXi.y()));
	const float sinTheta = std::sqrt(std::max(0.f, 1.f - cosTheta * cosTheta));
	return {sinTheta * std::cos(phi), sinTheta * std::sin(phi), cosTheta};
}

auto lerp(const math::vec3& iA, const math::vec3& iB, const float iT) -> math::vec3 {
	return iA * (1.f - iT) + iB * iT;
}

auto downsample(const FloatImage& iImage) -> FloatImage {
	FloatImage out;
	const uint32_t w = std::max(1u, iImage.size.x() / 2);
	const uint32_t h = std::max(1u, iImage.size.y() / 2);
	out.size = {w, h};
	out.pixels.resize(static_cast<size_t>(w) * h);
	const uint32_t maxX = iImage.size.x() - 1;
	const uint32_t maxY = iImage.size.y() - 1;
	for (uint32_t y = 0; y < h; ++y) {
		for (uint32_t x = 0; x < w; ++x) {
			const uint32_t x0 = std::min(2 * x, maxX);
			const uint32_t x1 = std::min(2 * x + 1, maxX);
			const uint32_t y0 = std::min(2 * y, maxY);
			const uint32_t y1 = std::min(2 * y + 1, maxY);
			out.pixels[static_cast<size_t>(y) * w + x] =
					(iImage.at(x0, y0) + iImage.at(x1, y0) + iImage.at(x0, y1) + iImage.at(x1, y1)) * 0.25f;
		}
	}
	return out;
}

// Mip chain of level 0 down to 4x2, each mip with four times the texel solid angle of the previous one.
auto buildSourceMips(FloatImage iBase) -> std::vector<FloatImage> {
	std::vector<FloatImage> mips;
	mips.push_back(std::move(iBase));
	while (mips.back().size.x() > 4 && mips.back().size.y() > 2) { mips.push_back(downsample(mips.back())); }
	return mips;
}

auto sampleMips(const std::vector<FloatImage>& iMips, const math::vec3& iDirection, const float iLod) -> math::vec3 {
	const auto last = static_cast<uint32_t>(iMips.size() - 1);
	const float clamped = std::clamp(iLod, 0.f, static_cast<float>(last));
	const auto lo = static_cast<uint32_t>(clamped);
	const float t = clamped - static_cast<float>(lo);
	const auto low = Equirectangular::sample(iMips[lo], iDirection);
	if (lo >= last || t <= 0.f) {
		return low;
	}
	return lerp(low, Equirectangular::sample(iMips[lo + 1], iDirection), t);
}

struct FilterSample {
	math::vec3 tangentLight;
	float nol = 0.f;
	float lod = 0.f;
};

auto makeFilterSamples(const float iAlpha, const uint32_t iSampleCount, const float iTexelSolidAngle)
		-> std::vector<FilterSample> {
	std::vector<FilterSample> samples;
	samples.reserve(iSampleCount);
	for (uint32_t k = 0; k < iSampleCount; ++k) {
		const auto h = sampleGgxHalf(hammersley(k, iSampleCount), iAlpha);
		// N = V = R: L is V mirrored around H, with V = (0, 0, 1) in tangent space.
		const math::vec3 l{2.f * h.z() * h.x(), 2.f * h.z() * h.y(), 2.f * h.z() * h.z() - 1.f};
		if (l.z() <= 0.f) {
			continue;
		}
		const float pdf = distributionGgx(h.z(), iAlpha) * 0.25f;
		const float sampleSolidAngle = 1.f / (static_cast<float>(iSampleCount) * std::max(pdf, 1e-8f));
		const float lod = 0.5f * std::log2(std::max(sampleSolidAngle / iTexelSolidAngle, 1e-8f)) + 1.f;
		samples.push_back({l, l.z(), std::max(0.f, lod)});
	}
	return samples;
}

auto filterLevel(const std::vector<FloatImage>& iMips, const math::vec2ui& iSize, const float iRoughness,
				 const uint32_t iSampleCount) -> FloatImage {
	FloatImage out;
	out.size = iSize;
	out.pixels.resize(static_cast<size_t>(iSize.x()) * iSize.y());
	const auto& base = iMips.front();
	const float texelSolidAngle = 4.f * g_pi / (static_cast<float>(base.size.x()) * static_cast<float>(base.size.y()));
	const float alpha = std::max(iRoughness * iRoughness, 1e-4f);
	const auto samples = makeFilterSamples(alpha, std::max(1u, iSampleCount), texelSolidAngle);
	const uint32_t w = iSize.x();
	const uint32_t h = iSize.y();
	parallelRows(h, [&](const uint32_t iRow) -> void {
		for (uint32_t x = 0; x < w; ++x) {
			const auto n = Equirectangular::toDirection({(static_cast<float>(x) + 0.5f) / static_cast<float>(w),
														 (static_cast<float>(iRow) + 0.5f) / static_cast<float>(h)});
			const math::vec3 up = std::abs(n.z()) < 0.999f ? math::vec3{0.f, 0.f, 1.f} : math::vec3{1.f, 0.f, 0.f};
			const auto t = (up ^ n).normalized();
			const auto b = n ^ t;
			math::vec3 sum{0.f, 0.f, 0.f};
			float weight = 0.f;
			for (const auto& sample: samples) {
				const auto l = t * sample.tangentLight.x() + b * sample.tangentLight.y() + n * sample.tangentLight.z();
				sum += sampleMips(iMips, l, sample.lod) * sample.nol;
				weight += sample.nol;
			}
			out.pixels[static_cast<size_t>(iRow) * w + x] = weight > 0.f ? sum / weight : sampleMips(iMips, n, 0.f);
		}
	});
	return out;
}

// Real SH basis, Y (world up) as polar axis: l=1 is (z, y, x), l=2 is (xz, zy, 3y^2-1, xy, x^2-z^2).
auto shBasis(const math::vec3& iDir) -> std::array<float, 9> {
	const float x = iDir.x();
	const float y = iDir.y();
	const float z = iDir.z();
	return {0.282095f,
			0.488603f * z,
			0.488603f * y,
			0.488603f * x,
			1.092548f * x * z,
			1.092548f * z * y,
			0.315392f * (3.f * y * y - 1.f),
			1.092548f * x * y,
			0.546274f * (x * x - z * z)};
}

// Per-band factor of the cosine convolution divided by pi: 1, 2/3, 1/4.
constexpr std::array<float, 9> g_shBandScale = {1.f,   2.f / 3.f, 2.f / 3.f, 2.f / 3.f, 0.25f,
												0.25f, 0.25f,     0.25f,     0.25f};

auto projectIrradiance(const FloatImage& iImage) -> SphericalHarmonics {
	const uint32_t w = iImage.size.x();
	const uint32_t h = iImage.size.y();
	std::vector<std::array<math::vec3, 9>> rowSums(h);
	std::vector<float> rowWeights(h, 0.f);
	parallelRows(h, [&](const uint32_t iRow) -> void {
		const float theta = (static_cast<float>(iRow) + 0.5f) / static_cast<float>(h) * g_pi;
		const float solidAngle =
				(2.f * g_pi / static_cast<float>(w)) * (g_pi / static_cast<float>(h)) * std::sin(theta);
		auto& sums = rowSums[iRow];
		for (auto& value: sums) { value = {0.f, 0.f, 0.f}; }
		for (uint32_t x = 0; x < w; ++x) {
			const auto dir = Equirectangular::toDirection({(static_cast<float>(x) + 0.5f) / static_cast<float>(w),
														   (static_cast<float>(iRow) + 0.5f) / static_cast<float>(h)});
			const auto basis = shBasis(dir);
			const auto& radiance = iImage.at(x, iRow);
			for (size_t i = 0; i < 9; ++i) { sums[i] += radiance * (basis[i] * solidAngle); }
			rowWeights[iRow] += solidAngle;
		}
	});
	std::array<math::vec3, 9> total{};
	double weight = 0.0;
	for (uint32_t y = 0; y < h; ++y) {
		for (size_t i = 0; i < 9; ++i) { total[i] += rowSums[y][i]; }
		weight += static_cast<double>(rowWeights[y]);
	}
	// The discrete solid angles sum to 4 pi only approximately: renormalise so a constant sky is exact.
	const float normalisation = weight > 0.0 ? static_cast<float>(4.0 * std::numbers::pi / weight) : 0.f;
	SphericalHarmonics out;
	for (size_t i = 0; i < 9; ++i) { out.coefficients[i] = total[i] * (normalisation * g_shBandScale[i]); }
	return out;
}

struct HdrDeleter {
	void operator()(float* iData) const noexcept { stbi_image_free(iData); }
};

using HdrPixels = uniq<float, HdrDeleter>;

auto toFloatImage(const HdrPixels& iData, const int iWidth, const int iHeight) -> std::optional<FloatImage> {
	if (iData == nullptr || iWidth <= 0 || iHeight <= 0) {
		return std::nullopt;
	}
	FloatImage image;
	image.size = {static_cast<uint32_t>(iWidth), static_cast<uint32_t>(iHeight)};
	const size_t count = static_cast<size_t>(iWidth) * static_cast<size_t>(iHeight);
	image.pixels.resize(count);
	const float* src = iData.get();
	for (size_t i = 0; i < count; ++i) {
		math::vec3 pixel{src[3 * i], src[3 * i + 1], src[3 * i + 2]};
		for (auto& channel: pixel) {
			if (!std::isfinite(channel) || channel < 0.f) {
				channel = 0.f;
			}
		}
		image.pixels[i] = pixel;
	}
	return image;
}

auto trim(std::string_view iText) -> std::string_view {
	constexpr std::string_view blanks = " \t\r\n";
	const auto first = iText.find_first_not_of(blanks);
	if (first == std::string_view::npos) {
		return {};
	}
	const auto last = iText.find_last_not_of(blanks);
	return iText.substr(first, last - first + 1);
}

auto parseFloat(std::string_view& ioText, float& oValue) -> bool {
	ioText = trim(ioText);
	if (ioText.empty()) {
		return false;
	}
	const auto [ptr, ec] = std::from_chars(ioText.data(), ioText.data() + ioText.size(), oValue);
	if (ec != std::errc{} || !std::isfinite(oValue)) {
		return false;
	}
	ioText.remove_prefix(static_cast<size_t>(ptr - ioText.data()));
	return true;
}

auto parseTriplet(std::string_view iText, math::vec3& oValue) -> bool {
	for (size_t i = 0; i < 3; ++i) {
		if (!parseFloat(iText, oValue[i])) {
			return false;
		}
	}
	return trim(iText).empty();
}

}// namespace

auto Equirectangular::toUv(const math::vec3& iDirection) -> math::vec2 {
	const float u = 0.5f + std::atan2(iDirection.x(), -iDirection.z()) / (2.f * g_pi);
	const float v = std::acos(std::clamp(iDirection.y(), -1.f, 1.f)) / g_pi;
	return {u, v};
}

auto Equirectangular::toDirection(const math::vec2& iUv) -> math::vec3 {
	const float phi = (iUv.x() - 0.5f) * 2.f * g_pi;
	const float theta = iUv.y() * g_pi;
	const float sinTheta = std::sin(theta);
	return {sinTheta * std::sin(phi), std::cos(theta), -sinTheta * std::cos(phi)};
}

auto Equirectangular::sample(const FloatImage& iImage, const math::vec3& iDirection) -> math::vec3 {
	if (iImage.isEmpty()) {
		return {0.f, 0.f, 0.f};
	}
	const auto uv = toUv(iDirection);
	const auto w = static_cast<int64_t>(iImage.size.x());
	const auto h = static_cast<int64_t>(iImage.size.y());
	const float px = uv.x() * static_cast<float>(w) - 0.5f;
	const float py = uv.y() * static_cast<float>(h) - 0.5f;
	const float fx = std::floor(px);
	const float fy = std::floor(py);
	const float tx = px - fx;
	const float ty = py - fy;
	const auto ix = static_cast<int64_t>(fx);
	const auto iy = static_cast<int64_t>(fy);
	const auto wrapX = [w](const int64_t iX) -> uint32_t { return static_cast<uint32_t>(((iX % w) + w) % w); };
	const auto clampY = [h](const int64_t iY) -> uint32_t {
		return static_cast<uint32_t>(std::clamp<int64_t>(iY, 0, h - 1));
	};
	const uint32_t x0 = wrapX(ix);
	const uint32_t x1 = wrapX(ix + 1);
	const uint32_t y0 = clampY(iy);
	const uint32_t y1 = clampY(iy + 1);
	const auto top = lerp(iImage.at(x0, y0), iImage.at(x1, y0), tx);
	const auto bottom = lerp(iImage.at(x0, y1), iImage.at(x1, y1), tx);
	return lerp(top, bottom, ty);
}

auto SphericalHarmonics::evaluate(const math::vec3& iNormal) const -> math::vec3 {
	const auto basis = shBasis(iNormal);
	math::vec3 out{0.f, 0.f, 0.f};
	for (size_t i = 0; i < 9; ++i) { out += coefficients[i] * basis[i]; }
	return {std::max(0.f, out.x()), std::max(0.f, out.y()), std::max(0.f, out.z())};
}

auto SphericalHarmonics::toShaderCoefficients() const -> std::array<math::vec3, 9> {
	constexpr std::array<float, 9> basisConstants{0.282095f, 0.488603f, 0.488603f, 0.488603f, 1.092548f,
												  1.092548f, 0.315392f, 1.092548f, 0.546274f};
	std::array<math::vec3, 9> out{};
	for (size_t i = 0; i < 9; ++i) out[i] = coefficients[i] * basisConstants[i];
	return out;
}

auto SphericalHarmonics::constant(const math::vec3& iRadiance) -> SphericalHarmonics {
	SphericalHarmonics out;
	out.coefficients[0] = iRadiance * std::sqrt(4.f * g_pi);
	return out;
}

auto EnvironmentMap::build(const FloatImage& iImage, const uint32_t iLevelCount, const uint32_t iSampleCount)
		-> EnvironmentMap {
	OWL_PROFILE_FUNCTION()

	EnvironmentMap out;
	if (iImage.isEmpty() || iImage.pixels.size() != static_cast<size_t>(iImage.size.x()) * iImage.size.y()) {
		OWL_CORE_WARN("EnvironmentMap: Cannot build from an empty or inconsistent image.")
		return out;
	}
	FloatImage base = iImage;
	while (base.size.x() > g_maxEnvironmentWidth) { base = downsample(base); }
	out.irradiance = projectIrradiance(base);

	std::vector<math::vec2ui> sizes{base.size};
	while (sizes.size() < std::max(1u, iLevelCount) && sizes.back().x() / 2 >= g_minLevelWidth &&
		   sizes.back().y() / 2 >= g_minLevelHeight) {
		sizes.emplace_back(sizes.back().x() / 2, sizes.back().y() / 2);
	}
	const auto levelCount = static_cast<uint32_t>(sizes.size());

	const auto mips = buildSourceMips(base);
	out.levels.reserve(levelCount);
	out.levels.push_back(std::move(base));
	for (uint32_t i = 1; i < levelCount; ++i) {
		const float roughness = static_cast<float>(i) / static_cast<float>(levelCount - 1);
		out.levels.push_back(filterLevel(mips, sizes[i], roughness, iSampleCount));
	}
	return out;
}

auto loadHdrImage(const std::filesystem::path& iFile) -> std::optional<FloatImage> {
	OWL_PROFILE_FUNCTION()

	stbi_set_flip_vertically_on_load_thread(0);
	int width = 0;
	int height = 0;
	int channels = 0;
	const HdrPixels data{stbi_loadf(iFile.string().c_str(), &width, &height, &channels, 3)};
	auto image = toFloatImage(data, width, height);
	if (!image) {
		OWL_CORE_WARN("EnvironmentMap: Cannot read the HDR image {}.", iFile.string())
	}
	return image;
}

auto decodeHdrImage(const std::span<const uint8_t> iBytes) -> std::optional<FloatImage> {
	OWL_PROFILE_FUNCTION()

	if (iBytes.empty()) {
		OWL_CORE_WARN("EnvironmentMap: Cannot decode an empty HDR image.")
		return std::nullopt;
	}
	stbi_set_flip_vertically_on_load_thread(0);
	int width = 0;
	int height = 0;
	int channels = 0;
	const HdrPixels data{
			stbi_loadf_from_memory(iBytes.data(), static_cast<int>(iBytes.size()), &width, &height, &channels, 3)};
	auto image = toFloatImage(data, width, height);
	if (!image) {
		OWL_CORE_WARN("EnvironmentMap: The bytes are not a readable HDR image.")
	}
	return image;
}

auto computeDfgTable(const uint32_t iSize, const uint32_t iSampleCount) -> std::vector<math::vec3> {
	OWL_PROFILE_FUNCTION()

	const uint32_t size = std::max(1u, iSize);
	const uint32_t count = std::max(1u, iSampleCount);
	std::vector<math::vec3> table(static_cast<size_t>(size) * size);
	parallelRows(size, [&](const uint32_t iRow) -> void {
		const float roughness = (static_cast<float>(iRow) + 0.5f) / static_cast<float>(size);
		const float alpha = std::max(roughness, 0.045f) * std::max(roughness, 0.045f);
		const float sheenAlpha = std::max(roughness * roughness, 0.0078f);
		for (uint32_t col = 0; col < size; ++col) {
			const float nov = (static_cast<float>(col) + 0.5f) / static_cast<float>(size);
			const math::vec3 v{std::sqrt(1.f - nov * nov), 0.f, nov};
			float scale = 0.f;
			float bias = 0.f;
			float sheen = 0.f;
			for (uint32_t k = 0; k < count; ++k) {
				const auto xi = hammersley(k, count);
				const auto h = sampleGgxHalf(xi, alpha);
				const float voh = v * h;
				const float noh = h.z();
				const float nol = 2.f * voh * noh - v.z();
				if (nol > 0.f && voh > 0.f) {
					const float a2 = alpha * alpha;
					const float vis = 0.5f / (nol * std::sqrt(nov * nov * (1.f - a2) + a2) +
											  nov * std::sqrt(nol * nol * (1.f - a2) + a2));
					const float gv = vis * 4.f * nol * voh / noh;
					const float fc = std::pow(1.f - voh, 5.f);
					scale += (1.f - fc) * gv;
					bias += fc * gv;
				}
				// Charlie sheen with cosine-weighted hemisphere samples (pdf = NoL / pi).
				const float phi = 2.f * g_pi * xi.x();
				const float cosL = std::sqrt(1.f - xi.y());
				const float sinL = std::sqrt(xi.y());
				const math::vec3 l{sinL * std::cos(phi), sinL * std::sin(phi), cosL};
				const auto hs = (v + l).normalized();
				const float sin2 = std::max(0.f, 1.f - hs.z() * hs.z());
				const float dCharlie = (2.f + 1.f / sheenAlpha) * std::pow(sin2, 0.5f / sheenAlpha) / (2.f * g_pi);
				const float visSheen = 1.f / (4.f * (cosL + nov - cosL * nov));
				sheen += dCharlie * visSheen * g_pi;
			}
			const float inv = 1.f / static_cast<float>(count);
			table[static_cast<size_t>(iRow) * size + col] = {scale * inv, bias * inv, sheen * inv};
		}
	});
	return table;
}

auto ColorLut::toStrip() const -> std::vector<math::vec3> {
	std::vector<math::vec3> strip(values.size());
	const size_t n = size;
	if (n * n * n != values.size()) {
		return {};
	}
	for (size_t b = 0; b < n; ++b) {
		for (size_t g = 0; g < n; ++g) {
			for (size_t r = 0; r < n; ++r) { strip[g * n * n + b * n + r] = values[r + g * n + b * n * n]; }
		}
	}
	return strip;
}

auto ColorLut::sample(const math::vec3& iColor) const -> math::vec3 {
	const size_t n = size;
	if (n < 2 || n * n * n != values.size()) {
		return iColor;
	}
	std::array<float, 3> pos{};
	for (size_t c = 0; c < 3; ++c) {
		const float range = domainMax[c] - domainMin[c];
		const float t = range > 0.f ? (iColor[c] - domainMin[c]) / range : 0.f;
		pos[c] = std::clamp(t, 0.f, 1.f) * static_cast<float>(n - 1);
	}
	std::array<size_t, 3> lo{};
	std::array<float, 3> frac{};
	for (size_t c = 0; c < 3; ++c) {
		lo[c] = std::min(static_cast<size_t>(pos[c]), n - 2);
		frac[c] = pos[c] - static_cast<float>(lo[c]);
	}
	const auto at = [&](const size_t iR, const size_t iG, const size_t iB) -> const math::vec3& {
		return values[iR + iG * n + iB * n * n];
	};
	const auto lerpRed = [&](const size_t iG, const size_t iB) -> math::vec3 {
		return lerp(at(lo[0], iG, iB), at(lo[0] + 1, iG, iB), frac[0]);
	};
	const auto g0 = lerp(lerpRed(lo[1], lo[2]), lerpRed(lo[1] + 1, lo[2]), frac[1]);
	const auto g1 = lerp(lerpRed(lo[1], lo[2] + 1), lerpRed(lo[1] + 1, lo[2] + 1), frac[1]);
	return lerp(g0, g1, frac[2]);
}

auto ColorLut::parse(const std::string_view iText) -> std::optional<ColorLut> {
	ColorLut lut;
	size_t expected = 0;
	size_t lineNumber = 0;
	std::string_view rest = iText;
	while (!rest.empty()) {
		const auto end = rest.find('\n');
		const auto line = trim(rest.substr(0, end));
		rest = end == std::string_view::npos ? std::string_view{} : rest.substr(end + 1);
		++lineNumber;
		if (line.empty() || line.front() == '#') {
			continue;
		}
		const bool isKeyword = std::isalpha(static_cast<unsigned char>(line.front())) != 0;
		if (!isKeyword) {
			math::vec3 value;
			if (expected == 0) {
				OWL_CORE_WARN("ColorLut: Line {} has data before LUT_3D_SIZE.", lineNumber)
				return std::nullopt;
			}
			if (!parseTriplet(line, value)) {
				OWL_CORE_WARN("ColorLut: Line {} is not three numbers.", lineNumber)
				return std::nullopt;
			}
			if (lut.values.size() >= expected) {
				OWL_CORE_WARN("ColorLut: More than {} values.", expected)
				return std::nullopt;
			}
			lut.values.push_back(value);
			continue;
		}
		const auto split = line.find_first_of(" \t");
		const auto keyword = line.substr(0, split);
		const auto args = split == std::string_view::npos ? std::string_view{} : trim(line.substr(split));
		if (keyword == "LUT_1D_SIZE") {
			OWL_CORE_WARN("ColorLut: 1D LUTs are not supported.")
			return std::nullopt;
		}
		if (keyword == "LUT_3D_SIZE") {
			uint32_t n = 0;
			const auto [ptr, ec] = std::from_chars(args.data(), args.data() + args.size(), n);
			if (ec != std::errc{} || ptr != args.data() + args.size() || n < 2 || n > g_maxLutSize) {
				OWL_CORE_WARN("ColorLut: Invalid LUT_3D_SIZE '{}'.", std::string{args})
				return std::nullopt;
			}
			lut.size = n;
			expected = static_cast<size_t>(n) * n * n;
			lut.values.reserve(expected);
		} else if (keyword == "DOMAIN_MIN" || keyword == "DOMAIN_MAX") {
			math::vec3 value;
			if (!parseTriplet(args, value)) {
				OWL_CORE_WARN("ColorLut: Line {} has an invalid {}.", lineNumber, std::string{keyword})
				return std::nullopt;
			}
			(keyword == "DOMAIN_MIN" ? lut.domainMin : lut.domainMax) = value;
		}
	}
	if (expected == 0) {
		OWL_CORE_WARN("ColorLut: LUT_3D_SIZE is missing.")
		return std::nullopt;
	}
	if (lut.values.size() != expected) {
		OWL_CORE_WARN("ColorLut: Expected {} values, found {}.", expected, lut.values.size())
		return std::nullopt;
	}
	return lut;
}

auto ColorLut::load(const std::filesystem::path& iFile) -> std::optional<ColorLut> {
	std::ifstream stream(iFile, std::ios::binary);
	if (!stream) {
		OWL_CORE_WARN("ColorLut: Cannot open {}.", iFile.string())
		return std::nullopt;
	}
	const std::string text{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
	return parse(text);
}

auto loadHdrAsset(const std::filesystem::path& iAssetPath) -> std::optional<FloatImage> {
	if (iAssetPath.empty())
		return std::nullopt;
	if (app::Application::instanced()) {
		const auto& app = app::Application::get();
		if (const std::string rel = iAssetPath.generic_string(); app.hasOpenPack() && app.packContains(rel)) {
			if (const auto bytes = app.loadFromPack(rel); bytes)
				return decodeHdrImage(*bytes);
		}
		if (iAssetPath.is_relative()) {
			for (const auto& [title, assetsPath]: app.getAssetDirectories()) {
				if (const auto full = assetsPath / iAssetPath; exists(full))
					return loadHdrImage(full);
			}
		}
	}
	if (exists(iAssetPath))
		return loadHdrImage(iAssetPath);
	OWL_CORE_WARN("EnvironmentMap: {} not found.", iAssetPath.generic_string())
	return std::nullopt;
}

}// namespace owl::renderer
