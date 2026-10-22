/**
 * @file CameraOrtho.h
 * @author Silmaen
 * @date 10/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Camera.h"
#include "ClipSpace.h"
#include "core/Core.h"

#include <cstdint>

namespace owl::renderer {
/**
 * @brief
 *  Orthographic camera.
 */
class OWL_API CameraOrtho final : public Camera {
public:
	/**
	 * @brief
	 *  Copy constructor.
	 */
	CameraOrtho(const CameraOrtho&) = default;

	/**
	 * @brief
	 *  Move constructor.
	 */
	CameraOrtho(CameraOrtho&&) = default;

	/**
	 * @brief
	 *  Copy assignment operator.
	 * @return A reference to this object.
	 */
	auto operator=(const CameraOrtho&) -> CameraOrtho& = default;

	/**
	 * @brief
	 *  Move assignment operator.
	 * @return A reference to this object.
	 */
	auto operator=(CameraOrtho&&) -> CameraOrtho& = default;

	/**
	 * @brief
	 *  Destructor.
	 */
	~CameraOrtho() override;

	/**
	 * @brief
	 *  Create the camera giving coordinates of corners.
	 * @param[in] iLeft Left of the screen's coordinate.
	 * @param[in] iRight Right of the screen's coordinate.
	 * @param[in] iBottom Bottom of the screen's coordinate.
	 * @param[in] iTop Top of the screen's coordinate.
	 */
	CameraOrtho(float iLeft, float iRight, float iBottom, float iTop);

	/**
	 * @brief
	 *  Set projection giving the camera coordinates of corners.
	 * @param[in] iLeft Left of the screen's coordinate.
	 * @param[in] iRight Right of the screen's coordinate.
	 * @param[in] iBottom Bottom of the screen's coordinate.
	 * @param[in] iTop Top of the screen's coordinate.
	 * @param[in] iNear The near distance.
	 * @param[in] iFar The far distance.
	 */
	void setProjection(float iLeft, float iRight, float iBottom, float iTop, float iNear = -1.0f, float iFar = 1.0f);

	/// Clip-space convention of a projection matrix.
	using ClipSpace = renderer::ClipSpace;

	/**
	 * @brief
	 *  Build the orthographic projection `setProjection` uses, in a given clip-space convention.
	 * @param[in] iLeft Left of the screen's coordinate.
	 * @param[in] iRight Right of the screen's coordinate.
	 * @param[in] iBottom Bottom of the screen's coordinate.
	 * @param[in] iTop Top of the screen's coordinate.
	 * @param[in] iNear The near distance.
	 * @param[in] iFar The far distance.
	 * @param[in] iClipSpace The convention of the target backend.
	 * @return The projection matrix.
	 */
	[[nodiscard]] static auto makeProjection(float iLeft, float iRight, float iBottom, float iTop, float iNear,
											 float iFar, ClipSpace iClipSpace) -> math::mat4;

	/**
	 * @brief
	 *  Clip-space convention of the active render backend.
	 * @return `Vulkan` on the Vulkan backend, `OpenGl` otherwise.
	 */
	[[nodiscard]] static auto getActiveClipSpace() -> ClipSpace;

	/**
	 * @brief
	 *  Access to camera's position.
	 * @return Camera's position.
	 */
	[[nodiscard]] auto getPosition() const -> const math::vec3& { return m_position; }

	/**
	 * @brief
	 *  Define camera's position.
	 * @param[in] iPosition New camera position.
	 */
	void setPosition(const math::vec3& iPosition) {
		m_position = iPosition;
		/**
		 * @brief
		 *  Recalculate view matrix.
		 */
		recalculateViewMatrix();
	}

	/**
	 * @brief
	 *  Access to camera's rotation.
	 * @return Camera's rotation.
	 */
	[[nodiscard]] auto getRotation() const -> float { return m_rotation; }

	/**
	 * @brief
	 *  Defines camera's rotation.
	 * @param[in] iRotation New camera rotation.
	 */
	void setRotation(const float iRotation) {
		m_rotation = iRotation;
		/**
		 * @brief
		 *  Recalculate view matrix.
		 */
		recalculateViewMatrix();
	}

private:
	/**
	 * @brief
	 *  Recompute the matrices.
	 */
	void recalculateViewMatrix();

	/// Camera's position.
	math::vec3 m_position = {0.0f, 0.0f, 0.0f};
	/// Camera's rotation.
	float m_rotation = 0.0f;
};
}// namespace owl::renderer
