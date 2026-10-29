/**
 * @file ReflectionProbes.h
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "renderer/gpu/Texture.h"

#include <cstdint>

namespace owl::renderer {

/// Most prefiltered levels of a reflection probe cube.
constexpr uint32_t g_maxProbeLevels = 6;

/**
 * @brief
 *  Get the number of prefiltered levels of a reflection probe cube: down to faces of 8 texels, at most
 *  `g_maxProbeLevels`.
 * @param[in] iResolution Size of a face of level 0, in texels.
 * @return The level count, at least 1.
 */
[[nodiscard]] OWL_API auto getProbeLevelCount(uint32_t iResolution) -> uint32_t;

/**
 * @brief
 *  Get the GGX alpha a level of a probe cube is filtered with: level `i` holds the radiance for the roughness
 *  `i / (levelCount - 1)`.
 * @param[in] iLevel The level.
 * @param[in] iLevelCount Number of levels of the cube.
 * @return The alpha (roughness squared), 0 for level 0.
 */
[[nodiscard]] OWL_API auto getProbeFilterAlpha(uint32_t iLevel, uint32_t iLevelCount) -> float;

/**
 * @brief
 *  The full-screen draws that prefilter a reflection probe, one face of one level at a time, into the bound `Rgba16F`
 *  target (copied into a cube by `Framebuffer::copyAttachmentToCube`): the capture first gets its mip chain
 *  (`drawDownsampledFace`), then each level of the probe is GGX filtered from that chain (`drawFilteredFace`).
 */
class OWL_API ReflectionProbes final {
public:
	/**
	 * @brief
	 *  Create the full-screen draw and its uniform buffer.
	 */
	static void init();

	/**
	 * @brief
	 *  Release the draw and its uniform buffer.
	 */
	static void shutdown();

	/**
	 * @brief
	 *  Check whether `init` ran.
	 * @return True between `init` and `shutdown`.
	 */
	[[nodiscard]] static auto isReady() -> bool;

	/**
	 * @brief
	 *  Draw one face of a level of the mip chain of a capture: the level above it, halved.
	 * @param[in] iCapture The captured cube, its levels above `iLevel` already filled.
	 * @param[in] iFace The face, 0 to 5 (+X, -X, +Y, -Y, +Z, -Z).
	 * @param[in] iLevel The level drawn, from 1.
	 */
	static void drawDownsampledFace(const shared<gpu::TextureCube>& iCapture, uint32_t iFace, uint32_t iLevel);

	/**
	 * @brief
	 *  Draw one face of a level of a probe, GGX filtered for the roughness of the level from the whole mip chain of
	 *  its capture.
	 * @param[in] iCapture The captured cube with all its levels.
	 * @param[in] iFace The face, 0 to 5 (+X, -X, +Y, -Y, +Z, -Z).
	 * @param[in] iLevel The level drawn, from 1.
	 */
	static void drawFilteredFace(const shared<gpu::TextureCube>& iCapture, uint32_t iFace, uint32_t iLevel);

private:
	/**
	 * @brief
	 *  Draw a face with the parameters of the shader.
	 * @param[in] iCapture The captured cube.
	 * @param[in] iFace The face.
	 * @param[in] iMode 0 downsample, 1 GGX filter.
	 * @param[in] iLevel The level drawn.
	 */
	static void drawFace(const shared<gpu::TextureCube>& iCapture, uint32_t iFace, int32_t iMode, uint32_t iLevel);
};

}// namespace owl::renderer
