/**
 * @file Device.h
 * @author Silmaen
 * @date 03/01/2024
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once
#include "input/video/Device.h"

#ifdef OWL_PLATFORM_LINUX
#include <linux/videodev2.h>

/**
 * @brief Linux video devices.
 */
namespace owl::input::video::linux64 {

/**
 * @brief Search for devices and them to the given list.
 * @param[in] ioList The device list to update.
 */
void updateList(std::vector<shared<Device>>& ioList);

/**
 * @brief Class Device.
 */
class Device final : public video::Device {
public:
	/**
	 * @brief Constructor using only a file.
	 * @param[in] iFile The file to read.
	 */
	explicit Device(std::string iFile);
	Device(Device&&) = delete;
	Device(const Device&) = delete;
	auto operator=(Device&&) -> Device& = delete;
	auto operator=(const Device&) -> Device& = delete;

	/**
	 * @brief Destructor.
	 */
	~Device() override;

	/**
	 * @brief Open this device.
	 */
	void open() override;

	/**
	 * @brief Close this device.
	 */
	void close() override;

	/**
	 * @brief Check if the device is open.
	 * @return True if open.
	 */
	[[nodiscard]] auto isOpened() const -> bool override;

	/**
	 * @brief Get the file.
	 * @return File name.
	 */
	[[nodiscard]] auto getFile() const -> const std::string& { return m_file; }

	/**
	 * @brief Retrieve a frame.
	 * @param[in,out] ioFrame The frame to update.
	 */
	void fillFrame(shared<renderer::Texture>& ioFrame) override;

	/**
	 * @brief Check if this device is valid.
	 * @return True if this device is valid.
	 */
	[[nodiscard]] auto isValid() const -> bool override;

private:
	/// The file representation of the device.
	std::string m_file;
	/// The file handler.
	int m_fileHandler = 0;
	/// buffer map to the device.
	void* mp_buffer = nullptr;
	/// Buffer info.
	v4l2_buffer m_bufferInfo{};
	/// if the streaming is started.
	bool m_streaming = false;

	void printSupportedFormat() const;

	/**
	 * \brief fist suitable pixel format.
	 * \param iFd File descriptor, use fileHandler if -1. File must already be opened.
	 * \return The first pixel format supported by both device and present code.
	 */
	[[nodiscard]] auto getFirstSupportedPixelFormat(int32_t iFd = -1) const -> PixelFormat;
};

}// namespace owl::input::video::linux64

#endif
