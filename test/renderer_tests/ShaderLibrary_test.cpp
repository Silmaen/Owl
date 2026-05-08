
#include "testHelper.h"

#include <renderer/Renderer.h>
#include <renderer/gpu/RenderCommand.h>

using namespace owl::renderer;
using namespace owl::renderer::gpu;

TEST(ShaderLibrary, creation) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	RenderCommand::create(RenderAPI::Type::Null);
	auto lib = Renderer::ShaderLibrary();
	EXPECT_FALSE(lib.exists("bob"));
	EXPECT_FALSE(lib.get("bob"));
	EXPECT_TRUE(lib.load("bob", "bob"));
	lib.load("bob2");
	EXPECT_TRUE(lib.exists("bob2"));
	auto shader = Shader::create(Shader::Specification{{.name = "superbob", .renderer = "renderer"}});
	EXPECT_TRUE(shader);
	lib.add("sha", shader);
	lib.add("sha", shader);
	EXPECT_FALSE(lib.exists("superbob"));
	RenderCommand::invalidate();
	owl::core::Log::invalidate();
}

TEST(Shader, basis) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	RenderCommand::create(RenderAPI::Type::Null);
	const auto shader2 = Shader::create(Shader::Specification{{.name = "super", .renderer = "renderer"}});
	shader2->bind();
	shader2->unbind();
	int ii[3] = {0, 1, 2};
	shader2->setIntArray("bob", ii, 3);
	shader2->setInt("fbob", 333);
	shader2->setFloat("fbob", 0.1f);
	shader2->setFloat2("fbob", {2, 3});
	shader2->setFloat3("fbob", {9, 8, 7});
	shader2->setFloat4("fbob", {255, 255, 45, 255});
	shader2->setMat4("fbob", {255, 255, 45, 255, 255, 255, 45, 255, 255, 255, 45, 255, 255, 255, 45, 255});

	EXPECT_STREQ(shader2->getFullName().c_str(), "renderer#super");
	EXPECT_STREQ(shader2->getName().c_str(), "super");
	EXPECT_STREQ(shader2->getRenderer().c_str(), "renderer");
	RenderCommand::invalidate();
	owl::core::Log::invalidate();
}

TEST(Shader, decomposeAndComposeName) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	// Plain name: no renderer.
	const auto a = Shader::decomposeName("texture");
	EXPECT_EQ(a.name, "texture");
	EXPECT_TRUE(a.renderer.empty());
	// renderer#name form.
	const auto b = Shader::decomposeName("vulkan#texture");
	EXPECT_EQ(b.renderer, "vulkan");
	EXPECT_EQ(b.name, "texture");
	// Trailing # — invalid, both fields cleared.
	const auto c = Shader::decomposeName("vulkan#");
	EXPECT_TRUE(c.name.empty());
	EXPECT_TRUE(c.renderer.empty());
	// Round-trip through composeName.
	EXPECT_EQ(Shader::composeName({.name = "x", .renderer = "y"}), "y#x");
	EXPECT_EQ(Shader::composeName({.name = "x", .renderer = ""}), "x");
	owl::core::Log::invalidate();
}

TEST(Shader, createBeforeRendererInitDoesNotCrash) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	// Renderer not initialised → `requireInit()` returns false, the function
	// must skip the texture-library lookup and bail out gracefully.
	const auto sh = Shader::create(Shader::Specification{{.name = "noInit", .renderer = ""}});
	(void) sh;// behaviour is null-API-dependent — exercise the early-bypass path.
	owl::core::Log::invalidate();
}
