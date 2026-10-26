/**
 * @file packAssetSource_test.cpp
 * @author Silmaen
 * @date 26/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <app/Application.h>
#include <data/assets/AssetSearchPaths.h>
#include <data/assets/pack/PackWriter.h>
#include <renderer/TextureDecoder.h>
#include <renderer/gpu/Texture.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace owl;
using namespace owl::data::assets;

namespace {

class PackAssetSource : public testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_app = mkShared<app::Application>(app::AppParams{.renderer = renderer::gpu::RenderAPI::Type::Null,
														  .hasGui = false,
														  .useDebugging = false,
														  .isDummy = true});
		m_dir = std::filesystem::temp_directory_path() / "owl_pack_asset_source_test";
		std::filesystem::remove_all(m_dir);
		std::filesystem::create_directories(m_dir);
		const std::string notes = "packed notes";
		const std::vector<uint8_t> rgba(2u * 2u * 4u, 200);
		pack::PackWriter writer;
		writer.addData(std::vector<uint8_t>(notes.begin(), notes.end()), "data/notes.txt", pack::AssetType::Other);
		writer.addData(renderer::encodeImagePng({2, 2}, rgba), "textures/ui/hero.png", pack::AssetType::Texture);
		ASSERT_TRUE(writer.write(m_dir / "game.owlpack"));
		ASSERT_TRUE(m_app->openPack(m_dir / "game.owlpack"));
	}

	void TearDown() override {
		m_app->closePack();
		std::filesystem::remove_all(m_dir);
		app::Application::invalidate();
		m_app.reset();
		core::Log::invalidate();
	}

	shared<app::Application> m_app;
	std::filesystem::path m_dir;
};

}// namespace

TEST_F(PackAssetSource, isPackedAssetMatchesEntriesOnly) {
	EXPECT_TRUE(isPackedAsset("data/notes.txt"));
	EXPECT_TRUE(isPackedAsset("data/../data/notes.txt"));
	EXPECT_FALSE(isPackedAsset("data/other.txt"));
	EXPECT_FALSE(isPackedAsset(m_dir / "data" / "notes.txt"));
	EXPECT_FALSE(isPackedAsset(""));
	m_app->closePack();
	EXPECT_FALSE(isPackedAsset("data/notes.txt"));
}

TEST_F(PackAssetSource, readAssetBytesPrefersThePack) {
	const auto bytes = readAssetBytes("data/notes.txt");
	ASSERT_TRUE(bytes.has_value());
	EXPECT_EQ(std::string(bytes->begin(), bytes->end()), "packed notes");
	EXPECT_EQ(readAssetText("data/notes.txt").value_or(""), "packed notes");
	EXPECT_FALSE(readAssetBytes("data/missing.txt").has_value());
	EXPECT_FALSE(readAssetBytes("").has_value());
}

TEST_F(PackAssetSource, readAssetBytesFallsBackToTheDisk) {
	const auto file = m_dir / "disk.txt";
	{
		std::ofstream out(file);
		out << "on disk";
	}
	EXPECT_EQ(readAssetText(file).value_or(""), "on disk");
}

TEST_F(PackAssetSource, findPackedAssetSearchesAsAnAssetFolder) {
	const std::vector<std::string> extensions{".jpg", ".png"};
	EXPECT_EQ(findPackedAsset("textures/ui/hero.png", {}).value_or(""), "textures/ui/hero.png");
	EXPECT_EQ(findPackedAsset("textures/ui/hero", extensions).value_or(""), "textures/ui/hero.png");
	EXPECT_EQ(findPackedAsset("hero", extensions).value_or(""), "textures/ui/hero.png");
	EXPECT_EQ(findPackedAsset("ui/hero.png", {}).value_or(""), "textures/ui/hero.png");
	EXPECT_FALSE(findPackedAsset("ero.png", {}).has_value());
	EXPECT_FALSE(findPackedAsset("hero", {".jpg"}).has_value());
	EXPECT_FALSE(findPackedAsset("", extensions).has_value());
}

TEST_F(PackAssetSource, textureLoadsFromAPackEntry) {
	const auto texture = renderer::gpu::Texture2D::create(std::filesystem::path{"textures/ui/hero.png"});
	ASSERT_NE(texture, nullptr);
	EXPECT_EQ(texture->getSize(), math::vec2ui(2, 2));
	EXPECT_EQ(texture->getSerializeString(), "nam:textures/ui/hero.png");
	EXPECT_NE(renderer::gpu::Texture2D::createFromSerialized("nam:textures/ui/hero.png"), nullptr);
}
