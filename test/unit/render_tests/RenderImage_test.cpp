/**
 * @file RenderImage_test.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include "ImageCompare.h"

#include <cstdio>
#include <math/vectors.h>
#include <renderer/TextureDecoder.h>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace owl;
using test::render::compareImages;
using test::render::ImageTolerance;

namespace {

// Default capture size: small enough for software rasterisers, large enough for the text.
constexpr math::vec2ui g_captureSize{320, 180};

// One reference image: a scene rendered by one backend on its software driver.
struct RenderCase {
	// Scene file stem in `test/unit/test_helper/assets/render_tests/scenes`.
	std::string scene;
	// Backend name passed to `OwlRunner --backend`.
	std::string backend;
	// Capture size; a size other than the default suffixes the reference name with it.
	math::vec2ui size = g_captureSize;
};

// Readable case names in the gtest output (the default prints the object bytes).
[[maybe_unused]] void PrintTo(const RenderCase& iCase, std::ostream* oStream) {
	*oStream << iCase.scene << " on " << iCase.backend << " at " << iCase.size.x() << "x" << iCase.size.y();
}

// Reference and capture stem of a case: the scene, plus the size when it is not the default one.
auto getCaseStem(const RenderCase& iCase) -> std::string {
	if (iCase.size == g_captureSize)
		return iCase.scene;
	return std::format("{}_{}x{}", iCase.scene, iCase.size.x(), iCase.size.y());
}
// Frames run before the capture (streaming, animations and async loads settle).
constexpr uint32_t g_warmupFrames = 60;

auto readFile(const std::filesystem::path& iPath) -> std::string {
	const std::ifstream in(iPath, std::ios::binary);
	std::stringstream ss;
	ss << in.rdbuf();
	return ss.str();
}

auto getEnv(const char* iName) -> std::string {
	const char* value = std::getenv(iName);
	return value == nullptr ? std::string{} : std::string{value};
}

auto getVulkanIcd() -> std::filesystem::path {
	if (auto custom = getEnv("OWL_RENDER_TESTS_VK_ICD"); !custom.empty())
		return custom;
	return "/usr/share/vulkan/icd.d/lvp_icd.json";
}

auto getOutputDir() -> std::filesystem::path {
#ifdef OWL_RENDER_TEST_OUTPUT_DIR
	return OWL_RENDER_TEST_OUTPUT_DIR;
#else
	return std::filesystem::temp_directory_path() / "owl_render_tests";
#endif
}

// Environment selecting the software driver of a backend (lavapipe or llvmpipe). The CPU features are capped at
// SSE4.1, so the tilemap seams do not drift with the host (AVX or not); Mesa's shader cache ignores that cap, so the
// image tests keep a cache of their own that only capped runs fill (the default one hands back AVX code and crashes).
auto getDriverEnvironment(const std::string& iBackend) -> std::string {
	const auto softwareCpu = std::format(R"(GALLIUM_OVERRIDE_CPU_CAPS=sse4.1 MESA_SHADER_CACHE_DIR="{}")",
										 (getOutputDir() / "mesa_shader_cache").string());
	if (iBackend == "vulkan")
		return std::format("{} VK_ICD_FILENAMES={}", softwareCpu, getVulkanIcd().string());
	return std::format("{} LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe __GLX_VENDOR_LIBRARY_NAME=mesa",
					   softwareCpu);
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
	const auto& [scene, backend, size] = GetParam();
	if (backend == "vulkan" && !exists(getVulkanIcd()))
		GTEST_SKIP() << "lavapipe not found (" << getVulkanIcd() << "); set OWL_RENDER_TESTS_VK_ICD.";
	if (getEnv("DISPLAY").empty() && getEnv("WAYLAND_DISPLAY").empty())
		GTEST_SKIP() << "No display: run the render tests under xvfb-run (ctest does it when xvfb-run exists).";

	const auto root = test::getRootPath();
	const auto fixtures = test::getTestAssetsPath("render_tests");
	const auto sceneFile = fixtures / "scenes" / (scene + ".owl");
	const auto reference = fixtures / "references" / backend / (getCaseStem(GetParam()) + ".png");
	const auto outDir = getOutputDir();
	std::filesystem::create_directories(outDir);
	const auto stem = std::format("{}.{}", getCaseStem(GetParam()), backend);
	const auto actualFile = outDir / (stem + ".actual.png");
	const auto diffFile = outDir / (stem + ".diff.png");
	const auto logFile = outDir / (stem + ".log");
	std::filesystem::remove(actualFile);
	std::filesystem::remove(diffFile);

	const auto command = std::format(R"(env -u OWL_OPENGL_SHADERS {} "{}" --frame-bench "{}" --project "{}" )"
									 R"(--assets "{}" --backend {} --frames 2 --warmup {} --size {}x{} --validation )"
									 R"(--capture "{}" > "{}" 2>&1)",
									 getDriverEnvironment(backend), OWL_RUNNER_EXECUTABLE, sceneFile.string(),
									 (root / "engine_demo").string(), fixtures.string(), backend, g_warmupFrames,
									 size.x(), size.y(), actualFile.string(), logFile.string());
	// NOLINTNEXTLINE(bugprone-command-processor) The runner must run in its own process (one GPU context).
	const int status = std::system(command.c_str());
	const auto log = readFile(logFile);
	ASSERT_EQ(status, 0) << "OwlRunner failed, log: " << logFile;
	EXPECT_NE(log.find("llvmpipe"), std::string::npos) << "Not rendered by a software driver, log: " << logFile;
	EXPECT_EQ(log.find("VUID-"), std::string::npos) << "Vulkan validation messages, log: " << logFile;
	EXPECT_EQ(log.find("could not start, falling back"), std::string::npos)
			<< "The requested backend did not start, log: " << logFile;
	if (backend == "vulkan") {
		EXPECT_NE(log.find("Vulkan: Using llvmpipe"), std::string::npos) << "lavapipe not selected, log: " << logFile;
	}

	const auto actual = renderer::decodeImageFile(actualFile, 4);
	ASSERT_TRUE(actual.valid) << "No capture written, log: " << logFile;
	ASSERT_EQ(actual.size.x(), size.x());
	ASSERT_EQ(actual.size.y(), size.y());

	if (!getEnv("OWL_RENDER_TESTS_UPDATE").empty()) {
		std::filesystem::create_directories(reference.parent_path());
		std::filesystem::copy_file(actualFile, reference, std::filesystem::copy_options::overwrite_existing);
		std::fputs(std::format("Reference updated: {}\n", reference.string()).c_str(), stdout);
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

INSTANTIATE_TEST_SUITE_P(
		Scenes, RenderImage,
		testing::Values(RenderCase{"sprites", "vulkan"}, RenderCase{"sprites", "opengl"}, RenderCase{"text", "vulkan"},
						RenderCase{"text", "opengl"}, RenderCase{"tilemap", "vulkan"}, RenderCase{"tilemap", "opengl"},
						RenderCase{"tilemap_opacity", "vulkan"}, RenderCase{"tilemap_opacity", "opengl"},
						RenderCase{"raycast", "vulkan"}, RenderCase{"raycast", "opengl"}, RenderCase{"voxel", "vulkan"},
						RenderCase{"voxel", "opengl"}, RenderCase{"mixed", "vulkan"}, RenderCase{"mixed", "opengl"},
						RenderCase{"stylemix", "vulkan"}, RenderCase{"stylemix", "opengl"},
						RenderCase{"isometric", "vulkan"}, RenderCase{"isometric", "opengl"}),
		[](const testing::TestParamInfo<RenderCase>& iInfo) -> std::string {
			return iInfo.param.scene + "_" + iInfo.param.backend;
		});

// Isometric buildings sliced per tile and vehicles at an integer zoom (pixel snapping), on a larger capture.
INSTANTIATE_TEST_SUITE_P(IsometricObjects, RenderImage,
						 testing::Values(RenderCase{"isometric_objects", "vulkan", {640, 360}},
										 RenderCase{"isometric_objects", "opengl", {640, 360}}),
						 [](const testing::TestParamInfo<RenderCase>& iInfo) -> std::string {
							 return std::format("{}_{}_{}x{}", iInfo.param.scene, iInfo.param.backend,
												iInfo.param.size.x(), iInfo.param.size.y());
						 });

// The isometric town of the Engine Demo: sloped terrain, roads along both axes, buildings, trees, vehicles and the
// 2D sky behind, under the town HUD. At night, the 2D lights light the buildings through their normal and depth
// atlases: a dim blue ambient, a warm street lamp and a cold light on the square.
INSTANTIATE_TEST_SUITE_P(IsometricTown, RenderImage,
						 testing::Values(RenderCase{"iso_town", "vulkan", {640, 360}},
										 RenderCase{"iso_town", "opengl", {640, 360}},
										 RenderCase{"iso_town_night", "vulkan", {640, 360}},
										 RenderCase{"iso_town_night", "opengl", {640, 360}}),
						 [](const testing::TestParamInfo<RenderCase>& iInfo) -> std::string {
							 return std::format("{}_{}_{}x{}", iInfo.param.scene, iInfo.param.backend,
												iInfo.param.size.x(), iInfo.param.size.y());
						 });

// The same HUD at three resolutions over a raycast scene (anchors, stretch and scale modes on screen), and over a voxel
// scene at the default one (the HUD composes with a 3D layer; the voxel frame is the slowest under llvmpipe).
INSTANTIATE_TEST_SUITE_P(Hud, RenderImage,
						 testing::Values(RenderCase{"hud_raycast", "vulkan"}, RenderCase{"hud_raycast", "opengl"},
										 RenderCase{"hud_raycast", "vulkan", {480, 270}},
										 RenderCase{"hud_raycast", "opengl", {480, 270}},
										 RenderCase{"hud_raycast", "vulkan", {240, 240}},
										 RenderCase{"hud_raycast", "opengl", {240, 240}},
										 RenderCase{"hud_voxel", "vulkan"}, RenderCase{"hud_voxel", "opengl"}),
						 [](const testing::TestParamInfo<RenderCase>& iInfo) -> std::string {
							 return std::format("{}_{}_{}x{}", iInfo.param.scene, iInfo.param.backend,
												iInfo.param.size.x(), iInfo.param.size.y());
						 });

// Windows over a 2D scene: title bars, layouts (column, grid), overlapping windows covering each other, content
// and panels clipped by the scissor, a window clamped into the screen; at a scale of 1 and of 1.5.
INSTANTIATE_TEST_SUITE_P(HudWindows, RenderImage,
						 testing::Values(RenderCase{"hud_windows", "vulkan"}, RenderCase{"hud_windows", "opengl"},
										 RenderCase{"hud_windows", "vulkan", {480, 270}},
										 RenderCase{"hud_windows", "opengl", {480, 270}}),
						 [](const testing::TestParamInfo<RenderCase>& iInfo) -> std::string {
							 return std::format("{}_{}_{}x{}", iInfo.param.scene, iInfo.param.backend,
												iInfo.param.size.x(), iInfo.param.size.y());
						 });

// A scroll view of buttons (placed by a column layout, scrolled by 40, with its bar) beside a list of 40 rows (row 5
// selected, scrolled by 66): the content cut by the scissor, the rows in view only; at a scale of 1 and of 1.5.
INSTANTIATE_TEST_SUITE_P(HudScroll, RenderImage,
						 testing::Values(RenderCase{"hud_scroll", "vulkan"}, RenderCase{"hud_scroll", "opengl"},
										 RenderCase{"hud_scroll", "vulkan", {480, 270}},
										 RenderCase{"hud_scroll", "opengl", {480, 270}}),
						 [](const testing::TestParamInfo<RenderCase>& iInfo) -> std::string {
							 return std::format("{}_{}_{}x{}", iInfo.param.scene, iInfo.param.backend,
												iInfo.param.size.x(), iInfo.param.size.y());
						 });

// The 2D lights: a dim blue ambient (night), a warm point light and a cold spot light, both casting soft shadows of
// two sprites, over a normal-mapped brick floor; the green marker is unlit and keeps its colour. The tilemap scene at
// dusk with a campfire: tilemaps take the same lights.
INSTANTIATE_TEST_SUITE_P(Lights2D, RenderImage,
						 testing::Values(RenderCase{"lights2d", "vulkan"}, RenderCase{"lights2d", "opengl"},
										 RenderCase{"tilemap_lit", "vulkan"}, RenderCase{"tilemap_lit", "opengl"}),
						 [](const testing::TestParamInfo<RenderCase>& iInfo) -> std::string {
							 return iInfo.param.scene + "_" + iInfo.param.backend;
						 });

// Draw order across primitive types: quads, circles and text at interleaved depths cover each other by their world z
// (a quad stored last but lowest goes under the others), not by their type.
INSTANTIATE_TEST_SUITE_P(DrawOrder, RenderImage,
						 testing::Values(RenderCase{"draworder", "vulkan"}, RenderCase{"draworder", "opengl"}),
						 [](const testing::TestParamInfo<RenderCase>& iInfo) -> std::string {
							 return iInfo.param.scene + "_" + iInfo.param.backend;
						 });

// Keyboard and gamepad focus: the button flagged `initial` takes it when the scene plays and is framed in gold, among
// two other buttons and a slider; at a scale of 1 and of 1.5.
INSTANTIATE_TEST_SUITE_P(HudFocus, RenderImage,
						 testing::Values(RenderCase{"hud_focus", "vulkan"}, RenderCase{"hud_focus", "opengl"},
										 RenderCase{"hud_focus", "vulkan", {480, 270}},
										 RenderCase{"hud_focus", "opengl", {480, 270}}),
						 [](const testing::TestParamInfo<RenderCase>& iInfo) -> std::string {
							 return std::format("{}_{}_{}x{}", iInfo.param.scene, iInfo.param.backend,
												iInfo.param.size.x(), iInfo.param.size.y());
						 });

// HUD text under a shrinking scale mode (reference 1280x720 shown at 480x270): the glyphs keep a legible height (the
// text scale floor) and are smoothed by the supersampled MSDF coverage.
INSTANTIATE_TEST_SUITE_P(HudText, RenderImage,
						 testing::Values(RenderCase{"hud_text", "vulkan", {480, 270}},
										 RenderCase{"hud_text", "opengl", {480, 270}}),
						 [](const testing::TestParamInfo<RenderCase>& iInfo) -> std::string {
							 return std::format("{}_{}_{}x{}", iInfo.param.scene, iInfo.param.backend,
												iInfo.param.size.x(), iInfo.param.size.y());
						 });

// Outlined and boxed HUD text on a light background: the outline is the glyph grown by its signed distance in the same
// draw, the box a quad behind the text block.
INSTANTIATE_TEST_SUITE_P(HudTextOutline, RenderImage,
						 testing::Values(RenderCase{"hud_text_outline", "vulkan", {640, 360}},
										 RenderCase{"hud_text_outline", "opengl", {640, 360}}),
						 [](const testing::TestParamInfo<RenderCase>& iInfo) -> std::string {
							 return std::format("{}_{}_{}x{}", iInfo.param.scene, iInfo.param.backend,
												iInfo.param.size.x(), iInfo.param.size.y());
						 });

// Large outlined HUD text (48 and 64 px, 3 and 4 px outlines): smooth edges and a smooth outline band at any size.
INSTANTIATE_TEST_SUITE_P(HudTextLarge, RenderImage,
						 testing::Values(RenderCase{"hud_text_large", "vulkan", {640, 360}},
										 RenderCase{"hud_text_large", "opengl", {640, 360}}),
						 [](const testing::TestParamInfo<RenderCase>& iInfo) -> std::string {
							 return std::format("{}_{}_{}x{}", iInfo.param.scene, iInfo.param.backend,
												iInfo.param.size.x(), iInfo.param.size.y());
						 });

// Static meshes of the mesh layer: a glTF cube with its embedded base-colour texture, the same cube tinted, a ground slab
// and a blended cube drawn after them.
INSTANTIATE_TEST_SUITE_P(Mesh, RenderImage, testing::Values(RenderCase{"mesh", "vulkan"}, RenderCase{"mesh", "opengl"}),
						 [](const testing::TestParamInfo<RenderCase>& iInfo) -> std::string {
							 return iInfo.param.scene + "_" + iInfo.param.backend;
						 });

// What a first-person walker aims at is outlined: in full where it shows, dimmed behind the crate in front of it.
INSTANTIATE_TEST_SUITE_P(Outline, RenderImage,
						 testing::Values(RenderCase{"outline", "vulkan"}, RenderCase{"outline", "opengl"}),
						 [](const testing::TestParamInfo<RenderCase>& iInfo) -> std::string {
							 return iInfo.param.scene + "_" + iInfo.param.backend;
						 });

// Billboard sprites of the raycaster drawn on the GPU in one instanced call, sorted far to near on the GPU, a sprite
// cut by a wall edge and two overlapping lamps.
INSTANTIATE_TEST_SUITE_P(RaycastSprites, RenderImage,
						 testing::Values(RenderCase{"raycast_sprites", "vulkan"},
										 RenderCase{"raycast_sprites", "opengl"}),
						 [](const testing::TestParamInfo<RenderCase>& iInfo) -> std::string {
							 return iInfo.param.scene + "_" + iInfo.param.backend;
						 });

// Occlusion culling of the mesh layer: a wall hides a row of cubes, one cube is half behind it; the capture must match
// the same scene drawn without occlusion (the reference).
INSTANTIATE_TEST_SUITE_P(Occlusion, RenderImage,
						 testing::Values(RenderCase{"occlusion", "vulkan"}, RenderCase{"occlusion", "opengl"}),
						 [](const testing::TestParamInfo<RenderCase>& iInfo) -> std::string {
							 return iInfo.param.scene + "_" + iInfo.param.backend;
						 });

namespace {

// One picking check: a scene, a backend and the entity expected under each pixel of the capture.
struct PickCase {
	// Scene file stem in `test/unit/test_helper/assets/render_tests/scenes`.
	std::string scene;
	// Backend name passed to `OwlRunner --backend`.
	std::string backend;
	// Pixels from the top left of the capture, with the tag of the entity drawn there (`none` for the background).
	std::vector<std::pair<math::vec2ui, std::string>> picks;
};

[[maybe_unused]] void PrintTo(const PickCase& iCase, std::ostream* oStream) {
	*oStream << iCase.scene << " on " << iCase.backend;
}

class RenderPick : public testing::TestWithParam<PickCase> {};

}// namespace

// The lights of the mesh layer: a directional sun casting two shadow cascades (the blended cube casts none), a red point
// light and a blue spot light.
INSTANTIATE_TEST_SUITE_P(Lights, RenderImage,
						 testing::Values(RenderCase{"lights", "vulkan"}, RenderCase{"lights", "opengl"}),
						 [](const testing::TestParamInfo<RenderCase>& iInfo) -> std::string {
							 return iInfo.param.scene + "_" + iInfo.param.backend;
						 });

// Area lights of the mesh layer on a glossy floor: a warm rectangle and a cool disc, both casting soft cube shadows,
// and a small green sphere light without shadow.
INSTANTIATE_TEST_SUITE_P(AreaLights, RenderImage,
						 testing::Values(RenderCase{"area_lights", "vulkan"}, RenderCase{"area_lights", "opengl"}),
						 [](const testing::TestParamInfo<RenderCase>& iInfo) -> std::string {
							 return iInfo.param.scene + "_" + iInfo.param.backend;
						 });

// A box reflection probe over a glossy floor: a chrome sphere mirrors the floor, the crate and the spheres around it
// with parallax, the gold and plastic spheres and the floor take the probe in their specular.
INSTANTIATE_TEST_SUITE_P(ReflectionProbes, RenderImage,
						 testing::Values(RenderCase{"reflection_probes", "vulkan"},
										 RenderCase{"reflection_probes", "opengl"}),
						 [](const testing::TestParamInfo<RenderCase>& iInfo) -> std::string {
							 return iInfo.param.scene + "_" + iInfo.param.backend;
						 });

// The probe scene through a lens: depth of field focused on the chrome sphere, chromatic aberration towards the edges
// and a film grain.
INSTANTIATE_TEST_SUITE_P(LensEffects, RenderImage,
						 testing::Values(RenderCase{"lens_effects", "vulkan"}, RenderCase{"lens_effects", "opengl"}),
						 [](const testing::TestParamInfo<RenderCase>& iInfo) -> std::string {
							 return iInfo.param.scene + "_" + iInfo.param.backend;
						 });

// The lights scene moved 2^20 m away on x and z: camera-relative rendering keeps the meshes, lights and shadows as
// precise as at the origin (the uploaded matrices never hold the far-away translation).
INSTANTIATE_TEST_SUITE_P(FarOrigin, RenderImage,
						 testing::Values(RenderCase{"far_origin", "vulkan"}, RenderCase{"far_origin", "opengl"}),
						 [](const testing::TestParamInfo<RenderCase>& iInfo) -> std::string {
							 return iInfo.param.scene + "_" + iInfo.param.backend;
						 });

// PBR materials under an HDR environment: gold, plastic, rough steel, clearcoat paint, an emissive and a toon sphere,
// the environment drawn as the sky, the PBR Neutral tone mapping.
INSTANTIATE_TEST_SUITE_P(Pbr, RenderImage, testing::Values(RenderCase{"pbr", "vulkan"}, RenderCase{"pbr", "opengl"}),
						 [](const testing::TestParamInfo<RenderCase>& iInfo) -> std::string {
							 return iInfo.param.scene + "_" + iInfo.param.backend;
						 });

// The entity-id attachment the editor picks in: the 3D layers write the id of every mesh and voxel world they draw.
TEST_P(RenderPick, entityUnderEachPixel) {
#ifndef OWL_RUNNER_EXECUTABLE
	GTEST_SKIP() << "OwlRunner is not built (OWL_BUILD_NEST=OFF).";
#else
	const auto& [scene, backend, picks] = GetParam();
	if (backend == "vulkan" && !exists(getVulkanIcd()))
		GTEST_SKIP() << "lavapipe not found (" << getVulkanIcd() << "); set OWL_RENDER_TESTS_VK_ICD.";
	if (getEnv("DISPLAY").empty() && getEnv("WAYLAND_DISPLAY").empty())
		GTEST_SKIP() << "No display: run the render tests under xvfb-run (ctest does it when xvfb-run exists).";
	const auto fixtures = test::getTestAssetsPath("render_tests");
	const auto outDir = getOutputDir();
	std::filesystem::create_directories(outDir);
	const auto stem = std::format("{}.{}.pick", scene, backend);
	const auto captureFile = outDir / (stem + ".png");
	const auto logFile = outDir / (stem + ".log");
	std::string pickArgs;
	for (const auto& [pixel, tag]: picks) pickArgs += std::format(" --pick {},{}", pixel.x(), pixel.y());
	const auto command = std::format(
			R"(env -u OWL_OPENGL_SHADERS {} "{}" --frame-bench "{}" --project "{}" )"
			R"(--assets "{}" --backend {} --frames 2 --warmup {} --size {}x{} --validation )"
			R"(--capture "{}"{} > "{}" 2>&1)",
			getDriverEnvironment(backend), OWL_RUNNER_EXECUTABLE, (fixtures / "scenes" / (scene + ".owl")).string(),
			(test::getRootPath() / "engine_demo").string(), fixtures.string(), backend, g_warmupFrames,
			g_captureSize.x(), g_captureSize.y(), captureFile.string(), pickArgs, logFile.string());
	// NOLINTNEXTLINE(bugprone-command-processor) The runner must run in its own process (one GPU context).
	const int status = std::system(command.c_str());
	const auto log = readFile(logFile);
	ASSERT_EQ(status, 0) << "OwlRunner failed, log: " << logFile;
	EXPECT_EQ(log.find("VUID-"), std::string::npos) << "Vulkan validation messages, log: " << logFile;
	for (const auto& [pixel, tag]: picks)
		EXPECT_NE(log.find(std::format("FrameBench: Pick {},{} -> {}.", pixel.x(), pixel.y(), tag)), std::string::npos)
				<< "Pixel " << pixel.x() << "," << pixel.y() << " should pick " << tag << ", log: " << logFile;
	std::filesystem::remove(captureFile);
#endif
}

INSTANTIATE_TEST_SUITE_P(
		Picking, RenderPick,
		testing::Values(PickCase{"mesh",
								 "vulkan",
								 {{{95, 80}, "TexturedCrate"},
								  {{160, 95}, "GlassCrate"},
								  {{215, 85}, "TintedCrate"},
								  {{40, 165}, "Ground"},
								  {{160, 10}, "none"}}},
						PickCase{"mesh",
								 "opengl",
								 {{{95, 80}, "TexturedCrate"},
								  {{160, 95}, "GlassCrate"},
								  {{215, 85}, "TintedCrate"},
								  {{40, 165}, "Ground"},
								  {{160, 10}, "none"}}},
						PickCase{"voxel", "vulkan", {{{160, 150}, "ProceduralTerrain"}, {{160, 5}, "none"}}},
						PickCase{"voxel", "opengl", {{{160, 150}, "ProceduralTerrain"}, {{160, 5}, "none"}}}),
		[](const testing::TestParamInfo<PickCase>& iInfo) -> std::string {
			return iInfo.param.scene + "_" + iInfo.param.backend;
		});

// A Vulkan run without any usable device falls back to OpenGL and renders the OpenGL reference.
TEST(RenderFallback, vulkanWithoutDeviceRendersWithOpenGl) {
#ifndef OWL_RUNNER_EXECUTABLE
	GTEST_SKIP() << "OwlRunner is not built (OWL_BUILD_NEST=OFF).";
#else
	if (getEnv("DISPLAY").empty() && getEnv("WAYLAND_DISPLAY").empty())
		GTEST_SKIP() << "No display: run the render tests under xvfb-run (ctest does it when xvfb-run exists).";
	const auto fixtures = test::getTestAssetsPath("render_tests");
	const auto outDir = getOutputDir();
	std::filesystem::create_directories(outDir);
	const auto actualFile = outDir / "fallback.actual.png";
	const auto logFile = outDir / "fallback.log";
	std::filesystem::remove(actualFile);
	// An ICD file that does not exist leaves the Vulkan loader without any device.
	const auto command = std::format(R"(env -u OWL_OPENGL_SHADERS {} VK_ICD_FILENAMES="{}" "{}" --frame-bench "{}" )"
									 R"(--project "{}" --assets "{}" --backend vulkan --frames 2 --warmup {} )"
									 R"(--size {}x{} --capture "{}" > "{}" 2>&1)",
									 getDriverEnvironment("opengl"), (outDir / "no_icd.json").string(),
									 OWL_RUNNER_EXECUTABLE, (fixtures / "scenes" / "sprites.owl").string(),
									 (test::getRootPath() / "engine_demo").string(), fixtures.string(), g_warmupFrames,
									 g_captureSize.x(), g_captureSize.y(), actualFile.string(), logFile.string());
	// NOLINTNEXTLINE(bugprone-command-processor) The runner must run in its own process (one GPU context).
	const int status = std::system(command.c_str());
	const auto log = readFile(logFile);
	ASSERT_EQ(status, 0) << "OwlRunner failed, log: " << logFile;
	EXPECT_NE(log.find("The Vulkan renderer could not start, falling back to OpenGL"), std::string::npos)
			<< "No fallback logged, log: " << logFile;
	const auto actual = renderer::decodeImageFile(actualFile, 4);
	ASSERT_TRUE(actual.valid) << "No capture written, log: " << logFile;
	const auto expected = renderer::decodeImageFile(fixtures / "references" / "opengl" / "sprites.png", 4);
	ASSERT_TRUE(expected.valid);
	ASSERT_TRUE(expected.size.x() == actual.size.x() && expected.size.y() == actual.size.y());
	EXPECT_TRUE(compareImages(actual.size, actual.pixels, expected.pixels, ImageTolerance{}).match)
			<< "The fallback capture differs from the OpenGL reference: " << actualFile;
	std::filesystem::remove(actualFile);
#endif
}
