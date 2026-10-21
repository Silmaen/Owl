/**
 * @file EnvironmentMap.h
 * @author Silmaen
 * @date 21/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "math/vectors.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace owl::renderer {

/**
 * @brief
 *  A linear RGB image stored as 32-bit floats, rows top first.
 */
struct OWL_API FloatImage {
	/// Size in pixels.
	math::vec2ui size{0, 0};
	/// `size.x * size.y` RGB triplets.
	std::vector<math::vec3> pixels;

	/**
	 * @brief
	 *  Check whether the image has no pixel.
	 * @return True when empty.
	 */
	[[nodiscard]] auto isEmpty() const -> bool { return pixels.empty(); }

	/**
	 * @brief
	 *  The pixel at a position.
	 * @param[in] iX Column.
	 * @param[in] iY Row, 0 at the top.
	 * @return The pixel.
	 */
	[[nodiscard]] auto at(const uint32_t iX, const uint32_t iY) const -> const math::vec3& {
		return pixels[static_cast<size_t>(iY) * size.x() + iX];
	}
};

/**
 * @brief
 *  Order-2 spherical harmonics of the irradiance around a point (9 RGB coefficients), already convolved with the
 *  cosine lobe: `evaluate(normal)` is the diffuse irradiance divided by pi.
 */
struct OWL_API SphericalHarmonics {
	/// Coefficients in the order (0,0), (1,-1), (1,0), (1,1), (2,-2), (2,-1), (2,0), (2,1), (2,2).
	std::array<math::vec3, 9> coefficients{};

	/**
	 * @brief
	 *  Irradiance arriving around a world direction, divided by pi (multiply by the albedo for the diffuse colour).
	 * @param[in] iNormal Unit direction.
	 * @return The value.
	 */
	[[nodiscard]] auto evaluate(const math::vec3& iNormal) const -> math::vec3;

	/**
	 * @brief
	 *  Harmonics of a uniform sky of one radiance.
	 * @param[in] iRadiance The radiance.
	 * @return The harmonics.
	 */
	[[nodiscard]] static auto constant(const math::vec3& iRadiance) -> SphericalHarmonics;

	/**
	 * @brief
	 *  Coefficients times their basis constant, for a shader evaluating the bare polynomials
	 *  (1, z, y, x, xz, zy, 3y^2 - 1, xy, x^2 - z^2): their weighted sum equals `evaluate` before its clamp.
	 * @return The scaled coefficients.
	 */
	[[nodiscard]] auto toShaderCoefficients() const -> std::array<math::vec3, 9>;
};

/**
 * @brief
 *  Mapping of an equirectangular image: column = azimuth, row = elevation. Direction (x, y, z), Y up:
 *  `u = 0.5 + atan2(x, -z) / (2 pi)`, `v = acos(y) / pi` (v = 0 at the zenith, top row).
 */
struct OWL_API Equirectangular {
	/**
	 * @brief
	 *  Texture coordinates of a direction.
	 * @param[in] iDirection Unit direction.
	 * @return (u, v) in [0, 1].
	 */
	[[nodiscard]] static auto toUv(const math::vec3& iDirection) -> math::vec2;

	/**
	 * @brief
	 *  Direction of texture coordinates.
	 * @param[in] iUv (u, v) in [0, 1].
	 * @return Unit direction.
	 */
	[[nodiscard]] static auto toDirection(const math::vec2& iUv) -> math::vec3;

	/**
	 * @brief
	 *  Bilinear sample of an image at a direction (wraps in u, clamps in v).
	 * @param[in] iImage The image.
	 * @param[in] iDirection Unit direction.
	 * @return The sample.
	 */
	[[nodiscard]] static auto sample(const FloatImage& iImage, const math::vec3& iDirection) -> math::vec3;
};

/**
 * @brief
 *  Image-based lighting built from an equirectangular HDR image.
 */
struct OWL_API EnvironmentMap {
	/**
	 * @brief
	 *  Radiance prefiltered by roughness: level 0 is the source (resized to at most `g_maxEnvironmentWidth`), level i
	 *  is half the size of level i - 1 and holds the GGX-filtered radiance for roughness i / (levelCount - 1).
	 */
	std::vector<FloatImage> levels;
	/// Diffuse irradiance.
	SphericalHarmonics irradiance;

	/**
	 * @brief
	 *  Build the prefiltered levels and the harmonics of an image, rows spread over the hardware threads. Takes some
	 *  hundred milliseconds for a 1024x512 image: call it off the frame (the scene builds its environment on a worker).
	 * @param[in] iImage The equirectangular radiance.
	 * @param[in] iLevelCount Number of prefiltered levels, at least 1 (at most until a level would be under 8x4).
	 * @param[in] iSampleCount GGX importance samples per texel of the filtered levels.
	 * @return The environment.
	 */
	[[nodiscard]] static auto build(const FloatImage& iImage, uint32_t iLevelCount = 6, uint32_t iSampleCount = 64)
			-> EnvironmentMap;
};

/// Widest level 0 of an environment, in pixels (2:1 aspect).
constexpr uint32_t g_maxEnvironmentWidth = 1024;

/**
 * @brief
 *  Read an equirectangular Radiance `.hdr` (RGBE) file.
 * @param[in] iFile Path of the file.
 * @return The image, nothing when it cannot be read.
 */
[[nodiscard]] OWL_API auto loadHdrImage(const std::filesystem::path& iFile) -> std::optional<FloatImage>;

/**
 * @brief
 *  Read an equirectangular Radiance `.hdr` image from memory (a pack entry).
 * @param[in] iBytes The bytes of the file.
 * @return The image, nothing when they are not an HDR image.
 */
[[nodiscard]] OWL_API auto decodeHdrImage(std::span<const uint8_t> iBytes) -> std::optional<FloatImage>;

/**
 * @brief
 *  Read an `.hdr` image by its asset path: the open pack first, then the asset directories, then the path itself.
 * @param[in] iAssetPath Asset-relative (or absolute) path.
 * @return The image, nothing when it cannot be found or read.
 */
[[nodiscard]] OWL_API auto loadHdrAsset(const std::filesystem::path& iAssetPath) -> std::optional<FloatImage>;

/**
 * @brief
 *  Split-sum tables of the image-based lighting, indexed by (N.V, perceptual roughness), both in [0, 1] with texel
 *  centres at (i + 0.5) / size: R = DFG scale, G = DFG bias (Karis split sum with the height-correlated Smith GGX
 *  visibility, so that `f0 * R + G` is the directional albedo of the specular lobe and `R + G` its multiple-scattering
 *  normalisation, Fdez-Agüera), B = directional albedo of the Charlie sheen lobe with the Neubelt visibility.
 * @param[in] iSize Width and height of the table.
 * @param[in] iSampleCount Samples per texel.
 * @return `iSize * iSize` RGB values, row 0 is roughness 0.
 */
[[nodiscard]] OWL_API auto computeDfgTable(uint32_t iSize = 64, uint32_t iSampleCount = 256) -> std::vector<math::vec3>;

/**
 * @brief
 *  A colour-grading 3D LUT read from an Adobe / Resolve `.cube` file.
 */
struct OWL_API ColorLut {
	/// Edge size N of the cube.
	uint32_t size = 0;
	/// `N^3` RGB values, red fastest, then green, then blue (the `.cube` order).
	std::vector<math::vec3> values;

	/**
	 * @brief
	 *  Check whether the LUT has no value.
	 * @return True when empty.
	 */
	[[nodiscard]] auto isEmpty() const -> bool { return values.empty(); }

	/**
	 * @brief
	 *  The LUT laid out as a 2D strip for a shader: `N` slices of `N x N` side by side, width `N * N`, height `N`;
	 *  texel (r + b * N, g) holds the value of (r, g, b).
	 * @return `N^3` values, row-major, row 0 is g = 0.
	 */
	[[nodiscard]] auto toStrip() const -> std::vector<math::vec3>;

	/**
	 * @brief
	 *  Trilinear lookup, for tests and the CPU.
	 * @param[in] iColor Input colour in [0, 1].
	 * @return The graded colour.
	 */
	[[nodiscard]] auto sample(const math::vec3& iColor) const -> math::vec3;

	/**
	 * @brief
	 *  Parse a `.cube` text: `LUT_3D_SIZE`, optional `DOMAIN_MIN` / `DOMAIN_MAX` (applied to the lookups), comments,
	 *  then the `N^3` triplets.
	 * @param[in] iText The text.
	 * @return The LUT, nothing on a malformed text (logged).
	 */
	[[nodiscard]] static auto parse(std::string_view iText) -> std::optional<ColorLut>;

	/**
	 * @brief
	 *  Read a `.cube` file.
	 * @param[in] iFile Path of the file.
	 * @return The LUT, nothing when it cannot be read or parsed.
	 */
	[[nodiscard]] static auto load(const std::filesystem::path& iFile) -> std::optional<ColorLut>;

	/// Smallest input of the domain.
	math::vec3 domainMin{0.f, 0.f, 0.f};
	/// Largest input of the domain.
	math::vec3 domainMax{1.f, 1.f, 1.f};
};

}// namespace owl::renderer
