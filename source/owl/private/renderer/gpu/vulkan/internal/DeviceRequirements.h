/**
 * @file DeviceRequirements.h
 * @author Silmaen
 * @date 19/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include "core/Core.h"

#include <cstdint>
#include <string>
#include <vector>

namespace owl::renderer::gpu::vulkan::internal {

/**
 * @brief
 *  Encode a Vulkan version the way `VK_MAKE_API_VERSION` does (variant 0).
 * @param[in] iMajor Major version.
 * @param[in] iMinor Minor version.
 * @param[in] iPatch Patch version.
 * @return The encoded version.
 */
constexpr auto makeApiVersion(const uint32_t iMajor, const uint32_t iMinor, const uint32_t iPatch = 0) -> uint32_t {
	return (iMajor << 22u) | (iMinor << 12u) | iPatch;
}

/// Lowest Vulkan version a physical device must support: the backend calls 1.3 core entry points.
inline constexpr uint32_t g_RequiredDeviceApiVersion = makeApiVersion(1, 3);

/**
 * @brief
 *  What a physical device offers among the things the Vulkan backend needs, queried from its properties,
 *  features, extensions and queue families.
 */
struct DeviceSupport {
	/// Vulkan version supported by the device (`VkPhysicalDeviceProperties::apiVersion`).
	uint32_t apiVersion = 0;
	/// The device has `VK_KHR_swapchain`.
	bool swapchain = false;
	/// A queue family supports graphics.
	bool graphicsQueue = false;
	/// A queue family can present to the window surface.
	bool presentQueue = false;
	/// The surface offers at least one format.
	bool surfaceFormat = false;
	/// The surface offers at least one present mode.
	bool presentMode = false;
	/// Vulkan 1.0 feature `samplerAnisotropy` (texture samplers).
	bool samplerAnisotropy = false;
	/// Vulkan 1.0 feature `independentBlend` (per-attachment blending of the framebuffers).
	bool independentBlend = false;
	/// Vulkan 1.0 feature `wideLines` (line pipelines are 2 px wide).
	bool wideLines = false;
	/// Vulkan 1.1 feature `shaderDrawParameters` (`SV_VertexID` / `SV_InstanceID` in Slang shaders).
	bool shaderDrawParameters = false;
	/// Vulkan 1.2 feature `shaderSampledImageArrayNonUniformIndexing` (texture arrays of the batch renderers).
	bool shaderSampledImageArrayNonUniformIndexing = false;
	/// Vulkan 1.3 feature `shaderDemoteToHelperInvocation` (`discard` in Slang shaders).
	bool shaderDemoteToHelperInvocation = false;
};

/**
 * @brief
 *  What a player is told about a physical device.
 */
struct DeviceIdentity {
	/// Device name (`VkPhysicalDeviceProperties::deviceName`).
	std::string name;
	/// PCI vendor id, which picks the driver version encoding.
	uint32_t vendorId = 0;
	/// Raw driver version (`VkPhysicalDeviceProperties::driverVersion`).
	uint32_t driverVersion = 0;
	/// Vulkan version supported by the device.
	uint32_t apiVersion = 0;
};

/**
 * @brief
 *  List what a device lacks to run the Vulkan backend.
 * @param[in] iSupport What the device offers.
 * @return One human-readable reason per missing requirement, empty when the device is suitable. Below
 * Vulkan 1.3 the newer features cannot be queried, so only the version and the Vulkan 1.0 items are reported.
 */
[[nodiscard]] OWL_API auto getMissingRequirements(const DeviceSupport& iSupport) -> std::vector<std::string>;

/**
 * @brief
 *  Word, for a player, why a device cannot run the Vulkan backend.
 * @param[in] iDevice The device.
 * @param[in] iMissing What it lacks (getMissingRequirements()).
 * @return One sentence telling what to do, ending with the OpenGL fallback.
 */
[[nodiscard]] OWL_API auto getUnsuitableDeviceNotice(const DeviceIdentity& iDevice,
													 const std::vector<std::string>& iMissing) -> std::string;

/**
 * @brief
 *  Format a Vulkan version as `major.minor.patch`.
 * @param[in] iVersion The encoded version.
 * @return The version text.
 */
[[nodiscard]] OWL_API auto formatApiVersion(uint32_t iVersion) -> std::string;

/**
 * @brief
 *  Format a driver version with the vendor's own encoding: NVIDIA `major.minor` (10.8.8.6 bits, as its
 *  driver names), Intel on Windows `major.minor` (18.14 bits), the others `major.minor.patch` as Vulkan.
 * @param[in] iVendorId The PCI vendor id of the device.
 * @param[in] iDriverVersion The raw `VkPhysicalDeviceProperties::driverVersion`.
 * @return The version text.
 */
[[nodiscard]] OWL_API auto formatDriverVersion(uint32_t iVendorId, uint32_t iDriverVersion) -> std::string;

}// namespace owl::renderer::gpu::vulkan::internal
