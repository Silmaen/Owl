/**
 * @file CameraPilot.h
 * @author Silmaen
 * @date 21/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <core/UUID.h>
#include <math/Transform.h>
#include <math/vectors.h>
#include <renderer/CameraEditor.h>

#include <cstdint>
#include <optional>

namespace owl::nest::panel {

/**
 * @brief
 *  State of the viewport while it pilots a scene camera: the view is the camera's, and moving the view moves it.
 *
 * Each frame the viewport follows the entity while no gesture runs (an undo or an inspector edit moves the view too),
 * lets the editor camera navigate, then calls `track`: the first change of pose begins a gesture (the entity is
 * captured before it moves), the next ones move the entity live, and the gesture is committed as one command once
 * the pose holds still with no button nor fly key held.
 */
class CameraPilot {
public:
	/**
	 * @brief
	 *  Pose of the editor camera that the pilot compares from frame to frame.
	 */
	struct Pose {
		/// Position of the eye.
		math::vec3 position{0.f, 0.f, 0.f};
		/// Pitch, in radians.
		float pitch = 0.f;
		/// Yaw, in radians.
		float yaw = 0.f;
	};

	/**
	 * @brief
	 *  What the viewport does to the entity after a call to `track`.
	 */
	enum struct Step : uint8_t {
		Idle,///< Nothing moved.
		Begin,///< A gesture starts: capture the entity, then move it.
		Move,///< The gesture goes on: move the entity.
		Commit,///< The gesture ended: record the move as one command.
	};

	/**
	 * @brief
	 *  Pose of an editor camera.
	 * @param[in] iView The editor camera.
	 * @return Its pose.
	 */
	[[nodiscard]] static auto getPose(const renderer::CameraEditor& iView) -> Pose;

	/**
	 * @brief
	 *  Start piloting a camera entity; the view already shows the camera's pose.
	 * @param[in] iEntity The camera entity.
	 * @param[in] iSavedView Editor camera to put back when the pilot stops.
	 * @param[in] iView Editor camera, moved onto the camera.
	 */
	void start(core::UUID iEntity, const renderer::CameraEditor& iSavedView, const renderer::CameraEditor& iView);

	/**
	 * @brief
	 *  Stop piloting.
	 * @return The editor camera saved by `start`, or nothing when no camera was piloted.
	 */
	auto stop() -> std::optional<renderer::CameraEditor>;

	/**
	 * @brief
	 *  Check whether a camera is piloted.
	 * @return True while piloting.
	 */
	[[nodiscard]] auto isActive() const -> bool { return m_savedView.has_value(); }

	/**
	 * @brief
	 *  The piloted entity.
	 * @return Its UUID, 0 when no camera is piloted.
	 */
	[[nodiscard]] auto getEntity() const -> core::UUID { return m_entity; }

	/**
	 * @brief
	 *  Check whether a gesture moves the camera.
	 * @return True between `Step::Begin` and `Step::Commit`.
	 */
	[[nodiscard]] auto isGestureActive() const -> bool { return m_gesture; }

	/**
	 * @brief
	 *  Check whether the view should be put back on the entity: no gesture runs and the view did not move since the
	 *  last frame (a wheel step between two frames has moved it).
	 * @param[in] iView The editor camera.
	 * @return True when the view should follow the entity.
	 */
	[[nodiscard]] auto shouldFollowEntity(const renderer::CameraEditor& iView) const -> bool;

	/**
	 * @brief
	 *  Remember the pose of the view after it followed the entity.
	 * @param[in] iView The editor camera.
	 */
	void markFollowed(const renderer::CameraEditor& iView) { m_lastPose = getPose(iView); }

	/**
	 * @brief
	 *  Compare the view with the last frame after the navigation.
	 * @param[in] iView The editor camera.
	 * @param[in] iInputHeld Whether a mouse button or a fly key is held.
	 * @return What to do to the entity.
	 */
	auto track(const renderer::CameraEditor& iView, bool iInputHeld) -> Step;

	/**
	 * @brief
	 *  World transform a camera takes from the view: the eye and the facing in perspective; the position across the
	 *  XY plane alone when orthographic, which keeps its depth and its rotation.
	 * @param[in] iView The editor camera.
	 * @param[in] iCurrent World transform of the camera before the move.
	 * @param[in] iOrthographic Whether the camera is orthographic.
	 * @return The new world transform, with the scale of `iCurrent`.
	 */
	[[nodiscard]] static auto getCameraWorld(const renderer::CameraEditor& iView, const math::Transform& iCurrent,
											 bool iOrthographic) -> math::Transform;

private:
	/// The piloted entity.
	core::UUID m_entity{0};
	/// Editor camera before the pilot started.
	std::optional<renderer::CameraEditor> m_savedView;
	/// Pose of the view at the end of the last frame.
	Pose m_lastPose;
	/// Whether a gesture moves the camera.
	bool m_gesture = false;
};

}// namespace owl::nest::panel
