/**
 * @file HelpIndex_test.cpp
 * @author Silmaen
 * @date 28/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 *
 * Sanity-checks that the bundled help index produced by `cmake/HelpAssets.cmake`
 * exists, parses, and contains the canonical landing pages used by `HelpPanel`.
 */

#include "testHelper.h"

#include <gtest/gtest.h>
#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {
[[nodiscard]] auto helpRoot() -> fs::path { return owl::test::getRootPath() / "engine_assets" / "help"; }

[[nodiscard]] auto readBundledFile(const fs::path& iPath) -> std::string {
	const std::ifstream stream(iPath, std::ios::binary);
	if (!stream)
		return {};
	std::stringstream buf;
	buf << stream.rdbuf();
	auto content = buf.str();
	// Normalise CRLF → LF so the substring assertions below stay portable: CMake's `file(WRITE)`
	// uses platform-default line endings, so on Windows `\n` searches in the assertions would
	// miss `\r\n` separators in the bundled markdown.
	std::erase(content, '\r');
	return content;
}

}// namespace

TEST(HelpIndex, BundledIndexExists) {
	const auto root = helpRoot();
	ASSERT_TRUE(fs::exists(root)) << "Help bundle missing — did CMake configure run?";
	ASSERT_TRUE(fs::exists(root / "index.yml"));
}

TEST(HelpIndex, IndexHasCoreLandingPages) {
	const auto indexPath = helpRoot() / "index.yml";
	ASSERT_TRUE(fs::exists(indexPath));
	YAML::Node root;
	ASSERT_NO_THROW(root = YAML::LoadFile(indexPath.string()));
	ASSERT_TRUE(root["Help"]);
	const auto pages = root["Help"]["Pages"];
	ASSERT_TRUE(pages);
	ASSERT_TRUE(pages.IsSequence());
	std::set<std::string> ids;
	for (const auto& entry: pages) {
		const auto id = entry["id"].as<std::string>("");
		const auto path = entry["path"].as<std::string>("");
		EXPECT_FALSE(id.empty());
		EXPECT_FALSE(path.empty());
		// Each indexed page must be physically present in the bundle.
		if (!path.empty()) {
			EXPECT_TRUE(fs::exists(helpRoot() / path)) << "missing bundled page: " << path;
		}
		ids.insert(id);
	}
	// Pages used by HelpPanel's default routing or by F1 contextual help.
	for (const char* required: {"editor", "scene", "scripting", "renderer", "physics", "sound"})
		EXPECT_TRUE(ids.contains(required)) << "expected page '" << required << "' missing from index";
	// At least the landing page bundled with this PR.
	EXPECT_TRUE(ids.contains("getting_started"));
}

TEST(HelpIndex, BundledPagesHaveDoxygenSyntaxScrubbed) {
	const auto root = helpRoot();
	const auto editor = readBundledFile(root / "editor.md");
	ASSERT_FALSE(editor.empty());
	// Heading-trailing Doxygen anchors must be stripped.
	EXPECT_EQ(editor.find("# Editor (Owl Nest) {#page-editor}"), std::string::npos);
	// `[TOC]` lines must be dropped (the source file has one).
	EXPECT_EQ(editor.find("\n[TOC]\n"), std::string::npos);
	// First H1 must be the clean form.
	EXPECT_NE(editor.find("# Editor (Owl Nest)\n"), std::string::npos);
}

TEST(HelpIndex, BundledPagesRewriteImageReferences) {
	const auto root = helpRoot();
	const auto scene = readBundledFile(root / "scene.md");
	ASSERT_FALSE(scene.empty());
	// Image references in source pages use `../images/foo.svg`; the bundled copy must rewrite them.
	EXPECT_EQ(scene.find("(../images/"), std::string::npos);
	EXPECT_NE(scene.find("(images/"), std::string::npos);
	// And the actual image files must be present alongside the page.
	EXPECT_TRUE(fs::exists(root / "images" / "scene_lifecycle.svg"));
	EXPECT_TRUE(fs::exists(root / "images" / "component_overview.svg"));
}

TEST(HelpIndex, BundledPagesPreserveExternalTextLinks) {
	const auto root = helpRoot();
	const auto readme = readBundledFile(root / "README.md");
	ASSERT_FALSE(readme.empty());
	// External text links (`[text](https://...)`) must be preserved; the renderer routes them
	// to the user's default browser. Image refs (`![alt](https://...)`) are downloaded at
	// configure time and rewritten to local paths — see `BadgesAreFetchedAndCachedLocally`.
	EXPECT_NE(readme.find("](https://owl.argawaen.net)"), std::string::npos);
}

TEST(HelpIndex, BadgesAreFetchedAndCachedLocally) {
	const auto root = helpRoot();
	const auto readme = readBundledFile(root / "README.md");
	ASSERT_FALSE(readme.empty());

	// Badge fetching happens at configure time over HTTPS (`img.shields.io`), one badge at a time.
	// A badge that downloads is rewritten in the README to `images/badges/<sha>.svg`; one that fails
	// (offline / sandboxed CI runner, rate-limiting, timeout) keeps its original shields.io URL. The
	// fetch is therefore partial in general, so we must NOT assert that every shields.io URL is gone —
	// that all-or-nothing check is what made this test flaky. Instead, collect the badges that WERE
	// rewritten and verify each resolves to a real on-disk SVG (the fetch + cache + rewrite pipeline).
	constexpr std::string_view marker = "](images/badges/";
	std::vector<std::string> rewritten;
	for (size_t pos = readme.find(marker); pos != std::string::npos; pos = readme.find(marker, pos + 1)) {
		const size_t start = pos + 2;// skip "]("
		const size_t end = readme.find(')', start);
		if (end == std::string::npos)
			break;
		rewritten.push_back(readme.substr(start, end - start));
	}
	if (rewritten.empty())
		GTEST_SKIP() << "no badges were fetched at configure time (offline build agent?)";

	for (const auto& relative: rewritten) {
		const auto body = readBundledFile(root / relative);
		EXPECT_FALSE(body.empty()) << "rewritten badge missing on disk: " << relative;
		EXPECT_NE(body.find("<svg"), std::string::npos) << "cached badge is not an SVG: " << relative;
	}
}

TEST(HelpIndex, ReadmeLogoCopiedToImages) {
	const auto root = helpRoot();
	const auto readme = readBundledFile(root / "README.md");
	ASSERT_FALSE(readme.empty());
	// `engine_assets/logo/logo_owl.png` (project-relative path used by the README) should be
	// rewritten to `images/logo_owl.png` and the file copied next to the bundled images.
	EXPECT_EQ(readme.find("engine_assets/logo/"), std::string::npos);
	EXPECT_NE(readme.find("](images/logo_owl.png)"), std::string::npos);
	EXPECT_TRUE(fs::exists(root / "images" / "logo_owl.png"));
}
