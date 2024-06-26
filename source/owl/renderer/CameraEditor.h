/**
 * @file CameraEditor.h
 * @author Silmaen
 * @date 30/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <math/sizingTypes.h>

#include "Camera.h"
#include "core/Timestep.h"
#include "event/Event.h"
#include "event/MouseEvent.h"

namespace owl::renderer {
/**
 * @brief Class CameraEditor.
 */
class OWL_API CameraEditor final : public Camera {
public:
	CameraEditor() = default;
	~CameraEditor() override;
	CameraEditor(const CameraEditor&) = default;
	CameraEditor(CameraEditor&&) = default;
	CameraEditor& operator=(const CameraEditor&) = default;
	CameraEditor& operator=(CameraEditor&&) = default;

	/**
	 * @brief Constructor
	 * @param[in] iFov Field of view.
	 * @param[in] iAspectRatio Aspect rtio.
	 * @param[in] iNearClip Near clip distance.
	 * @param[in] iFarClip Far clip distance.
	 */
	CameraEditor(float iFov, float iAspectRatio, float iNearClip, float iFarClip);

	/**
	 * @brief Update the camera (Need input setup).
	 * @param[in] iTimeStep The timestamp.
	 */
	void onUpdate(const core::Timestep& iTimeStep);

	/**
	 * @brief Treat an event.
	 * @param[in,out] ioEvent Teh event to treat.
	 */
	void onEvent(event::Event& ioEvent);

	/**
	 * @brief Get the camera distance to the focal point.
	 * @return Camera distance to the focal point.
	 */
	[[nodiscard]] float getDistance() const { return m_distance; }

	/**
	 * @brief Set the camera distance to the focal point.
	 * @param[in] iDistance The new distance to the focal point.
	 */
	void setDistance(const float iDistance) { m_distance = iDistance; }

	/**
	 * @brief Set the camera viewport size.
	 * @param[in] iSize New size.
	 */
	void setViewportSize(const math::FrameSize& iSize) {
		m_viewportSize = iSize;
		updateProjection();
	}

	/**
	 * @brief Get the camera view matrix.
	 * @return Camera view matrix.
	 */
	[[nodiscard]] const math::mat4& getViewMatrix() const { return m_viewMatrix; }

	/**
	 * @brief Get the camera view projection matrix.
	 * @return Camera view projection matrix.
	 */
	[[nodiscard]] math::mat4 getViewProjection() const { return m_projection * m_viewMatrix; }

	/**
	 * @brief Get the camera up vector.
	 * @return Camera up vector.
	 */
	[[nodiscard]] math::vec3 getUpDirection() const;

	/**
	 * @brief Get the camera right vector.
	 * @return Camera right vector.
	 */
	[[nodiscard]] math::vec3 getRightDirection() const;

	/**
	 * @brief Get the camera forward vector.
	 * @return Camera forward vector.
	 */
	[[nodiscard]] math::vec3 getForwardDirection() const;
	/**
	 * @brief Get the camera position.
	 * @return Camera position.
	 */
	[[nodiscard]] const math::vec3& getPosition() const { return m_position; }

	/**
	 * @brief Get the camera orientation quaternion.
	 * @return Camera orientation.
	 */
	[[nodiscard]] math::quat getOrientation() const;

	/**
	 * @brief Get the camera pitch.
	 * @return Camera pitch.
	 */
	[[nodiscard]] float getPitch() const { return m_pitch; }

	/**
	 * @brief Get the camera yaw.
	 * @return Camera yaw.
	 */
	[[nodiscard]] float getYaw() const { return m_yaw; }

private:
	/**
	 * @brief Update the projection matrix.
	 */
	void updateProjection();

	/**
	 * @brief Update the view matrix.
	 */
	void updateView();

	/**
	 * @brief Event when mouse is scrolled.
	 * @param[in] iEvent Mouse scroll event.
	 * @return If treated.
	 */
	bool onMouseScroll(const event::MouseScrolledEvent& iEvent);

	/**
	 * @brief Mouse panoramic move.
	 * @param[in] iDelta The X Y deltas.
	 */
	void mousePan(const math::vec2& iDelta);

	/**
	 * @brief Mouse rotation.
	 * @param[in] iDelta The X Y deltas.
	 */
	void mouseRotate(const math::vec2& iDelta);

	/**
	 * @brief Mouse zoom.
	 * @param[in] iDelta The delta.
	 */
	void mouseZoom(float iDelta);

	/**
	 * @brief Calculate the camera position.
	 * @return Camera position.
	 */
	[[nodiscard]] math::vec3 calculatePosition() const;

	/**
	 * @brief Get the pan speeds.
	 * @return Pan speeds.
	 */
	[[nodiscard]] std::pair<float, float> panSpeed() const;

	/**
	 * @brief Get the rotation speed.
	 * @return The rotation speed.
	 */
	[[nodiscard]] float rotationSpeed() const;

	/**
	 * @brief Get the zoom speed.
	 * @return The zoom speed.
	 */
	[[nodiscard]] float zoomSpeed() const;

	// NOLINTBEGIN(*-magic-numbers)
	/// Field of View.
	float m_fov = 45.0f;
	/// Aspect ratio.
	float m_aspectRatio = 1.778f;
	/// Near clipping distance.
	float m_nearClip = 0.1f;
	/// Far clipping distance.
	float m_farClip = 1000.0f;

	/// Internal view matrix.
	math::mat4 m_viewMatrix{};
	/// Camera position.
	math::vec3 m_position = {0.0f, 0.0f, 0.0f};
	/// Camera Focal point.
	math::vec3 m_focalPoint = {0.0f, 0.0f, 0.0f};

	/// Save mouse position for drag.
	math::vec2 m_initialMousePosition = {0.0f, 0.0f};

	/// Camera distance.
	float m_distance = 10.0f;
	/// Camera's pitch.
	float m_pitch = 0.0f;
	/// Camera's yaw.
	float m_yaw = 0.0f;
	// NOLINTEND(*-magic-numbers)

	/// Viewport size.
	math::FrameSize m_viewportSize = {1280, 720};
};
}// namespace owl::renderer
