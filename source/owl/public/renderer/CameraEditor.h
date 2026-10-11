/**
 * @file CameraEditor.h
 * @author Silmaen
 * @date 30/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Camera.h"
#include "core/Timestep.h"
#include "event/Event.h"
#include "event/MouseEvent.h"

#include <cstdint>
#include <utility>

namespace owl::renderer {
/**
 * @brief
 *  Class use to manage camera in the editor.
 */
class OWL_API CameraEditor final : public Camera {
public:
	CameraEditor() = default;

	/**
	 * @brief
	 *  Destructor.
	 */
	~CameraEditor() override;

	/**
	 * @brief
	 *  Copy constructor.
	 */
	CameraEditor(const CameraEditor&) = default;

	/**
	 * @brief
	 *  Move constructor.
	 */
	CameraEditor(CameraEditor&&) = default;

	/**
	 * @brief
	 *  Copy assignment operator.
	 * @return A reference to this object.
	 */
	auto operator=(const CameraEditor&) -> CameraEditor& = default;

	/**
	 * @brief
	 *  Move assignment operator.
	 * @return A reference to this object.
	 */
	auto operator=(CameraEditor&&) -> CameraEditor& = default;

	/**
	 * @brief
	 *  Constructor
	 * @param[in] iFov Field of view.
	 * @param[in] iAspectRatio Aspect ratio.
	 * @param[in] iNearClip Near clip distance.
	 * @param[in] iFarClip Far clip distance.
	 */
	CameraEditor(float iFov, float iAspectRatio, float iNearClip, float iFarClip);

	/**
	 * @brief
	 *  How the editor camera projects the scene.
	 */
	enum struct Projection : uint8_t {
		Perspective,///< Perspective with the field of view, orbit and free look allowed.
		Orthographic,///< Orthographic front view of the XY plane: no rotation, the distance sets the visible height.
	};

	/**
	 * @brief
	 *  Change the projection; the orthographic one faces the XY plane and frames what the perspective one showed at
	 *  the reference point.
	 * @param[in] iProjection The projection.
	 */
	void setProjection(Projection iProjection);

	/**
	 * @brief
	 *  Current projection.
	 * @return The projection.
	 */
	[[nodiscard]] auto getProjectionMode() const -> Projection { return m_projectionMode; }

	/**
	 * @brief
	 *  Move the reference point onto a sphere and back off until it fills the view.
	 * @param[in] iCenter Centre of what to show.
	 * @param[in] iRadius Radius of what to show, in world units.
	 */
	void frame(const math::vec3& iCenter, float iRadius);

	/**
	 * @brief
	 *  Axis-aligned views, named after the side of the scene the camera looks from.
	 */
	enum struct ViewAxis : uint8_t {
		Front,///< From +Z towards -Z: the XY plane, as in the 2D view.
		Back,///< From -Z towards +Z.
		Top,///< From +Y down to -Y: the XZ plane, -Z at the top of the view.
		Bottom,///< From -Y up to +Y.
		Right,///< From +X towards -X: the YZ plane.
		Left,///< From -X towards +X.
	};

	/**
	 * @brief
	 *  Turn the camera to look along an axis, around the same reference point and at the same distance.
	 * @param[in] iAxis The side to look from.
	 */
	void viewAlong(ViewAxis iAxis);

	/**
	 * @brief
	 *  Put the camera back to its default pose: facing the XY plane from +Z, ten units away from the origin.
	 */
	void resetView();

	/**
	 * @brief
	 *  Place the eye and turn it to a direction; the reference point goes in front of it, at the current distance.
	 * @param[in] iEye Position of the eye.
	 * @param[in] iForward Direction to look at; ignored when null. The camera never rolls.
	 */
	void lookFrom(const math::vec3& iEye, const math::vec3& iForward);

	/**
	 * @brief
	 *  How mouse drags and the wheel move the camera.
	 */
	struct Navigation {
		/// Factor on the orbit and look-around speed.
		float rotate = 1.f;
		/// Factor on the pan speed.
		float pan = 1.f;
		/// Factor on the wheel zoom and the dolly speed.
		float zoom = 1.f;
		/// Turn the vertical drag of the orbit and look-around the other way.
		bool invertY = false;
		/// Distance in pixels the cursor travels before a drag moves the camera (filters a shaky click).
		float deadZone = 0.f;
	};

	/**
	 * @brief
	 *  Change how drags and the wheel move the camera.
	 * @param[in] iNavigation The new navigation settings.
	 */
	void setNavigation(const Navigation& iNavigation) { m_navigation = iNavigation; }

	/**
	 * @brief
	 *  Current navigation settings.
	 * @return The navigation settings.
	 */
	[[nodiscard]] auto getNavigation() const -> const Navigation& { return m_navigation; }

	/**
	 * @brief
	 *  Reference point the camera orbits and pans around.
	 * @return The reference point.
	 */
	[[nodiscard]] auto getFocalPoint() const -> const math::vec3& { return m_focalPoint; }

	/**
	 * @brief
	 *  Update the camera (Need input setup).
	 * @param[in] iTimeStep The timestamp.
	 */
	void onUpdate(const core::Timestep& iTimeStep);

	/**
	 * @brief
	 *  Treat an event.
	 * @param[in,out] ioEvent The event to treat.
	 */
	void onEvent(event::Event& ioEvent);

	/**
	 * @brief
	 *  Vertical field of view of the perspective projection; the orthographic one shows the same height at the
	 *  reference point.
	 * @return The field of view, in degrees.
	 */
	[[nodiscard]] auto getFov() const -> float { return m_fov; }

	/**
	 * @brief
	 *  Get the camera distance to the focal point.
	 * @return Camera distance to the focal point.
	 */
	[[nodiscard]] auto getDistance() const -> float { return m_distance; }

	/**
	 * @brief
	 *  Set the camera distance to the focal point.
	 * @param[in] iDistance The new distance to the focal point.
	 */
	void setDistance(const float iDistance) { m_distance = iDistance; }

	/**
	 * @brief
	 *  Set the camera viewport size.
	 * @param[in] iSize New size.
	 */
	void setViewportSize(const math::vec2ui& iSize) {
		m_viewportSize = iSize;
		/**
		 * @brief
		 *  Update projection.
		 */
		updateProjection();
	}

	/**
	 * @brief
	 *  Get the camera up vector.
	 * @return Camera up vector.
	 */
	[[nodiscard]] auto getUpDirection() const -> math::vec3;

	/**
	 * @brief
	 *  Get the camera right vector.
	 * @return Camera right vector.
	 */
	[[nodiscard]] auto getRightDirection() const -> math::vec3;

	/**
	 * @brief
	 *  Get the camera forward vector.
	 * @return Camera forward vector.
	 */
	[[nodiscard]] auto getForwardDirection() const -> math::vec3;

	/**
	 * @brief
	 *  Get the camera position.
	 * @return Camera position.
	 */
	[[nodiscard]] auto getPosition() const -> const math::vec3& { return m_position; }

	/**
	 * @brief
	 *  Get the camera orientation quaternion.
	 * @return Camera orientation.
	 */
	[[nodiscard]] auto getOrientation() const -> math::quat;

	/**
	 * @brief
	 *  Get the camera pitch.
	 * @return Camera pitch.
	 */
	[[nodiscard]] auto getPitch() const -> float { return m_pitch; }

	/**
	 * @brief
	 *  Get the camera yaw.
	 * @return Camera yaw.
	 */
	[[nodiscard]] auto getYaw() const -> float { return m_yaw; }

private:
	/**
	 * @brief
	 *  Update the projection matrix.
	 */
	void updateProjection();

	/**
	 * @brief
	 *  Update the view matrix.
	 */
	void updateView();

	/**
	 * @brief
	 *  Event when mouse is scrolled.
	 * @param[in] iEvent Mouse scroll event.
	 * @return If treated.
	 */
	auto onMouseScroll(const event::MouseScrolledEvent& iEvent) -> bool;

	/**
	 * @brief
	 *  Mouse panoramic move.
	 * @param[in] iDelta The X Y deltas.
	 */
	void mousePan(const math::vec2& iDelta);

	/**
	 * @brief
	 *  Mouse rotation: orbit around the focal point (the focal point stays fixed, the camera circles it).
	 * @param[in] iDelta The X Y deltas.
	 */
	void mouseRotate(const math::vec2& iDelta);

	/**
	 * @brief
	 *  Rotate the camera around its own position (look around in place; the focal point follows the new facing).
	 * @param[in] iDelta The X Y deltas.
	 */
	void rotateInPlace(const math::vec2& iDelta);

	/**
	 * @brief
	 *  Dolly the camera (and its focal point) forward / backward along the view direction.
	 * @param[in] iDelta The signed amount (drag up moves forward).
	 */
	void mouseDolly(float iDelta);

	/**
	 * @brief
	 *  Mouse zoom.
	 * @param[in] iDelta The delta.
	 */
	void mouseZoom(float iDelta);

	/**
	 * @brief
	 *  Calculate the camera position.
	 * @return Camera position.
	 */
	[[nodiscard]] auto calculatePosition() const -> math::vec3;

	/**
	 * @brief
	 *  Get the pan speeds.
	 * @return Pan speeds.
	 */
	[[nodiscard]] auto panSpeed() const -> std::pair<float, float>;

	/**
	 * @brief
	 *  Get the rotation speed.
	 * @return The rotation speed.
	 */
	[[nodiscard]] auto rotationSpeed() const -> float;

	/**
	 * @brief
	 *  Sign of the vertical drag on the pitch.
	 * @return -1 when the navigation inverts it, else 1.
	 */
	[[nodiscard]] auto pitchSign() const -> float;

	/**
	 * @brief
	 *  Get the zoom speed.
	 * @return The zoom speed.
	 */
	[[nodiscard]] auto zoomSpeed() const -> float;

	/// Projection mode.
	Projection m_projectionMode = Projection::Perspective;
	/// Field of View.
	float m_fov = 45.0f;
	/// Aspect ratio.
	float m_aspectRatio = 1.778f;
	/// Near clipping distance.
	float m_nearClip = 0.1f;
	/// Far clipping distance.
	float m_farClip = 1000.0f;

	/// Camera position.
	math::vec3 m_position = {0.0f, 0.0f, 0.0f};
	/// Camera Focal point.
	math::vec3 m_focalPoint = {0.0f, 0.0f, 0.0f};

	/// Save mouse position for drag.
	math::vec2 m_initialMousePosition = {0.0f, 0.0f};
	/// Pixels the cursor travelled since the current drag started, compared with the dead zone.
	float m_dragTravel = 0.f;
	/// Drag and wheel settings.
	Navigation m_navigation;

	/// Camera distance.
	float m_distance = 10.0f;
	/// Camera's pitch.
	float m_pitch = 0.0f;
	/// Camera's yaw.
	float m_yaw = 0.0f;

	/// Viewport size.
	math::vec2ui m_viewportSize = {1280, 720};
};
}// namespace owl::renderer
