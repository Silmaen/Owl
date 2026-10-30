/**
 * @file LensFlare.h
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once
#include "Camera.h"
#include "RenderGraph.h"
#include "core/Core.h"
#include "math/matrices.h"
#include "math/vectors.h"

#include <cstdint>
#include <span>

namespace owl::renderer {

/**
 * @brief
 *  A light that flares: a point in the world, or a direction for the sun.
 */
struct OWL_API LensFlareSource {
	/// World position of the light, or the direction towards it when `directional`.
	math::vec3 position{0.f, 0.f, 0.f};
	/// Whether the light is at infinity (a directional light, the sun).
	bool directional = false;
	/// Colour of its flare, linear and pre-exposed.
	math::vec3 color{0.f, 0.f, 0.f};
};

/**
 * @brief
 *  Lens flares of the bright lights and the sun, as a post-process of the linear scene colour.
 *
 * The flare pass copies the scene colour into a target of its own (the entity ids of the scene colour stay untouched),
 * then adds for each source a glow with streaks, a halo around the centre of the frame and ghosts along the line from
 * the source through the centre. Each source is occlusion tested on the GPU against the depth of the frame (16 taps
 * around it), so a light hidden behind a wall does not flare and one half hidden flares half.
 */
class OWL_API LensFlare final {
public:
	/**
	 * @brief
	 *  Default constructor (deleted: static class).
	 */
	LensFlare() = delete;

	/**
	 * @brief
	 *  Destructor.
	 */
	~LensFlare() = default;

	/**
	 * @brief
	 *  Copy constructor (deleted).
	 */
	LensFlare(const LensFlare&) = delete;

	/**
	 * @brief
	 *  Move constructor (deleted).
	 */
	LensFlare(LensFlare&&) = delete;

	/**
	 * @brief
	 *  Copy assignment (deleted).
	 * @return This object.
	 */
	auto operator=(const LensFlare&) -> LensFlare& = delete;

	/**
	 * @brief
	 *  Move assignment (deleted).
	 * @return This object.
	 */
	auto operator=(LensFlare&&) -> LensFlare& = delete;

	/// Most sources flaring in a frame (the brightest are kept).
	static constexpr uint32_t g_maxSources = 8;
	/// Most ghosts per source.
	static constexpr uint32_t g_maxGhosts = 8;
	/// Largest brightness of a flare, so a sun at full exposure stays a flare and not a white frame.
	static constexpr float g_maxBrightness = 16.f;

	/**
	 * @brief
	 *  Create the draws and the uniform block.
	 */
	static void init();

	/**
	 * @brief
	 *  Release the draws and the uniform block.
	 */
	static void shutdown();

	/**
	 * @brief
	 *  Add the lens flares of a camera to a frame graph.
	 * @param[in,out] ioGraph The graph.
	 * @param[in] iColor The linear scene colour.
	 * @param[in] iDepth The depth of the frame (reversed-Z, 0 where empty).
	 * @param[in] iSettings The post-process settings of the camera (`lensFlare*`).
	 * @param[in] iViewProjection The view-projection of the camera.
	 * @param[in] iSources The flaring lights, at most `g_maxSources` are kept.
	 * @param[in] iFrameSize Size of the frame in pixels.
	 * @return The colour with the flares, invalid when nothing flares (keep `iColor`).
	 */
	[[nodiscard]] static auto recordLensFlares(RenderGraph& ioGraph, RenderGraph::Resource iColor,
											   RenderGraph::Resource iDepth, const PostProcessSettings& iSettings,
											   const math::mat4& iViewProjection,
											   std::span<const LensFlareSource> iSources,
											   const math::vec2ui& iFrameSize) -> RenderGraph::Resource;

	/**
	 * @brief
	 *  Position of a ghost along the line from the centre of the frame (0) to the source (1).
	 * @param[in] iGhost Index of the ghost.
	 * @param[in] iSpacing Distance between two ghosts, as a fraction of that line.
	 * @return The position, negative beyond the centre.
	 */
	[[nodiscard]] static auto ghostOffset(uint32_t iGhost, float iSpacing) -> float;

	/**
	 * @brief
	 *  Brightness of the flare of a light seen from a point: the pre-exposed illuminance it gives there (inverse square
	 *  for a point light), times its flare strength, clamped to `g_maxBrightness`.
	 * @param[in] iIntensity Luminous intensity in candela (or illuminance in lux when `iDistance` is 0, a sun).
	 * @param[in] iDistance Distance to the light, 0 for a directional light.
	 * @param[in] iExposure The exposure of the camera.
	 * @param[in] iStrength The flare strength of the light.
	 * @return The brightness.
	 */
	[[nodiscard]] static auto brightnessOf(float iIntensity, float iDistance, float iExposure, float iStrength)
			-> float;
};

}// namespace owl::renderer
