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

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

namespace {
[[nodiscard]] auto helpRoot() -> fs::path { return OWL_HELP_BUILD_DIR; }

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
	// The start page of the panel and the tour of the File screen.
	EXPECT_TRUE(ids.contains("nest_help"));
	EXPECT_TRUE(ids.contains("getting_started"));
}

TEST(HelpIndex, BundledPagesHaveDoxygenSyntaxScrubbed) {
	const auto root = helpRoot();
	const auto editor = readBundledFile(root / "editor.md");
	ASSERT_FALSE(editor.empty());
	// `[TOC]` lines must be dropped (the source file has one).
	EXPECT_EQ(editor.find("\n[TOC]\n"), std::string::npos);
	// Heading anchors stay: the renderer takes them as the link targets of their headings.
	EXPECT_NE(editor.find("# The Owl Nest Window {#page-editor}\n"), std::string::npos);
}

TEST(HelpIndex, BundledPagesRewriteImageReferences) {
	const auto root = helpRoot();
	const auto scene = readBundledFile(root / "scene.md");
	ASSERT_FALSE(scene.empty());
	// Image references in source pages use `../images/foo.svg`; the bundled copy must rewrite them.
	EXPECT_EQ(scene.find("(../images/"), std::string::npos);
	EXPECT_NE(scene.find("(images/"), std::string::npos);
	// And every image the page references (mermaid diagrams included) must be present alongside it.
	const std::regex imageRef{R"(\]\((images/[^)]+)\))"};
	for (auto it = std::sregex_iterator(scene.begin(), scene.end(), imageRef); it != std::sregex_iterator(); ++it)
		EXPECT_TRUE(fs::exists(root / (*it)[1].str())) << (*it)[1].str();
}

TEST(HelpIndex, BundledPagesPreserveExternalTextLinks) {
	const auto root = helpRoot();
	const auto readme = readBundledFile(root / "README.md");
	ASSERT_FALSE(readme.empty());
	// External text links (`[text](https://...)`) must be preserved; the renderer routes them
	// to the user's default browser.
	EXPECT_NE(readme.find("](https://owl.argawaen.net)"), std::string::npos);
}

TEST(HelpIndex, BadgesBecomeColouredTags) {
	const auto roadmap = readBundledFile(helpRoot() / "roadmap.md");
	ASSERT_FALSE(roadmap.empty());
	// The status badges are `badge:` images the renderer draws: nothing is fetched from the network.
	EXPECT_EQ(roadmap.find("img.shields.io"), std::string::npos);
	EXPECT_EQ(roadmap.find("![Done][done]"), std::string::npos);
	EXPECT_NE(roadmap.find("![Done](badge:2ea043)"), std::string::npos);
}

TEST(HelpIndex, ReadmeLogoCopiedToImages) {
	const auto root = helpRoot();
	const auto readme = readBundledFile(root / "README.md");
	ASSERT_FALSE(readme.empty());
	// `engine_assets/logo/logo_owl.png` (project-relative path used by the README) should be
	// rewritten to `images/logo_owl.png` and the file copied next to the bundled images.
	EXPECT_EQ(readme.find("engine_assets/logo/"), std::string::npos);
	EXPECT_NE(readme.find("src=\"images/logo_owl.png\""), std::string::npos);
	EXPECT_TRUE(fs::exists(root / "images" / "logo_owl.png"));
}

TEST(HelpIndex, MermaidFencesAreReplacedByRenderedSvg) {
	const auto root = helpRoot();
	const std::regex reference{R"(\]\((images/mermaid/[0-9a-f]+\.svg)\))"};
	size_t references = 0;
	for (const auto& entry: fs::directory_iterator(root)) {
		if (entry.path().extension() != ".md")
			continue;
		const auto page = readBundledFile(entry.path());
		// A fence opens a line; prose may still mention the word between backticks.
		EXPECT_FALSE(std::regex_search(page, std::regex{R"((^|\n)[ \t]*```mermaid)"}))
				<< "unrendered mermaid fence in " << entry.path();
		for (std::sregex_iterator it(page.begin(), page.end(), reference), end; it != end; ++it) {
			++references;
			const auto svg = readBundledFile(root / (*it)[1].str());
			EXPECT_NE(svg.find("<svg"), std::string::npos) << "missing diagram " << (*it)[1].str();
			EXPECT_EQ(svg.find("foreignObject"), std::string::npos);
		}
	}
	EXPECT_GT(references, 0u) << "no mermaid diagram was bundled";
}
