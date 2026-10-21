/**
 * @file PostProcess_test.cpp
 * @author Silmaen
 * @date 21/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <renderer/ColorEncoder.h>
#include <renderer/PostProcess.h>
#include <renderer/RenderGraph.h>
#include <renderer/Renderer.h>
#include <renderer/Renderer3D.h>
#include <renderer/gpu/RenderCommand.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace owl;
using namespace owl::renderer;
using Graph = RenderGraph;

namespace {
class PostProcessTest : public testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		gpu::RenderCommand::create(gpu::RenderAPI::Type::Null);
		Renderer::init();
	}

	void TearDown() override {
		Renderer::shutdown();
		gpu::RenderCommand::invalidate();
		core::Log::invalidate();
	}
};

}// namespace

TEST_F(PostProcessTest, theBloomLevelsHalveWhileTheFrameAllowsIt) {
	EXPECT_EQ(PostProcess::getBloomLevelCount({1920, 1080}), PostProcess::g_maxBloomLevels);
	EXPECT_EQ(PostProcess::getBloomLevelCount({64, 64}), 3u);
	EXPECT_EQ(PostProcess::getBloomLevelCount({8, 8}), 0u);
}

TEST_F(PostProcessTest, theBloomChainDownsamplesThenUpsamplesIntoTheEncode) {
	Graph graph;
	const auto target = graph.importTarget("target");
	const auto scene = graph.createTarget("scene", Graph::TargetFormat::Linear);
	graph.addPass("world", [scene](Graph::PassBuilder& ioBuilder) -> void { ioBuilder.write(scene); }, {});
	PostProcessSettings settings;
	settings.bloom = true;
	const auto bloom = PostProcess::recordBloom(graph, scene, settings, {256, 128});
	ASSERT_TRUE(bloom.isValid());
	graph.addPass("encode",
				  [scene, bloom, target](Graph::PassBuilder& ioBuilder) -> void {
					  ioBuilder.read(scene);
					  ioBuilder.read(bloom);
					  ioBuilder.write(target);
				  },
				  {});
	ASSERT_TRUE(graph.compile());
	const auto passes = graph.getCompiledPasses();
	const auto levels = PostProcess::getBloomLevelCount({256, 128});
	EXPECT_EQ(std::ranges::count(passes, std::string{"bloom down"}), levels);
	EXPECT_EQ(std::ranges::count(passes, std::string{"bloom up"}), levels - 1);
	EXPECT_EQ(passes.back(), "encode");
	EXPECT_NO_THROW(graph.execute({256, 128}));
}

TEST_F(PostProcessTest, noBloomAddsNoPass) {
	Graph graph;
	const auto scene = graph.createTarget("scene", Graph::TargetFormat::Linear);
	EXPECT_FALSE(PostProcess::recordBloom(graph, scene, {}, {256, 128}).isValid());
}

TEST_F(PostProcessTest, aLutIsLoadedOnceAndAMissingOneHasNoTexture) {
	const auto file = std::filesystem::temp_directory_path() / "owl_postprocess_identity.cube";
	{
		std::ofstream out(file);
		out << "LUT_3D_SIZE 2\n";
		for (int b = 0; b < 2; ++b)
			for (int g = 0; g < 2; ++g)
				for (int r = 0; r < 2; ++r) out << r << ' ' << g << ' ' << b << '\n';
	}
	const auto& lut = PostProcess::getLut(file);
	EXPECT_EQ(lut.size, 2u);
	EXPECT_NE(lut.texture, nullptr);
	EXPECT_EQ(&PostProcess::getLut(file), &lut);
	EXPECT_EQ(PostProcess::getLut(std::filesystem::temp_directory_path() / "missing.cube").texture, nullptr);
	EXPECT_EQ(PostProcess::getLut({}).texture, nullptr);
	std::filesystem::remove(file);
}

TEST_F(PostProcessTest, theEncoderTakesTheSettings) {
	PostProcessSettings settings;
	settings.exposure = 1.f;
	settings.vignette = true;
	const auto texture = gpu::Texture2D::create(gpu::Texture2D::Specification{.size = {1, 1}});
	EncodePost post;
	post.settings = &settings;
	post.frameSize = {4, 4};
	EXPECT_NO_THROW(ColorEncoder::draw(texture, ToneMapping::Aces, post));
}

TEST_F(PostProcessTest, theEnvironmentFallsBackToTheAmbient) {
	EnvironmentLighting environment;
	environment.levelCount = 2;
	environment.irradiance = SphericalHarmonics::constant({0.5f, 0.5f, 0.5f});
	EXPECT_NO_THROW(Renderer3D::setEnvironment(environment));
	EXPECT_NO_THROW(Renderer3D::drawSky());
	EXPECT_NO_THROW(Renderer3D::setEnvironment({}));
	EXPECT_NO_THROW(Renderer3D::drawSky());
}

TEST_F(PostProcessTest, theLayerSunAndTheEditorAmbientAreSwitches) {
	EXPECT_TRUE(Renderer3D::isLayerSunEnabled());
	Renderer3D::setLayerSun(false);
	EXPECT_FALSE(Renderer3D::isLayerSunEnabled());
	Renderer3D::setLayerSun(true);
	EXPECT_TRUE(Renderer3D::isLayerSunEnabled());
	EXPECT_NO_THROW(Renderer3D::setEditorAmbient({0.2f, 0.2f, 0.2f}));
	EnvironmentLighting constantSky;
	constantSky.ambient = math::vec3{0.f, 0.f, 0.f};
	EXPECT_NO_THROW(Renderer3D::setEnvironment(constantSky));
	EXPECT_NO_THROW(Renderer3D::setLighting({0.f, -1.f, 0.f}, 100000.f, {3000.f, 3000.f, 3000.f}));
	EXPECT_NO_THROW(Renderer3D::setEditorAmbient({}));
}
