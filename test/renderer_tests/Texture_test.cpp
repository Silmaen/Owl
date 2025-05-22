#include "testHelper.h"

#include "renderer/Texture.h"
#include "renderer/null/Texture.h"

#include "renderer/RenderCommand.h"

using namespace owl::renderer;

TEST(TextureSpecifications, stringDefinition) {
	Texture::Specification spec{.size = {15, 156}, .format = ImageFormat::R8, .generateMips = true};
	{
		const null::Texture2D tex(spec);
		EXPECT_STREQ(tex.getSerializeString().c_str(), "spec:15:156:R8:true");
	}
	spec.size = {0, 0};
	{
		const null::Texture2D tex(spec);
		EXPECT_STREQ(tex.getSerializeString().c_str(), "emp:");
	}
}

TEST(TextureSpecifications, PixelSize) {
	Texture::Specification spec{.size = {15, 156}, .format = ImageFormat::R8, .generateMips = true};
	EXPECT_EQ(spec.getPixelSize(), 1);
	spec.format = ImageFormat::None;
	EXPECT_EQ(spec.getPixelSize(), 0);
	spec.format = ImageFormat::Rgb8;
	EXPECT_EQ(spec.getPixelSize(), 3);
	spec.format = ImageFormat::Rgba8;
	EXPECT_EQ(spec.getPixelSize(), 4);
	spec.format = ImageFormat::Rgba32F;
	EXPECT_EQ(spec.getPixelSize(), 16);
}

TEST(Texture, CreateFromString) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	{
		const auto tex = Texture2D::createFromSerialized("");
		EXPECT_EQ(tex, nullptr);
	}
	{
		const auto tex = Texture2D::createFromSerialized("superbob");
		EXPECT_EQ(tex, nullptr);
	}
	{
		// unitilialized renderer
		const auto tex = Texture2D::createFromSerialized("emp:");
		EXPECT_EQ(tex, nullptr);
	}
	RenderCommand::create(RenderAPI::Type::Null);
	{
		const auto tex = Texture2D::createFromSerialized("emp:");
		EXPECT_FALSE(tex->isLoaded());
	}
	{
		const auto tex = Texture2D::createFromSerialized("siz:15:156:R8:true");
		EXPECT_EQ(tex->getSpecification().format, ImageFormat::R8);
	}
	{
		const auto tex = Texture2D::createFromSerialized("nam:bob");
		EXPECT_EQ(tex, nullptr);
	}
	{
		const auto tex = Texture2D::createFromSerialized("pat:bob");
		EXPECT_EQ(tex, nullptr);
	}
	owl::core::Log::invalidate();
}
