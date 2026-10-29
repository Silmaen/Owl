/**
 * @file AssetKind_test.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "AssetKind.h"
#include "testHelper.h"

#include <gtest/gtest.h>

using namespace owl::nest;

TEST(AssetKind, ClassifiesTheEngineFormats) {
	EXPECT_EQ(classifyAsset("scenes/level.owl"), AssetKind::Scene);
	EXPECT_EQ(classifyAsset("flow.owlflow"), AssetKind::NodeGraph);
	EXPECT_EQ(classifyAsset("walk.owlanim"), AssetKind::Animation);
	EXPECT_EQ(classifyAsset("map.owltilemap"), AssetKind::Tilemap);
	EXPECT_EQ(classifyAsset("tiles.owltileset"), AssetKind::Tileset);
	EXPECT_EQ(classifyAsset("prefabs/door.owlprefab"), AssetKind::Prefab);
}

TEST(AssetKind, ClassifiesMaterials) { EXPECT_EQ(classifyAsset("materials/gold.owlmat"), AssetKind::Material); }

TEST(AssetKind, ClassifiesSoundCues) {
	EXPECT_EQ(classifyAsset("sounds/steps.owlsound"), AssetKind::SoundCue);
	for (const auto* name: {"a.wav", "a.ogg", "a.flac", "a.mp3"})
		EXPECT_EQ(classifyAsset(name), AssetKind::Audio) << name;
}

TEST(AssetKind, ClassifiesTheMeshFormats) {
	for (const auto* name: {"meshes/crate.glb", "crate.gltf", "crate.obj"})
		EXPECT_EQ(classifyAsset(name), AssetKind::Mesh) << name;
	EXPECT_EQ(classifyAsset("crate.fbx"), AssetKind::Unsupported);
}

TEST(AssetKind, ClassifiesTextAsCode) {
	for (const auto* name: {"a.lua", "b.yml", "c.json", "d.md", "e.svg", "f.cpp", "g.hpp", "gamecontrollerdb.txt"})
		EXPECT_EQ(classifyAsset(name), AssetKind::Code) << name;
}

TEST(AssetKind, ClassifiesImagesAndShaders) {
	for (const auto* name: {"a.png", "b.jpg", "c.jpeg", "textures/d.bmp", "e.tga"})
		EXPECT_EQ(classifyAsset(name), AssetKind::Image) << name;
	EXPECT_EQ(classifyAsset("shaders/text.slang"), AssetKind::Code);
	EXPECT_EQ(classifyAsset("A.PNG"), AssetKind::Unsupported);
}

TEST(AssetKind, LeavesTheRestUnsupported) {
	EXPECT_EQ(classifyAsset("sound.aiff"), AssetKind::Unsupported);
	EXPECT_EQ(classifyAsset("noextension"), AssetKind::Unsupported);
	EXPECT_EQ(classifyAsset("LEVEL.OWL"), AssetKind::Unsupported);
}
