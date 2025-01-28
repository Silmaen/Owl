/**
 * @file Camera.h
 * @author Silmaen
 * @date 23/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "math/matrixCreation.h"
#include "core/Core.h"

namespace owl::renderer {
/**
 * @brief Base class for camera.
 */
class OWL_API Camera {
public:
	Camera(const Camera&) = default;
	Camera(Camera&&) = default;
	auto operator=(const Camera&) -> Camera& = default;
	auto operator=(Camera&&) -> Camera& = default;
	Camera() = default;
	/**
	 * @brief Destructor.
	 */
	virtual ~Camera();

	/**
	 * @brief Construct the camera with a projection.
	 * @param[in] iProjection projection matrix.
	 */
	explicit Camera(const math::mat4& iProjection) : m_projection{iProjection} {}

	/**
	 * @brief Get the projection matrix of the camera.
	 * @return The camera's projection matrix.
	 */
	[[nodiscard]] auto getProjection() const -> const math::mat4& { return m_projection; }
	/**
	 * @brief Get the camera view projection matrix.
	 * @return Camera view projection matrix.
	 */
	[[nodiscard]] auto getView() const -> const math::mat4& { return m_view; }
	/**
	 * @brief Get the camera view projection matrix.
	 * @return Camera view projection matrix.
	 */
	[[nodiscard]] auto getViewProjection() const -> const math::mat4& { return m_viewProjection; }

	/**
	 * @brief Update the view matrix with a transformation.
	 * @param iTransform The transformation.
	 */
	void setTransform(const math::mat4& iTransform) {
		m_view = inverse(iTransform);
		m_viewProjection = m_projection * m_view;
	}

protected:
	/**
	 * @brief Update the view-projection matrix.
	 */
	void updateViewProjection();
	/// Camera's projection.
	math::mat4 m_projection = math::identity<float, 4>();
	/// The view matrix.
	math::mat4 m_view = math::identity<float, 4>();
	/// The view projection matrix.
	math::mat4 m_viewProjection = math::identity<float, 4>();
};
}// namespace owl::renderer
