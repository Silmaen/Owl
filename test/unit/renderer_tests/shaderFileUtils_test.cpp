/**
 * @file shaderFileUtils_test.cpp
 * @author Silmaen
 * @date 07/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

// Slang and SPIRV-Cross come with the render module.
#if OWL_WITH_RENDER
#include "testHelper.h"

#include <app/Application.h>
#include <core/Log.h>
#include <renderer/utils/shaderFileUtils.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <utility>
#include <vector>

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

/// Smallest module `checkSpirv` accepts: header, `OpMemoryModel`, a fragment `OpEntryPoint`, `iExtra` capabilities,
/// and an `OpFunctionEnd` standing for the function.
auto minimalSpirv(const uint32_t iExtra = 0, const uint32_t iModel = 4) -> std::vector<uint32_t> {
	std::vector<uint32_t> words{0x07230203, 0x00010600, 0,      2, 0, 0x0003000e, 0,
								1,          0x0004000f, iModel, 1, 0, 0x00010038};
	for (uint32_t i = 0; i < iExtra; ++i) words.insert(words.begin() + 5, {0x00020011, 1});
	return words;
}

/// Slang source with one compute entry point.
const std::string g_computeSource = "[shader(\"compute\")]\n[numthreads(1, 1, 1)]\n"
									"void computeMain(uint3 iId: SV_DispatchThreadID) {}\n";

/// Write raw bytes to a file.
void writeBytes(const std::filesystem::path& iFile, const std::string& iBytes) {
	std::ofstream out(iFile, std::ios::binary | std::ios::trunc);
	out.write(iBytes.data(), static_cast<std::streamsize>(iBytes.size()));
}

/**
 * @brief
 *  Damage the cached compute stage of `owl_empty` with `iDamage`, load it again, and check that it was recompiled
 *  into a valid module that replaced the damaged one in the cache.
 */
void expectRecompiledAfter(const std::function<void(const std::filesystem::path&, const std::string&)>& iDamage) {
	const auto cached =
			renderer::utils::getShaderCachedPath("owl_empty", "owl_test_recover", "vulkan", ShaderType::Compute);
	std::filesystem::remove_all(renderer::utils::getCacheDirectory("owl_test_recover", ""));
	const auto first = renderer::utils::loadOrCompileSpirv(g_computeSource, "owl_empty", "owl_test_recover", true,
														   {ShaderType::Compute});
	ASSERT_TRUE(first.has_value());
	const auto key = renderer::utils::getShaderCacheKey(g_computeSource, "owl_test_recover/owl_empty", true);
	iDamage(cached, key);
	const auto again = renderer::utils::loadOrCompileSpirv(g_computeSource, "owl_empty", "owl_test_recover", true,
														   {ShaderType::Compute});
	ASSERT_TRUE(again.has_value());
	EXPECT_TRUE(again->origin.empty());
	EXPECT_EQ(again->stages.at(ShaderType::Compute), first->stages.at(ShaderType::Compute));
	EXPECT_FALSE(renderer::utils::checkSpirv(again->stages.at(ShaderType::Compute)).has_value());
	// Back in the cache: the next load reads it.
	const auto third = renderer::utils::loadOrCompileSpirv(g_computeSource, "owl_empty", "owl_test_recover", true,
														   {ShaderType::Compute});
	ASSERT_TRUE(third.has_value());
	EXPECT_FALSE(third->origin.empty());
	EXPECT_EQ(third->stages.at(ShaderType::Compute), first->stages.at(ShaderType::Compute));
	std::filesystem::remove_all(renderer::utils::getCacheDirectory("owl_test_recover", ""));
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
TEST(ShaderFileUtils, PrecompiledPathSitsUnderTheRendererSpirvFolder) {
	EXPECT_EQ(renderer::utils::getPrecompiledShaderPath("quad", "renderer2D", "vulkan", ShaderType::Fragment),
			  std::filesystem::path("shaders") / "renderer2D" / "spirv" / "vulkan" / "quad.frag.spv");
}

TEST(ShaderFileUtils, LoadOrCompileUsesThePrecompiledStagesWithoutSlang) {
	core::Log::init(core::Log::Level::Off);
	// Without an application, the asset search path is the working directory.
	const auto root = std::filesystem::current_path();
	const auto folder = root / "shaders" / "owl_test_precompiled";
	std::filesystem::remove_all(folder);
	// Not Slang: only a precompiled hit can succeed.
	const std::string source = "not a slang source";
	const auto key = renderer::utils::getShaderCacheKey(source, "owl_test_precompiled/fake", true);
	const std::vector<uint32_t> vert = minimalSpirv(0, 0);
	const std::vector<uint32_t> frag = minimalSpirv(1);
	for (const auto& [stage, data]: {std::pair{ShaderType::Vertex, vert}, std::pair{ShaderType::Fragment, frag}}) {
		const auto file =
				root / renderer::utils::getPrecompiledShaderPath("fake", "owl_test_precompiled", "vulkan", stage);
		std::filesystem::create_directories(file.parent_path());
		ASSERT_TRUE(renderer::utils::writeCachedShader(file, data));
		renderer::utils::writeShaderHash(file, key);
	}
	const auto stages = renderer::utils::loadOrCompileSpirv(source, "fake", "owl_test_precompiled", true,
															{ShaderType::Vertex, ShaderType::Fragment});
	ASSERT_TRUE(stages.has_value());
	EXPECT_EQ(stages->stages.at(ShaderType::Vertex), vert);
	EXPECT_EQ(stages->stages.at(ShaderType::Fragment), frag);
	EXPECT_EQ(stages->origin, folder / "spirv" / "vulkan");
	// The OpenGL output was not baked, and an edited source does not match the stored hash: both compile, and fail.
	EXPECT_FALSE(renderer::utils::loadOrCompileSpirv(source, "fake", "owl_test_precompiled", false,
													 {ShaderType::Vertex, ShaderType::Fragment})
						 .has_value());
	EXPECT_FALSE(renderer::utils::loadOrCompileSpirv(source + " edited", "fake", "owl_test_precompiled", true,
													 {ShaderType::Vertex, ShaderType::Fragment})
						 .has_value());
	std::filesystem::remove_all(folder);
	core::Log::invalidate();
}

TEST(ShaderFileUtils, LoadOrCompileFillsThenReadsTheCache) {
	core::Log::init(core::Log::Level::Off);
	auto app = makeDummyApp("shaderLoadOrCompile");
	const std::string& source = g_computeSource;
	const auto cached =
			renderer::utils::getShaderCachedPath("owl_empty", "owl_test_cache", "vulkan", ShaderType::Compute);
	std::filesystem::remove_all(renderer::utils::getCacheDirectory("owl_test_cache", ""));
	const auto compiled =
			renderer::utils::loadOrCompileSpirv(source, "owl_empty", "owl_test_cache", true, {ShaderType::Compute});
	ASSERT_TRUE(compiled.has_value());
	ASSERT_FALSE(compiled->stages.at(ShaderType::Compute).empty());
	EXPECT_TRUE(compiled->origin.empty());
	const auto key = renderer::utils::getShaderCacheKey(source, "owl_test_cache/owl_empty", true);
	EXPECT_TRUE(renderer::utils::isShaderCacheValid(cached, key));
	const auto again =
			renderer::utils::loadOrCompileSpirv(source, "owl_empty", "owl_test_cache", true, {ShaderType::Compute});
	ASSERT_TRUE(again.has_value());
	EXPECT_EQ(again->stages.at(ShaderType::Compute), compiled->stages.at(ShaderType::Compute));
	EXPECT_EQ(again->origin, cached.parent_path());
	std::filesystem::remove_all(renderer::utils::getCacheDirectory("owl_test_cache", ""));
	app::Application::invalidate();
	app.reset();
	core::Log::invalidate();
}
TEST(ShaderFileUtils, CheckSpirvRejectsMalformedModules) {
	EXPECT_FALSE(renderer::utils::checkSpirv(minimalSpirv()).has_value());
	EXPECT_FALSE(renderer::utils::checkSpirv(minimalSpirv(), ShaderType::Fragment).has_value());
	// A vertex module stored as the fragment stage.
	EXPECT_TRUE(renderer::utils::checkSpirv(minimalSpirv(0, 0), ShaderType::Fragment).has_value());
	EXPECT_TRUE(renderer::utils::checkSpirv({}).has_value());
	EXPECT_TRUE(renderer::utils::checkSpirv({0x07230203, 0x00010600, 0}).has_value());
	auto words = minimalSpirv();
	words[0] = 0x03022307;
	EXPECT_TRUE(renderer::utils::checkSpirv(words).has_value());
	words = minimalSpirv();
	words[3] = 0;
	EXPECT_TRUE(renderer::utils::checkSpirv(words).has_value());
	words = minimalSpirv();
	words.pop_back();
	EXPECT_TRUE(renderer::utils::checkSpirv(words).has_value());
	words = minimalSpirv();
	words[5] = 0;
	EXPECT_TRUE(renderer::utils::checkSpirv(words).has_value());
	// No entry point.
	words = minimalSpirv();
	words.resize(8);
	EXPECT_TRUE(renderer::utils::checkSpirv(words).has_value());
}

TEST(ShaderFileUtils, CorruptCachedSpirvIsRecompiled) {
	core::Log::init(core::Log::Level::Off);
	auto app = makeDummyApp("shaderCorruptCache");
	expectRecompiledAfter([](const std::filesystem::path& iFile, const std::string&) -> void {
		writeBytes(iFile, std::string(64, '\x5a'));
	});
	app::Application::invalidate();
	app.reset();
	core::Log::invalidate();
}

TEST(ShaderFileUtils, TruncatedCachedSpirvIsRecompiled) {
	core::Log::init(core::Log::Level::Off);
	auto app = makeDummyApp("shaderTruncatedCache");
	expectRecompiledAfter([](const std::filesystem::path& iFile, const std::string&) -> void {
		const auto size = std::filesystem::file_size(iFile);
		// A whole number of words: the instruction walk, not the size check, finds the cut.
		std::filesystem::resize_file(iFile, size / 2 & ~std::uintmax_t{3});
	});
	app::Application::invalidate();
	app.reset();
	core::Log::invalidate();
}

TEST(ShaderFileUtils, AbsentCachedSpirvIsRecompiled) {
	core::Log::init(core::Log::Level::Off);
	auto app = makeDummyApp("shaderAbsentCache");
	expectRecompiledAfter(
			[](const std::filesystem::path& iFile, const std::string&) -> void { std::filesystem::remove(iFile); });
	app::Application::invalidate();
	app.reset();
	core::Log::invalidate();
}

TEST(ShaderFileUtils, CachedSpirvOfAnotherKeyIsRecompiled) {
	core::Log::init(core::Log::Level::Off);
	auto app = makeDummyApp("shaderWrongKeyCache");
	expectRecompiledAfter([](const std::filesystem::path& iFile, const std::string& iKey) -> void {
		// A well-formed module stored under another key (another Slang version) must not be used.
		ASSERT_TRUE(renderer::utils::writeCachedShader(iFile, minimalSpirv()));
		renderer::utils::writeShaderHash(iFile, iKey + " slang=other");
	});
	app::Application::invalidate();
	app.reset();
	core::Log::invalidate();
}

TEST(ShaderFileUtils, CachedSpirvOfAnotherStageIsRecompiled) {
	core::Log::init(core::Log::Level::Off);
	auto app = makeDummyApp("shaderWrongStageCache");
	expectRecompiledAfter([](const std::filesystem::path& iFile, const std::string& iKey) -> void {
		// A well-formed fragment module under the right key, where the compute stage is expected.
		ASSERT_TRUE(renderer::utils::writeCachedShader(iFile, minimalSpirv()));
		renderer::utils::writeShaderHash(iFile, iKey);
	});
	app::Application::invalidate();
	app.reset();
	core::Log::invalidate();
}

TEST(ShaderFileUtils, CorruptPrecompiledSpirvIsRecompiled) {
	core::Log::init(core::Log::Level::Off);
	// Without an application, the asset search path is the working directory and nothing is cached.
	const auto root = std::filesystem::current_path();
	const auto folder = root / "shaders" / "owl_test_recover_pre";
	std::filesystem::remove_all(folder);
	const auto key = renderer::utils::getShaderCacheKey(g_computeSource, "owl_test_recover_pre/owl_empty", true);
	const auto file = root / renderer::utils::getPrecompiledShaderPath("owl_empty", "owl_test_recover_pre", "vulkan",
																	   ShaderType::Compute);
	std::filesystem::create_directories(file.parent_path());
	writeBytes(file, std::string(10, '\x01'));
	renderer::utils::writeShaderHash(file, key);
	const auto loaded = renderer::utils::loadOrCompileSpirv(g_computeSource, "owl_empty", "owl_test_recover_pre", true,
															{ShaderType::Compute});
	ASSERT_TRUE(loaded.has_value());
	EXPECT_TRUE(loaded->origin.empty());
	EXPECT_FALSE(renderer::utils::checkSpirv(loaded->stages.at(ShaderType::Compute)).has_value());
	std::filesystem::remove_all(folder);
	core::Log::invalidate();
}

TEST(ShaderFileUtils, RecompileSpirvReplacesRefusedStoredOutput) {
	core::Log::init(core::Log::Level::Off);
	auto app = makeDummyApp("shaderRecompile");
	std::filesystem::remove_all(renderer::utils::getCacheDirectory("owl_test_refused", ""));
	// Just compiled: nothing better to try.
	EXPECT_FALSE(renderer::utils::recompileSpirv(g_computeSource, "owl_empty", "owl_test_refused", true, {},
												 "refused by the test")
						 .has_value());
	const auto cached =
			renderer::utils::getShaderCachedPath("owl_empty", "owl_test_refused", "vulkan", ShaderType::Compute);
	const auto loaded = renderer::utils::recompileSpirv(g_computeSource, "owl_empty", "owl_test_refused", true,
														cached.parent_path(), "refused by the test");
	ASSERT_TRUE(loaded.has_value());
	EXPECT_TRUE(loaded->origin.empty());
	const auto key = renderer::utils::getShaderCacheKey(g_computeSource, "owl_test_refused/owl_empty", true);
	EXPECT_TRUE(renderer::utils::isShaderCacheValid(cached, key));
	std::filesystem::remove_all(renderer::utils::getCacheDirectory("owl_test_refused", ""));
	app::Application::invalidate();
	app.reset();
	core::Log::invalidate();
}
#endif
