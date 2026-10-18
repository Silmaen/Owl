/**
 * @file fontLibrary_test.cpp
 * @author Silmaen
 * @date 23/10/2024
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <app/Application.h>
#include <data/fonts/FontLibrary.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <string>
#include <tuple>

using namespace owl::data::fonts;
using namespace owl;
using namespace owl::core;
using namespace owl::app;

TEST(FontLibrary, basic) {
	FontLibrary fontLibrary;
	EXPECT_EQ(fontLibrary.getLoadedFontNames().size(), 2);
	EXPECT_EQ(fontLibrary.getFont("null"), nullptr);
	EXPECT_EQ(fontLibrary.getDefaultFont(), nullptr);
	EXPECT_EQ(fontLibrary.getFoundFontNames().size(), 0);
}

TEST(FontLibrary, basic_with_app) {
	Log::init(Log::Level::Off);
	auto app = owl::mkShared<Application>(AppParams{.args = nullptr,
													.frameLogFrequency = 0,
													.name = "test",
													.assetsPattern = "",
													.icon = "",
													.width = 0,
													.height = 0,
													.argCount = 0,
													.renderer = renderer::gpu::RenderAPI::Type::Null,
													.hasGui = false,
													.useDebugging = false,
													.isDummy = true});
	auto& fontLibrary = app->getFontLibrary();
	EXPECT_EQ(fontLibrary.getLoadedFontNames().size(), 2);
	EXPECT_EQ(fontLibrary.getFoundFontNames().size(), 17);// Inter, JetBrains Mono, Lucide, Open Sans, Roboto

	EXPECT_EQ(fontLibrary.getFont("null"), nullptr);
	const auto myFont = fontLibrary.getDefaultFont();
	ASSERT_NE(myFont, nullptr);
	EXPECT_TRUE(myFont->isDefault());
	EXPECT_STREQ(myFont->getName().c_str(), "OpenSans-Regular");
	EXPECT_NE(myFont->getAtlasTexture(), nullptr);

	EXPECT_NEAR(myFont->getScaledLineHeight(), 1, 0.001);
	auto [quad, uv] = myFont->getGlyphBox('a');
	EXPECT_NEAR(quad.min().x(), -0.02937, 0.001);
	EXPECT_NEAR(quad.min().y(), -0.07006, 0.001);
	EXPECT_NEAR(quad.max().x(), 0.41122, 0.001);
	EXPECT_NEAR(quad.max().y(), 0.46232, 0.001);
	EXPECT_NEAR(uv.min().x(), 0.13721, 0.001);
	EXPECT_NEAR(uv.min().y(), 0.68604, 0.001);
	EXPECT_NEAR(uv.max().x(), 0.16064, 0.001);
	EXPECT_NEAR(uv.max().y(), 0.71436, 0.001);
	EXPECT_NEAR(myFont->getAdvance('a', 'b'), 0.408, 0.001);

	Application::invalidate();
	app.reset();
	Log::invalidate();
}

TEST(FontLibrary, UnloadFontsFromAProjectDirectory) {
	Log::init(Log::Level::Off);
	auto app = owl::mkShared<Application>(AppParams{.name = "fontUnload",
													.renderer = renderer::gpu::RenderAPI::Type::Null,
													.hasGui = false,
													.isDummy = true});
	const auto project = std::filesystem::temp_directory_path() / "owl_font_unload_test";
	std::filesystem::remove_all(project);
	std::filesystem::create_directories(project / "fonts");
	std::filesystem::copy_file(owl::test::getRootPath() / "engine_assets" / "fonts" / "Inter-Regular.ttf",
							   project / "fonts" / "ProjectOnlyFont.ttf");
	app->addAssetDirectory({.title = "project", .assetsPath = project});
	auto& fontLibrary = app->getFontLibrary();
	ASSERT_NE(fontLibrary.getFont("ProjectOnlyFont"), nullptr);
	const auto countOf = [&fontLibrary](const std::string& iName) -> size_t {
		return static_cast<size_t>(std::ranges::count(fontLibrary.getLoadedFontNames(), iName));
	};
	EXPECT_EQ(countOf("ProjectOnlyFont"), 1);

	EXPECT_EQ(fontLibrary.unloadFontsFrom(project), 1);
	EXPECT_EQ(countOf("ProjectOnlyFont"), 0);
	EXPECT_EQ(fontLibrary.unloadFontsFrom(project), 0);
	ASSERT_NE(fontLibrary.getDefaultFont(), nullptr);

	std::ignore = fontLibrary.getFont("ProjectOnlyFont");
	app->removeAssetDirectory(project);
	EXPECT_EQ(countOf("ProjectOnlyFont"), 0);

	std::filesystem::remove_all(project);
	Application::invalidate();
	app.reset();
	Log::invalidate();
}
