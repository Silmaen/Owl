/**
 * @file Camera.h
 * @author Silmaen
 * @date 23/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "math/matrixCreation.h"
#include "math/vectors.h"

#include <cstdint>
#include <string>

namespace owl::renderer {

/**
 * @brief
 *  The post-process stack of a camera, applied to the linear scene colour before the encoding.
 */
struct PostProcessSettings {
	/// Exposure value at ISO 100 the camera is set to (15 sunny day, 12 overcast, 8 bright interior, 5 dim room, 3 candle light).
	float ev100 = 15.f;
	/// Whether the exposure follows the average brightness of the frame instead of `ev100`.
	bool autoExposure = false;
	/// Darkest scene the automatic exposure adapts to, in EV100.
	float autoExposureMin = -2.f;
	/// Brightest scene the automatic exposure adapts to, in EV100.
	float autoExposureMax = 16.f;
	/// Speed of the automatic exposure, in EV per second.
	float autoExposureSpeed = 2.f;
	/// Exposure compensation, in EV, on top of `ev100` or the automatic exposure (+1 doubles the brightness).
	float exposure = 0.f;
	/// Whether the bright parts glow.
	bool bloom = false;
	/// Luminance above which the colour blooms.
	float bloomThreshold = 1.f;
	/// Softness of the threshold, 0 hard to 1 soft.
	float bloomKnee = 0.5f;
	/// Strength of the glow added to the scene.
	float bloomIntensity = 0.1f;
	/// Spread of the glow, 0 tight to 1 wide.
	float bloomScatter = 0.7f;
	/// Whether the edges of the image darken.
	bool vignette = false;
	/// Strength of the vignette.
	float vignetteIntensity = 0.3f;
	/// Softness of the vignette edge.
	float vignetteSmoothness = 0.4f;
	/// Colour the edges fade to, sRGB.
	math::vec3 vignetteColor{0.f, 0.f, 0.f};
	/// Asset path of a `.cube` colour-grading LUT, empty for none.
	std::string lut;
	/// Weight of the LUT, 0 to 1.
	float lutContribution = 1.f;
	/// Whether the red and blue channels split towards the edges of the image, like a simple lens.
	bool chromaticAberration = false;
	/// Strength of the split, 0 to 1 (1: the channels 1 % of the frame width apart at the corners).
	float chromaticAberrationIntensity = 0.3f;
	/// Whether a film grain covers the image, a new one each frame.
	bool filmGrain = false;
	/// Strength of the grain, 0 to 1, strongest in the mid-tones.
	float filmGrainIntensity = 0.15f;
	/// Size of a grain, in pixels, 1 to 4.
	float filmGrainSize = 1.5f;
	/// Whether what is away from the focus distance blurs (depth of field).
	bool depthOfField = false;
	/// Distance from the camera that stays sharp, in world units.
	float focusDistance = 5.f;
	/// Distance from the focus over which the blur grows to its largest, in world units.
	float focusRange = 5.f;
	/// Largest blur radius, in pixels of a frame 1080 pixels high.
	float bokehRadius = 8.f;
	/// Whether the image blurs along the motion of the camera, in Play.
	bool motionBlur = false;
	/// Fraction of the frame the shutter stays open, 0 to 1.
	float motionBlurIntensity = 0.5f;
	/// Whether the lights with a flare strength (and the sun) flare in the lens, hidden ones excepted.
	bool lensFlare = false;
	/// Strength of every flare.
	float lensFlareIntensity = 1.f;
	/// Number of ghosts along the line from a light through the centre of the frame, 0 to 8.
	uint32_t lensFlareGhosts = 4;
	/// Distance between two ghosts, as a fraction of the line from the centre to the light.
	float lensFlareSpacing = 0.45f;
	/// Radius of the halo around the centre of the frame, as a fraction of the frame height.
	float lensFlareHalo = 0.6f;
	/// Size of the glow at a light, as a fraction of the frame height.
	float lensFlareSize = 0.12f;
};

/**
 * @brief
 *  How a camera maps the linear scene colour to the display range before the sRGB encoding.
 */
enum struct ToneMapping : uint8_t {
	None,///< Clamp to [0, 1]: the authored colours come out unchanged.
	PbrNeutral,///< Khronos PBR Neutral: keeps the base colours, rolls off the highlights.
	Aces,///< ACES filmic fit (Narkowicz): contrasted, desaturated highlights.
};

/**
 * @brief
 *  Base class for camera.
 */
class OWL_API Camera {
public:
	/**
	 * @brief
	 *  Copy constructor.
	 */
	Camera(const Camera&) = default;

	/**
	 * @brief
	 *  Move constructor.
	 */
	Camera(Camera&&) = default;

	/**
	 * @brief
	 *  Copy assignment operator.
	 * @return A reference to this object.
	 */
	auto operator=(const Camera&) -> Camera& = default;

	/**
	 * @brief
	 *  Move assignment operator.
	 * @return A reference to this object.
	 */
	auto operator=(Camera&&) -> Camera& = default;

	Camera() = default;

	/**
	 * @brief
	 *  Destructor.
	 */
	virtual ~Camera();

	/**
	 * @brief
	 *  Construct the camera with a projection.
	 * @param[in] iProjection projection matrix.
	 */
	explicit Camera(const math::mat4& iProjection) : m_projection{iProjection} {}

	/**
	 * @brief
	 *  Get the projection matrix of the camera.
	 * @return The camera's projection matrix.
	 */
	[[nodiscard]] auto getProjection() const -> const math::mat4& { return m_projection; }

	/**
	 * @brief
	 *  Get the camera view projection matrix.
	 * @return Camera view projection matrix.
	 */
	[[nodiscard]] auto getView() const -> const math::mat4& { return m_view; }

	/**
	 * @brief
	 *  Get the camera view projection matrix.
	 * @return Camera view projection matrix.
	 */
	[[nodiscard]] auto getViewProjection() const -> const math::mat4& { return m_viewProjection; }

	/**
	 * @brief
	 *  Update the view matrix with a transformation.
	 * @param iTransform The transformation.
	 */
	void setTransform(const math::mat4& iTransform) {
		m_view = inverse(iTransform);
		m_viewProjection = m_projection * m_view;
	}

	/**
	 * @brief
	 *  Get the tone mapping of the camera.
	 * @return The tone mapping.
	 */
	[[nodiscard]] auto getToneMapping() const -> ToneMapping { return m_toneMapping; }

	/**
	 * @brief
	 *  Set the tone mapping of the camera.
	 * @param[in] iToneMapping The tone mapping.
	 */
	void setToneMapping(const ToneMapping iToneMapping) { m_toneMapping = iToneMapping; }

	/**
	 * @brief
	 *  Get the post-process stack of the camera.
	 * @return The settings.
	 */
	[[nodiscard]] auto getPostProcess() const -> const PostProcessSettings& { return m_postProcess; }

	/**
	 * @brief
	 *  Set the post-process stack of the camera.
	 * @param[in] iSettings The settings.
	 */
	void setPostProcess(const PostProcessSettings& iSettings) { m_postProcess = iSettings; }

protected:
	/**
	 * @brief
	 *  Update the view-projection matrix.
	 */
	void updateViewProjection();
	/// Camera's projection.
	math::mat4 m_projection = math::identity<float, 4>();
	/// The view matrix.
	math::mat4 m_view = math::identity<float, 4>();
	/// The view projection matrix.
	math::mat4 m_viewProjection = math::identity<float, 4>();
	/// Tone mapping of the scene colour.
	ToneMapping m_toneMapping = ToneMapping::None;
	/// Post-process stack.
	PostProcessSettings m_postProcess;
};
}// namespace owl::renderer
