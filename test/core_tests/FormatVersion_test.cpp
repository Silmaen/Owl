/**
 * @file FormatVersion_test.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/FormatVersion.h>
#include <core/FormatVersionYaml.h>
#include <core/Log.h>
#include <core/SerializerImpl.h>

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>

using namespace owl;
using namespace owl::core;

namespace {

auto renameOldToMid(const Serializer& ioDocument) -> bool {
	auto& root = ioDocument.getImpl()->document;
	if (!root["old"])
		return false;
	root["mid"] = root["old"].as<int>() * 10;
	root.remove("old");
	return true;
}

auto renameMidToNew(const Serializer& ioDocument) -> bool {
	auto& root = ioDocument.getImpl()->document;
	if (!root["mid"])
		return false;
	root["new"] = root["mid"].as<int>() + 1;
	root.remove("mid");
	return true;
}

auto throwingStep(const Serializer& ioDocument) -> bool {
	if (ioDocument.getImpl()->document["a"])
		throw std::runtime_error("boom");
	return true;
}

constexpr std::array<MigrationStep, 2> g_fakeMigrations{renameOldToMid, renameMidToNew};
constexpr DocumentFormat g_fakeFormat{.name = "Fake", .migrations = g_fakeMigrations};
constexpr std::array<MigrationStep, 0> g_noMigrations{};
constexpr DocumentFormat g_currentOnly{.name = "Plain", .migrations = g_noMigrations};
constexpr std::array<MigrationStep, 1> g_throwingMigrations{throwingStep};
constexpr DocumentFormat g_throwingFormat{.name = "Throwing", .migrations = g_throwingMigrations};

auto upgrade(const DocumentFormat& iFormat, const std::string& iYaml, YAML::Node& oRoot)
		-> expected<uint32_t, FormatError> {
	oRoot = YAML::Load(iYaml);
	return upgradeYamlDocument(iFormat, oRoot, "test");
}

class FormatVersionTest : public ::testing::Test {
protected:
	void SetUp() override { Log::init(Log::Level::Off); }
	void TearDown() override { Log::invalidate(); }
};

}// namespace

TEST_F(FormatVersionTest, CurrentVersionCountsMigrations) {
	EXPECT_EQ(g_currentOnly.currentVersion(), 1u);
	EXPECT_EQ(g_fakeFormat.currentVersion(), 3u);
}

TEST_F(FormatVersionTest, MissingVersionReadsAsVersionOne) {
	const Serializer document;
	document.getImpl()->document = YAML::Load("Scene: x\n");
	const auto version = readFormatVersion(document);
	ASSERT_TRUE(version);
	EXPECT_EQ(*version, g_UnversionedFormat);
	document.getImpl()->document = YAML::Load("- a\n- b\n");
	EXPECT_EQ(readFormatVersion(document).value(), g_UnversionedFormat);
}

TEST_F(FormatVersionTest, InvalidVersionIsRefused) {
	for (const auto* yaml: {"FormatVersion: abc\n", "FormatVersion: 0\n", "FormatVersion: -2\n", "FormatVersion: 1.5\n",
							"FormatVersion: [1]\n", "FormatVersion: 99999999999\n"}) {
		YAML::Node root;
		const auto version = upgrade(g_currentOnly, yaml, root);
		ASSERT_FALSE(version) << yaml;
		EXPECT_EQ(version.error(), FormatError::InvalidVersion) << yaml;
	}
}

TEST_F(FormatVersionTest, NewerVersionIsRefused) {
	YAML::Node root;
	const auto version = upgrade(g_fakeFormat, "FormatVersion: 4\nnew: 1\n", root);
	ASSERT_FALSE(version);
	EXPECT_EQ(version.error(), FormatError::NewerVersion);
	EXPECT_EQ(root["FormatVersion"].as<int>(), 4);
	EXPECT_FALSE(describe(FormatError::NewerVersion).empty());
}

TEST_F(FormatVersionTest, FakeMigrationChainRunsFromVersionOne) {
	YAML::Node root;
	const auto version = upgrade(g_fakeFormat, "old: 4\n", root);
	ASSERT_TRUE(version);
	EXPECT_EQ(*version, 1u);
	EXPECT_FALSE(root["old"]);
	EXPECT_FALSE(root["mid"]);
	EXPECT_EQ(root["new"].as<int>(), 41);
	EXPECT_EQ(root["FormatVersion"].as<uint32_t>(), 3u);
}

TEST_F(FormatVersionTest, FakeMigrationChainStartsAtTheFileVersion) {
	YAML::Node root;
	const auto version = upgrade(g_fakeFormat, "FormatVersion: 2\nmid: 7\n", root);
	ASSERT_TRUE(version);
	EXPECT_EQ(*version, 2u);
	EXPECT_EQ(root["new"].as<int>(), 8);
	EXPECT_EQ(root["FormatVersion"].as<uint32_t>(), 3u);
}

TEST_F(FormatVersionTest, CurrentDocumentIsLeftAsIs) {
	YAML::Node root;
	const auto version = upgrade(g_fakeFormat, "FormatVersion: 3\nnew: 5\n", root);
	ASSERT_TRUE(version);
	EXPECT_EQ(*version, 3u);
	EXPECT_EQ(root["new"].as<int>(), 5);
}

TEST_F(FormatVersionTest, FailingMigrationIsReported) {
	YAML::Node root;
	const auto version = upgrade(g_fakeFormat, "unrelated: 1\n", root);
	ASSERT_FALSE(version);
	EXPECT_EQ(version.error(), FormatError::MigrationFailed);
	const auto thrown = upgrade(g_throwingFormat, "a: 1\n", root);
	ASSERT_FALSE(thrown);
	EXPECT_EQ(thrown.error(), FormatError::MigrationFailed);
}

TEST_F(FormatVersionTest, SerializerVariantMigratesInPlace) {
	const Serializer document;
	document.getImpl()->document = YAML::Load("old: 1\n");
	const auto version = upgradeDocument(g_fakeFormat, document, "test");
	ASSERT_TRUE(version);
	EXPECT_EQ(document.getImpl()->document["new"].as<int>(), 11);
	EXPECT_EQ(readFormatVersion(document).value(), 3u);
}

TEST_F(FormatVersionTest, TextVariantRewritesOnlyWhenMigrated) {
	std::string current = "FormatVersion: 3\nnew: 2\n";
	const std::string before = current;
	ASSERT_TRUE(upgradeDocumentText(g_fakeFormat, current, "test"));
	EXPECT_EQ(current, before);

	std::string legacy = "old: 2\n";
	ASSERT_TRUE(upgradeDocumentText(g_fakeFormat, legacy, "test"));
	const auto root = YAML::Load(legacy);
	EXPECT_EQ(root["new"].as<int>(), 21);
	EXPECT_EQ(root["FormatVersion"].as<int>(), 3);

	std::string broken = "a: [unterminated";
	EXPECT_FALSE(upgradeDocumentText(g_fakeFormat, broken, "test"));
}

TEST_F(FormatVersionTest, EmitWritesTheCurrentVersion) {
	YAML::Emitter out;
	out << YAML::BeginMap;
	emitFormatVersion(out, g_fakeFormat);
	out << YAML::EndMap;
	EXPECT_EQ(YAML::Load(out.c_str())["FormatVersion"].as<uint32_t>(), 3u);
}
