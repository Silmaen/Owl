/**
 * @file HotReload_test.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <app/HotReload.h>
#include <platform/FileWatcher.h>
#include <renderer/Renderer.h>
#include <renderer/TextureDecoder.h>
#include <renderer/gpu/RenderCommand.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace owl;
using namespace owl::renderer;
using namespace owl::renderer::gpu;

namespace {

constexpr auto g_ValidShader = R"(
struct VertexOutput {
	float4 position : SV_Position;
};

[shader("vertex")]
VertexOutput vertexMain(uint vertexId : SV_VertexID) {
	VertexOutput output;
	output.position = float4(float(vertexId), 0.0, 0.0, 1.0);
	return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target {
	return float4(%RED%, 0.0, 0.0, 1.0);
}
)";

auto shaderSource(const std::string& iRed) -> std::string {
	std::string source = g_ValidShader;
	source.replace(source.find("%RED%"), 5, iRed);
	return source;
}

void writeText(const std::filesystem::path& iPath, const std::string& iContent) {
	std::ofstream file(iPath, std::ios::binary | std::ios::trunc);
	file << iContent;
}

void writeImage(const std::filesystem::path& iPath, const uint32_t iSide) {
	const std::vector<uint8_t> pixels(static_cast<size_t>(iSide) * iSide * 4, 0x80);
	ASSERT_TRUE(writeImagePng(iPath, {iSide, iSide}, pixels));
}

class HotReloadTest : public testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		RenderCommand::create(RenderAPI::Type::Null);
		m_dir = std::filesystem::temp_directory_path() / "owl_hotreload_test";
		std::filesystem::remove_all(m_dir);
		std::filesystem::create_directories(m_dir / "shaders" / "hot" / "slang");
	}

	void TearDown() override {
		Renderer::reset();
		RenderCommand::invalidate();
		std::filesystem::remove_all(m_dir);
		core::Log::invalidate();
	}

	std::filesystem::path m_dir;
};

}// namespace

TEST_F(HotReloadTest, TextureTakesTheNewPixelsAndKeepsThemOnError) {
	const auto file = m_dir / "tex.png";
	writeImage(file, 2);
	const auto texture = Texture2D::create(file);
	ASSERT_NE(texture, nullptr);

	writeImage(file, 4);
	EXPECT_TRUE(texture->reloadFromFile());
	EXPECT_EQ(texture->getSize(), (math::vec2ui{4, 4}));

	writeText(file, "not an image");
	EXPECT_FALSE(texture->reloadFromFile());
	EXPECT_EQ(texture->getSize(), (math::vec2ui{4, 4}));

	const auto unnamed = Texture2D::create(Texture::Specification{.size = {1, 1}});
	EXPECT_FALSE(unnamed->reloadFromFile());
}

TEST_F(HotReloadTest, ShaderRecompilesAndKeepsThePreviousVersionOnError) {
	OWL_REQUIRE_MODULE(RENDER);
	const auto file = m_dir / "shaders" / "hot" / "slang" / "flat.slang";
	writeText(file, shaderSource("1.0"));
	const auto shader = Shader::create(m_dir / "shaders" / "hot" / "slang" / "flat");
	ASSERT_NE(shader, nullptr);
	EXPECT_TRUE(platform::isSameFile(shader->getSourcePath(), file));
	EXPECT_EQ(shader->getGeneration(), 0u);

	writeText(file, shaderSource("0.5"));
	EXPECT_TRUE(shader->reload());
	EXPECT_EQ(shader->getGeneration(), 1u);

	writeText(file, shaderSource("undefinedSymbol"));
	EXPECT_FALSE(shader->reload());
	EXPECT_EQ(shader->getGeneration(), 1u);

	std::filesystem::remove(file);
	EXPECT_FALSE(shader->reload());
}

TEST_F(HotReloadTest, WatchedFileIsDispatchedToTheLibrariesAndListeners) {
	OWL_REQUIRE_MODULE(RENDER);
	const auto image = m_dir / "sprite.png";
	writeImage(image, 2);
	auto texture = Texture2D::create(image);
	ASSERT_NE(texture, nullptr);
	Renderer::getTextureLibrary().add("sprite", texture);
	const auto source = m_dir / "shaders" / "hot" / "slang" / "flat.slang";
	writeText(source, shaderSource("1.0"));
	auto shader = Shader::create(m_dir / "shaders" / "hot" / "slang" / "flat");
	ASSERT_NE(shader, nullptr);
	Renderer::getShaderLibrary().add("hot#flat", shader);

	app::HotReload hotReload;
	hotReload.watchDirectory(m_dir);
	std::vector<std::filesystem::path> heard;
	const auto listener =
			hotReload.addListener([&heard](const std::filesystem::path& iFile) -> void { heard.push_back(iFile); });

	writeImage(image, 8);
	writeText(source, shaderSource("broken("));
	hotReload.onFrame();
	EXPECT_TRUE(heard.empty());
	hotReload.getWatcher().scan();
	hotReload.getWatcher().scan();
	hotReload.onFrame();

	EXPECT_EQ(heard.size(), 2u);
	EXPECT_EQ(texture->getSize(), (math::vec2ui{8, 8}));
	EXPECT_EQ(shader->getGeneration(), 0u);

	const auto report = hotReload.reloadFile(source);
	EXPECT_EQ(report.reloaded, 0u);
	EXPECT_EQ(report.failed, 1u);
	writeText(source, shaderSource("0.25"));
	const auto fixed = hotReload.reloadFile(source);
	EXPECT_EQ(fixed.reloaded, 1u);
	EXPECT_EQ(hotReload.getLastReport().reloaded, 1u);
	EXPECT_EQ(shader->getGeneration(), 1u);

	hotReload.removeListener(listener);
	heard.clear();
	static_cast<void>(hotReload.reloadFile(m_dir / "other.lua"));
	EXPECT_TRUE(heard.empty());

	hotReload.setEnabled(true);
	EXPECT_TRUE(hotReload.isEnabled());
	hotReload.setEnabled(false);
	EXPECT_FALSE(hotReload.isEnabled());
}
