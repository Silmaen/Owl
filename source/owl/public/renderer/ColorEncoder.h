/**
 * @file ColorEncoder.h
 * @author Silmaen
 * @date 21/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Camera.h"
#include "core/Core.h"
#include "gpu/Texture.h"

#include <cstdint>

namespace owl::renderer {

/**
 * @brief
 *  What the encoding applies besides the tone mapping.
 */
struct EncodePost {
	/// The post-process settings of the camera.
	const PostProcessSettings* settings = nullptr;
	/// The half-size bloom chain result, null without bloom.
	shared<gpu::Texture2D> bloom;
	/// The colour-grading LUT strip, null without one.
	shared<gpu::Texture2D> lut;
	/// Edge of the LUT cube.
	uint32_t lutSize = 0;
	/// Size of the frame in pixels.
	math::vec2ui frameSize{1, 1};
	/// The one-texel adapted EV100 of the automatic exposure, null for the manual `settings->ev100`.
	shared<gpu::Texture2D> exposure;
};

/**
 * @brief
 *  Last pass of the world chain: tone maps the linear scene colour and encodes it to sRGB into the bound display
 *  target, pixel for pixel (the scene colour has the size of the target).
 */
class OWL_API ColorEncoder {
public:
	/**
	 * @brief
	 *  Default constructor.
	 */
	ColorEncoder() = default;

	/**
	 * @brief
	 *  Destructor.
	 */
	~ColorEncoder() = default;

	/**
	 * @brief
	 *  Copy constructor (deleted).
	 */
	ColorEncoder(const ColorEncoder&) = delete;

	/**
	 * @brief
	 *  Move constructor (deleted).
	 */
	ColorEncoder(ColorEncoder&&) = delete;

	/**
	 * @brief
	 *  Copy assignment (deleted).
	 * @return This encoder.
	 */
	auto operator=(const ColorEncoder&) -> ColorEncoder& = delete;

	/**
	 * @brief
	 *  Move assignment (deleted).
	 * @return This encoder.
	 */
	auto operator=(ColorEncoder&&) -> ColorEncoder& = delete;

	/**
	 * @brief
	 *  Create the draw and the uniform block.
	 */
	static void init();

	/**
	 * @brief
	 *  Release the draw and the uniform block.
	 */
	static void shutdown();

	/**
	 * @brief
	 *  Draw the encoded scene colour over the whole bound target.
	 * @param[in] iSceneColor The linear scene colour, shader readable.
	 * @param[in] iToneMapping Tone mapping applied before the encoding.
	 * @param[in] iPost Exposure, bloom, vignette and LUT of the camera; empty for none.
	 */
	static void draw(const shared<gpu::Texture2D>& iSceneColor, ToneMapping iToneMapping, const EncodePost& iPost = {});

	/**
	 * @brief
	 *  Decode an sRGB channel to linear (exact piecewise curve), as the world shaders do.
	 * @param[in] iValue The sRGB value, in [0, 1].
	 * @return The linear value.
	 */
	[[nodiscard]] static auto srgbToLinear(float iValue) -> float;
};

}// namespace owl::renderer
