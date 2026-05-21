/**
 * @file assetScanner_test.cpp
 * @author Silmaen
 * @date 17/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <io/pack/AssetScanner.h>
#include <io/pack/PackWriter.h>

#include <fstream>
#include <gtest/gtest.h>

using namespace owl::io::pack;

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

TEST_F(AssetScannerTest, PackWriterProgressCallback) {
	// Verify PackWriter invokes the progress callback per entry.
	owl::io::pack::PackWriter writer;
	const std::vector<uint8_t> data = {1, 2, 3, 4};
	writer.addData(data, "a.bin", AssetType::Other);
	writer.addData(data, "b.bin", AssetType::Other);
	writer.addData(data, "c.bin", AssetType::Other);

	std::vector<std::pair<uint32_t, uint32_t>> progressCalls;
	const auto packPath = m_tempDir / "test.owlpack";
	const bool ok = writer.write(packPath, PackFlags::Default,
								 [&progressCalls](const uint32_t iCurrent, const uint32_t iTotal) {
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
	owl::io::pack::PackWriter writer;
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
