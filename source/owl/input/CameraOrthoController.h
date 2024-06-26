/**
 * @file CameraOrthoController.h
 * @author Silmaen
 * @date 17/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <math/sizingTypes.h>

#include "core/Timestep.h"
#include "event/AppEvent.h"
#include "event/MouseEvent.h"
#include "renderer/CameraOrtho.h"

namespace owl::input {
/**
 * @brief Class CameraOrthoController.
 */
class OWL_API CameraOrthoController final {
public:
	CameraOrthoController(const CameraOrthoController&) = default;
	CameraOrthoController(CameraOrthoController&&) = default;
	CameraOrthoController& operator=(const CameraOrthoController&) = default;
	CameraOrthoController& operator=(CameraOrthoController&&) = default;

	/**
	 * @brief Constructor.
	 * @param[in] iAspectRatio Camera's aspect ratio.
	 * @param[in] iRotation Camera's rotation.
	 */
	explicit CameraOrthoController(float iAspectRatio, bool iRotation = false);

	/**
	 * @brief Destructor.
	 */
	~CameraOrthoController() = default;

	/**
	 * @brief Frame function.
	 * @param[in] iTimeStep Time step.
	 */
	void onUpdate(const core::Timestep& iTimeStep);

	/**
	 * @brief Event Management.
	 * @param[in,out] ioEvent The received event.
	 */
	void onEvent(event::Event& ioEvent);

	/**
	 * @brief Action when view port is resized.
	 * @param[in] iSize New size.
	 */
	void onResize(const math::FrameSize& iSize);

	/**
	 * @brief Access to the camera.
	 * @return The camera.
	 */
	renderer::CameraOrtho& getCamera() { return m_camera; }

	/**
	 * @brief Access to the camera.
	 * @return The camera.
	 */
	[[nodiscard]] const renderer::CameraOrtho& getCamera() const { return m_camera; }

	/**
	 * @brief Access to the zoom level.
	 * @return Teh Zoom level.
	 */
	[[nodiscard]] float getZoomLevel() const { return m_zoomLevel; }

	/**
	 * @brief Define the new zoom level.
	 * @param[in] iLevel The new level.
	 */
	void setZoomLevel(const float iLevel) { m_zoomLevel = iLevel; }

private:
	/**
	 * @brief Action on mouse scroll.
	 * @param[in] iEvent The Mouse event.
	 * @return True if treated.
	 */
	bool onMouseScrolled(const event::MouseScrolledEvent& iEvent);

	/**
	 * @brief Action on window resize.
	 * @param[in] iEvent The Window resize event.
	 * @return True if treated.
	 */
	bool onWindowResized(const event::WindowResizeEvent& iEvent);

	/// Aspect ratio of the camera.
	float m_aspectRatio;
	/// Camera's zoom level.
	float m_zoomLevel{1.0f};
	/// The camera.
	renderer::CameraOrtho m_camera;
	/// if camera's rotation allowed.
	bool m_rotation;
	/// Camera's position in the world.
	math::vec3 m_cameraPosition = {0.0f, 0.0f, 0.0f};
	/// Camera's rotation in degrees, in the anti-clockwise direction.
	float m_cameraRotation{0.0f};

	// NOLINTBEGIN(*-magic-numbers)
	/// Camera's Translation speed.
	float m_cameraTranslationSpeed{5.0f};
	/// Camera's Rotation Speed.
	float m_cameraRotationSpeed{180.0f};
	// NOLINTEND(*-magic-numbers)
};
}// namespace owl::input
