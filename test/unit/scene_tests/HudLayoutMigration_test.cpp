/**
 * @file HudLayoutMigration_test.cpp
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <core/YamlNode.h>
#include <scene/Entity.h>
#include <scene/EntityLinkMigration.h>
#include <scene/PrefabSerializer.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/component/Canvas.h>
#include <scene/component/Tag.h>
#include <scene/component/UiRect.h>

#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <vector>

using namespace owl;
using namespace owl::scene;

namespace {

class HudLayoutMigrationTest : public ::testing::Test {
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

auto findByTag(const Scene& iScene, const std::string& iTag) -> Entity {
	for (const auto& entity: iScene.getAllEntities())
		if (entity.getComponent<component::Tag>().tag == iTag)
			return entity;
	return {};
}

// A version 3 HUD in the Y-up layout: a health bar 20 px below the top-right corner, a hint 16 px above the bottom.
constexpr auto g_legacyScene =
		"Scene: legacy\nFormatVersion: 3\nEntities:\n"
		"  - Entity: 1\n    Tag: {tag: Hud}\n    Canvas: {space: ScreenOverlay, sortOrder: 0}\n"
		"  - Entity: 2\n    Tag: {tag: Health}\n    Hierarchy: {parentId: 1}\n"
		"    UiRect: {anchor: TopRight, pivotX: 1, pivotY: 1, sizeX: 200, sizeY: 22, offsetX: -20, offsetY: -20}\n"
		"  - Entity: 3\n    Tag: {tag: Hint}\n    Hierarchy: {parentId: 1}\n"
		"    UiRect:\n      anchor: BottomCenter\n      pivotX: 0.5\n      pivotY: 0\n      sizeX: 600\n"
		"      sizeY: 32\n      offsetX: 0\n      offsetY: 16\n"
		"  - Entity: 4\n    Tag: {tag: Defaults}\n    Hierarchy: {parentId: 1}\n    UiRect: {}\n";

// Format-3 screen rectangle (Y-up centre, pivot and offset from the bottom left) turned into a Y-down top-left corner.
auto legacyTopLeft(const math::vec2& iAnchorUp, const math::vec2& iPivot, const math::vec2& iSize,
				   const math::vec2& iOffset, const math::vec2& iScreen) -> math::vec2 {
	const float centerX = iAnchorUp.x() + iOffset.x() + iSize.x() * (0.5f - iPivot.x());
	const float centerYUp = iAnchorUp.y() + iOffset.y() + iSize.y() * (0.5f - iPivot.y());
	return {centerX - iSize.x() * 0.5f, iScreen.y() - centerYUp - iSize.y() * 0.5f};
}

}// namespace

TEST_F(HudLayoutMigrationTest, LegacyWidgetsKeepTheirPlaceOnScreen) {
	const auto scene = load(g_legacyScene);
	ASSERT_NE(scene, nullptr);
	const auto canvas = findByTag(*scene, "Hud");
	ASSERT_TRUE(canvas);
	EXPECT_EQ(canvas.getComponent<component::Canvas>().scaleMode, component::Canvas::ScaleMode::ConstantPixels);
	auto& layout = scene->getHudLayout();
	for (const math::vec2ui screen: {math::vec2ui{1280u, 720u}, math::vec2ui{640u, 480u}, math::vec2ui{333u, 999u}}) {
		const math::vec2 size{static_cast<float>(screen.x()), static_cast<float>(screen.y())};
		layout.compute(*scene, screen, false);
		const auto* health = layout.findNode(static_cast<entt::entity>(findByTag(*scene, "Health")));
		const auto* hint = layout.findNode(static_cast<entt::entity>(findByTag(*scene, "Hint")));
		ASSERT_NE(health, nullptr);
		ASSERT_NE(hint, nullptr);
		const auto healthExpected =
				legacyTopLeft({size.x(), size.y()}, {1.f, 1.f}, {200.f, 22.f}, {-20.f, -20.f}, size);
		const auto hintExpected = legacyTopLeft({size.x() * 0.5f, 0.f}, {0.5f, 0.f}, {600.f, 32.f}, {0.f, 16.f}, size);
		EXPECT_NEAR(health->bounds.position.x(), healthExpected.x(), 1e-3f);
		EXPECT_NEAR(health->bounds.position.y(), healthExpected.y(), 1e-3f);
		EXPECT_NEAR(hint->bounds.position.x(), hintExpected.x(), 1e-3f);
		EXPECT_NEAR(hint->bounds.position.y(), hintExpected.y(), 1e-3f);
		EXPECT_NEAR(health->bounds.size.x(), 200.f, 1e-3f);
		EXPECT_NEAR(hint->bounds.size.y(), 32.f, 1e-3f);
	}
	const auto defaults = findByTag(*scene, "Defaults").getComponent<component::UiRect>();
	EXPECT_EQ(defaults.getAnchor(), component::UiRect::Anchor::Center);
	EXPECT_NEAR(defaults.size.x(), 100.f, 1e-4f);
	EXPECT_NEAR(defaults.pivot.y(), 0.5f, 1e-4f);
}

TEST_F(HudLayoutMigrationTest, MigratedSceneSavesTheNewKeys) {
	const auto scene = load(g_legacyScene);
	ASSERT_NE(scene, nullptr);
	const auto saved = SceneSerializer(scene).serializeToString();
	EXPECT_NE(saved.find(std::format("FormatVersion: {}", SceneSerializer::format().currentVersion())),
			  std::string::npos);
	EXPECT_NE(saved.find("anchorMin: [1, 0]"), std::string::npos);
	EXPECT_NE(saved.find("offset: [-20, 20]"), std::string::npos);
	EXPECT_NE(saved.find("scaleMode: ConstantPixels"), std::string::npos);
	EXPECT_EQ(saved.find("pivotX"), std::string::npos);
	EXPECT_EQ(saved.find("offsetY"), std::string::npos);
	const auto reloaded = load(saved);
	ASSERT_NE(reloaded, nullptr);
	const auto rect = findByTag(*reloaded, "Health").getComponent<component::UiRect>();
	EXPECT_EQ(rect.getAnchor(), component::UiRect::Anchor::TopRight);
	EXPECT_NEAR(rect.pivot.x(), 1.f, 1e-4f);
	EXPECT_NEAR(rect.pivot.y(), 0.f, 1e-4f);
	EXPECT_NEAR(rect.anchorOffset.y(), 20.f, 1e-4f);
}

TEST_F(HudLayoutMigrationTest, OnlyLegacyHudNeedsTheMigration) {
	const auto canRead = [](const std::string& iYaml) -> bool {
		const core::YamlDocument document{iYaml};
		return canReadWithoutMigration(document.getRoot(), SceneSerializer::format());
	};
	EXPECT_FALSE(canRead(g_legacyScene));
	EXPECT_FALSE(canRead("Scene: a\nFormatVersion: 3\nEntities:\n  - Entity: 1\n    Canvas: {sortOrder: 0}\n"));
	EXPECT_TRUE(canRead("Scene: a\nFormatVersion: 3\nEntities:\n  - Entity: 1\n    UiRect: {anchorMin: [0, 0]}\n"));
	EXPECT_TRUE(canRead("Scene: a\nFormatVersion: 3\nEntities:\n  - Entity: 1\n    Canvas: {scaleMode: "
						"ConstantPixels}\n"));
	EXPECT_TRUE(canRead("Scene: a\nFormatVersion: 5\nEntities:\n  - Entity: 1\n    UiRect: {}\n"));
}

TEST_F(HudLayoutMigrationTest, CurrentCanvasKeepsItsScaleMode) {
	const auto scene =
			load("Scene: current\nFormatVersion: 4\nEntities:\n  - Entity: 5\n    Tag: {tag: Hud}\n"
				 "    Canvas: {sortOrder: 2, referenceResolution: [640, 360], scaleMode: MatchSmallerSide}\n");
	ASSERT_NE(scene, nullptr);
	const auto canvas = findByTag(*scene, "Hud").getComponent<component::Canvas>();
	EXPECT_EQ(canvas.sortOrder, 2);
	EXPECT_EQ(canvas.scaleMode, component::Canvas::ScaleMode::MatchSmallerSide);
	EXPECT_NEAR(canvas.referenceResolution.x(), 640.f, 1e-4f);
	EXPECT_NEAR(canvas.referenceResolution.y(), 360.f, 1e-4f);
}

TEST_F(HudLayoutMigrationTest, LegacyPrefabIsMigrated) {
	const auto dir = std::filesystem::temp_directory_path() / "owl_hud_migration_test";
	std::filesystem::remove_all(dir);
	std::filesystem::create_directories(dir);
	const auto prefabPath = dir / "button.owlprefab";
	{
		std::ofstream out(prefabPath);
		out << "Prefab: Button\nVersion: 1\nFormatVersion: 3\nEntities:\n"
			   "  - Entity: 7\n    Tag: {tag: Button}\n"
			   "    UiRect: {anchor: BottomLeft, pivotX: 0, pivotY: 0, sizeX: 50, sizeY: 10, offsetX: 4, offsetY: 6}\n";
	}
	const auto scene = mkShared<Scene>();
	const auto root = PrefabSerializer::instantiate(prefabPath, scene);
	ASSERT_TRUE(root);
	const auto& rect = root.getComponent<component::UiRect>();
	EXPECT_EQ(rect.getAnchor(), component::UiRect::Anchor::BottomLeft);
	EXPECT_NEAR(rect.pivot.y(), 1.f, 1e-4f);
	EXPECT_NEAR(rect.anchorOffset.x(), 4.f, 1e-4f);
	EXPECT_NEAR(rect.anchorOffset.y(), -6.f, 1e-4f);
	std::filesystem::remove_all(dir);
}
