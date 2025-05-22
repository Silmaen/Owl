/**
 * @file CameraSystem.h
 * @author Silmaen
 * @date 28/09/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include <owl.h>

namespace drone::IO {

/**
 * @brief Class CameraSystem
 */
class CameraSystem final {
public:
	/**
	 * @brief Destructor.
	 */
	~CameraSystem();

	CameraSystem(const CameraSystem&) = delete;
	CameraSystem(CameraSystem&&) = delete;
	auto operator=(const CameraSystem&) -> CameraSystem& = delete;
	auto operator=(CameraSystem&&) -> CameraSystem& = delete;

	/**
	 * @brief Singleton get.
	 * @return Instance of the Camera System.
	 */
	static auto get() -> CameraSystem& {
		static CameraSystem instance;
		return instance;
	}

	/**
	 * @brief Frame update of the camera
	 */
	void onUpdate(const owl::core::Timestep& iTs);

	/**
	 * @brief Get the camera frame.
	 * @return The camera frame.
	 */
	[[nodiscard]] auto getFrame() const -> const owl::shared<owl::renderer::Texture>& { return m_frame; }

	/**
	 * @brief Set the camera by its ID.
	 * @param[in] iId Id of the camera.
	 *
	 * @note If the Camera does not match an id of the list, id will be set to 0.
	 */
	void setCamera(int32_t iId);

	/**
	 * @brief Get the current Camera Id.
	 * @return The current camera ID.
	 */
	[[nodiscard]] auto getCurrentCameraId() const -> int32_t;
	/**
	 * @brief Get the current Camera Id.
	 * @return The current camera ID.
	 */
	[[nodiscard]] auto getCurrentCameraName() const -> std::string;

	/**
	 * @brief Actualize the list of available cameras.
	 */
	void actualiseList();

	/**
	 * @brief Gat the list of camera.
	 * @return Lit of Camera.
	 */
	[[nodiscard]] auto getListOfCameraNames() const -> std::vector<std::string>;

	/**
	 * @brief Destroy internal objects.
	 */
	void invalidate();

private:
	/**
	 * @brief Constructor.
	 */
	CameraSystem();

	void resize(const owl::math::vec2ui& iSize);

	owl::math::vec2ui m_size;

	int32_t m_frameSkip = 0;
	int32_t m_frameCheck = 50;
	int32_t m_frameCount = 0;

	owl::shared<owl::renderer::Texture> m_frame;
};

}// namespace drone::IO
