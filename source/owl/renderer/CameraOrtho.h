/**
 * @file CameraOrtho.h
 * @author Silmaen
 * @date 10/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"

namespace owl::renderer {
/**
 * @brief Orthographic camera.
 */
class OWL_API CameraOrtho {
public:
	CameraOrtho(const CameraOrtho &) = default;
	CameraOrtho(CameraOrtho &&) = default;
	CameraOrtho &operator=(const CameraOrtho &) = default;
	CameraOrtho &operator=(CameraOrtho &&) = default;
	~CameraOrtho() = default;
	/**
	 * @brief Create the camera giving coordinates of corners.
	 * @param[in] iLeft Left of the screen's coordinate.
	 * @param[in] iRight Right of the screen's coordinate.
	 * @param[in] iBottom Bottom of the screen's coordinate.
	 * @param[in] iTop Top of the screen's coordinate.
	 */
	CameraOrtho(float iLeft, float iRight, float iBottom, float iTop);

	/**
	 * @brief Set projection giving the camera coordinates of corners.
	 * @param[in] iLeft Left of the screen's coordinate.
	 * @param[in] iRight Right of the screen's coordinate.
	 * @param[in] iBottom Bottom of the screen's coordinate.
	 * @param[in] iTop Top of the screen's coordinate.
	 * @param[in] iNear The near distance.
	 * @param[in] iFar The far distance.
	 */
	void setProjection(float iLeft, float iRight, float iBottom, float iTop, float iNear = -1.0f, float iFar = 1.0f);

	/**
	 * @brief Access to camera's position.
	 * @return Camera's position.
	 */
	[[nodiscard]] const math::vec3 &getPosition() const { return m_position; }

	/**
	 * @brief Define camera's position.
	 * @param[in] iPosition New camera position.
	 */
	void setPosition(const math::vec3 &iPosition) {
		m_position = iPosition;
		recalculateViewMatrix();
	}

	/**
	 * @brief Access to camera's rotation.
	 * @return Camera's rotation.
	 */
	[[nodiscard]] float getRotation() const { return m_rotation; }

	/**
	 * @brief Defines camera's rotation.
	 * @param[in] iRotation New camera rotation.
	 */
	void setRotation(const float iRotation) {
		m_rotation = iRotation;
		recalculateViewMatrix();
	}

	/**
	 * @brief Access to projection matrix.
	 * @return The projection matrix.
	 */
	[[nodiscard]] const math::mat4 &getProjectionMatrix() const { return m_projectionMatrix; }

	/**
	 * @brief Access to view matrix.
	 * @return The view matrix.
	 */
	[[nodiscard]] const math::mat4 &getViewMatrix() const { return m_viewMatrix; }

	/**
	 * @brief Access to view projection matrix.
	 * @return The view position matrix.
	 */
	[[nodiscard]] const math::mat4 &getViewProjectionMatrix() const { return m_viewProjectionMatrix; }

private:
	/**
	 * @brief Recompute the matrices.
	 */
	void recalculateViewMatrix();

	/// The projection matrix.
	math::mat4 m_projectionMatrix{};
	/// The view matrix.
	math::mat4 m_viewMatrix{};
	/// The view projection matrix.
	math::mat4 m_viewProjectionMatrix{};
	/// Camera's position.
	math::vec3 m_position = {0.0f, 0.0f, 0.0f};
	/// Camera's rotation.
	float m_rotation = 0.0f;
};
}// namespace owl::renderer
