/**
 * @file PostProcess.h
 * @author Silmaen
 * @date 21/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Camera.h"
#include "RenderGraph.h"
#include "core/Core.h"
#include "gpu/Texture.h"
#include "math/vectors.h"

#include <cstdint>
#include <filesystem>

namespace owl::renderer {

/**
 * @brief
 *  The post-process passes of a camera ahead of the colour encoding: the bloom chain, and the colour-grading LUTs.
 */
class OWL_API PostProcess {
public:
	/**
	 * @brief
	 *  Default constructor.
	 */
	PostProcess() = default;

	/**
	 * @brief
	 *  Destructor.
	 */
	~PostProcess() = default;

	/**
	 * @brief
	 *  Copy constructor (deleted).
	 */
	PostProcess(const PostProcess&) = delete;

	/**
	 * @brief
	 *  Move constructor (deleted).
	 */
	PostProcess(PostProcess&&) = delete;

	/**
	 * @brief
	 *  Copy assignment (deleted).
	 * @return This object.
	 */
	auto operator=(const PostProcess&) -> PostProcess& = delete;

	/**
	 * @brief
	 *  Move assignment (deleted).
	 * @return This object.
	 */
	auto operator=(PostProcess&&) -> PostProcess& = delete;

	/**
	 * @brief
	 *  Create the draw and the uniform block of the bloom chain.
	 */
	static void init();

	/**
	 * @brief
	 *  Release the draw, the uniform block and the cached LUTs.
	 */
	static void shutdown();

	/// Most levels of the bloom chain.
	static constexpr uint32_t g_maxBloomLevels = 6;

	/**
	 * @brief
	 *  Number of bloom levels for a frame: halving from half the frame size while both sides stay at least 8 pixels.
	 * @param[in] iFrameSize Size of the frame.
	 * @return The level count, 0 for a frame too small.
	 */
	[[nodiscard]] static auto getBloomLevelCount(const math::vec2ui& iFrameSize) -> uint32_t;

	/**
	 * @brief
	 *  Add the bloom chain of a camera to a frame graph: a thresholded downsample of the scene colour, the
	 *  downsamples, then the upsamples adding each level back.
	 * @param[in,out] ioGraph The frame graph.
	 * @param[in] iSceneColor The linear scene colour.
	 * @param[in] iSettings The post-process settings of the camera.
	 * @param[in] iFrameSize Size of the frame.
	 * @param[in] iExposure The adapted exposure of `recordExposure`, applied before the threshold; invalid for none.
	 * @return The half-size bloom target to add to the scene colour, invalid when the bloom is off.
	 */
	[[nodiscard]] static auto recordBloom(RenderGraph& ioGraph, RenderGraph::Resource iSceneColor,
										  const PostProcessSettings& iSettings, const math::vec2ui& iFrameSize,
										  RenderGraph::Resource iExposure = {}) -> RenderGraph::Resource;

	/**
	 * @brief
	 *  Add the automatic exposure of a camera to a frame graph: the log-average luminance of the scene colour,
	 *  reduced to one texel, then the EV100 adapted towards it at `autoExposureSpeed` from the previous frame's.
	 * @param[in,out] ioGraph The frame graph.
	 * @param[in] iSceneColor The linear scene colour, pre-exposed at `iSettings.ev100`.
	 * @param[in] iSettings The post-process settings of the camera.
	 * @param[in] iFrameSize Size of the frame.
	 * @return The one-texel target holding the adapted EV100 in red, invalid when the exposure is manual.
	 */
	[[nodiscard]] static auto recordExposure(RenderGraph& ioGraph, RenderGraph::Resource iSceneColor,
											 const PostProcessSettings& iSettings, const math::vec2ui& iFrameSize)
			-> RenderGraph::Resource;

	/// Edge of the target the scene luminance is measured into before its reduction.
	static constexpr uint32_t g_exposureMeasureSize = 64;

	/**
	 * @brief
	 *  A colour-grading LUT as a texture strip (`ColorLut::toStrip`), loaded once per asset path.
	 */
	struct Lut {
		/// The strip, RGBA float, `size * size` by `size`.
		shared<gpu::Texture2D> texture;
		/// Edge of the cube.
		uint32_t size = 0;
	};

	/**
	 * @brief
	 *  The LUT of a `.cube` asset (the open pack first, then the asset directories).
	 * @param[in] iAssetPath Asset path of the file.
	 * @return The LUT, with a null texture when it cannot be read (logged once).
	 */
	[[nodiscard]] static auto getLut(const std::filesystem::path& iAssetPath) -> const Lut&;
};

}// namespace owl::renderer
