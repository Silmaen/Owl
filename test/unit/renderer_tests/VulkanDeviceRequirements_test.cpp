/**
 * @file VulkanDeviceRequirements_test.cpp
 * @author Silmaen
 * @date 19/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <renderer/gpu/RenderAPI.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

// The Vulkan backend comes with the render module.
#if OWL_WITH_RENDER
#include <renderer/gpu/vulkan/internal/DeviceRequirements.h>

using namespace owl::renderer::gpu::vulkan::internal;

namespace {

auto suitableDevice() -> DeviceSupport {
	return {.apiVersion = makeApiVersion(1, 4, 303),
			.swapchain = true,
			.graphicsQueue = true,
			.presentQueue = true,
			.surfaceFormat = true,
			.presentMode = true,
			.samplerAnisotropy = true,
			.independentBlend = true,
			.wideLines = true,
			.shaderDrawParameters = true,
			.shaderSampledImageArrayNonUniformIndexing = true,
			.shaderDemoteToHelperInvocation = true};
}

auto contains(const std::vector<std::string>& iList, const std::string& iPart) -> bool {
	return std::ranges::any_of(
			iList, [&iPart](const std::string& iItem) -> bool { return iItem.find(iPart) != std::string::npos; });
}

}// namespace

TEST(VulkanDeviceRequirements, suitableDeviceHasNoMissingRequirement) {
	EXPECT_TRUE(getMissingRequirements(suitableDevice()).empty());
	auto exact = suitableDevice();
	exact.apiVersion = makeApiVersion(1, 3, 0);
	EXPECT_TRUE(getMissingRequirements(exact).empty());
}

TEST(VulkanDeviceRequirements, vulkan12DeviceIsRejectedWithItsVersion) {
	auto device = suitableDevice();
	device.apiVersion = makeApiVersion(1, 2, 198);
	// Below 1.3 the newer features cannot be queried: they stay false and are not listed.
	device.shaderDrawParameters = false;
	device.shaderSampledImageArrayNonUniformIndexing = false;
	device.shaderDemoteToHelperInvocation = false;
	const auto missing = getMissingRequirements(device);
	ASSERT_EQ(missing.size(), 1u);
	EXPECT_EQ(missing.front(), "Vulkan 1.3.0 (device has 1.2.198)");
}

TEST(VulkanDeviceRequirements, everyMissingFeatureIsListed) {
	DeviceSupport device;
	device.apiVersion = makeApiVersion(1, 3, 0);
	const auto missing = getMissingRequirements(device);
	EXPECT_EQ(missing.size(), 11u);
	for (const auto* name: {"VK_KHR_swapchain", "graphics queue", "present queue", "surface format", "present mode",
							"samplerAnisotropy", "independentBlend", "wideLines", "shaderDrawParameters",
							"shaderSampledImageArrayNonUniformIndexing", "shaderDemoteToHelperInvocation"}) {
		EXPECT_TRUE(contains(missing, name)) << name;
	}
}

TEST(VulkanDeviceRequirements, singleMissingFeature) {
	auto device = suitableDevice();
	device.shaderDemoteToHelperInvocation = false;
	const auto missing = getMissingRequirements(device);
	ASSERT_EQ(missing.size(), 1u);
	EXPECT_EQ(missing.front(), "shaderDemoteToHelperInvocation");
	device = suitableDevice();
	device.presentQueue = false;
	EXPECT_TRUE(contains(getMissingRequirements(device), "present queue"));
}

TEST(VulkanDeviceRequirements, versionFormatting) {
	EXPECT_EQ(formatApiVersion(makeApiVersion(1, 3, 280)), "1.3.280");
	EXPECT_EQ(formatApiVersion(0), "0.0.0");
	EXPECT_EQ(g_RequiredDeviceApiVersion, makeApiVersion(1, 3));
	// NVIDIA 10 / 8 / 8 / 6 bits, shown as its driver names: 456.71, 582.08.
	EXPECT_EQ(formatDriverVersion(0x10DE, (456u << 22u) | (71u << 14u) | (3u << 6u)), "456.71");
	EXPECT_EQ(formatDriverVersion(0x10DE, (582u << 22u) | (8u << 14u)), "582.08");
	// AMD (and Mesa) use the Vulkan encoding.
	EXPECT_EQ(formatDriverVersion(0x1002, makeApiVersion(2, 0, 302)), "2.0.302");
}

TEST(VulkanDeviceRequirements, noticeTellsToUpdateAnOldDriver) {
	const DeviceIdentity gtx1060{.name = "NVIDIA GeForce GTX 1060 6GB",
								 .vendorId = 0x10DE,
								 .driverVersion = (456u << 22u) | (71u << 14u),
								 .apiVersion = makeApiVersion(1, 2, 145)};
	EXPECT_EQ(getUnsuitableDeviceNotice(gtx1060, {"Vulkan 1.3.0 (device has 1.2.145)"}),
			  "Vulkan 1.3 is not available on NVIDIA GeForce GTX 1060 6GB (driver 456.71, Vulkan 1.2): update the "
			  "graphics driver. Owl Engine uses OpenGL meanwhile.");
	const DeviceIdentity recent{.name = "GPU",
								.vendorId = 0x1002,
								.driverVersion = makeApiVersion(2, 0, 302),
								.apiVersion = makeApiVersion(1, 3, 0)};
	EXPECT_EQ(getUnsuitableDeviceNotice(recent, {"wideLines", "independentBlend"}),
			  "GPU (driver 2.0.302) lacks wideLines, independentBlend for Vulkan: update the graphics driver. Owl "
			  "Engine uses OpenGL meanwhile.");
}
#endif

TEST(RenderApiFallback, vulkanFallsBackToOpenGl) {
	using owl::renderer::gpu::RenderAPI;
	EXPECT_EQ(RenderAPI::getFallback(RenderAPI::Type::Vulkan), RenderAPI::Type::OpenGL);
	EXPECT_FALSE(RenderAPI::getFallback(RenderAPI::Type::OpenGL).has_value());
	EXPECT_FALSE(RenderAPI::getFallback(RenderAPI::Type::Null).has_value());
}
