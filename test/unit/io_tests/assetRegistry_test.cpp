/**
 * @file assetRegistry_test.cpp
 * @author Silmaen
 * @date 26/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <data/assets/AssetMeta.h>
#include <data/assets/AssetReference.h>
#include <data/assets/AssetRegistry.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <tuple>
#include <vector>

using namespace owl::data::assets;

namespace {

void writeFile(const std::filesystem::path& iPath, const std::string& iContent) {
	std::filesystem::create_directories(iPath.parent_path());
	std::ofstream file(iPath, std::ios::binary);
	file << iContent;
}

class AssetRegistryTest : public ::testing::Test {
protected:
	void SetUp() override {
		m_root = std::filesystem::temp_directory_path() / "owl_asset_registry";
		std::filesystem::remove_all(m_root);
		writeFile(m_root / "textures" / "hero.png", "png");
		writeFile(m_root / "scripts" / "hero.lua", "-- lua");
		writeFile(m_root / "notes.txt", "not an asset");
		writeFile(m_root / ".owl" / "cache" / "hidden.png", "hidden");
	}

	void TearDown() override { std::filesystem::remove_all(m_root); }

	std::filesystem::path m_root;
};

}// namespace

TEST(AssetReference, GuidTextRoundTrip) {
	const AssetGuid guid{0x0123456789abcdefULL};
	EXPECT_EQ(formatGuid(guid), "0123456789abcdef");
	EXPECT_EQ(parseGuid("0123456789ABCDEF"), guid);
	EXPECT_FALSE(parseGuid("0123"));
	EXPECT_FALSE(parseGuid("0123456789abcdeg"));
}

TEST(AssetReference, ParseGuidAndPlainForms) {
	const AssetGuid guid{42};
	const auto reference = formatReference(guid, "textures/hero.png");
	EXPECT_EQ(reference, "gid:000000000000002a:textures/hero.png");
	const auto parsed = parseReference(reference);
	ASSERT_TRUE(parsed.guid);
	EXPECT_EQ(*parsed.guid, guid);
	EXPECT_EQ(parsed.path, "textures/hero.png");

	const auto plain = parseReference("textures/hero.png");
	EXPECT_FALSE(plain.guid);
	EXPECT_EQ(plain.path, "textures/hero.png");

	const auto malformed = parseReference("gid:nothex:x.png");
	EXPECT_FALSE(malformed.guid);
	EXPECT_EQ(malformed.path, "gid:nothex:x.png");
}

TEST(AssetMeta, ImporterFromExtension) {
	EXPECT_EQ(importerOf("a/b.PNG"), Importer::Texture);
	EXPECT_EQ(importerOf("a.glb"), Importer::Mesh);
	EXPECT_EQ(importerOf("a.owlmat"), Importer::Document);
	EXPECT_EQ(importerOf("a.lua"), Importer::Script);
	EXPECT_FALSE(importerOf("a.owlmeta"));
	EXPECT_FALSE(importerOf("a.bin"));
	EXPECT_EQ(metaPathOf("a/b.png"), std::filesystem::path("a/b.png.owlmeta"));
}

TEST(AssetMeta, TextRoundTrip) {
	const AssetMeta meta{.guid = AssetGuid{7},
						 .importer = Importer::Texture,
						 .texture = {.nearest = true, .compression = TextureCompression::Uastc, .mips = false},
						 .mesh = {}};
	const auto text = serializeMeta(meta);
	EXPECT_NE(text.find("FormatVersion"), std::string::npos);
	const auto read = parseMeta(text, "test");
	ASSERT_TRUE(read);
	EXPECT_EQ(*read, meta);
}

TEST(AssetMeta, RejectsBadSidecars) {
	EXPECT_EQ(parseMeta("Guid: [", "bad").error(), MetaError::Unreadable);
	EXPECT_EQ(parseMeta("Guid: 12", "short").error(), MetaError::InvalidGuid);
	EXPECT_EQ(parseMeta("FormatVersion: 99\nGuid: 0000000000000001", "newer").error(), MetaError::NewerVersion);
	const auto unknownImporter = parseMeta("Guid: 0000000000000001\nImporter: Alien", "importer");
	ASSERT_TRUE(unknownImporter);
	EXPECT_EQ(unknownImporter->importer, Importer::Copy);
}

TEST_F(AssetRegistryTest, EditableScanCreatesSidecars) {
	AssetRegistry registry;
	const auto report = registry.addDirectory(m_root, true);
	EXPECT_EQ(report.assets, 2u);
	EXPECT_EQ(report.created, 2u);
	EXPECT_TRUE(std::filesystem::exists(m_root / "textures" / "hero.png.owlmeta"));
	EXPECT_FALSE(std::filesystem::exists(m_root / "notes.txt.owlmeta"));
	EXPECT_FALSE(std::filesystem::exists(m_root / ".owl" / "cache" / "hidden.png.owlmeta"));

	const auto guid = registry.findGuid("textures/hero.png");
	ASSERT_TRUE(guid);
	EXPECT_EQ(registry.findGuid((m_root / "textures" / "hero.png").string()), guid);
	EXPECT_EQ(registry.find(*guid)->meta.importer, Importer::Texture);

	AssetRegistry again;
	const auto second = again.addDirectory(m_root, true);
	EXPECT_EQ(second.created, 0u);
	EXPECT_EQ(again.findGuid("textures/hero.png"), guid);
}

TEST_F(AssetRegistryTest, ReadOnlyScanWritesNothing) {
	AssetRegistry registry;
	const auto report = registry.addDirectory(m_root, false);
	EXPECT_EQ(report.assets, 0u);
	EXPECT_FALSE(std::filesystem::exists(m_root / "textures" / "hero.png.owlmeta"));
	EXPECT_EQ(registry.toAssetReference("textures/hero.png"), "textures/hero.png");
}

TEST_F(AssetRegistryTest, ReferencesFollowARename) {
	AssetRegistry registry;
	std::ignore = registry.addDirectory(m_root, true);
	const auto reference = registry.toAssetReference("textures/hero.png");
	ASSERT_TRUE(reference.starts_with("gid:"));
	EXPECT_TRUE(reference.ends_with(":textures/hero.png"));

	ASSERT_TRUE(registry.moveFile(m_root / "textures" / "hero.png", m_root / "sprites" / "player.png"));
	EXPECT_TRUE(std::filesystem::exists(m_root / "sprites" / "player.png.owlmeta"));
	EXPECT_FALSE(std::filesystem::exists(m_root / "textures" / "hero.png.owlmeta"));
	EXPECT_EQ(registry.toAssetPath(reference), "sprites/player.png");

	ASSERT_TRUE(registry.moveFile(m_root / "sprites", m_root / "art"));
	EXPECT_EQ(registry.toAssetPath(reference), "art/player.png");

	AssetRegistry fresh;
	std::ignore = fresh.addDirectory(m_root, true);
	EXPECT_EQ(fresh.toAssetPath(reference), "art/player.png");
}

TEST_F(AssetRegistryTest, UnknownGuidFallsBackToHint) {
	const AssetRegistry registry;
	EXPECT_EQ(registry.toAssetPath("gid:00000000000000ff:textures/hero.png"), "textures/hero.png");
	EXPECT_EQ(registry.toAssetPath("textures/hero.png"), "textures/hero.png");
}

TEST_F(AssetRegistryTest, CopyGetsNewGuidAndSettings) {
	AssetRegistry registry;
	std::ignore = registry.addDirectory(m_root, true);
	const auto original = registry.findGuid("textures/hero.png");
	ASSERT_TRUE(original);
	auto meta = registry.find(*original)->meta;
	meta.texture.nearest = true;
	ASSERT_TRUE(registry.setMeta(meta));

	const auto copy = registry.copyFile(m_root / "textures" / "hero.png", m_root / "textures" / "hero2.png");
	ASSERT_TRUE(copy);
	EXPECT_NE(*copy, *original);
	EXPECT_TRUE(registry.find(*copy)->meta.texture.nearest);
	EXPECT_EQ(registry.findGuid("textures/hero2.png"), copy);
}

TEST_F(AssetRegistryTest, DuplicatedSidecarIsReassignedOnTheCopy) {
	{
		AssetRegistry registry;
		std::ignore = registry.addDirectory(m_root, true);
	}
	const auto originalMeta = readMeta(m_root / "textures" / "hero.png.owlmeta");
	ASSERT_TRUE(originalMeta);
	std::filesystem::copy_file(m_root / "textures" / "hero.png", m_root / "textures" / "copy.png");
	std::filesystem::copy_file(m_root / "textures" / "hero.png.owlmeta", m_root / "textures" / "copy.png.owlmeta");
	std::filesystem::last_write_time(m_root / "textures" / "copy.png",
									 std::filesystem::last_write_time(m_root / "textures" / "hero.png") +
											 std::chrono::seconds{10});

	AssetRegistry registry;
	const auto report = registry.addDirectory(m_root, true);
	EXPECT_EQ(report.reassigned, 1u);
	EXPECT_EQ(registry.findGuid("textures/hero.png"), originalMeta->guid);
	const auto copyGuid = registry.findGuid("textures/copy.png");
	ASSERT_TRUE(copyGuid);
	EXPECT_NE(*copyGuid, originalMeta->guid);
}

TEST_F(AssetRegistryTest, DeleteRemovesSidecarAndOrphansAreCounted) {
	AssetRegistry registry;
	std::ignore = registry.addDirectory(m_root, true);
	const auto guid = registry.findGuid("scripts/hero.lua");
	ASSERT_TRUE(registry.deleteFile(m_root / "scripts" / "hero.lua"));
	EXPECT_FALSE(std::filesystem::exists(m_root / "scripts" / "hero.lua.owlmeta"));
	EXPECT_FALSE(registry.find(*guid));

	std::filesystem::remove(m_root / "textures" / "hero.png");
	AssetRegistry fresh;
	EXPECT_EQ(fresh.addDirectory(m_root, true).orphans, 1u);
}

TEST_F(AssetRegistryTest, PackIndexRoundTrip) {
	AssetRegistry registry;
	std::ignore = registry.addDirectory(m_root, true);
	const auto guid = registry.findGuid("textures/hero.png");
	auto meta = registry.find(*guid)->meta;
	meta.texture.nearest = true;
	ASSERT_TRUE(registry.setMeta(meta));
	const auto index = registry.serializeIndex({"textures/hero.png", "scripts/hero.lua", "unknown.png"});

	AssetRegistry runner;
	EXPECT_EQ(runner.loadIndex(index), 2u);
	EXPECT_EQ(runner.toAssetPath(formatReference(*guid, "old/path.png")), "textures/hero.png");
	EXPECT_TRUE(runner.find(*guid)->meta.texture.nearest);
	EXPECT_FALSE(runner.find(*registry.findGuid("scripts/hero.lua"))->meta.texture.nearest);
	EXPECT_EQ(runner.loadIndex("not: [yaml"), 0u);
}

TEST_F(AssetRegistryTest, NewFileIsRegistered) {
	AssetRegistry registry;
	std::ignore = registry.addDirectory(m_root, true);
	writeFile(m_root / "sounds" / "jump.wav", "wav");
	const auto guid = registry.registerFile(m_root / "sounds" / "jump.wav");
	ASSERT_TRUE(guid);
	EXPECT_EQ(registry.registerFile(m_root / "sounds" / "jump.wav"), guid);
	EXPECT_FALSE(registry.registerFile(m_root / "notes.txt"));
	registry.forgetFile(m_root / "sounds" / "jump.wav");
	EXPECT_FALSE(registry.find(*guid));
	registry.removeDirectory(m_root);
	EXPECT_EQ(registry.size(), 0u);
}

TEST_F(AssetRegistryTest, RewriteReferencesTouchesOnlyReferenceKeys) {
	AssetRegistry registry;
	std::ignore = registry.addDirectory(m_root, true);
	const auto texture = formatReference(*registry.findGuid("textures/hero.png"), "textures/hero.png");
	const auto script = formatReference(*registry.findGuid("scripts/hero.lua"), "scripts/hero.lua");
	const std::string text = std::format("# comment\n"
										 "Entities:\n"
										 "  - Entity: 1\n"
										 "    SpriteRenderer:\n"
										 "      texture: nam:textures/hero.png\n"
										 "    LuaScript:\n"
										 "      scriptPath: 'scripts/hero.lua'\n"
										 "      properties:\n"
										 "        - name: icon\n"
										 "          value: textures/hero.png\n"
										 "    Other:\n"
										 "      mesh: missing.glb\n"
										 "      tilesetPath: {}\n"
										 "...\n"
										 "texture: textures/hero.png\n",
										 texture);
	const auto rewritten = registry.rewriteReferences(text);
	EXPECT_NE(rewritten.find("      texture: " + texture + "\n"), std::string::npos);
	EXPECT_NE(rewritten.find("      scriptPath: '" + script + "'\n"), std::string::npos);
	EXPECT_NE(rewritten.find("          value: textures/hero.png\n"), std::string::npos);
	EXPECT_NE(rewritten.find("      mesh: missing.glb\n"), std::string::npos);
	EXPECT_TRUE(rewritten.starts_with("# comment\nEntities:\n"));
	EXPECT_TRUE(rewritten.ends_with("...\ntexture: textures/hero.png\n"));
	EXPECT_EQ(registry.rewriteReferences(rewritten), rewritten);
	EXPECT_EQ(registry.getEditableDirectories().size(), 1u);
}
