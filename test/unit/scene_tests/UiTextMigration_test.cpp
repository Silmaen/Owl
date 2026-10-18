/**
 * @file UiTextMigration_test.cpp
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <core/YamlNode.h>
#include <scene/Entity.h>
#include <scene/EntityLinkMigration.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/component/UiText.h>

#include <cstdint>
#include <format>
#include <string>
#include <vector>

using namespace owl;
using namespace owl::scene;
using data::fonts::HorizontalAlignment;
using data::fonts::TextFit;
using data::fonts::VerticalAlignment;

namespace {

class UiTextMigrationTest : public ::testing::Test {
protected:
	void SetUp() override { core::Log::init(core::Log::Level::Off); }
	void TearDown() override { core::Log::invalidate(); }
};

auto load(const std::string& iYaml) -> shared<Scene> {
	auto scene = mkShared<Scene>();
	const std::vector<uint8_t> bytes(iYaml.begin(), iYaml.end());
	if (!SceneSerializer(scene).deserializeFromBuffer(bytes, "test"))
		return nullptr;
	return scene;
}

auto firstUiText(const Scene& iScene) -> const component::UiText* {
	for (const auto& entity: iScene.getAllEntities())
		if (entity.hasComponent<component::UiText>())
			return &entity.getComponent<component::UiText>();
	return nullptr;
}

constexpr auto g_legacyScene = "Scene: legacy\nFormatVersion: 2\nEntities:\n"
							   "  - Entity: 21\n    Tag:\n      tag: Label\n"
							   "    UiText: {text: Score, fontSize: 24, alignment: Left, kerning: 0, lineSpacing: 0}\n";

}// namespace

TEST_F(UiTextMigrationTest, LegacyUiTextKeepsItsCentredLayout) {
	const auto scene = load(g_legacyScene);
	ASSERT_NE(scene, nullptr);
	const auto* text = firstUiText(*scene);
	ASSERT_NE(text, nullptr);
	EXPECT_EQ(text->text, "Score");
	EXPECT_NEAR(text->fontSize, 24.f, 1e-4f);
	EXPECT_EQ(text->horizontalAlignment, HorizontalAlignment::Center);
	EXPECT_EQ(text->verticalAlignment, VerticalAlignment::Middle);
	EXPECT_FALSE(text->wrap);
	EXPECT_EQ(text->fit, TextFit::Overflow);
	const auto saved = SceneSerializer(scene).serializeToString();
	EXPECT_NE(saved.find(std::format("FormatVersion: {}", SceneSerializer::format().currentVersion())),
			  std::string::npos);
	EXPECT_NE(saved.find("horizontalAlignment: Center"), std::string::npos);
	EXPECT_EQ(saved.find("alignment: Left"), std::string::npos);
}

TEST_F(UiTextMigrationTest, CurrentUiTextKeepsItsLayout) {
	const auto scene = load("Scene: current\nFormatVersion: 3\nEntities:\n  - Entity: 22\n    UiText: {text: Hi, "
							"horizontalAlignment: Right, verticalAlignment: Baseline, wrap: true, fit: Shrink, "
							"minFontSize: 6}\n");
	ASSERT_NE(scene, nullptr);
	const auto* text = firstUiText(*scene);
	ASSERT_NE(text, nullptr);
	EXPECT_EQ(text->horizontalAlignment, HorizontalAlignment::Right);
	EXPECT_EQ(text->verticalAlignment, VerticalAlignment::Baseline);
	EXPECT_TRUE(text->wrap);
	EXPECT_EQ(text->fit, TextFit::Shrink);
	EXPECT_NEAR(text->minFontSize, 6.f, 1e-4f);
}

TEST_F(UiTextMigrationTest, OnlyLegacyUiTextNeedsTheMigration) {
	const auto canRead = [](const std::string& iYaml) -> bool {
		const core::YamlDocument document{iYaml};
		return canReadWithoutMigration(document.getRoot(), SceneSerializer::format());
	};
	EXPECT_FALSE(canRead(g_legacyScene));
	EXPECT_TRUE(canRead("Scene: a\nFormatVersion: 2\nEntities:\n  - Entity: 1\n    UiText: {text: a, "
						"horizontalAlignment: Left}\n"));
	EXPECT_TRUE(canRead("Scene: a\nFormatVersion: 3\nEntities:\n  - Entity: 1\n    UiText: {text: a}\n"));
}

TEST_F(UiTextMigrationTest, LayoutFollowsTheComponent) {
	component::UiText text;
	text.fontSize = 20.f;
	text.horizontalAlignment = HorizontalAlignment::Right;
	text.verticalAlignment = VerticalAlignment::Top;
	text.wrap = true;
	text.fit = TextFit::Clip;
	text.minFontSize = 9.f;
	text.kerning = 0.1f;
	text.lineSpacing = 0.2f;
	const auto layout = text.getLayout({300.f, 40.f});
	EXPECT_NEAR(layout.fontSize, 20.f, 1e-4f);
	EXPECT_NEAR(layout.boxSize.x(), 300.f, 1e-4f);
	EXPECT_NEAR(layout.boxSize.y(), 40.f, 1e-4f);
	EXPECT_EQ(layout.horizontal, HorizontalAlignment::Right);
	EXPECT_EQ(layout.vertical, VerticalAlignment::Top);
	EXPECT_TRUE(layout.wrap);
	EXPECT_EQ(layout.fit, TextFit::Clip);
	EXPECT_NEAR(layout.minFontSize, 9.f, 1e-4f);
	EXPECT_NEAR(layout.kerning, 0.1f, 1e-4f);
	EXPECT_NEAR(layout.lineSpacing, 0.2f, 1e-4f);
}
