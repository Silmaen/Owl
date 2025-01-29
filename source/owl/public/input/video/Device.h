/**
 * @file Device.h
 * @author Silmaen
 * @date 03/01/2024
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once
#include "core/Core.h"
#include "renderer/Texture.h"

namespace owl::input::video {

/**
 * @brief Class describing the video input device.
 */
class OWL_API Device {
public:
	/**
	 * @brief Default Constructor.
	 */
	explicit Device(std::string iName);
	Device(const Device&) = delete;
	Device(Device&&) = delete;
	auto operator=(const Device&) -> Device& = delete;
	auto operator=(Device&&) -> Device& = delete;

	/**
	 * @brief Destructor.
	 */
	virtual ~Device();

	/**
	 * @brief Get the name of the device.
	 * @return The name of the device.
	 */
	[[nodiscard]] auto getName() const -> const std::string& { return m_name; }

	/**
	 * @brief Open this device.
	 */
	virtual void open() = 0;

	/**
	 * @brief Close this device.
	 */
	virtual void close() = 0;

	/**
	 * @brief Check if the device is open.
	 * @return True if open.
	 */
	[[nodiscard]] virtual auto isOpened() const -> bool = 0;

	/**
	 * @brief Check the validity of the device.
	 * @return True if valid.
	 */
	[[nodiscard]] virtual auto isValid() const -> bool = 0;

	/**
	 * @brief Retrieve a frame.
	 * @param[in] iFrame The frame to update.
	 */
	virtual void fillFrame(shared<renderer::Texture>& iFrame) = 0;

	/**
	 * @brief Get the unique bus information.
	 * @return Bus information.
	 *
	 * @note This bus information identifies the device as unique.
	 */
	[[nodiscard]] auto getBusInfo() const -> const std::string& { return m_busInfo; }

	/**
	 * @brief list of PixelFormat
	 */
	enum struct PixelFormat : uint8_t {
		Rgb24,///< R8G8B8 format
		Nv12,///< NV12 pixel format.
		YuYv,///< YUYV pixel format.
		MJpeg,///< MJPG pixel format.
		Unknwon,///< unknown or unsupported pixel format.
	};

	/**
	 * @brief Get the pixel format of the device.
	 * @return The pixel format for this device.
	 */
	[[nodiscard]] auto getPixelFormat() const -> const PixelFormat& { return m_pixFormat; }

	/**
	 * @brief Check the support for the pixel format.
	 * @param[in] iPixFormat The pixel format to test.
	 * @return True if supported.
	 */
	[[nodiscard]] static auto isPixelFormatSupported(const PixelFormat& iPixFormat) -> bool;

protected:
	// NOLINTBEGIN(readability-redundant-member-init)
	/// Bus info of the device, this is unique.
	std::string m_busInfo{};
	/// Name of the device.
	std::string m_name;
	/// The pixel format.
	PixelFormat m_pixFormat = PixelFormat::Unknwon;
	/// The size of the frame.
	math::vec2ui m_size;
	// NOLINTEND(readability-redundant-member-init)

	/**
	 * @brief Convert a raw buffer of pixel to RGB24 format.
	 * @param[in] iInputBuffer The input buffer.
	 * @param[in] iBufferSize The size of the buffer
	 * @return A converted RGB24 buffer.
	 */
	[[nodiscard]] auto getRgbBuffer(const uint8_t* iInputBuffer, int32_t iBufferSize) const -> std::vector<uint8_t>;
};

}// namespace owl::input::video
