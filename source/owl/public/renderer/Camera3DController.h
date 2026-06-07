/**
 * @file Camera3DController.h
 * @author Silmaen
 * @date 06/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Timestep.h"
#include "math/Transform.h"

namespace owl::renderer {
/**
 * @brief
 *  Reusable free-fly / first-person 3D camera controller.
 *
 * Holds the kinematic state of a 3D camera (position, yaw, pitch) and turns
 * keyboard input into motion. The orientation convention matches the engine's
 * Euler-angle transform: yaw rotates around world up (Y), pitch around the
 * camera right axis, with forward pointing down -Z when yaw and pitch are zero.
 * The controller is renderer-agnostic and free of ECS dependencies so it can
 * back both the editor viewport and a Play-mode camera component.
 */
class OWL_API Camera3DController final {
public:
	Camera3DController(const Camera3DController&) = default;

	Camera3DController(Camera3DController&&) = default;

	auto operator=(const Camera3DController&) -> Camera3DController& = default;

	auto operator=(Camera3DController&&) -> Camera3DController& = default;

	/**
	 * @brief
	 *  Constructor.
	 */
	Camera3DController() = default;

	/**
	 * @brief
	 *  Destructor.
	 */
	~Camera3DController() = default;

	/**
	 * @brief
	 *  Frame update: read keyboard input and advance the camera.
	 *
	 * WASD moves in the facing plane, Space/E rises and Left-Shift/Q descends,
	 * and the arrow keys look around. Movement is frame-rate independent.
	 * @param[in] iTimeStep Elapsed time for this frame.
	 */
	void onUpdate(const core::Timestep& iTimeStep);

	/**
	 * @brief
	 *  Move the camera along its local basis.
	 * @param[in] iAxes Per-axis amount, components in [-1, 1]: x = right,
	 *  y = world up, z = forward.
	 * @param[in] iAmount World-space distance applied to the combined direction.
	 */
	void moveLocal(const math::vec3& iAxes, float iAmount);

	/**
	 * @brief
	 *  Add to the yaw angle.
	 * @param[in] iDelta Yaw increment in radians.
	 */
	void addYaw(float iDelta);

	/**
	 * @brief
	 *  Add to the pitch angle, clamped to the pitch limit.
	 * @param[in] iDelta Pitch increment in radians.
	 */
	void addPitch(float iDelta);

	/**
	 * @brief
	 *  Get the camera forward direction.
	 * @return The unit forward vector.
	 */
	[[nodiscard]] auto getForwardDirection() const -> math::vec3;

	/**
	 * @brief
	 *  Get the camera right direction (horizontal).
	 * @return The unit right vector.
	 */
	[[nodiscard]] auto getRightDirection() const -> math::vec3;

	/**
	 * @brief
	 *  Get the camera up direction.
	 * @return The unit up vector.
	 */
	[[nodiscard]] auto getUpDirection() const -> math::vec3;

	/**
	 * @brief
	 *  Get the camera position.
	 * @return The camera position.
	 */
	[[nodiscard]] auto getPosition() const -> const math::vec3& { return m_position; }

	/**
	 * @brief
	 *  Set the camera position.
	 * @param[in] iPosition The new position.
	 */
	void setPosition(const math::vec3& iPosition) { m_position = iPosition; }

	/**
	 * @brief
	 *  Get the yaw angle.
	 * @return The yaw in radians.
	 */
	[[nodiscard]] auto getYaw() const -> float { return m_yaw; }

	/**
	 * @brief
	 *  Set the yaw angle.
	 * @param[in] iYaw The new yaw in radians.
	 */
	void setYaw(const float iYaw) { m_yaw = iYaw; }

	/**
	 * @brief
	 *  Get the pitch angle.
	 * @return The pitch in radians.
	 */
	[[nodiscard]] auto getPitch() const -> float { return m_pitch; }

	/**
	 * @brief
	 *  Set the pitch angle, clamped to the pitch limit.
	 * @param[in] iPitch The new pitch in radians.
	 */
	void setPitch(float iPitch);

	/**
	 * @brief
	 *  Get the orientation as engine Euler angles.
	 * @return Euler rotation vector (pitch, yaw, 0) in radians.
	 */
	[[nodiscard]] auto getEulerRotation() const -> math::vec3 { return {m_pitch, m_yaw, 0.0f}; }

	/**
	 * @brief
	 *  Initialise the orientation from engine Euler angles.
	 * @param[in] iEuler Euler rotation vector (pitch, yaw, roll) in radians.
	 */
	void setEulerRotation(const math::vec3& iEuler);

	/**
	 * @brief
	 *  Get the camera transform.
	 * @return The transform placing the camera in world space.
	 */
	[[nodiscard]] auto getTransform() const -> math::Transform;

	/**
	 * @brief
	 *  Get the camera view matrix.
	 * @return The inverse of the camera transform.
	 */
	[[nodiscard]] auto getViewMatrix() const -> math::mat4;

	/**
	 * @brief
	 *  Get the movement speed.
	 * @return The movement speed in world units per second.
	 */
	[[nodiscard]] auto getMoveSpeed() const -> float { return m_moveSpeed; }

	/**
	 * @brief
	 *  Set the movement speed.
	 * @param[in] iSpeed The new movement speed in world units per second.
	 */
	void setMoveSpeed(const float iSpeed) { m_moveSpeed = iSpeed; }

	/**
	 * @brief
	 *  Get the look (rotation) speed.
	 * @return The look speed in radians per second.
	 */
	[[nodiscard]] auto getLookSpeed() const -> float { return m_lookSpeed; }

	/**
	 * @brief
	 *  Set the look (rotation) speed.
	 * @param[in] iSpeed The new look speed in radians per second.
	 */
	void setLookSpeed(const float iSpeed) { m_lookSpeed = iSpeed; }

private:
	/// Camera position in world space.
	math::vec3 m_position = {0.0f, 0.0f, 0.0f};
	/// Yaw angle in radians (rotation around world up).
	float m_yaw = 0.0f;
	/// Pitch angle in radians (rotation around camera right).
	float m_pitch = 0.0f;

	// NOLINTBEGIN(*-magic-numbers)
	/// Movement speed in world units per second.
	float m_moveSpeed = 8.0f;
	/// Look speed in radians per second.
	float m_lookSpeed = 1.5f;
	/// Pitch clamp in radians (just under 90 degrees).
	float m_pitchLimit = 1.5f;
	// NOLINTEND(*-magic-numbers)
};
}// namespace owl::renderer
