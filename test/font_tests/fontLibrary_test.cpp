
#include "testHelper.h"

#include <fonts/FontLibrary.h>
#include <core/Application.h>

using namespace owl::fonts;
using namespace owl;
using namespace owl::core;

TEST(FontLibrary, basic) {
	FontLibrary fontLibrary;
	EXPECT_EQ(fontLibrary.getLoadedFontNames().size(), 2);
	EXPECT_EQ(fontLibrary.getFont("null"), nullptr);
	EXPECT_EQ(fontLibrary.getDefaultFont(), nullptr);
	EXPECT_EQ(fontLibrary.getFoundFontNames().size(), 0);
}

TEST(FontLibrary, basic_with_app) {
	Log::init(spdlog::level::off);
	auto app = owl::mkShared<Application>(AppParams{.args = nullptr,
													.frameLogFrequency = 0,
													.name = "test",
													.assetsPattern = "",
													.icon = "",
													.width = 0,
													.height = 0,
													.argCount = 0,
													.renderer = owl::renderer::RenderAPI::Type::Null,
													.hasGui = false,
													.useDebugging = false,
													.isDummy = true});
	auto& fontLibrary = app->getFontLibrary();
	EXPECT_EQ(fontLibrary.getLoadedFontNames().size(), 2);
	EXPECT_EQ(fontLibrary.getFoundFontNames().size(), 12);

	EXPECT_EQ(fontLibrary.getFont("null"), nullptr);
	const auto myFont = fontLibrary.getDefaultFont();
	ASSERT_NE(myFont, nullptr);
	EXPECT_TRUE(myFont->isDefault());
	EXPECT_STREQ(myFont->getName().c_str(), "OpenSans-Regular");
	EXPECT_NE(myFont->getAtlasTexture(), nullptr);

	EXPECT_NEAR(myFont->getScaledLineHeight(), 1, 0.001);
	auto [quad, uv] = myFont->getGlyphBox('a');
	EXPECT_NEAR(quad.min().x(), 0.00735, 0.001);
	EXPECT_NEAR(quad.min().y(), -0.0333, 0.001);
	EXPECT_NEAR(quad.max().x(), 0.3745, 0.001);
	EXPECT_NEAR(quad.max().y(), 0.4256, 0.001);
	EXPECT_NEAR(uv.min().x(), 0.3642, 0.001);
	EXPECT_NEAR(uv.min().y(), 0.5244, 0.001);
	EXPECT_NEAR(uv.max().x(), 0.4033, 0.001);
	EXPECT_NEAR(uv.max().y(), 0.5732, 0.001);
	EXPECT_NEAR(myFont->getAdvance('a', 'b'), 0.408, 0.001);

	Application::invalidate();
	app.reset();
	Log::invalidate();
}
