/**
 * @file RemoteController.h
 * @author Silmaen
 * @date 21/09/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include <owl.h>

namespace drone::controller {

/**
 * @brief Class RemoteController
 */
class RemoteController final {
public:
	/**
	 * @brief Constructor.
	 * @param motorNumber The number of motor in the device.
	 */
	explicit RemoteController(size_t motorNumber = 4);

	/**
	 * @brief Destructor.
	 */
	~RemoteController();

	/**
	 * @brief Copy constructor.
	 */
	RemoteController(const RemoteController &) = default;

	/**
	 * @brief Move constructor.
	 */
	RemoteController(RemoteController &&) = default;

	/**
	 * @brief Copy affectation operator.
	 */
	RemoteController &operator=(const RemoteController &) = default;

	/**
	 * @brief Move affectation operator.
	 */
	RemoteController &operator=(RemoteController &&) = default;


	/**
	 * @brief Reads the horizontal velocity.
	 * @return The horizontal velocity.
	 */
	[[nodiscard]] const float &getHorizontalVelocity() const { return horizontalVelocity; }

	/**
	 * @brief Reads the vertical velocity.
	 * @return The vertical velocity.
	 */
	[[nodiscard]] const float &getVerticalVelocity() const { return verticalVelocity; }

	/**
	 * @brief Reads the altitude.
	 * @return The altitude.
	 */
	[[nodiscard]] const float &getAltitude() const { return altitude; }

	/**
	 * @brief Reads the rotation vector.
	 * @return The rotation vector.
	 */
	[[nodiscard]] const owl::math::vec3 &getRotations() const { return rotations; }

	/**
	 * @brief Reads the motor rates.
	 * @return The motor rates.
	 */
	[[nodiscard]] const std::vector<float> &getMotorRates() const { return motors; }

	/**
	 * @brief Defines the horizontal velocity.
	 * @param velocity The horizontal velocity.
	 */
	void setHorizontalVelocity(float velocity) { horizontalVelocity = velocity; }

	/**
	 * @brief Defines the vertical velocity.
	 * @param velocity The vertical velocity.
	 */
	void setVerticalVelocity(float velocity) { verticalVelocity = velocity; }

	/**
	 * @brief Defines the current altitude.
	 * @param alt The altitude.
	 */
	void setAltitude(float alt) { altitude = alt; }

	/**
	 * @brief Defines the rotation vector.
	 * @param rot The rotation vector.
	 */
	void setRotation(const owl::math::vec3 &rot) { rotations = rot; }

	/**
	 * @brief Defines the motor rates.
	 * @param mot The motor rates.
	 */
	void setMotorRates(const std::vector<float> &mot);

private:
	/// The horizontal velocity.
	float horizontalVelocity = 0;
	/// The vertical velocity.
	float verticalVelocity = 0;
	/// The current altitude.
	float altitude = 0;
	/// The rotation vector roll pitch yaw.
	owl::math::vec3 rotations = {0.f, 0.f, 0.f};
	/// The vertical velocity.
	std::vector<float> motors;
};

}// namespace drone::controller
