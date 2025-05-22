
#include "testHelper.h"

#include <renderer/RenderCommand.h>
#include <renderer/null/Framebuffer.h>

using namespace owl::renderer;

TEST(Renderer, FramebufferBase) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	RenderCommand::create(RenderAPI::Type::Null);

	const auto fbuf = Framebuffer::create(
			{.size = {12, 12},
			 .attachments = {{.format=AttachmentSpecification::Format::Surface, .tiling=AttachmentSpecification::Tiling::Optimal},
							 {.format=AttachmentSpecification::Format::RedInteger, .tiling=AttachmentSpecification::Tiling::Optimal}},
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
