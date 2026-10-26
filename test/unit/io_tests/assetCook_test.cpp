/**
 * @file assetCook_test.cpp
 * @author Silmaen
 * @date 26/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <data/assets/cook/AssetCooker.h>
#include <data/assets/cook/CookCache.h>
#include <renderer/TextureDecoder.h>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace owl;
using namespace owl::data::assets;
using namespace owl::data::assets::cook;

namespace {

// A 64x32 gradient, opaque.
auto makePng() -> std::vector<uint8_t> {
	std::vector<uint8_t> pixels(64 * 32 * 4);
	for (uint32_t y = 0; y < 32; ++y) {
		for (uint32_t x = 0; x < 64; ++x) {
			const auto index = (static_cast<size_t>(y) * 64 + x) * 4;
			pixels[index] = static_cast<uint8_t>(x * 4);
			pixels[index + 1] = static_cast<uint8_t>(y * 8);
			pixels[index + 2] = 128;
			pixels[index + 3] = 255;
		}
	}
	return renderer::encodeImagePng({64, 32}, pixels);
}

auto textureMeta(const TextureCompression iCompression) -> AssetMeta {
	return AssetMeta{.guid = AssetGuid{1},
					 .importer = Importer::Texture,
					 .texture = {.nearest = false, .compression = iCompression, .mips = true}};
}

void writeBytes(const std::filesystem::path& iFile, const std::vector<uint8_t>& iBytes) {
	std::filesystem::create_directories(iFile.parent_path());
	std::ofstream file(iFile, std::ios::binary);
	file.write(reinterpret_cast<const char*>(iBytes.data()), static_cast<std::streamsize>(iBytes.size()));
}

}// namespace

TEST(AssetCook, UncompressedTextureKeepsItsSource) {
	const auto png = makePng();
	const auto cooked = cookAsset(textureMeta(TextureCompression::None), png, "gradient.png");
	ASSERT_TRUE(cooked);
	EXPECT_EQ(*cooked, png);
	const std::vector<uint8_t> broken{1, 2, 3, 4};
	EXPECT_EQ(cookAsset(textureMeta(TextureCompression::None), broken, "broken.png").error(), CookError::InvalidSource);
	EXPECT_EQ(cookAsset(textureMeta(TextureCompression::Uastc), broken, "broken.png").error(), CookError::InvalidSource);
}

TEST(AssetCook, UastcTextureTranscodesWithItsMipChain) {
	const auto cooked = cookAsset(textureMeta(TextureCompression::Uastc), makePng(), "gradient.png");
	ASSERT_TRUE(cooked);
	EXPECT_TRUE(renderer::isKtx2(*cooked));
	EXPECT_EQ(renderer::peekImageSize(*cooked), (math::vec2ui{64, 32}));

	const auto rgba = renderer::transcodeKtx2(*cooked, renderer::gpu::ImageFormat::Rgba8);
	ASSERT_TRUE(rgba);
	EXPECT_EQ(rgba->size, (math::vec2ui{64, 32}));
	ASSERT_EQ(rgba->levels.size(), 7u);
	EXPECT_EQ(rgba->levels.front().size(), 64u * 32u * 4u);
	EXPECT_EQ(rgba->levels.back().size(), 4u);
	// UASTC is near lossless: a texel of the gradient stays within a few steps.
	const auto& top = rgba->levels.front();
	const size_t texel = (static_cast<size_t>(10) * 64 + 20) * 4;
	EXPECT_NEAR(top[texel], 80, 8);
	EXPECT_NEAR(top[texel + 2], 128, 8);

	const auto bc7 = renderer::transcodeKtx2(*cooked, renderer::gpu::ImageFormat::Bc7);
	ASSERT_TRUE(bc7);
	EXPECT_EQ(bc7->levels.front().size(), (64u / 4) * (32u / 4) * 16u);
	const auto astc = renderer::transcodeKtx2(*cooked, renderer::gpu::ImageFormat::Astc4x4);
	ASSERT_TRUE(astc);
	EXPECT_EQ(astc->levels.front().size(), (64u / 4) * (32u / 4) * 16u);
}

TEST(AssetCook, Etc1sWithoutMipsHasOneLevel) {
	auto meta = textureMeta(TextureCompression::Etc1s);
	meta.texture.mips = false;
	const auto cooked = cookAsset(meta, makePng(), "gradient.png");
	ASSERT_TRUE(cooked);
	const auto rgba = renderer::transcodeKtx2(*cooked, renderer::gpu::ImageFormat::Rgba8);
	ASSERT_TRUE(rgba);
	EXPECT_EQ(rgba->levels.size(), 1u);
	const std::vector<uint8_t> notKtx{0, 1, 2};
	EXPECT_FALSE(renderer::transcodeKtx2(notKtx, renderer::gpu::ImageFormat::Rgba8));
}

TEST(AssetCook, OtherAssetsAreCopied) {
	const std::vector<uint8_t> lua{'-', '-', ' ', 'x'};
	const auto cooked = cookAsset(AssetMeta{.guid = AssetGuid{2}, .importer = Importer::Script, .texture = {}}, lua,
								  "a.lua");
	ASSERT_TRUE(cooked);
	EXPECT_EQ(*cooked, lua);
	EXPECT_FALSE(describe(CookError::EncodeFailed).empty());
}

TEST(AssetCook, CacheKeyFollowsBytesAndSettings) {
	const auto png = makePng();
	const auto key = CookCache::computeKey(png, textureMeta(TextureCompression::Uastc));
	EXPECT_EQ(key.size(), 32u);
	EXPECT_EQ(key, CookCache::computeKey(png, textureMeta(TextureCompression::Uastc)));
	EXPECT_NE(key, CookCache::computeKey(png, textureMeta(TextureCompression::Etc1s)));
	auto changed = png;
	changed.back() ^= 0xFFu;
	EXPECT_NE(key, CookCache::computeKey(changed, textureMeta(TextureCompression::Uastc)));
	auto guid = textureMeta(TextureCompression::Uastc);
	guid.guid = AssetGuid{99};
	EXPECT_EQ(key, CookCache::computeKey(png, guid));
}

TEST(AssetCook, CacheCooksOnceAndPersists) {
	const auto root = std::filesystem::temp_directory_path() / "owl_cook_cache";
	std::filesystem::remove_all(root);
	const auto source = root / "src" / "gradient.png";
	writeBytes(source, makePng());
	const auto meta = textureMeta(TextureCompression::Uastc);
	{
		CookCache cache(root / "cache");
		EXPECT_FALSE(cache.isCached(source, meta));
		const auto first = cache.cook(source, meta, "gradient.png");
		ASSERT_TRUE(first);
		EXPECT_TRUE(cache.isCached(source, meta));
		const auto second = cache.cook(source, meta, "gradient.png");
		ASSERT_TRUE(second);
		EXPECT_EQ(*first, *second);
		EXPECT_EQ(cache.getStats().cooked, 1u);
		EXPECT_EQ(cache.getStats().hits, 1u);
	}
	{
		CookCache cache(root / "cache");
		EXPECT_TRUE(cache.isCached(source, meta));
		ASSERT_TRUE(cache.cook(source, meta, "gradient.png"));
		EXPECT_EQ(cache.getStats().hits, 1u);
		EXPECT_EQ(cache.getStats().cooked, 0u);
		EXPECT_FALSE(cache.isCached(source, textureMeta(TextureCompression::Etc1s)));
		EXPECT_EQ(cache.cook(root / "src" / "missing.png", meta, "missing.png").error(), CookError::Unreadable);
		EXPECT_EQ(cache.getStats().failed, 1u);
		cache.clear();
		EXPECT_FALSE(cache.isCached(source, meta));
	}
	std::filesystem::remove_all(root);
}
