/**
 * @file assetScanner_test.cpp
 * @author Silmaen
 * @date 17/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Macros.h>
#include <data/assets/pack/AssetScanner.h>
#include <data/assets/pack/PackWriter.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>
#include <utility>
#include <vector>

using namespace owl::data::assets::pack;

namespace {
auto getTempDir() -> std::filesystem::path {
	auto dir = std::filesystem::temp_directory_path() / "owl_asset_scanner_tests";
	std::filesystem::create_directories(dir);
	return dir;
}

void writeFile(const std::filesystem::path& iPath, const std::string& iContent) {
	std::filesystem::create_directories(iPath.parent_path());
	std::ofstream out(iPath);
	out << iContent;
}

void writeDummyBinary(const std::filesystem::path& iPath) {
	std::filesystem::create_directories(iPath.parent_path());
	std::ofstream out(iPath, std::ios::binary);
	const char data[] = "DUMMY";
	out.write(data, sizeof(data));
}

}// namespace

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wweak-vtables")
class AssetScannerTest : public ::testing::Test {
protected:
	void SetUp() override {
		m_tempDir = getTempDir();
		std::filesystem::remove_all(m_tempDir);
		std::filesystem::create_directories(m_tempDir);
	}
	void TearDown() override { std::filesystem::remove_all(m_tempDir); }
	std::filesystem::path m_tempDir;
};
OWL_DIAG_POP

// Edge-case scanScene tests — exercise the private resolveX helpers indirectly
// through the public scanning entry points. These cover early-return /
// "no Application instance" branches.

TEST_F(AssetScannerTest, ScanSceneWithUnresolvableTextureLogsWarning) {
	const auto scenePath = m_tempDir / "broken.owl";
	writeFile(scenePath, "Scene: broken\nEntities:\n"
						 "  - Entity: 1\n"
						 "    SpriteRenderer:\n"
						 "      texture: \"pat:/no/such/file.png\"\n");
	std::vector<std::string> warnings;
	const auto assets = AssetScanner::scanScene(scenePath, &warnings);
	// Scene itself is added; missing texture surfaces as a warning.
	EXPECT_GE(assets.size(), 1u);
	EXPECT_FALSE(warnings.empty());
}

TEST_F(AssetScannerTest, ScanSceneWithEmptyTextureFieldDoesNotAddAsset) {
	const auto scenePath = m_tempDir / "emptytex.owl";
	writeFile(scenePath, "Scene: emptytex\nEntities:\n"
						 "  - Entity: 1\n"
						 "    SpriteRenderer:\n"
						 "      texture: \"emp:\"\n");
	const auto assets = AssetScanner::scanScene(scenePath);
	// "emp:" is reserved (no texture asset), so the only asset is the scene file.
	EXPECT_EQ(assets.size(), 1u);
}

TEST_F(AssetScannerTest, ScanSceneWithSizePrefixedTextureIgnored) {
	const auto scenePath = m_tempDir / "siztex.owl";
	writeFile(scenePath, "Scene: siztex\nEntities:\n"
						 "  - Entity: 1\n"
						 "    SpriteRenderer:\n"
						 "      texture: \"siz:128\"\n");
	const auto assets = AssetScanner::scanScene(scenePath);
	EXPECT_EQ(assets.size(), 1u);
}

TEST_F(AssetScannerTest, ScanSceneWithUnknownTexturePrefixIgnored) {
	const auto scenePath = m_tempDir / "unktex.owl";
	writeFile(scenePath, "Scene: unktex\nEntities:\n"
						 "  - Entity: 1\n"
						 "    SpriteRenderer:\n"
						 "      texture: \"xxx:foo\"\n");
	const auto assets = AssetScanner::scanScene(scenePath);
	EXPECT_EQ(assets.size(), 1u);
}

TEST_F(AssetScannerTest, ScanSceneWithFontMissingLogsWarning) {
	const auto scenePath = m_tempDir / "missingfont.owl";
	writeFile(scenePath, "Scene: missingfont\nEntities:\n"
						 "  - Entity: 1\n"
						 "    TextRenderer:\n"
						 "      font: \"NoSuchFont\"\n");
	std::vector<std::string> warnings;
	(void) AssetScanner::scanScene(scenePath, &warnings);
	EXPECT_FALSE(warnings.empty());
}

TEST_F(AssetScannerTest, ScanSceneWithMissingSoundAssetLogsWarning) {
	const auto scenePath = m_tempDir / "missingsound.owl";
	writeFile(scenePath, "Scene: missingsound\nEntities:\n"
						 "  - Entity: 1\n"
						 "    SoundSource:\n"
						 "      soundAsset: \"missing.wav\"\n");
	std::vector<std::string> warnings;
	(void) AssetScanner::scanScene(scenePath, &warnings);
	EXPECT_FALSE(warnings.empty());
}

TEST_F(AssetScannerTest, ScanSceneWithSoundAbsolutePathResolved) {
	const auto sndPath = m_tempDir / "snd.wav";
	writeDummyBinary(sndPath);
	const auto scenePath = m_tempDir / "withsound.owl";
	writeFile(scenePath, "Scene: withsound\nEntities:\n"
						 "  - Entity: 1\n"
						 "    SoundSource:\n"
						 "      soundAsset: \"" +
								 sndPath.generic_string() + "\"\n");
	const auto assets = AssetScanner::scanScene(scenePath);
	bool found = false;
	for (const auto& ref: assets)
		if (ref.assetType == AssetType::Sound && ref.diskPath == sndPath)
			found = true;
	EXPECT_TRUE(found);
}

TEST_F(AssetScannerTest, ScanSceneWithMalformedYamlSilent) {
	const auto scenePath = m_tempDir / "garbage.owl";
	writeFile(scenePath, "Scene: garbage\nEntities: [\n");
	const auto assets = AssetScanner::scanScene(scenePath);
	// Malformed YAML → only the scene file itself is added.
	EXPECT_EQ(assets.size(), 1u);
}

TEST_F(AssetScannerTest, ScanProjectMissingFirstSceneReturnsEmpty) {
	const auto assets = AssetScanner::scanProject(m_tempDir, "nope.owl");
	EXPECT_TRUE(assets.empty());
}

TEST_F(AssetScannerTest, ScanProjectScansEntryScene) {
	const auto firstScene = m_tempDir / "first.owl";
	writeFile(firstScene, "Scene: first\nEntities: []\n");
	const auto assets = AssetScanner::scanProject(m_tempDir, "first.owl");
	// Entry scene must be in the asset list.
	bool foundFirst = false;
	for (const auto& ref: assets) {
		if (ref.assetType == AssetType::Scene && ref.diskPath == firstScene)
			foundFirst = true;
	}
	EXPECT_TRUE(foundFirst);
}

TEST_F(AssetScannerTest, ScanEmptyScene) {
	const auto scenePath = m_tempDir / "empty.owl";
	writeFile(scenePath, "Scene: empty\n");

	const auto assets = AssetScanner::scanScene(scenePath);
	// Should contain at least the scene itself.
	ASSERT_GE(assets.size(), 1);
	EXPECT_EQ(assets[0].assetType, AssetType::Scene);
}

TEST_F(AssetScannerTest, ScanNonExistentScene) {
	const auto assets = AssetScanner::scanScene(m_tempDir / "nonexistent.owl");
	EXPECT_TRUE(assets.empty());
}

TEST_F(AssetScannerTest, ScanSceneWithSpriteTexture) {
	// Create a dummy texture file.
	const auto texPath = m_tempDir / "textures" / "player.png";
	writeDummyBinary(texPath);

	// Create a scene referencing the texture via pat: (absolute path).
	const auto scenePath = m_tempDir / "test.owl";
	writeFile(scenePath, "Scene: test\nEntities:\n"
						 "  - Entity: 1\n"
						 "    SpriteRenderer:\n"
						 "      color: [1, 1, 1, 1]\n"
						 "      texture: \"pat:" +
								 texPath.generic_string() + "\"\n");

	const auto assets = AssetScanner::scanScene(scenePath);
	// Should have the scene + the texture.
	ASSERT_GE(assets.size(), 2);

	bool foundTexture = false;
	for (const auto& ref: assets) {
		if (ref.assetType == AssetType::Texture && ref.diskPath == texPath)
			foundTexture = true;
	}
	EXPECT_TRUE(foundTexture);
}

TEST_F(AssetScannerTest, ScanSceneWithAnimatedSprite) {
	const auto texPath = m_tempDir / "textures" / "coin.png";
	writeDummyBinary(texPath);

	const auto scenePath = m_tempDir / "test.owl";
	writeFile(scenePath, "Scene: test\nEntities:\n"
						 "  - Entity: 1\n"
						 "    AnimatedSpriteRenderer:\n"
						 "      color: [1, 1, 1, 1]\n"
						 "      texture: \"pat:" +
								 texPath.generic_string() + "\"\n");

	const auto assets = AssetScanner::scanScene(scenePath);
	bool foundTexture = false;
	for (const auto& ref: assets) {
		if (ref.assetType == AssetType::Texture && ref.diskPath == texPath)
			foundTexture = true;
	}
	EXPECT_TRUE(foundTexture);
}

TEST_F(AssetScannerTest, ScanSceneWithUIImage) {
	const auto texPath = m_tempDir / "textures" / "logo.png";
	writeDummyBinary(texPath);

	const auto scenePath = m_tempDir / "test.owl";
	writeFile(scenePath, "Scene: test\nEntities:\n"
						 "  - Entity: 1\n"
						 "    UiImage:\n"
						 "      tint: [1, 1, 1, 1]\n"
						 "      texture: \"pat:" +
								 texPath.generic_string() + "\"\n");

	const auto assets = AssetScanner::scanScene(scenePath);
	bool foundTexture = false;
	for (const auto& ref: assets) {
		if (ref.assetType == AssetType::Texture && ref.diskPath == texPath)
			foundTexture = true;
	}
	EXPECT_TRUE(foundTexture);
}

TEST_F(AssetScannerTest, ScanSceneWithUIToolCursor) {
	const auto texPath = m_tempDir / "textures" / "cursor.png";
	writeDummyBinary(texPath);

	const auto scenePath = m_tempDir / "test.owl";
	writeFile(scenePath, "Scene: test\nEntities:\n"
						 "  - Entity: 1\n"
						 "    UiToolCursor:\n"
						 "      tint: [1, 1, 1, 1]\n"
						 "      texture: \"pat:" +
								 texPath.generic_string() + "\"\n");

	const auto assets = AssetScanner::scanScene(scenePath);
	const bool foundTexture = std::ranges::any_of(assets, [&texPath](const AssetReference& iRef) -> bool {
		return iRef.assetType == AssetType::Texture && iRef.diskPath == texPath;
	});
	EXPECT_TRUE(foundTexture);
}

TEST_F(AssetScannerTest, ScanSceneWithBackgroundTexture) {
	const auto texPath = m_tempDir / "textures" / "sky.png";
	writeDummyBinary(texPath);

	const auto scenePath = m_tempDir / "test.owl";
	writeFile(scenePath, "Scene: test\nEntities:\n"
						 "  - Entity: 1\n"
						 "    BackgroundTexture:\n"
						 "      mode: 0\n"
						 "      texture: \"pat:" +
								 texPath.generic_string() + "\"\n");

	const auto assets = AssetScanner::scanScene(scenePath);
	bool foundTexture = false;
	for (const auto& ref: assets) {
		if (ref.assetType == AssetType::Texture && ref.diskPath == texPath)
			foundTexture = true;
	}
	EXPECT_TRUE(foundTexture);
}

TEST_F(AssetScannerTest, ScanSceneWithLuaScript) {
	// Create a Lua script.
	const auto scriptPath = m_tempDir / "scripts" / "player.lua";
	writeFile(scriptPath, "function on_create()\n    log.info('hello')\nend\n");

	const auto scenePath = m_tempDir / "test.owl";
	writeFile(scenePath, "Scene: test\nEntities:\n"
						 "  - Entity: 1\n"
						 "    LuaScript:\n"
						 "      scriptPath: " +
								 scriptPath.generic_string() + "\n");

	const auto assets = AssetScanner::scanScene(scenePath);
	bool foundScript = false;
	for (const auto& ref: assets) {
		if (ref.assetType == AssetType::Script && ref.diskPath == scriptPath)
			foundScript = true;
	}
	EXPECT_TRUE(foundScript);
}

TEST_F(AssetScannerTest, ScanLuaScriptWithSceneLoadScene) {
	// Create a second scene that the Lua script references.
	const auto scene2Path = m_tempDir / "scenes" / "level2.owl";
	writeFile(scene2Path, "Scene: level2\n");

	// Create a Lua script that calls scene.load_scene with the second scene.
	const auto scriptPath = m_tempDir / "scripts" / "menu.lua";
	writeFile(scriptPath, "function on_play()\n"
						  "    scene.load_scene(\"" +
								  scene2Path.generic_string() + "\")\nend\n");

	const auto scenePath = m_tempDir / "main.owl";
	writeFile(scenePath, "Scene: main\nEntities:\n"
						 "  - Entity: 1\n"
						 "    LuaScript:\n"
						 "      scriptPath: " +
								 scriptPath.generic_string() + "\n");

	const auto assets = AssetScanner::scanScene(scenePath);

	// Should contain: main.owl, menu.lua, level2.owl.
	bool foundScript = false;
	bool foundScene2 = false;
	for (const auto& ref: assets) {
		if (ref.assetType == AssetType::Script && ref.diskPath == scriptPath)
			foundScript = true;
		if (ref.assetType == AssetType::Scene && ref.diskPath == scene2Path)
			foundScene2 = true;
	}
	EXPECT_TRUE(foundScript);
	EXPECT_TRUE(foundScene2);
}

TEST_F(AssetScannerTest, ScanLuaScriptWithDeferredSceneLoad) {
	// Test the deferred pattern: pending_scene = "path.owl" (not inside scene.load_scene)
	const auto scene2Path = m_tempDir / "gameplay.owl";
	writeFile(scene2Path, "Scene: gameplay\n");

	const auto scriptPath = m_tempDir / "scripts" / "menu.lua";
	writeFile(scriptPath, "local pending = nil\n"
						  "function on_play()\n"
						  "    pending = \"" +
								  scene2Path.generic_string() + "\"\nend\n");

	const auto scenePath = m_tempDir / "main.owl";
	writeFile(scenePath, "Scene: main\nEntities:\n"
						 "  - Entity: 1\n"
						 "    LuaScript:\n"
						 "      scriptPath: " +
								 scriptPath.generic_string() + "\n");

	const auto assets = AssetScanner::scanScene(scenePath);
	bool foundScene2 = false;
	for (const auto& ref: assets) {
		if (ref.assetType == AssetType::Scene && ref.diskPath == scene2Path)
			foundScene2 = true;
	}
	EXPECT_TRUE(foundScene2);
}

TEST_F(AssetScannerTest, ScanLuaScriptWithSoundPlay) {
	// Create a dummy sound file.
	const auto soundPath = m_tempDir / "sounds" / "jump.wav";
	writeDummyBinary(soundPath);

	// Lua script uses sound.play with an absolute path.
	const auto scriptPath = m_tempDir / "scripts" / "player.lua";
	writeFile(scriptPath, "function on_update()\n"
						  "    sound.play(\"" +
								  soundPath.generic_string() + "\")\nend\n");

	const auto scenePath = m_tempDir / "test.owl";
	writeFile(scenePath, "Scene: test\nEntities:\n"
						 "  - Entity: 1\n"
						 "    LuaScript:\n"
						 "      scriptPath: " +
								 scriptPath.generic_string() + "\n");

	const auto assets = AssetScanner::scanScene(scenePath);
	bool foundSound = false;
	for (const auto& ref: assets) {
		if (ref.assetType == AssetType::Sound && ref.diskPath == soundPath)
			foundSound = true;
	}
	EXPECT_TRUE(foundSound);
}

TEST_F(AssetScannerTest, ScanLuaScriptFollowsQuotedSoundVariants) {
	const auto first = m_tempDir / "sounds" / "birds_1.wav";
	const auto second = m_tempDir / "sounds" / "birds_2.ogg";
	writeDummyBinary(first);
	writeDummyBinary(second);
	// The variants sit in a table and `sound.play` gets a variable: the scanner still follows them.
	const auto scriptPath = m_tempDir / "scripts" / "ambient.lua";
	writeFile(scriptPath, "local BIRDS = { \"" + first.generic_string() + "\", '" + second.generic_string() +
								  "' }\nfunction on_update()\n    sound.play(BIRDS[math.random(2)])\nend\n");
	const auto scenePath = m_tempDir / "ambient.owl";
	writeFile(scenePath, "Scene: ambient\nEntities:\n"
						 "  - Entity: 1\n"
						 "    LuaScript:\n"
						 "      scriptPath: " +
								 scriptPath.generic_string() + "\n");

	const auto assets = AssetScanner::scanScene(scenePath);
	size_t found = 0;
	for (const auto& ref: assets) {
		if (ref.assetType == AssetType::Sound && (ref.diskPath == first || ref.diskPath == second))
			++found;
	}
	EXPECT_EQ(found, 2u);
}

TEST_F(AssetScannerTest, ScanLuaScriptFollowsQuotedSerializedTexture) {
	const auto texturePath = m_tempDir / "textures" / "icon.png";
	writeDummyBinary(texturePath);
	const auto scriptPath = m_tempDir / "scripts" / "list.lua";
	writeFile(scriptPath, "local ICON = \"pat:" + texturePath.generic_string() + "\"\n");
	const auto scenePath = m_tempDir / "list.owl";
	writeFile(scenePath, "Scene: list\nEntities:\n"
						 "  - Entity: 1\n"
						 "    LuaScript:\n"
						 "      scriptPath: " +
								 scriptPath.generic_string() + "\n");

	const auto assets = AssetScanner::scanScene(scenePath);
	EXPECT_TRUE(std::ranges::any_of(assets, [&texturePath](const AssetReference& iRef) -> bool {
		return iRef.assetType == AssetType::Texture && iRef.diskPath == texturePath;
	}));
}

TEST_F(AssetScannerTest, ScanScriptPropertiesFollowsScenesRecursively) {
	const auto lastPath = m_tempDir / "scenes" / "level_3.owl";
	writeFile(lastPath, "Scene: level_3\n");
	const auto secondPath = m_tempDir / "scenes" / "level_2.owl";
	writeFile(secondPath, "Scene: level_2\nEntities:\n"
						  "  - Entity: 2\n"
						  "    LuaScript:\n"
						  "      scriptPath: \"\"\n"
						  "      properties:\n"
						  "        - name: next_scene\n"
						  "          type: string\n"
						  "          value: " +
								  lastPath.generic_string() + "\n");
	const auto scenePath = m_tempDir / "scenes" / "level_1.owl";
	writeFile(scenePath, "Scene: level_1\nEntities:\n"
						 "  - Entity: 1\n"
						 "    LuaScript:\n"
						 "      scriptPath: \"\"\n"
						 "      properties:\n"
						 "        - name: level\n"
						 "          type: int\n"
						 "          value: 1\n"
						 "        - name: next_scene\n"
						 "          type: string\n"
						 "          value: " +
								 secondPath.generic_string() + "\n");

	std::vector<std::string> warnings;
	const auto assets = AssetScanner::scanScene(scenePath, &warnings);
	const auto hasScene = [&assets](const std::filesystem::path& iPath) -> bool {
		return std::ranges::any_of(assets, [&iPath](const AssetReference& iRef) -> bool {
			return iRef.assetType == AssetType::Scene && iRef.diskPath == iPath;
		});
	};
	EXPECT_TRUE(hasScene(secondPath));
	EXPECT_TRUE(hasScene(lastPath));
	EXPECT_TRUE(warnings.empty());
}

TEST_F(AssetScannerTest, ScanScriptPropertiesFollowsSoundsAndTextures) {
	const auto soundPath = m_tempDir / "sounds" / "win.ogg";
	writeDummyBinary(soundPath);
	const auto texturePath = m_tempDir / "textures" / "badge.png";
	writeDummyBinary(texturePath);
	const auto scenePath = m_tempDir / "props.owl";
	writeFile(scenePath, "Scene: props\nEntities:\n"
						 "  - Entity: 1\n"
						 "    LuaScript:\n"
						 "      scriptPath: \"\"\n"
						 "      properties:\n"
						 "        - name: win_sound\n"
						 "          type: string\n"
						 "          value: " +
								 soundPath.generic_string() +
								 "\n"
								 "        - name: badge\n"
								 "          type: string\n"
								 "          value: \"pat:" +
								 texturePath.generic_string() + "\"\n");

	const auto assets = AssetScanner::scanScene(scenePath);
	const auto has = [&assets](const std::filesystem::path& iPath, const AssetType iType) -> bool {
		return std::ranges::any_of(assets, [&iPath, iType](const AssetReference& iRef) -> bool {
			return iRef.assetType == iType && iRef.diskPath == iPath;
		});
	};
	EXPECT_TRUE(has(soundPath, AssetType::Sound));
	EXPECT_TRUE(has(texturePath, AssetType::Texture));
}

TEST_F(AssetScannerTest, ScanScriptPropertiesIgnoresPlainValuesAndWarnsOnMissingScene) {
	const auto scenePath = m_tempDir / "plain.owl";
	writeFile(scenePath, "Scene: plain\nEntities:\n"
						 "  - Entity: 1\n"
						 "    LuaScript:\n"
						 "      scriptPath: \"\"\n"
						 "      properties:\n"
						 "        - name: title\n"
						 "          type: string\n"
						 "          value: Cellar\n"
						 "        - name: speed\n"
						 "          type: float\n"
						 "          value: 2.5\n");
	std::vector<std::string> warnings;
	EXPECT_EQ(AssetScanner::scanScene(scenePath, &warnings).size(), 1u);
	EXPECT_TRUE(warnings.empty());

	writeFile(scenePath, "Scene: plain\nEntities:\n"
						 "  - Entity: 1\n"
						 "    LuaScript:\n"
						 "      scriptPath: \"\"\n"
						 "      properties:\n"
						 "        - name: next_scene\n"
						 "          type: string\n"
						 "          value: scenes/no_such_level.owl\n");
	warnings.clear();
	(void) AssetScanner::scanScene(scenePath, &warnings);
	ASSERT_EQ(warnings.size(), 1u);
	EXPECT_NE(warnings.front().find("next_scene"), std::string::npos);
}

TEST_F(AssetScannerTest, ScanSceneWithTeleportTrigger) {
	// Create a target scene.
	const auto scene2Path = m_tempDir / "level2.owl";
	writeFile(scene2Path, "Scene: level2\n");

	const auto scenePath = m_tempDir / "main.owl";
	writeFile(scenePath, "Scene: main\nEntities:\n"
						 "  - Entity: 1\n"
						 "    Trigger:\n"
						 "      Type: Teleport\n"
						 "      LevelName: " +
								 scene2Path.generic_string() + "\n");

	const auto assets = AssetScanner::scanScene(scenePath);
	bool foundScene2 = false;
	for (const auto& ref: assets) {
		if (ref.assetType == AssetType::Scene && ref.diskPath == scene2Path)
			foundScene2 = true;
	}
	EXPECT_TRUE(foundScene2);
}

TEST_F(AssetScannerTest, ScanSceneWithDeathTriggerLevelName) {
	// Death trigger with LevelName should also be followed.
	const auto gameOverPath = m_tempDir / "game_over.owl";
	writeFile(gameOverPath, "Scene: gameover\n");

	const auto scenePath = m_tempDir / "main.owl";
	writeFile(scenePath, "Scene: main\nEntities:\n"
						 "  - Entity: 1\n"
						 "    Trigger:\n"
						 "      Type: Death\n"
						 "      LevelName: " +
								 gameOverPath.generic_string() + "\n");

	const auto assets = AssetScanner::scanScene(scenePath);
	bool foundGameOver = false;
	for (const auto& ref: assets) {
		if (ref.assetType == AssetType::Scene && ref.diskPath == gameOverPath)
			foundGameOver = true;
	}
	EXPECT_TRUE(foundGameOver);
}

TEST_F(AssetScannerTest, ScanSceneWithVictoryTriggerLevelName) {
	const auto victoryPath = m_tempDir / "victory.owl";
	writeFile(victoryPath, "Scene: victory\n");

	const auto scenePath = m_tempDir / "main.owl";
	writeFile(scenePath, "Scene: main\nEntities:\n"
						 "  - Entity: 1\n"
						 "    Trigger:\n"
						 "      Type: Victory\n"
						 "      LevelName: " +
								 victoryPath.generic_string() + "\n");

	const auto assets = AssetScanner::scanScene(scenePath);
	bool foundVictory = false;
	for (const auto& ref: assets) {
		if (ref.assetType == AssetType::Scene && ref.diskPath == victoryPath)
			foundVictory = true;
	}
	EXPECT_TRUE(foundVictory);
}

TEST_F(AssetScannerTest, ScanSceneNoDuplicates) {
	const auto texPath = m_tempDir / "textures" / "shared.png";
	writeDummyBinary(texPath);

	// Two entities reference the same texture.
	const auto scenePath = m_tempDir / "test.owl";
	writeFile(scenePath, "Scene: test\nEntities:\n"
						 "  - Entity: 1\n"
						 "    SpriteRenderer:\n"
						 "      texture: \"pat:" +
								 texPath.generic_string() +
								 "\"\n"
								 "  - Entity: 2\n"
								 "    SpriteRenderer:\n"
								 "      texture: \"pat:" +
								 texPath.generic_string() + "\"\n");

	const auto assets = AssetScanner::scanScene(scenePath);
	int texCount = 0;
	for (const auto& ref: assets) {
		if (ref.assetType == AssetType::Texture)
			++texCount;
	}
	EXPECT_EQ(texCount, 1);
}

TEST_F(AssetScannerTest, ScanSceneRecursiveNoCycle) {
	// Two scenes reference each other via Teleport triggers — should not infinite-loop.
	const auto scene1 = m_tempDir / "a.owl";
	const auto scene2 = m_tempDir / "b.owl";

	writeFile(scene1, "Scene: a\nEntities:\n"
					  "  - Entity: 1\n"
					  "    Trigger:\n"
					  "      Type: Teleport\n"
					  "      LevelName: " +
							  scene2.generic_string() + "\n");
	writeFile(scene2, "Scene: b\nEntities:\n"
					  "  - Entity: 1\n"
					  "    Trigger:\n"
					  "      Type: Teleport\n"
					  "      LevelName: " +
							  scene1.generic_string() + "\n");

	const auto assets = AssetScanner::scanScene(scene1);
	// Should contain both scenes, no infinite loop.
	int sceneCount = 0;
	for (const auto& ref: assets) {
		if (ref.assetType == AssetType::Scene)
			++sceneCount;
	}
	EXPECT_EQ(sceneCount, 2);
}

TEST_F(AssetScannerTest, ScanSceneEmptyTriggerLevelName) {
	// Trigger with empty LevelName should not cause issues.
	const auto scenePath = m_tempDir / "test.owl";
	writeFile(scenePath, "Scene: test\nEntities:\n"
						 "  - Entity: 1\n"
						 "    Trigger:\n"
						 "      Type: Death\n"
						 "      LevelName: \"\"\n");

	const auto assets = AssetScanner::scanScene(scenePath);
	// Should contain just the scene itself.
	int sceneCount = 0;
	for (const auto& ref: assets) {
		if (ref.assetType == AssetType::Scene)
			++sceneCount;
	}
	EXPECT_EQ(sceneCount, 1);
}

TEST_F(AssetScannerTest, ScanSceneWithTilemapChainPacksTilesetAndTexture) {
	// Tilemap component → .owltilemap → .owltileset → atlas texture must all be packed.
	const auto texPath = m_tempDir / "textures" / "atlas.png";
	writeDummyBinary(texPath);
	const auto tilesetPath = m_tempDir / "world.owltileset";
	writeFile(tilesetPath, "Tileset: world\ntexture: \"pat:" + texPath.generic_string() + "\"\ncolumns: 4\nrows: 4\n");
	const auto tilemapPath = m_tempDir / "world.owltilemap";
	writeFile(tilemapPath, "Tilemap: world\ntilesetPath: " + tilesetPath.generic_string() + "\nwidth: 4\nheight: 4\n");
	const auto scenePath = m_tempDir / "test.owl";
	writeFile(scenePath, "Scene: test\nEntities:\n"
						 "  - Entity: 1\n"
						 "    Tilemap:\n"
						 "      tilemapPath: " +
								 tilemapPath.generic_string() + "\n");

	const auto assets = AssetScanner::scanScene(scenePath);
	bool foundTilemap = false;
	bool foundTileset = false;
	bool foundTexture = false;
	for (const auto& ref: assets) {
		if (ref.diskPath == tilemapPath)
			foundTilemap = true;
		if (ref.diskPath == tilesetPath)
			foundTileset = true;
		if (ref.assetType == AssetType::Texture && ref.diskPath == texPath)
			foundTexture = true;
	}
	EXPECT_TRUE(foundTilemap);
	EXPECT_TRUE(foundTileset);
	EXPECT_TRUE(foundTexture);
}

TEST_F(AssetScannerTest, ScanSceneWithIsometricObjectPacksSheetAndTexture) {
	const auto sheetPath = m_tempDir / "iso" / "buildings.json";
	writeFile(sheetPath, R"({"zoom": "1x", "buildings": {}})");
	const auto texPath = m_tempDir / "iso" / "buildings.png";
	writeDummyBinary(texPath);
	const auto scenePath = m_tempDir / "test.owl";
	writeFile(scenePath, "Scene: test\nEntities:\n"
						 "  - Entity: 1\n"
						 "    IsometricObject:\n"
						 "      atlasPath: " +
								 sheetPath.generic_string() + "\n      building: cottage\n");

	const auto assets = AssetScanner::scanScene(scenePath);
	bool foundSheet = false;
	bool foundTexture = false;
	for (const auto& ref: assets) {
		if (ref.diskPath == sheetPath)
			foundSheet = true;
		if (ref.assetType == AssetType::Texture && ref.diskPath == texPath)
			foundTexture = true;
	}
	EXPECT_TRUE(foundSheet);
	EXPECT_TRUE(foundTexture);
}

TEST_F(AssetScannerTest, ScanSceneWithMissingIsometricSheetLogsWarning) {
	const auto scenePath = m_tempDir / "test.owl";
	writeFile(scenePath, "Scene: test\nEntities:\n"
						 "  - Entity: 1\n"
						 "    IsometricObject:\n"
						 "      atlasPath: /no/such/buildings.json\n");
	std::vector<std::string> warnings;
	(void) AssetScanner::scanScene(scenePath, &warnings);
	EXPECT_FALSE(warnings.empty());
}

TEST_F(AssetScannerTest, ScanSceneWithMissingTilemapLogsWarning) {
	const auto scenePath = m_tempDir / "test.owl";
	writeFile(scenePath, "Scene: test\nEntities:\n"
						 "  - Entity: 1\n"
						 "    Tilemap:\n"
						 "      tilemapPath: /no/such/world.owltilemap\n");
	std::vector<std::string> warnings;
	(void) AssetScanner::scanScene(scenePath, &warnings);
	EXPECT_FALSE(warnings.empty());
}

TEST_F(AssetScannerTest, ScanSceneWithRaycastDoorPacksTilesetAndTexture) {
	const auto texPath = m_tempDir / "textures" / "walls.png";
	writeDummyBinary(texPath);
	const auto tilesetPath = m_tempDir / "walls.owltileset";
	writeFile(tilesetPath, "Tileset: walls\ntexture: \"pat:" + texPath.generic_string() + "\"\ncolumns: 8\nrows: 8\n");
	const auto scenePath = m_tempDir / "test.owl";
	writeFile(scenePath, "Scene: test\nEntities:\n"
						 "  - Entity: 1\n"
						 "    RaycastDoor:\n"
						 "      tilesetPath: " +
								 tilesetPath.generic_string() + "\n      faceTileIndex: 0\n");

	const auto assets = AssetScanner::scanScene(scenePath);
	bool foundTileset = false;
	bool foundTexture = false;
	for (const auto& ref: assets) {
		if (ref.diskPath == tilesetPath)
			foundTileset = true;
		if (ref.assetType == AssetType::Texture && ref.diskPath == texPath)
			foundTexture = true;
	}
	EXPECT_TRUE(foundTileset);
	EXPECT_TRUE(foundTexture);
}

TEST_F(AssetScannerTest, PackWriterProgressCallback) {
	// Verify PackWriter invokes the progress callback per entry.
	owl::data::assets::pack::PackWriter writer;
	const std::vector<uint8_t> data = {1, 2, 3, 4};
	writer.addData(data, "a.bin", AssetType::Other);
	writer.addData(data, "b.bin", AssetType::Other);
	writer.addData(data, "c.bin", AssetType::Other);

	std::vector<std::pair<uint32_t, uint32_t>> progressCalls;
	const auto packPath = m_tempDir / "test.owlpack";
	const bool ok = writer.write(packPath, PackFlags::Default,
								 [&progressCalls](const uint32_t iCurrent, const uint32_t iTotal) -> void {
									 progressCalls.emplace_back(iCurrent, iTotal);
								 });
	EXPECT_TRUE(ok);
	ASSERT_EQ(progressCalls.size(), 3);
	EXPECT_EQ(progressCalls[0], std::make_pair(0u, 3u));
	EXPECT_EQ(progressCalls[1], std::make_pair(1u, 3u));
	EXPECT_EQ(progressCalls[2], std::make_pair(2u, 3u));
}

TEST_F(AssetScannerTest, PackWriterCancelCheck) {
	// Verify PackWriter respects the cancel check.
	owl::data::assets::pack::PackWriter writer;
	const std::vector<uint8_t> data = {1, 2, 3, 4};
	writer.addData(data, "a.bin", AssetType::Other);
	writer.addData(data, "b.bin", AssetType::Other);
	writer.addData(data, "c.bin", AssetType::Other);

	int callCount = 0;
	const auto packPath = m_tempDir / "test_cancel.owlpack";
	const bool ok = writer.write(packPath, PackFlags::Default, {}, [&callCount]() -> bool {
		++callCount;
		return callCount >= 2;// Cancel after 1st entry
	});
	EXPECT_FALSE(ok);// Should fail due to cancellation.
	EXPECT_EQ(callCount, 2);
}

TEST_F(AssetScannerTest, ScanSceneFollowsTheMeshFilesOfAMeshRenderer) {
	const auto gltf = m_tempDir / "meshes" / "crate.gltf";
	writeFile(gltf, R"({"asset": {"version": "2.0"}, "buffers": [{"uri": "crate.bin", "byteLength": 5}, )"
					R"({"uri": "data:application/octet-stream;base64,AAAA", "byteLength": 3}], )"
					R"("images": [{"uri": "crate.png"}, {"uri": "missing.png"}]})");
	writeDummyBinary(m_tempDir / "meshes" / "crate.bin");
	writeDummyBinary(m_tempDir / "meshes" / "crate.png");
	const auto scenePath = m_tempDir / "meshes.owl";
	writeFile(scenePath, "Scene: meshes\nEntities:\n"
						 "  - Entity: 1\n"
						 "    MeshRenderer:\n"
						 "      mesh: \"" +
								 gltf.generic_string() +
								 "\"\n"
								 "  - Entity: 2\n"
								 "    MeshRenderer:\n"
								 "      mesh: \"" +
								 (m_tempDir / "meshes" / "barrel.glb").generic_string() + "\"\n");
	std::vector<std::string> warnings;
	const auto assets = AssetScanner::scanScene(scenePath, &warnings);
	const auto find = [&assets](const std::filesystem::path& iDisk) -> const AssetReference* {
		const auto found = std::ranges::find_if(assets, [&iDisk](const AssetReference& iRef) -> bool {
			return iRef.diskPath.lexically_normal() == iDisk.lexically_normal();
		});
		return found == assets.end() ? nullptr : &*found;
	};
	const auto* mesh = find(gltf);
	ASSERT_NE(mesh, nullptr);
	EXPECT_EQ(mesh->assetType, AssetType::Mesh);
	EXPECT_NE(find(m_tempDir / "meshes" / "crate.bin"), nullptr);
	EXPECT_NE(find(m_tempDir / "meshes" / "crate.png"), nullptr);
	EXPECT_EQ(assets.size(), 4u);// the scene, the glTF and its two external files
	// The missing image of the glTF and the missing GLB are reported.
	EXPECT_EQ(warnings.size(), 2u);
}
