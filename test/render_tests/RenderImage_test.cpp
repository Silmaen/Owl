/**
 * @file RenderImage_test.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include "ImageCompare.h"

#include <renderer/TextureDecoder.h>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <print>
#include <sstream>
#include <string>
#include <vector>

using namespace owl;
using test::render::compareImages;
using test::render::ImageTolerance;

namespace {

// One reference image: a scene rendered by one backend on its software driver.
struct RenderCase {
	// Scene file stem in `test/render_tests/scenes`.
	std::string scene;
	// Backend name passed to `OwlRunner --backend`.
	std::string backend;
};

// Capture size: small enough for software rasterisers, large enough for the text.
constexpr math::vec2ui g_captureSize{320, 180};
// Frames run before the capture (streaming, animations and async loads settle).
constexpr uint32_t g_warmupFrames = 60;

auto readFile(const std::filesystem::path& iPath) -> std::string {
	std::ifstream in(iPath, std::ios::binary);
	std::stringstream ss;
	ss << in.rdbuf();
	return ss.str();
}

auto getEnv(const char* iName) -> std::string {
	// NOLINTNEXTLINE(concurrency-mt-unsafe): tests read the environment before spawning anything.
	const char* value = std::getenv(iName);
	return value == nullptr ? std::string{} : std::string{value};
}

auto getVulkanIcd() -> std::filesystem::path {
	if (const auto custom = getEnv("OWL_RENDER_TESTS_VK_ICD"); !custom.empty())
		return custom;
	return "/usr/share/vulkan/icd.d/lvp_icd.json";
}

// Environment selecting the software driver of a backend (lavapipe or llvmpipe).
auto getDriverEnvironment(const std::string& iBackend) -> std::string {
	if (iBackend == "vulkan")
		return std::format("VK_ICD_FILENAMES={}", getVulkanIcd().string());
	return "LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe __GLX_VENDOR_LIBRARY_NAME=mesa";
}

auto getOutputDir() -> std::filesystem::path {
#ifdef OWL_RENDER_TEST_OUTPUT_DIR
	return OWL_RENDER_TEST_OUTPUT_DIR;
#else
	return std::filesystem::temp_directory_path() / "owl_render_tests";
#endif
}

class RenderImage : public testing::TestWithParam<RenderCase> {};

}// namespace

TEST(RenderImageCompare, identicalImagesMatch) {
	const std::vector<uint8_t> image(4u * 4u * 4u, 128);
	const auto result = compareImages({4, 4}, image, image, {});
	EXPECT_TRUE(result.match);
	EXPECT_EQ(result.differingPixels, 0u);
	EXPECT_EQ(result.maxDifference, 0u);
}

TEST(RenderImageCompare, toleranceAppliesPerChannelAndRatio) {
	std::vector<uint8_t> reference(10u * 10u * 4u, 100);
	auto actual = reference;
	actual[0] = 120;// below the channel tolerance
	actual[4 * 5 + 1] = 200;// one pixel above it
	EXPECT_TRUE(compareImages({10, 10}, actual, reference, {.channel = 24, .ratio = 0.01}).match);
	const auto strict = compareImages({10, 10}, actual, reference, {.channel = 24, .ratio = 0.0});
	EXPECT_FALSE(strict.match);
	EXPECT_EQ(strict.differingPixels, 1u);
	EXPECT_EQ(strict.maxDifference, 100u);
	EXPECT_EQ(strict.diff[4 * 5], 255u);
	actual[3] = 0;// alpha is ignored
	EXPECT_EQ(compareImages({10, 10}, actual, reference, {}).differingPixels, 1u);
}

TEST(RenderImageCompare, flipRowsSwapsTopAndBottom) {
	const std::vector<uint8_t> image{1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4};
	const auto flipped = test::render::flipRows({2, 2}, image);
	EXPECT_EQ(flipped, (std::vector<uint8_t>{3, 3, 3, 3, 4, 4, 4, 4, 1, 1, 1, 1, 2, 2, 2, 2}));
}

TEST(RenderImageCompare, sizeMismatchNeverMatches) {
	const std::vector<uint8_t> small(4u * 4u * 4u, 0);
	const std::vector<uint8_t> big(8u * 8u * 4u, 0);
	EXPECT_FALSE(compareImages({4, 4}, small, big, {}).match);
	EXPECT_FALSE(compareImages({0, 0}, {}, {}, {}).match);
}

TEST_P(RenderImage, matchesReference) {
#ifndef OWL_RUNNER_EXECUTABLE
	GTEST_SKIP() << "OwlRunner is not built (OWL_BUILD_NEST=OFF).";
#else
	const auto& [scene, backend] = GetParam();
	if (backend == "vulkan" && !exists(getVulkanIcd()))
		GTEST_SKIP() << "lavapipe not found (" << getVulkanIcd() << "); set OWL_RENDER_TESTS_VK_ICD.";
	if (getEnv("DISPLAY").empty() && getEnv("WAYLAND_DISPLAY").empty())
		GTEST_SKIP() << "No display: run the render tests under xvfb-run (ctest does it when xvfb-run exists).";

	const auto root = test::getRootPath();
	const auto sceneFile = root / "test" / "render_tests" / "scenes" / (scene + ".owl");
	const auto reference = root / "test" / "render_tests" / "references" / backend / (scene + ".png");
	const auto outDir = getOutputDir();
	std::filesystem::create_directories(outDir);
	const auto stem = std::format("{}.{}", scene, backend);
	const auto actualFile = outDir / (stem + ".actual.png");
	const auto diffFile = outDir / (stem + ".diff.png");
	const auto logFile = outDir / (stem + ".log");
	std::filesystem::remove(actualFile);
	std::filesystem::remove(diffFile);

	const auto command = std::format(R"(env -u OWL_OPENGL_SHADERS {} "{}" --frame-bench "{}" --project "{}" )"
									 R"(--backend {} --frames 2 --warmup {} --size {}x{} --validation )"
									 R"(--capture "{}" > "{}" 2>&1)",
									 getDriverEnvironment(backend), OWL_RUNNER_EXECUTABLE, sceneFile.string(),
									 (root / "sample_project").string(), backend, g_warmupFrames, g_captureSize.x(),
									 g_captureSize.y(), actualFile.string(), logFile.string());
	// NOLINTNEXTLINE(concurrency-mt-unsafe,cert-env33-c): the runner must run in its own process (one GPU context).
	const int status = std::system(command.c_str());
	const auto log = readFile(logFile);
	ASSERT_EQ(status, 0) << "OwlRunner failed, log: " << logFile;
	EXPECT_NE(log.find("llvmpipe"), std::string::npos) << "Not rendered by a software driver, log: " << logFile;
	EXPECT_EQ(log.find("VUID-"), std::string::npos) << "Vulkan validation messages, log: " << logFile;

	const auto actual = renderer::decodeImageFile(actualFile, 4);
	ASSERT_TRUE(actual.valid) << "No capture written, log: " << logFile;
	ASSERT_EQ(actual.size.x(), g_captureSize.x());
	ASSERT_EQ(actual.size.y(), g_captureSize.y());

	if (!getEnv("OWL_RENDER_TESTS_UPDATE").empty()) {
		std::filesystem::create_directories(reference.parent_path());
		std::filesystem::copy_file(actualFile, reference, std::filesystem::copy_options::overwrite_existing);
		std::println("Reference updated: {}", reference.string());
		return;
	}
	const auto expected = renderer::decodeImageFile(reference, 4);
	ASSERT_TRUE(expected.valid) << "Missing reference " << reference
								<< " (regenerate with OWL_RENDER_TESTS_UPDATE=1), capture: " << actualFile;
	ASSERT_TRUE(expected.size.x() == actual.size.x() && expected.size.y() == actual.size.y())
			<< "Reference size differs: " << reference;
	constexpr ImageTolerance tolerance;
	const auto result = compareImages(actual.size, actual.pixels, expected.pixels, tolerance);
	if (!result.match)
		renderer::writeImagePng(diffFile, actual.size, test::render::flipRows(actual.size, result.diff));
	EXPECT_TRUE(result.match) << std::format("{:.3f} % of the pixels differ by more than {} (max {} %, largest {}); "
											 "capture: {}, diff: {}",
											 result.ratio * 100.0, tolerance.channel, tolerance.ratio * 100.0,
											 result.maxDifference, actualFile.string(), diffFile.string());
	if (result.match)
		std::filesystem::remove(actualFile);
#endif
}

INSTANTIATE_TEST_SUITE_P(Scenes, RenderImage,
						 testing::Values(RenderCase{"sprites", "vulkan"}, RenderCase{"sprites", "opengl"},
										 RenderCase{"text", "vulkan"}, RenderCase{"text", "opengl"},
										 RenderCase{"tilemap", "vulkan"}, RenderCase{"tilemap", "opengl"},
										 RenderCase{"raycast", "vulkan"}, RenderCase{"raycast", "opengl"},
										 RenderCase{"voxel", "vulkan"}, RenderCase{"voxel", "opengl"},
										 RenderCase{"mixed", "vulkan"}, RenderCase{"mixed", "opengl"}),
						 [](const testing::TestParamInfo<RenderCase>& iInfo) -> std::string {
							 return iInfo.param.scene + "_" + iInfo.param.backend;
						 });
