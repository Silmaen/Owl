/**
 * @file AreaLights.h
 * @author Silmaen
 * @date 28/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "math/vectors.h"

#include <cstdint>
#include <span>
#include <vector>

namespace owl::renderer {

/// Width and height of the tables of the area lights.
constexpr uint32_t g_ltcTableSize = 64;

/**
 * @brief
 *  Fit the linearly transformed cosines (Heitz et al. 2016) of the GGX specular lobe, with the height-correlated
 *  Smith visibility and no Fresnel.
 *
 * Column `i` is the perceptual roughness `i / (size - 1)`, row `j` the view angle with `sqrt(1 - N.V) = j / (size - 1)`.
 * Each texel holds the inverse matrix of its cosine, in the frame of the view (X towards the view, Z the normal),
 * scaled to a middle element of 1: `(m00, m20, m02, m22)`, the other elements 0.
 * @param[in] iSize Width and height of the table (2 at least).
 * @return `iSize * iSize` texels, row by row.
 */
[[nodiscard]] OWL_API auto computeLtcTable(uint32_t iSize = g_ltcTableSize) -> std::vector<math::vec4>;

/**
 * @brief
 *  The table of `computeLtcTable` at `g_ltcTableSize`, fitted once and stored in the engine.
 * @return `g_ltcTableSize * g_ltcTableSize` texels.
 */
[[nodiscard]] OWL_API auto getLtcTable() -> std::span<const math::vec4>;

/**
 * @brief
 *  Error of a linearly transformed cosine against the GGX lobe it stands for: the mean of the cubed difference of
 *  the two distributions, weighted by their sampling densities (the error the fit minimises).
 * @param[in] iRoughness Perceptual roughness.
 * @param[in] iCosTheta Cosine between the normal and the view.
 * @param[in] iInverse Inverse matrix of the cosine, as stored in the table.
 * @return The error, 0 for a perfect match.
 */
[[nodiscard]] OWL_API auto getLtcFitError(float iRoughness, float iCosTheta, const math::vec4& iInverse) -> float;

/**
 * @brief
 *  Table of the horizon-clipped sphere: the part above the horizon of the form factor of a sphere, divided by its
 *  unclipped form factor. It turns the vector form factor of a polygon or a disc into a clipped one.
 *
 * Column `i` is the cosine of the elevation of the sphere `2 * i / (size - 1) - 1`, row `j` its unclipped form factor
 * `j / (size - 1)` (the squared sine of its angular radius).
 * @param[in] iSize Width and height of the table (2 at least).
 * @return `iSize * iSize` values, row by row.
 */
[[nodiscard]] OWL_API auto computeClippedSphereTable(uint32_t iSize = g_ltcTableSize) -> std::vector<float>;

/**
 * @brief
 *  Luminance of a rectangle light emitting its power from one face, like a diffuse panel.
 * @param[in] iPower Luminous power, in lumens.
 * @param[in] iSize Width and height of the rectangle, in world units.
 * @return The luminance, in nits.
 */
[[nodiscard]] OWL_API auto getRectangleLuminance(float iPower, const math::vec2& iSize) -> float;

/**
 * @brief
 *  Luminance of a disc light emitting its power from one face, like a diffuse panel.
 * @param[in] iPower Luminous power, in lumens.
 * @param[in] iRadius Radius of the disc, in world units.
 * @return The luminance, in nits.
 */
[[nodiscard]] OWL_API auto getDiscLuminance(float iPower, float iRadius) -> float;

}// namespace owl::renderer
