/**
 * @file AssetField_test.cpp
 * @author Silmaen
 * @date 26/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "testHelper.h"

#include "gui/widgets/AssetField.h"

using namespace owl::gui::widgets;

TEST(AssetField, IsPathOfKindAcceptsTextureExtensions) {
	for (const auto* ext: {".png", ".jpg", ".jpeg", ".bmp", ".tga", ".hdr"})
		EXPECT_TRUE(isPathOfKind(std::string{"sprite"} + ext, AssetKind::Texture)) << ext;
	EXPECT_FALSE(isPathOfKind("sprite.ttf", AssetKind::Texture));
	EXPECT_FALSE(isPathOfKind("sprite.wav", AssetKind::Texture));
	EXPECT_FALSE(isPathOfKind("sprite", AssetKind::Texture));
}

TEST(AssetField, IsPathOfKindAcceptsFontExtensions) {
	EXPECT_TRUE(isPathOfKind("hero.ttf", AssetKind::Font));
	EXPECT_TRUE(isPathOfKind("hero.otf", AssetKind::Font));
	EXPECT_FALSE(isPathOfKind("hero.png", AssetKind::Font));
}

TEST(AssetField, IsPathOfKindAcceptsSoundExtensions) {
	for (const auto* ext: {".wav", ".mp3", ".ogg", ".flac"})
		EXPECT_TRUE(isPathOfKind(std::string{"snd"} + ext, AssetKind::Sound)) << ext;
	EXPECT_FALSE(isPathOfKind("snd.png", AssetKind::Sound));
}

TEST(AssetField, IsPathOfKindLuaScriptIsLuaOnly) {
	EXPECT_TRUE(isPathOfKind("logic.lua", AssetKind::LuaScript));
	EXPECT_FALSE(isPathOfKind("logic.py", AssetKind::LuaScript));
	EXPECT_FALSE(isPathOfKind("logic.cpp", AssetKind::LuaScript));
}

TEST(AssetField, IsPathOfKindAnyScriptAcceptsCommonSources) {
	for (const auto* ext: {".lua", ".py", ".c", ".cpp", ".cc", ".cxx", ".h", ".hpp"})
		EXPECT_TRUE(isPathOfKind(std::string{"src"} + ext, AssetKind::AnyScript)) << ext;
	EXPECT_FALSE(isPathOfKind("src.png", AssetKind::AnyScript));
	EXPECT_FALSE(isPathOfKind("src.txt", AssetKind::AnyScript));
}

TEST(AssetField, IsPathOfKindSceneAndPrefab) {
	EXPECT_TRUE(isPathOfKind("level.owl", AssetKind::Scene));
	EXPECT_FALSE(isPathOfKind("level.owlprefab", AssetKind::Scene));
	EXPECT_TRUE(isPathOfKind("npc.owlprefab", AssetKind::Prefab));
	EXPECT_FALSE(isPathOfKind("level.owl", AssetKind::Prefab));
}

TEST(AssetField, IsPathOfKindRejectsUnknownExtension) {
	EXPECT_FALSE(isPathOfKind("file.xyz", AssetKind::Texture));
	EXPECT_FALSE(isPathOfKind("file.xyz", AssetKind::Font));
	EXPECT_FALSE(isPathOfKind("file.xyz", AssetKind::Sound));
	EXPECT_FALSE(isPathOfKind("file.xyz", AssetKind::LuaScript));
	EXPECT_FALSE(isPathOfKind("file.xyz", AssetKind::AnyScript));
}

TEST(AssetField, IsPathOfKindAnyAcceptsAll) {
	EXPECT_TRUE(isPathOfKind("anything.xyz", AssetKind::Any));
	EXPECT_TRUE(isPathOfKind("noextension", AssetKind::Any));
	EXPECT_TRUE(isPathOfKind("", AssetKind::Any));
}

TEST(AssetField, IsPathOfKindHandlesUppercaseExtension) {
	EXPECT_TRUE(isPathOfKind("HERO.PNG", AssetKind::Texture));
	EXPECT_TRUE(isPathOfKind("HERO.TTF", AssetKind::Font));
	EXPECT_TRUE(isPathOfKind("LEVEL.OWL", AssetKind::Scene));
	EXPECT_TRUE(isPathOfKind("LOGIC.LUA", AssetKind::LuaScript));
}

TEST(AssetField, IsPathOfKindHandlesSubdirectories) {
	EXPECT_TRUE(isPathOfKind("textures/sub/hero.png", AssetKind::Texture));
	EXPECT_TRUE(isPathOfKind("/abs/path/level.owl", AssetKind::Scene));
}
