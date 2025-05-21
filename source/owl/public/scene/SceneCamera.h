/**
 * @file SceneCamera.h
 * @author Silmaen
 * @date 23/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "math/trigonometry.h"
#include "renderer/Camera.h"

namespace owl::scene {
/**
 * @brief Class describing a camera in the scene.
 */
class OWL_API SceneCamera final : public renderer::Camera {
public:
	/**
	 * @brief Default constructor.
	 */
	SceneCamera();
	SceneCamera(const SceneCamera&) = default;
	SceneCamera(SceneCamera&&) = default;
	auto operator=(const SceneCamera&) -> SceneCamera& = default;
	auto operator=(SceneCamera&&) -> SceneCamera& = default;

	/**
	 * @brief Destructor.
	 */
	~SceneCamera() override;

	/**
	 * @brief Defines otho-graphic limits.
	 * @param[in] iSize Size of the view.
	 * @param[in] iNearClip Near clip distance.
	 * @param[in] iFarClip Far clip distance.
	 */
	void setOrthographic(float iSize, float iNearClip, float iFarClip);

	/**
	 * @brief Defines perspective parameters.
	 * @param[in] iVerticalFov vertical Field of view.
	 * @param[in] iNearClip Near distance clipping.
	 * @param[in] iFarClip Far distance clipping.
	 */
	void setPerspective(float iVerticalFov, float iNearClip, float iFarClip);

	/**
	 * @brief Defines the viewport.
	 * @param[in] iSize Viewport's size.
	 */
	void setViewportSize(const math::vec2ui& iSize);

	/**
	 * @brief Get actual orthographic size.
	 * @return The orthographic size.
	 */
	[[nodiscard]] auto getOrthographicSize() const -> float { return m_orthographicSize; }

	/**
	 * @brief Define the orthographic size.
	 * @param[in] iSize The new size.
	 */
	void setOrthographicSize(const float iSize) {
		m_orthographicSize = iSize;
		recalculateProjection();
	}

	/**
	 * @brief Projection types.
	 */
	enum struct ProjectionType : uint8_t {
		Perspective = 0,/// Perspective projection.
		Orthographic = 1/// Orthographic projection.
	};

	/**
	 * @brief Get the projection's type.
	 * @return The projection's type.
	 */
	[[nodiscard]] auto getProjectionType() const -> ProjectionType { return m_projectionType; }

	/**
	 * @brief Defines the projection's type.
	 * @param[in] iType New projection's type.
	 */
	void setProjectionType(const ProjectionType iType) {
		m_projectionType = iType;
		recalculateProjection();
	}

	/**
	 * @brief Get the orthographic near clip distance.
	 * @return The orthographic near clip distance.
	 */
	[[nodiscard]] auto getOrthographicNearClip() const -> float { return m_orthographicNear; }

	/**
	 * @brief Defines orthographic near clip distance.
	 * @param[in] iNearClip New distance for near clipping.
	 */
	void setOrthographicNearClip(const float iNearClip) {
		m_orthographicNear = iNearClip;
		recalculateProjection();
	}

	/**
	 * @brief Get the orthographic far clip distance.
	 * @return The orthographic far clip distance.
	 */
	[[nodiscard]] auto getOrthographicFarClip() const -> float { return m_orthographicFar; }

	/**
	 * @brief Defines orthographic far clip distance.
	 * @param[in] iFarClip New distance for far clipping.
	 */
	void setOrthographicFarClip(const float iFarClip) {
		m_orthographicFar = iFarClip;
		recalculateProjection();
	}

	/**
	 * @brief Get the perspective vertical field of vew.
	 * @return The perspective vertical field of view.
	 */
	[[nodiscard]] auto getPerspectiveVerticalFov() const -> float { return m_perspectiveFov; }

	/**
	 * @brief Defines perspective vertical field of view.
	 * @param[in] iVerticalFov New vertical field of view.
	 */
	void setPerspectiveVerticalFov(const float iVerticalFov) {
		m_perspectiveFov = iVerticalFov;
		recalculateProjection();
	}

	/**
	 * @brief Get the perspective near clip distance.
	 * @return The perspective near clip distance.
	 */
	[[nodiscard]] auto getPerspectiveNearClip() const -> float { return m_perspectiveNear; }

	/**
	 * @brief Defines perspective near clip distance.
	 * @param[in] iNearClip New distance for near clipping.
	 */
	void setPerspectiveNearClip(const float iNearClip) {
		m_perspectiveNear = iNearClip;
		recalculateProjection();
	}

	/**
	 * @brief Get the perspective far clip distance.
	 * @return The perspective far clip distance.
	 */
	[[nodiscard]] auto getPerspectiveFarClip() const -> float { return m_perspectiveFar; }

	/**
	 * @brief Defines perspective far clip distance.
	 * @param[in] iFarClip New distance for far clipping.
	 */
	void setPerspectiveFarClip(const float iFarClip) {
		m_perspectiveFar = iFarClip;
		recalculateProjection();
	}

private:
	/**
	 * @brief Recompute projections.
	 */
	void recalculateProjection();
	/// Actual projection's type.
	ProjectionType m_projectionType = ProjectionType::Orthographic;

	/// Perspective field of view.
	float m_perspectiveFov = math::radians(45.0f);
	/// Perspective near clip distance.
	float m_perspectiveNear = 0.01f;
	/// Perspective far clip distance.
	float m_perspectiveFar = 1000.0f;
	/// Orthographic size.
	float m_orthographicSize = 10.0f;
	/// Orthographic near clip distance.
	float m_orthographicNear = -1.0f;
	/// Orthographic far clip distance.
	float m_orthographicFar = 1.0f;
	/// Aspect ratio.
	float m_aspectRatio = 0.0f;
};
}// namespace owl::scene
