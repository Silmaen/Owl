/**
 * @file TextureCube_test.cpp
 * @author Silmaen
 * @date 28/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <renderer/gpu/Framebuffer.h>
#include <renderer/gpu/RenderCommand.h>
#include <renderer/gpu/Texture.h>
#include <renderer/gpu/null/RenderAPI.h>

#include <array>
#include <cstdint>
#include <vector>

using namespace owl;
using namespace owl::renderer::gpu;

namespace {

auto cubeSpec(const uint32_t iFace, const ImageFormat iFormat, const uint32_t iLevels = 0) -> Texture::Specification {
	return {.size = {iFace, iFace}, .format = iFormat, .levelCount = iLevels};
}

auto faceTarget(const uint32_t iFace, const AttachmentSpecification::Format iFormat) -> shared<Framebuffer> {
	return Framebuffer::create(
			{.size = {iFace, iFace}, .attachments = {{.format = iFormat}}, .samples = 1, .debugName = "face"});
}

}// namespace

TEST(TextureSpecifications, FloatPixelSizes) {
	Texture::Specification spec{.size = {4, 4}, .format = ImageFormat::R32F};
	EXPECT_EQ(spec.getPixelSize(), 4);
	spec.format = ImageFormat::Rgba16F;
	EXPECT_EQ(spec.getPixelSize(), 8);
}

TEST(TextureCube, ValidSpecificationIsSquareAndUncompressed) {
	EXPECT_TRUE(TextureCube::isValidSpecification(cubeSpec(64, ImageFormat::R32F)));
	EXPECT_TRUE(TextureCube::isValidSpecification(cubeSpec(64, ImageFormat::Rgba16F)));
	EXPECT_TRUE(TextureCube::isValidSpecification(cubeSpec(64, ImageFormat::Rgba32F)));
	EXPECT_TRUE(TextureCube::isValidSpecification(cubeSpec(64, ImageFormat::Rgba8)));
	EXPECT_FALSE(TextureCube::isValidSpecification(cubeSpec(0, ImageFormat::R32F)));
	EXPECT_FALSE(TextureCube::isValidSpecification({.size = {64, 32}, .format = ImageFormat::R32F}));
	EXPECT_FALSE(TextureCube::isValidSpecification(cubeSpec(64, ImageFormat::Bc7)));
	EXPECT_FALSE(TextureCube::isValidSpecification(cubeSpec(64, ImageFormat::Rgb8)));
}

TEST(TextureCube, CreateRejectsAnInvalidSpecification) {
	core::Log::init(core::Log::Level::Off);
	RenderCommand::create(RenderAPI::Type::Null);
	EXPECT_EQ(TextureCube::create({.size = {16, 8}, .format = ImageFormat::R32F}), nullptr);
	EXPECT_NE(TextureCube::create(cubeSpec(16, ImageFormat::R32F)), nullptr);
	RenderCommand::invalidate();
	core::Log::invalidate();
}

TEST(TextureCube, FaceSizesFollowTheLevels) {
	core::Log::init(core::Log::Level::Off);
	RenderCommand::create(RenderAPI::Type::Null);
	const auto single = TextureCube::create(cubeSpec(128, ImageFormat::Rgba16F));
	ASSERT_NE(single, nullptr);
	EXPECT_EQ(single->getLevelCount(), 1u);
	EXPECT_EQ(single->getFaceSize(0), 128u);

	const auto mipped = TextureCube::create(cubeSpec(128, ImageFormat::Rgba16F, 5));
	ASSERT_NE(mipped, nullptr);
	EXPECT_EQ(mipped->getLevelCount(), 5u);
	EXPECT_EQ(mipped->getFaceSize(4), 8u);
	EXPECT_EQ(mipped->getFaceSize(12), 1u);
	RenderCommand::invalidate();
	core::Log::invalidate();
}

TEST(TextureCube, SetLevelsNeedsTheCubeSizeAndFormat) {
	core::Log::init(core::Log::Level::Off);
	RenderCommand::create(RenderAPI::Type::Null);
	const auto cube = TextureCube::create(cubeSpec(4, ImageFormat::Rgba8));
	ASSERT_NE(cube, nullptr);
	TextureLevels levels{.size = {4, 4}, .format = ImageFormat::Rgba8, .levels = {}};
	EXPECT_FALSE(cube->setLevels(levels));
	levels.levels.emplace_back(std::vector<uint8_t>(4 * 4 * 4 * TextureCube::g_faceCount, 0));
	levels.levels.emplace_back(std::vector<uint8_t>(2 * 2 * 4 * TextureCube::g_faceCount, 0));
	EXPECT_TRUE(cube->setLevels(levels));
	EXPECT_EQ(cube->getLevelCount(), 2u);
	levels.format = ImageFormat::Rgba16F;
	EXPECT_FALSE(cube->setLevels(levels));
	RenderCommand::invalidate();
	core::Log::invalidate();
}

TEST(TextureCube, CopyToCubeChecksSizeFormatFaceAndLevel) {
	core::Log::init(core::Log::Level::Off);
	RenderCommand::create(RenderAPI::Type::Null);
	const auto cube = TextureCube::create(cubeSpec(32, ImageFormat::R32F, 2));
	ASSERT_NE(cube, nullptr);
	const auto depth = faceTarget(32, AttachmentSpecification::Format::RedFloat);
	EXPECT_TRUE(depth->canCopyToCube(0, *cube, 0, 0));
	EXPECT_TRUE(depth->copyAttachmentToCube(0, *cube, 5, 0));
	EXPECT_FALSE(depth->copyAttachmentToCube(0, *cube, 6, 0));
	EXPECT_FALSE(depth->copyAttachmentToCube(1, *cube, 0, 0));
	EXPECT_FALSE(depth->copyAttachmentToCube(0, *cube, 0, 1));
	EXPECT_FALSE(depth->copyAttachmentToCube(0, *cube, 0, 2));

	const auto half = faceTarget(16, AttachmentSpecification::Format::RedFloat);
	EXPECT_TRUE(half->copyAttachmentToCube(0, *cube, 2, 1));

	const auto colour = faceTarget(32, AttachmentSpecification::Format::Rgba16F);
	EXPECT_FALSE(colour->copyAttachmentToCube(0, *cube, 0, 0));
	const auto radiance = TextureCube::create(cubeSpec(32, ImageFormat::Rgba16F));
	ASSERT_NE(radiance, nullptr);
	EXPECT_TRUE(colour->copyAttachmentToCube(0, *radiance, 3, 0));
	RenderCommand::invalidate();
	core::Log::invalidate();
}

TEST(TextureCube, NullBackendCountsTheBoundCubes) {
	core::Log::init(core::Log::Level::Off);
	null::RenderAPI api;
	api.init();
	const std::array<shared<Texture2D>, 2> textures{};
	const std::array<shared<TextureCube>, 3> cubes{};
	api.bindTextures(textures, cubes);
	EXPECT_EQ(api.getBoundTextureCount(), 2u);
	EXPECT_EQ(api.getBoundCubeCount(), 3u);
	api.bindTextures(textures);
	EXPECT_EQ(api.getBoundCubeCount(), 0u);
	core::Log::invalidate();
}
