/**
 * @file AndroidApk_test.cpp
 * @author Silmaen
 * @date 23/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include "Project.h"
#include "android/AndroidApk.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace owl;
using namespace owl::nest;
using namespace owl::nest::android;

namespace {

auto u16(const std::vector<uint8_t>& iData, const size_t iOffset) -> uint32_t {
	return static_cast<uint32_t>(iData[iOffset]) | static_cast<uint32_t>(iData[iOffset + 1]) << 8u;
}

auto u32(const std::vector<uint8_t>& iData, const size_t iOffset) -> uint32_t {
	return u16(iData, iOffset) | u16(iData, iOffset + 2) << 16u;
}

auto contains(const std::vector<uint8_t>& iData, const std::string_view iText) -> bool {
	return std::ranges::search(iData, iText).begin() != iData.end();
}

auto readFile(const std::filesystem::path& iFile) -> std::vector<uint8_t> {
	std::ifstream in(iFile, std::ios::binary);
	return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

void writeFile(const std::filesystem::path& iFile, const std::string_view iContent) {
	std::filesystem::create_directories(iFile.parent_path());
	std::ofstream{iFile, std::ios::binary} << iContent;
}

// Central directory entries of a zip: name and local header offset.
auto centralEntries(const std::vector<uint8_t>& iZip) -> std::vector<std::pair<std::string, uint32_t>> {
	std::vector<std::pair<std::string, uint32_t>> entries;
	const size_t end = iZip.size() - 22;
	size_t cursor = u32(iZip, end + 16);
	const auto count = u16(iZip, end + 10);
	for (uint32_t i = 0; i < count; ++i) {
		const auto nameLength = u16(iZip, cursor + 28);
		const auto extraLength = u16(iZip, cursor + 30);
		const auto commentLength = u16(iZip, cursor + 32);
		entries.emplace_back(std::string{reinterpret_cast<const char*>(iZip.data() + cursor + 46), nameLength},
							 u32(iZip, cursor + 42));
		cursor += 46 + nameLength + extraLength + commentLength;
	}
	return entries;
}

auto makeManifest() -> ApkManifest {
	return {.package = "org.owlengine.test",
			.label = "Test Game",
			.versionName = "1.2.3",
			.versionCode = 10203,
			.minSdk = 33,
			.targetSdk = 36,
			.debuggable = true,
			.hasIcon = true};
}

class AndroidApkFixture : public ::testing::Test {
protected:
	void SetUp() override {
		m_root = std::filesystem::temp_directory_path() / "owl_android_apk";
		std::filesystem::remove_all(m_root);
		writeFile(m_root / "game" / "Test.owlpack", "PACK");
		writeFile(m_root / "game" / "runner.yml", "Runner: {}");
		writeFile(m_root / "game" / "export_report.yml", "Report: {}");
		writeFile(m_root / "runner" / "lib" / "arm64-v8a" / "libOwlGameRunner.so", "RUNNER");
		writeFile(m_root / "runner" / "lib" / "arm64-v8a" / "libc++_shared.so", "LIBCXX");
		writeFile(m_root / "icon.png", "\x89PNG fake");
	}

	void TearDown() override { std::filesystem::remove_all(m_root); }

	std::filesystem::path m_root;
};

}// namespace

TEST(AndroidApk, TheManifestIsBinaryXmlWithTheGameValues) {
	const auto xml = makeBinaryManifest(makeManifest());
	ASSERT_GT(xml.size(), 8u);
	EXPECT_EQ(u16(xml, 0), 0x0003u);
	EXPECT_EQ(u32(xml, 4), xml.size());
	// The string pool follows, then the resource map of the android attribute names.
	EXPECT_EQ(u16(xml, 8), 0x0001u);
	const auto pool = u32(xml, 12);
	EXPECT_EQ(u16(xml, 8 + pool), 0x0180u);
	EXPECT_TRUE(contains(xml, "org.owlengine.test"));
	EXPECT_TRUE(contains(xml, "android.app.NativeActivity"));
	EXPECT_TRUE(contains(xml, "OwlGameRunner"));
	EXPECT_TRUE(contains(xml, "android.hardware.vulkan.version"));
	EXPECT_TRUE(contains(xml, "1.2.3"));
}

TEST(AndroidApk, TheIconIsTheOnlyResource) {
	const auto table = makeIconResources("org.owlengine.test");
	EXPECT_EQ(u16(table, 0), 0x0002u);
	EXPECT_EQ(u32(table, 4), table.size());
	EXPECT_EQ(u32(table, 8), 1u);
	EXPECT_TRUE(contains(table, "res/mipmap/icon.png"));
	EXPECT_TRUE(contains(table, "mipmap"));
}

TEST(AndroidApk, TheVersionCodeGrowsWithTheVersion) {
	EXPECT_EQ(getVersionCode("1.2.3"), 10203u);
	EXPECT_EQ(getVersionCode("0.8.0"), 800u);
	EXPECT_EQ(getVersionCode("2"), 20000u);
	EXPECT_EQ(getVersionCode(""), 1u);
	EXPECT_LT(getVersionCode("1.9.9"), getVersionCode("1.10.0"));
}

TEST(AndroidApk, APackageNameIsMadeFromTheProjectName) {
	Project project;
	project.name = "My Great Game!";
	EXPECT_EQ(project.getAndroidPackage(), "org.owlengine.my_great_game");
	project.name = "3D Maze";
	EXPECT_EQ(project.getAndroidPackage(), "org.owlengine.game3d_maze");
	project.androidPackage = "com.studio.maze";
	EXPECT_EQ(project.getAndroidPackage(), "com.studio.maze");
}

TEST_F(AndroidApkFixture, TheApkHoldsTheRunnerTheGameAndASignature) {
	const auto key = SigningKey::loadOrCreateDebug(m_root / "keys" / "debug.p12");
	ASSERT_TRUE(key.has_value());
	EXPECT_FALSE(key->getCertificate().empty());
	const auto apk = m_root / "Test.apk";
	ASSERT_TRUE(writeApk(m_root / "game", m_root / "runner", m_root / "icon.png", makeManifest(), *key, apk));
	const auto content = readFile(apk);
	ASSERT_GT(content.size(), 22u);
	EXPECT_EQ(u32(content, content.size() - 22), 0x06054b50u);

	const auto entries = centralEntries(content);
	std::vector<std::string> names;
	for (const auto& [name, offset]: entries) {
		names.push_back(name);
		if (name == "resources.arsc" || name == "res/mipmap/icon.png") {
			// Stored and aligned: Android maps them in place.
			const auto dataStart = offset + 30 + u16(content, offset + 26) + u16(content, offset + 28);
			EXPECT_EQ(dataStart % 4, 0u) << name;
			EXPECT_EQ(u16(content, offset + 8), 0u) << name;
		}
	}
	for (const auto* expected:
		 {"AndroidManifest.xml", "resources.arsc", "res/mipmap/icon.png", "lib/arm64-v8a/libOwlGameRunner.so",
		  "lib/arm64-v8a/libc++_shared.so", "assets/Test.owlpack", "assets/runner.yml", "assets/owl_assets.txt"})
		EXPECT_NE(std::ranges::find(names, expected), names.end()) << expected;
	EXPECT_EQ(std::ranges::find(names, "assets/export_report.yml"), names.end());

	// The APK Signing Block ends right before the central directory.
	const auto directory = u32(content, content.size() - 22 + 16);
	const std::string_view magic{reinterpret_cast<const char*>(content.data() + directory - 16), 16};
	EXPECT_EQ(magic, "APK Sig Block 42");
	EXPECT_EQ(u32(content, directory), 0x02014b50u);
}

TEST_F(AndroidApkFixture, TheDebugKeyIsKeptAcrossExports) {
	const auto first = SigningKey::loadOrCreateDebug(m_root / "keys" / "debug.p12");
	const auto second = SigningKey::loadOrCreateDebug(m_root / "keys" / "debug.p12");
	ASSERT_TRUE(first.has_value());
	ASSERT_TRUE(second.has_value());
	EXPECT_EQ(first->getCertificate(), second->getCertificate());
	const auto wrong = SigningKey::load(m_root / "keys" / "debug.p12", "not the password");
	ASSERT_FALSE(wrong.has_value());
	EXPECT_EQ(wrong.error(), SigningError::Keystore);
}

TEST_F(AndroidApkFixture, WithoutRunnerNoApkIsWritten) {
	const auto key = SigningKey::loadOrCreateDebug(m_root / "keys" / "debug.p12");
	ASSERT_TRUE(key.has_value());
	const auto written = writeApk(m_root / "game", m_root / "nowhere", {}, makeManifest(), *key, m_root / "Test.apk");
	ASSERT_FALSE(written.has_value());
	EXPECT_EQ(written.error(), ApkError::NoRunner);
	EXPECT_FALSE(exists(m_root / "Test.apk"));
	EXPECT_FALSE(getErrorMessage(ApkError::NoRunner).empty());
	const auto noGame = writeApk(m_root / "nothing", m_root / "runner", {}, makeManifest(), *key, m_root / "Test.apk");
	ASSERT_FALSE(noGame.has_value());
	EXPECT_EQ(noGame.error(), ApkError::NoGame);
}

TEST(ApkSigner, ANonZipIsRefused) {
	const auto key =
			SigningKey::loadOrCreateDebug(std::filesystem::temp_directory_path() / "owl_apk_key" / "debug.p12");
	ASSERT_TRUE(key.has_value());
	const std::vector<uint8_t> notZip(64, 0x42);
	const auto signedApk = signApk(notZip, *key);
	ASSERT_FALSE(signedApk.has_value());
	EXPECT_EQ(signedApk.error(), SigningError::Archive);
	std::filesystem::remove_all(std::filesystem::temp_directory_path() / "owl_apk_key");
}
