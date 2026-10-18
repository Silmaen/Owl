/**
 * @file assetLibrary_test.cpp
 * @author Silmaen
 * @date 22/01/2025
 * Copyright (c) 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <app/Application.h>
#include <data/assets/AssetSearchPaths.h>
#include <renderer/Renderer.h>
#include <renderer/TextureDecoder.h>
#include <renderer/gpu/RenderCommand.h>
#include <renderer/gpu/Texture.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

using namespace owl::core;
using namespace owl::app;
using namespace owl::renderer;
using namespace owl::renderer::gpu;

namespace {

auto makeTexture(const uint32_t iSide) -> owl::shared<Texture2D> {
	return Texture2D::create(Texture::Specification{.size = {iSide, iSide}, .format = ImageFormat::Rgba8});
}

void writeImage(const std::filesystem::path& iPath, const uint32_t iSide) {
	std::filesystem::create_directories(iPath.parent_path());
	const std::vector<uint8_t> pixels(static_cast<size_t>(iSide) * iSide * 4, 0x80);
	ASSERT_TRUE(writeImagePng(iPath, {iSide, iSide}, pixels));
}

}// namespace

TEST(AssetLibrary, DummyTexture) {
	Log::init(Log::Level::Off);
	const AppParams params{.name = "super boby",
						   .renderer = gpu::RenderAPI::Type::Null,
						   .hasGui = false,
						   .isDummy = true};
	auto app = owl::mkShared<Application>(params);
	{
		const auto lib = Renderer::TextureLibrary();
		const auto lists = lib.list();
		EXPECT_GT(lists.size(), 0);
	}
	Application::invalidate();
	app.reset();
	Log::invalidate();
}

TEST(AssetLibrary, RemoveClearAndSize) {
	Log::init(Log::Level::Off);
	RenderCommand::create(RenderAPI::Type::Null);
	{
		Renderer::TextureLibrary lib;
		EXPECT_EQ(lib.size(), 0);
		auto small = makeTexture(2);
		auto big = makeTexture(8);
		auto other = makeTexture(4);
		lib.add("small", small);
		lib.add("big", big);
		lib.add("other", other);
		EXPECT_EQ(lib.size(), 3);

		EXPECT_TRUE(lib.remove("small"));
		EXPECT_FALSE(lib.remove("small"));
		EXPECT_FALSE(lib.exists("small"));
		EXPECT_EQ(lib.size(), 2);
		EXPECT_EQ(small->getSize().x(), 2);

		const auto removed = lib.removeIf([](const std::string&, const owl::shared<Texture2D>& iTexture) -> bool {
			return iTexture->getSize().x() > 4;
		});
		EXPECT_EQ(removed, 1);
		EXPECT_FALSE(lib.exists("big"));
		EXPECT_TRUE(lib.exists("other"));

		lib.clear();
		EXPECT_EQ(lib.size(), 0);
		EXPECT_FALSE(lib.exists("other"));
	}
	RenderCommand::invalidate();
	Log::invalidate();
}

TEST(AssetLibrary, IsInDirectory) {
	const auto root = std::filesystem::temp_directory_path() / "owl_is_in_directory";
	EXPECT_TRUE(owl::data::assets::isInDirectory(root / "textures" / "a.png", root));
	EXPECT_TRUE(owl::data::assets::isInDirectory(root / "x" / ".." / "a.png", root));
	EXPECT_FALSE(owl::data::assets::isInDirectory(root / ".." / "a.png", root));
	EXPECT_FALSE(owl::data::assets::isInDirectory(root.parent_path() / "owl_is_in_directory_other" / "a.png", root));
	EXPECT_FALSE(owl::data::assets::isInDirectory({}, root));
	EXPECT_FALSE(owl::data::assets::isInDirectory(root / "a.png", {}));
}

TEST(AssetLibrary, ProjectSwitchReadsTheNewTextureOfTheSameName) {
	Log::init(Log::Level::Off);
	const AppParams params{.name = "project switch",
						   .renderer = gpu::RenderAPI::Type::Null,
						   .hasGui = false,
						   .isDummy = true};
	auto app = owl::mkShared<Application>(params);
	const auto root = std::filesystem::temp_directory_path() / "owl_project_switch_test";
	std::filesystem::remove_all(root);
	const auto first = root / "first";
	const auto second = root / "second";
	writeImage(first / "textures" / "same.png", 2);
	writeImage(second / "textures" / "same.png", 4);
	const std::string name = "nam:textures/same.png";
	auto& library = Renderer::getTextureLibrary();
	auto kept = makeTexture(1);
	library.add("kept", kept);

	app->addAssetDirectory({.title = "first", .assetsPath = first});
	const auto fromFirst = Texture2D::createFromSerializedForDeserialize(name);
	ASSERT_NE(fromFirst, nullptr);
	EXPECT_EQ(fromFirst->getSize().x(), 2);
	EXPECT_EQ(Texture2D::createFromSerializedForDeserialize(name), fromFirst);

	app->removeAssetDirectory(first);
	EXPECT_FALSE(library.exists(name));
	EXPECT_TRUE(library.exists("kept"));

	app->addAssetDirectory({.title = "second", .assetsPath = second});
	const auto fromSecond = Texture2D::createFromSerializedForDeserialize(name);
	ASSERT_NE(fromSecond, nullptr);
	EXPECT_NE(fromSecond, fromFirst);
	EXPECT_EQ(fromSecond->getSize().x(), 4);
	EXPECT_TRUE(owl::data::assets::isInDirectory(fromSecond->getPath(), second));
	EXPECT_EQ(app->unloadAssetsFrom(second), 1);
	EXPECT_EQ(app->unloadAssetsFrom({}), 0);

	app->removeAssetDirectory(second);
	library.clear();
	std::filesystem::remove_all(root);
	Application::invalidate();
	app.reset();
	Log::invalidate();
}
