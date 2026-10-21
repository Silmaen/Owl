/**
 * @file FrameBuffer_tests.cpp
 * @author Silmaen
 * @date 03/07/2024
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <renderer/gpu/RenderCommand.h>
#include <renderer/gpu/null/Framebuffer.h>

#include <algorithm>
#include <cstdint>

using namespace owl::renderer;
using namespace owl::renderer::gpu;

TEST(Renderer, FramebufferBase) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	RenderCommand::create(RenderAPI::Type::Null);

	const auto fbuf = Framebuffer::create({.size = {12, 12},
										   .attachments = {{.format = AttachmentSpecification::Format::Surface,
															.tiling = AttachmentSpecification::Tiling::Optimal},
														   {.format = AttachmentSpecification::Format::RedInteger,
															.tiling = AttachmentSpecification::Tiling::Optimal}},
										   .samples = 1,
										   .swapChainTarget = false,
										   .debugName = "boby"});
	EXPECT_FALSE(fbuf->isUpsideDown());
	fbuf->bind();
	fbuf->unbind();
	EXPECT_EQ(fbuf->getSpecification().size.surface(), 144);
	EXPECT_EQ(fbuf->getColorAttachmentRendererId(0), 0);
	fbuf->resize({13, 13});
	EXPECT_EQ(fbuf->getSpecification().size.surface(), 169);
	fbuf->resize(10, 10);
	EXPECT_EQ(fbuf->getSpecification().size.surface(), 100);
	fbuf->clearAttachment(0, 0);
	fbuf->clearAttachment(0, owl::math::vec4{0.f, 0.f, 0.f, 0.f});
	EXPECT_EQ(fbuf->readPixel(0, 0, 0), 0);
	EXPECT_EQ(fbuf->getLowerData().x(), 0);
	EXPECT_EQ(fbuf->getUpperData().x(), 1);

	RenderCommand::invalidate();
	owl::core::Log::invalidate();
}

TEST(Renderer, NullFramebufferReadsBackABlankImage) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	RenderCommand::create(RenderAPI::Type::Null);
	const auto fbuf = Framebuffer::create({.size = {4, 3},
										   .attachments = {{.format = AttachmentSpecification::Format::Surface,
															.tiling = AttachmentSpecification::Tiling::Optimal},
														   {.format = AttachmentSpecification::Format::RedInteger,
															.tiling = AttachmentSpecification::Tiling::Optimal}},
										   .samples = 1,
										   .swapChainTarget = false,
										   .debugName = "blank"});
	const auto pixels = fbuf->readColorAttachment(0);
	EXPECT_EQ(pixels.size(), 4u * 3u * 4u);
	EXPECT_TRUE(std::ranges::all_of(pixels, [](const uint8_t iByte) -> bool { return iByte == 0; }));
	EXPECT_TRUE(fbuf->readColorAttachment(1).empty());
	EXPECT_TRUE(fbuf->readColorAttachment(2).empty());
	RenderCommand::invalidate();
	owl::core::Log::invalidate();
}

TEST(Renderer, FramebufferTracksTheBoundTargetAndItsColourSpace) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	RenderCommand::create(RenderAPI::Type::Null);
	const auto linear = Framebuffer::create({.size = {4, 4},
											 .attachments = {{.format = AttachmentSpecification::Format::Rgba16F,
															  .tiling = AttachmentSpecification::Tiling::Optimal}},
											 .samples = 1,
											 .swapChainTarget = false,
											 .debugName = "linear"});
	const auto display = Framebuffer::create({.size = {4, 4},
											  .attachments = {{.format = AttachmentSpecification::Format::Surface,
															   .tiling = AttachmentSpecification::Tiling::Optimal}},
											  .samples = 1,
											  .swapChainTarget = false,
											  .debugName = "display"});
	EXPECT_TRUE(linear->isLinearColor());
	EXPECT_FALSE(display->isLinearColor());
	EXPECT_EQ(Framebuffer::getBound(), nullptr);
	linear->bind();
	EXPECT_EQ(Framebuffer::getBound(), linear.get());
	EXPECT_TRUE(Framebuffer::isLinearTargetBound());
	display->bind();
	EXPECT_FALSE(Framebuffer::isLinearTargetBound());
	display->unbind();
	EXPECT_EQ(Framebuffer::getBound(), nullptr);
	EXPECT_EQ(linear->getAttachmentTexture(0), nullptr);
	linear->copyAttachment(0, *display);

	RenderCommand::invalidate();
	owl::core::Log::invalidate();
}

TEST(Renderer, RenderCommandRemembersTheClearColour) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	RenderCommand::create(RenderAPI::Type::Null);
	RenderCommand::setClearColor({0.25f, 0.5f, 0.75f, 1.f});
	EXPECT_FLOAT_EQ(RenderCommand::getClearColor().g(), 0.5f);
	RenderCommand::invalidate();
	owl::core::Log::invalidate();
}
