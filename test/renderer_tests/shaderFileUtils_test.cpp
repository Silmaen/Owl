/**
 * @file shaderFileUtils_test.cpp
 * @author Silmaen
 * @date 07/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <app/Application.h>
#include <core/Log.h>
#include <renderer/utils/shaderFileUtils.h>

#include <filesystem>
#include <fstream>

using namespace owl;
using owl::renderer::gpu::ShaderType;

namespace {

auto makeDummyApp(const char* iName) -> shared<app::Application> {
	return mkShared<app::Application>(app::AppParams{.args = nullptr,
													 .frameLogFrequency = 0,
													 .name = iName,
													 .assetsPattern = "",
													 .icon = "",
													 .width = 0,
													 .height = 0,
													 .argCount = 0,
													 .renderer = renderer::gpu::RenderAPI::Type::Null,
													 .hasGui = false,
													 .useDebugging = false,
													 .isDummy = true});
}

}// namespace

TEST(ShaderFileUtils, ExtensionNamesMatchStage) {
	core::Log::init(core::Log::Level::Off);
	EXPECT_EQ(renderer::utils::getExtension(ShaderType::Vertex), ".vert");
	EXPECT_EQ(renderer::utils::getExtension(ShaderType::Fragment), ".frag");
	EXPECT_EQ(renderer::utils::getCacheExtension(ShaderType::Vertex), ".vert.spv");
	EXPECT_EQ(renderer::utils::getCacheExtension(ShaderType::Fragment), ".frag.spv");
	core::Log::invalidate();
}

TEST(ShaderFileUtils, RelativePathBuildsUnderShaders) {
	core::Log::init(core::Log::Level::Off);
	const auto rel = renderer::utils::getRelativeShaderPath("base", "vulkan", "1.4", ShaderType::Fragment);
	EXPECT_EQ(rel, std::filesystem::path("shaders") / "vulkan" / "1.4" / "base.frag");
	core::Log::invalidate();
}

TEST(ShaderFileUtils, CacheDirectoryIsCreatedAndReused) {
	core::Log::init(core::Log::Level::Off);
	auto app = makeDummyApp("shaderCacheDir");
	const auto dir = renderer::utils::getCacheDirectory("test_renderer", "v1");
	std::filesystem::remove_all(dir);
	EXPECT_FALSE(exists(dir));
	renderer::utils::createCacheDirectoryIfNeeded("test_renderer", "v1");
	EXPECT_TRUE(exists(dir));
	// Calling again is idempotent.
	renderer::utils::createCacheDirectoryIfNeeded("test_renderer", "v1");
	EXPECT_TRUE(exists(dir));
	std::filesystem::remove_all(dir);
	app::Application::invalidate();
	app.reset();
	core::Log::invalidate();
}

TEST(ShaderFileUtils, CacheDirectoryWithEmptyApiDoesNotAppendApiSegment) {
	core::Log::init(core::Log::Level::Off);
	auto app = makeDummyApp("shaderCacheEmpty");
	const auto dirA = renderer::utils::getCacheDirectory("vulkan", "");
	const auto dirB = renderer::utils::getCacheDirectory("", "");
	// `dirA` is `<cwd>/cache/shader/vulkan` (no trailing api segment, no trailing separator).
	EXPECT_TRUE(dirA.string().ends_with("vulkan"));
	EXPECT_EQ(dirA.filename(), "vulkan");
	EXPECT_TRUE(dirB.filename() == "shader");
	app::Application::invalidate();
	app.reset();
	core::Log::invalidate();
}

TEST(ShaderFileUtils, ShaderCachedPathUsesShaderName) {
	core::Log::init(core::Log::Level::Off);
	auto app = makeDummyApp("shaderCachedPath");
	const auto path = renderer::utils::getShaderCachedPath("foo", "vulkan", "1.4", ShaderType::Vertex);
	EXPECT_EQ(path.filename(), "foo.vert.spv");
	app::Application::invalidate();
	app.reset();
	core::Log::invalidate();
}

TEST(ShaderFileUtils, ComputeShaderHashIsStable) {
	core::Log::init(core::Log::Level::Off);
	const auto h1 = renderer::utils::computeShaderHash("hello world");
	const auto h2 = renderer::utils::computeShaderHash("hello world");
	const auto h3 = renderer::utils::computeShaderHash("hello world!");
	EXPECT_EQ(h1, h2);
	EXPECT_NE(h1, h3);
	core::Log::invalidate();
}

TEST(ShaderFileUtils, WriteAndReadCachedShaderRoundTrip) {
	core::Log::init(core::Log::Level::Off);
	const auto file = std::filesystem::temp_directory_path() / "owl_test_shader_cache.spv";
	std::filesystem::remove(file);
	const std::vector<uint32_t> data{0x07230203, 0xdeadbeef, 0x12345678, 0xcafebabe};
	EXPECT_TRUE(renderer::utils::writeCachedShader(file, data));
	const auto loaded = renderer::utils::readCachedShader(file);
	ASSERT_EQ(loaded.size(), data.size());
	for (size_t i = 0; i < data.size(); ++i) { EXPECT_EQ(loaded[i], data[i]); }
	std::filesystem::remove(file);
	core::Log::invalidate();
}

TEST(ShaderFileUtils, WriteCachedShaderFailsOnUnopenablePath) {
	core::Log::init(core::Log::Level::Off);
	// A path inside a non-existent directory cannot be opened for writing.
	const auto bogus = std::filesystem::temp_directory_path() / "owl_no_such_dir__/sub/foo.spv";
	std::filesystem::remove_all(std::filesystem::temp_directory_path() / "owl_no_such_dir__");
	const std::vector<uint32_t> data{1, 2, 3};
	EXPECT_FALSE(renderer::utils::writeCachedShader(bogus, data));
	core::Log::invalidate();
}

TEST(ShaderFileUtils, IsShaderCacheValidFalseWhenMissingOrStale) {
	core::Log::init(core::Log::Level::Off);
	const auto file = std::filesystem::temp_directory_path() / "owl_test_shader_cache_valid.spv";
	const auto hashFile = std::filesystem::path(file.string() + ".hash");
	std::filesystem::remove(file);
	std::filesystem::remove(hashFile);
	// Missing cache file → not valid.
	EXPECT_FALSE(renderer::utils::isShaderCacheValid(file, "src"));
	// Cache file but no hash file → not valid.
	std::ofstream(file) << "blob";
	EXPECT_FALSE(renderer::utils::isShaderCacheValid(file, "src"));
	// Hash file present and matching.
	renderer::utils::writeShaderHash(file, "src");
	EXPECT_TRUE(renderer::utils::isShaderCacheValid(file, "src"));
	// Hash file present but stale.
	EXPECT_FALSE(renderer::utils::isShaderCacheValid(file, "different"));
	std::filesystem::remove(file);
	std::filesystem::remove(hashFile);
	core::Log::invalidate();
}

TEST(ShaderFileUtils, ShaderReflectReturnsEmptyForEmptyData) {
	core::Log::init(core::Log::Level::Off);
	const auto refl = renderer::utils::shaderReflect("noop", "vulkan", "1.4", ShaderType::Vertex, {});
	EXPECT_TRUE(refl.uniformBuffers.empty());
	EXPECT_TRUE(refl.sampledImages.empty());
	core::Log::invalidate();
}
