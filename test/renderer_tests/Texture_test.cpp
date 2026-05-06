#include "testHelper.h"

#include "renderer/gpu/Texture.h"
#include "renderer/gpu/null/Texture.h"

#include "renderer/gpu/RenderCommand.h"

using namespace owl::renderer;
using namespace owl::renderer::gpu;

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

// Round-trip Specification::toString/fromString — covers the parsing branches.
TEST(TextureSpecifications, FromStringRoundTrip) {
	Texture::Specification spec{.size = {640, 480}, .format = ImageFormat::Rgba8, .generateMips = false};
	const std::string serialized = spec.toString();
	Texture::Specification parsed;
	parsed.fromString(serialized);
	EXPECT_EQ(parsed.size.x(), 640u);
	EXPECT_EQ(parsed.size.y(), 480u);
	EXPECT_EQ(parsed.format, ImageFormat::Rgba8);
	EXPECT_FALSE(parsed.generateMips);

	// Unknown enum name → falls back to Rgb8.
	parsed.fromString("32:32:NoSuchFormat:true");
	EXPECT_EQ(parsed.format, ImageFormat::Rgb8);
	EXPECT_TRUE(parsed.generateMips);
}

// Texture2D::createFromSerialized with no API initialised still handles the
// short / unknown-prefix cases and returns nullptr.
TEST(Texture, CreateFromSerializedShortAndUnknown) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	EXPECT_EQ(Texture2D::createFromSerialized(""), nullptr);
	EXPECT_EQ(Texture2D::createFromSerialized("ab"), nullptr);
	EXPECT_EQ(Texture2D::createFromSerialized("xxx:foo"), nullptr);
	owl::core::Log::invalidate();
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
