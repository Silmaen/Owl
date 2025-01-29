
#include "testHelper.h"

#include <renderer/Framebuffer.h>
#include <renderer/GraphContext.h>
#include <renderer/RenderAPI.h>
#include <renderer/RenderCommand.h>
#include <renderer/Shader.h>
#include <renderer/Texture.h>
#include <renderer/UniformBuffer.h>

using namespace owl::renderer;

TEST(RenderAPI, RenderComamand) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	RenderCommand::create(RenderAPI::Type{255});
	EXPECT_EQ(RenderCommand::getState(), RenderAPI::State::Error);
	RenderCommand::create(RenderAPI::Type::Null);
	EXPECT_EQ(RenderCommand::getState(), RenderAPI::State::Created);
	EXPECT_EQ(RenderCommand::getApi(), RenderAPI::Type::Null);
	RenderCommand::setClearColor({.4f, .4f, .4f, 1.f});
	RenderCommand::clear();
	RenderCommand::init();
	EXPECT_EQ(RenderCommand::getState(), RenderAPI::State::Ready);
	RenderCommand::setViewport(1, 1, 45, 65);
	RenderCommand::beginTextureLoad();
	RenderCommand::endTextureLoad();
	RenderCommand::beginFrame();
	RenderCommand::beginBatch();
	RenderCommand::endBatch();
	RenderCommand::endFrame();
	RenderCommand::invalidate();
	EXPECT_EQ(RenderCommand::getState(), RenderAPI::State::Error);
	owl::core::Log::invalidate();
}

TEST(RenderAPI, badCreation) {
	// bad Type value
	owl::core::Log::init(owl::core::Log::Level::Off);
	const owl::uniq<RenderAPI> api = RenderAPI::create(RenderAPI::Type{255});
	const auto gf = GraphContext::create(nullptr);
	const auto fb = Framebuffer::create({});
	const auto ub = UniformBuffer::create(0, 0, "Renderer2D");
	const auto tex = Texture2D::create(std::filesystem::path());
	const auto tex2 = Texture2D::create(Texture2D::Specification{.size = {1, 1}});
	const auto shader = Shader::create(std::filesystem::path());
	const auto shader2 = Shader::create("bob2");
	EXPECT_TRUE(api == nullptr);
	EXPECT_TRUE(gf == nullptr);
	EXPECT_TRUE(fb == nullptr);
	EXPECT_TRUE(ub == nullptr);
	EXPECT_TRUE(tex == nullptr);
	EXPECT_TRUE(tex2 == nullptr);
	EXPECT_TRUE(shader == nullptr);
	EXPECT_TRUE(shader2 == nullptr);
	owl::core::Log::invalidate();
}

TEST(RenderAPI, creation) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	const auto api = RenderAPI::create(RenderAPI::Type::Null);
	ASSERT_TRUE(api);
	EXPECT_EQ(api->getState(), RenderAPI::State::Created);
	api->init();
	EXPECT_EQ(api->getState(), RenderAPI::State::Ready);
	EXPECT_EQ(api->getApi(), RenderAPI::Type::Null);
	owl::core::Log::invalidate();
}
