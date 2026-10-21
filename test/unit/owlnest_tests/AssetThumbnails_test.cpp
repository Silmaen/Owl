/**
 * @file AssetThumbnails_test.cpp
 * @author Silmaen
 * @date 11/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "panel/AssetThumbnails.h"
#include "testHelper.h"

#include <core/Log.h>
#include <data/geometry/MeshLoader.h>
#include <data/geometry/StaticMesh.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <utility>

using namespace owl;
using namespace owl::nest::panel;

TEST(AssetThumbnails, KindFollowsTheExtension) {
	EXPECT_EQ(AssetThumbnails::getKind("a/b.png"), AssetThumbnails::Kind::Image);
	EXPECT_EQ(AssetThumbnails::getKind("b.jpeg"), AssetThumbnails::Kind::Image);
	EXPECT_EQ(AssetThumbnails::getKind("level.owl"), AssetThumbnails::Kind::Scene);
	EXPECT_EQ(AssetThumbnails::getKind("coin.owlprefab"), AssetThumbnails::Kind::Prefab);
	EXPECT_EQ(AssetThumbnails::getKind("jump.wav"), AssetThumbnails::Kind::Sound);
	EXPECT_EQ(AssetThumbnails::getKind("theme.ogg"), AssetThumbnails::Kind::Sound);
	EXPECT_EQ(AssetThumbnails::getKind("atlas.owltileset"), AssetThumbnails::Kind::Tileset);
	EXPECT_EQ(AssetThumbnails::getKind("ship.glb"), AssetThumbnails::Kind::Mesh);
	EXPECT_EQ(AssetThumbnails::getKind("hero.lua"), AssetThumbnails::Kind::None);
}

TEST(AssetThumbnails, SoundGetsAWaveformAndCountsAsReady) {
	OWL_REQUIRE_MODULE(AUDIO);
	core::Log::init(core::Log::Level::Off);
	const auto dir = std::filesystem::temp_directory_path() / "owl_asset_thumbnails_test";
	std::filesystem::create_directories(dir);
	const auto wav = dir / "beep.wav";
	{
		// Mono 16-bit PCM, 800 frames of a square wave.
		std::ofstream file(wav, std::ios::binary);
		const auto le = [&file](const uint32_t iValue, const int iBytes) -> void {
			for (int i = 0; i < iBytes; ++i) file.put(static_cast<char>((iValue >> (8 * i)) & 0xffU));
		};
		file.write("RIFF", 4);
		le(36 + 1600, 4);
		file.write("WAVEfmt ", 8);
		le(16, 4);
		le(1, 2);
		le(1, 2);
		le(8000, 4);
		le(16000, 4);
		le(2, 2);
		le(16, 2);
		file.write("data", 4);
		le(1600, 4);
		for (uint32_t i = 0; i < 800; ++i) le(i % 2 == 0 ? 8000U : static_cast<uint16_t>(-8000), 2);
	}
	AssetThumbnails previews;
	previews.beginFrame();
	EXPECT_EQ(previews.get(dir / "notes.txt"), nullptr);
	const auto* preview = previews.get(wav);
	ASSERT_NE(preview, nullptr);
	EXPECT_EQ(preview->waveform.size(), AssetThumbnails::waveformBuckets);
	EXPECT_GT(preview->waveform.front(), 0.2f);
	EXPECT_EQ(previews.getProgress(), (std::pair<size_t, size_t>{0, 1}));
	previews.beginFrame();
	EXPECT_NE(previews.get(wav), nullptr);
	EXPECT_EQ(previews.getProgress(), (std::pair<size_t, size_t>{1, 1}));
	// A missing sound fails once and is not read again.
	previews.beginFrame();
	EXPECT_EQ(previews.get(dir / "missing.wav"), nullptr);
	EXPECT_EQ(previews.get(dir / "missing.wav"), nullptr);
	EXPECT_EQ(previews.getProgress(), (std::pair<size_t, size_t>{1, 2}));
	previews.clear();
	std::filesystem::remove_all(dir);
	core::Log::invalidate();
}

TEST(AssetThumbnails, MeshProjectsIntoTheUnitSquareBackToFront) {
	core::Log::init(core::Log::Level::Off);
	const auto dir = std::filesystem::temp_directory_path() / "owl_asset_thumbnails_mesh_test";
	std::filesystem::create_directories(dir);
	const auto obj = dir / "tetra.obj";
	{
		std::ofstream file(obj);
		file << "v 0 0 0\nv 1 0 0\nv 0 1 0\nv 0 0 1\nf 1 3 2\nf 1 2 4\nf 1 4 3\nf 2 3 4\n";
	}
	const auto mesh = data::geometry::MeshLoader::loadStaticMesh(obj);
	ASSERT_NE(mesh, nullptr);
	const auto triangles = AssetThumbnails::projectMesh(*mesh);
	ASSERT_EQ(triangles.size(), 4u);
	for (const auto& triangle: triangles) {
		EXPECT_GT(triangle.shade, 0.f);
		EXPECT_LE(triangle.shade, 1.f);
		for (const auto& corner: triangle.corners) {
			EXPECT_GE(corner.x(), 0.f);
			EXPECT_LE(corner.x(), 1.f);
			EXPECT_GE(corner.y(), 0.f);
			EXPECT_LE(corner.y(), 1.f);
		}
	}
	EXPECT_TRUE(AssetThumbnails::projectMesh(data::geometry::StaticMesh{}).empty());
	std::filesystem::remove_all(dir);
	core::Log::invalidate();
}

TEST(AssetThumbnails, MeshStyleDefaultsToTexturedAndChanges) {
	AssetThumbnails thumbnails;
	EXPECT_EQ(thumbnails.getMeshStyle(), AssetThumbnails::MeshStyle::Textured);
	thumbnails.setMeshStyle(AssetThumbnails::MeshStyle::Flat);
	EXPECT_EQ(thumbnails.getMeshStyle(), AssetThumbnails::MeshStyle::Flat);
	thumbnails.beginFrame();
	// Flat previews are made on the CPU: the request is answered in the frame.
	const auto mesh = test::getTestAssetsPath("render_tests") / "meshes" / "textured_cube.glb";
	ASSERT_NE(thumbnails.get(mesh), nullptr);
	EXPECT_FALSE(thumbnails.get(mesh)->mesh.empty());
	thumbnails.setMeshStyle(AssetThumbnails::MeshStyle::Shaded);
	thumbnails.beginFrame();
	// A rendered preview waits for its GPU job.
	EXPECT_EQ(thumbnails.get(mesh), nullptr);
	thumbnails.clear();
}
