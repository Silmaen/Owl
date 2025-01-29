/**
 * @file Device.cpp
 * @author Silmaen
 * @date 03/01/2024
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "input/video/Device.h"
#include <stb_image.h>

namespace owl::input::video {

namespace {

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG16("-Wunsafe-buffer-usage")
// NOLINTBEGIN(*-magic-numbers)
void convertNv12ToRgb24(const uint8_t* iNv12Buffer, const math::vec2ui& iSize, uint8_t* oRgb24Buffer) {
	// Each component Y occupy width * height bytes.
	const uint32_t ySize = iSize.surface();

	// Pointers to component Y, U and V in buffer NV12
	const uint8_t* yComponent = iNv12Buffer;
	const uint8_t* uvComponent = iNv12Buffer + ySize;

	for (uint32_t i = 0; i < iSize.y(); i++) {
		for (uint32_t j = 0; j < iSize.x(); j++) {
			// index in buffer NV12
			const uint32_t yIndex = i * iSize.x() + j;
			const uint32_t uvIndex = (i / 2ul * iSize.x()) + (j & ~1ul);
			// index in buffer RGB24
			const uint32_t rgbIndex = ((i + 1) * iSize.x() - j - 1) * 3;
			// Convert YUV to RGB
			const int32_t c = yComponent[yIndex] - 16;
			const int32_t d = uvComponent[uvIndex] - 128;
			const int32_t e = uvComponent[uvIndex + 1] - 128;
			// Compute RGB component, limit values in range [0, 255]
			oRgb24Buffer[rgbIndex] = static_cast<uint8_t>(math::clamp((298 * c + 409 * e + 128) >> 8, 0, 255));
			oRgb24Buffer[rgbIndex + 1] =
					static_cast<uint8_t>(math::clamp((298 * c - 100 * d - 208 * e + 128) >> 8, 0, 255));
			oRgb24Buffer[rgbIndex + 2] = static_cast<uint8_t>(math::clamp((298 * c + 516 * d + 128) >> 8, 0, 255));
		}
	}
}

void convertYuYvToRgb24(const uint8_t* iYuYvBuffer, const math::vec2ui& iSize, uint8_t* oRgb24Buffer) {
	// Each component YUV takes 2 bytes in YUYV format.
	const uint32_t yuyvSize = iSize.surface() * 2;

	for (uint32_t i = 0; i < yuyvSize; i += 4) {
		// Convert YUV to RGB for first pixel
		const int32_t c0 = iYuYvBuffer[i] - 16;
		const int32_t d0 = iYuYvBuffer[i + 1] - 128;
		const int32_t e0 = iYuYvBuffer[i + 3] - 128;
		// Convert YUV to RGB for second pixel
		const int32_t c1 = iYuYvBuffer[i + 2] - 16;
		// Indices dans le tampon RGB24
		const uint32_t rgbIndex = i * 3 / 2;

		// Store RGB component in RGB24 buffer for the 2 pixels.
		oRgb24Buffer[rgbIndex] = static_cast<uint8_t>(math::clamp((298 * c0 + 409 * e0 + 128) >> 8, 0, 255));
		oRgb24Buffer[rgbIndex + 1] =
				static_cast<uint8_t>(math::clamp((298 * c0 - 100 * d0 - 208 * e0 + 128) >> 8, 0, 255));
		oRgb24Buffer[rgbIndex + 2] = static_cast<uint8_t>(math::clamp((298 * c0 + 516 * d0 + 128) >> 8, 0, 255));

		oRgb24Buffer[rgbIndex + 3] = static_cast<uint8_t>(math::clamp((298 * c1 + 409 * e0 + 128) >> 8, 0, 255));
		oRgb24Buffer[rgbIndex + 4] =
				static_cast<uint8_t>(math::clamp((298 * c1 - 100 * d0 - 208 * e0 + 128) >> 8, 0, 255));
		oRgb24Buffer[rgbIndex + 5] = static_cast<uint8_t>(math::clamp((298 * c1 + 516 * d0 + 128) >> 8, 0, 255));
	}
}

void convertMJpegToRgb24(const uint8_t* iJpegBuffer, const int32_t iJpegSize, const math::vec2ui& iSize,
						 uint8_t* oRgb24Buffer) {
	int comp = 0;
	int width = 0;
	int height = 0;
	stbi_set_flip_vertically_on_load(0);
	uint8_t* buffer = stbi_load_from_memory(iJpegBuffer, iJpegSize, &width, &height, &comp, 3);
	if (buffer == nullptr) {
		OWL_CORE_WARN("Jpeg decoding: nullptr result.")
		return;
	}
	if (iSize != math::vec2ui{static_cast<uint32_t>(width), static_cast<uint32_t>(height)}) {
		OWL_CORE_WARN("Jpeg decoding: size missmatch ({} {}) expecting {} {}.", width, height, iSize.x(), iSize.y())
		stbi_image_free(buffer);
		return;
	}
	if (comp != 3) {
		OWL_CORE_WARN("Jpeg decoding: wrong number of channels: {} expecting 3.", comp)
		stbi_image_free(buffer);
		return;
	}
	//int rowSize = width * 3;
	for (int i = 0; i < height; ++i) {
		for (int j = 0; j < width; ++j) {
			const int sourceIndex = (i * width + j) * 3;
			const int destinationIndex = ((i + 1) * width - j - 1) * 3;// Inversion horizontale
			std::memcpy(oRgb24Buffer + destinationIndex, buffer + sourceIndex, 3);
		}
	}
	//std::memcpy(rgb24Buffer, buffer, frameSize.surface() * 3);
	stbi_image_free(buffer);
}
// NOLINTEND(*-magic-numbers)
OWL_DIAG_POP

}// namespace

Device::Device(std::string iName) : m_name(std::move(iName)) {}

Device::~Device() = default;

auto Device::getRgbBuffer(const uint8_t* iInputBuffer, const int32_t iBufferSize) const -> std::vector<uint8_t> {
	std::vector<uint8_t> output;
	if (m_pixFormat == PixelFormat::Nv12) {
		output.resize(3ull * m_size.surface());
		convertNv12ToRgb24(iInputBuffer, m_size, output.data());
	} else if (m_pixFormat == PixelFormat::Rgb24) {
		output.resize(3ull * m_size.surface());
		memcpy(output.data(), iInputBuffer, output.size());
	} else if (m_pixFormat == PixelFormat::YuYv) {
		output.resize(3ull * m_size.surface());
		convertYuYvToRgb24(iInputBuffer, m_size, output.data());
	} else if (m_pixFormat == PixelFormat::MJpeg) {
		output.resize(3ull * m_size.surface());
		convertMJpegToRgb24(iInputBuffer, iBufferSize, m_size, output.data());
	} else {
		OWL_CORE_WARN("Unknown or unsupported pixel format, empty output buffer.")
	}
	return output;
}

auto Device::isPixelFormatSupported(const PixelFormat& iPixFormat) -> bool {
	switch (iPixFormat) {
		case PixelFormat::Rgb24:
		case PixelFormat::YuYv:
		case PixelFormat::Nv12:
		case PixelFormat::MJpeg:
			return true;
		case PixelFormat::Unknwon:
			return false;
	}
	return false;
}

}// namespace owl::input::video
