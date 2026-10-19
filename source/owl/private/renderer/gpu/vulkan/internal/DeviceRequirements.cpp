/**
 * @file DeviceRequirements.cpp
 * @author Silmaen
 * @date 19/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "DeviceRequirements.h"

#include <cstdint>
#include <format>
#include <string>
#include <vector>

namespace owl::renderer::gpu::vulkan::internal {

namespace {
constexpr uint32_t g_VendorNvidia = 0x10DE;
#if defined(_WIN32)
constexpr uint32_t g_VendorIntel = 0x8086;
#endif

void require(std::vector<std::string>& ioMissing, const bool iPresent, const char* iWhat) {
	if (!iPresent)
		ioMissing.emplace_back(iWhat);
}
}// namespace

auto getMissingRequirements(const DeviceSupport& iSupport) -> std::vector<std::string> {
	std::vector<std::string> missing;
	const bool versionOk = iSupport.apiVersion >= g_RequiredDeviceApiVersion;
	if (!versionOk)
		missing.push_back(std::format("Vulkan {} (device has {})", formatApiVersion(g_RequiredDeviceApiVersion),
									  formatApiVersion(iSupport.apiVersion)));
	require(missing, iSupport.swapchain, "VK_KHR_swapchain");
	require(missing, iSupport.graphicsQueue, "graphics queue");
	require(missing, iSupport.presentQueue, "present queue");
	require(missing, iSupport.surfaceFormat, "surface format");
	require(missing, iSupport.presentMode, "present mode");
	require(missing, iSupport.samplerAnisotropy, "samplerAnisotropy");
	require(missing, iSupport.independentBlend, "independentBlend");
	require(missing, iSupport.wideLines, "wideLines");
	if (!versionOk)
		return missing;
	require(missing, iSupport.shaderDrawParameters, "shaderDrawParameters");
	require(missing, iSupport.shaderSampledImageArrayNonUniformIndexing, "shaderSampledImageArrayNonUniformIndexing");
	require(missing, iSupport.shaderDemoteToHelperInvocation, "shaderDemoteToHelperInvocation");
	return missing;
}

auto getUnsuitableDeviceNotice(const DeviceIdentity& iDevice, const std::vector<std::string>& iMissing) -> std::string {
	const auto driver = formatDriverVersion(iDevice.vendorId, iDevice.driverVersion);
	if (iDevice.apiVersion < g_RequiredDeviceApiVersion)
		return std::format("Vulkan 1.3 is not available on {} (driver {}, Vulkan {}.{}): update the graphics driver. "
						   "Owl Engine uses OpenGL meanwhile.",
						   iDevice.name, driver, (iDevice.apiVersion >> 22u) & 0x7Fu,
						   (iDevice.apiVersion >> 12u) & 0x3FFu);
	std::string missing;
	for (const auto& item: iMissing) missing += (missing.empty() ? "" : ", ") + item;
	return std::format("{} (driver {}) lacks {} for Vulkan: update the graphics driver. Owl Engine uses OpenGL "
					   "meanwhile.",
					   iDevice.name, driver, missing);
}

auto formatApiVersion(const uint32_t iVersion) -> std::string {
	return std::format("{}.{}.{}", (iVersion >> 22u) & 0x7Fu, (iVersion >> 12u) & 0x3FFu, iVersion & 0xFFFu);
}

auto formatDriverVersion(const uint32_t iVendorId, const uint32_t iDriverVersion) -> std::string {
	if (iVendorId == g_VendorNvidia)
		return std::format("{}.{:02}", (iDriverVersion >> 22u) & 0x3FFu, (iDriverVersion >> 14u) & 0xFFu);
#if defined(_WIN32)
	if (iVendorId == g_VendorIntel)
		return std::format("{}.{}", iDriverVersion >> 14u, iDriverVersion & 0x3FFFu);
#endif
	return formatApiVersion(iDriverVersion);
}

}// namespace owl::renderer::gpu::vulkan::internal
