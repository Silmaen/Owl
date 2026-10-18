/**
 * @file UiRendererTagMigration_test.cpp
 * @author Silmaen
 * @date 16/10/2026
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
#include <scene/component/RendererTag.h>
#include <scene/component/Tag.h>

#include <cstdint>
#include <format>
#include <string>
#include <vector>

using namespace owl;
using namespace owl::scene;

namespace {

class UiRendererTagMigrationTest : public ::testing::Test {
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

// A version 4 scene as the v0.3.0 tooling wrote it: the canvas and its widgets name the `ui` layer, a world sprite too.
constexpr auto g_legacyScene =
		"Scene: legacy\nFormatVersion: 4\nEntities:\n"
		"  - Entity: 1\n    Tag: {tag: Hud}\n    Canvas: {space: ScreenOverlay, sortOrder: 0, referenceResolution: "
		"[1280, 720], scaleMode: ScaleWithHeight}\n    RendererTag: {Name: ui}\n"
		"  - Entity: 2\n    Tag: {tag: Label}\n    Hierarchy: {parentId: 1}\n    RendererTag: {Name: ui}\n"
		"    UiRect: {anchorMin: [0, 0], anchorMax: [0, 0], pivot: [0, 0], size: [100, 20], offset: [0, 0]}\n"
		"  - Entity: 3\n    Tag: {tag: World}\n    RendererTag: {Name: ui}\n";

}// namespace

TEST_F(UiRendererTagMigrationTest, CanvasAndWidgetsLoseTheUiTag) {
	const auto scene = load(g_legacyScene);
	ASSERT_NE(scene, nullptr);
	EXPECT_FALSE(findByTag(*scene, "Hud").hasComponent<component::RendererTag>());
	EXPECT_FALSE(findByTag(*scene, "Label").hasComponent<component::RendererTag>());
	const auto world = findByTag(*scene, "World");
	ASSERT_TRUE(world.hasComponent<component::RendererTag>());
	EXPECT_EQ(world.getComponent<component::RendererTag>().rendererName, "ui");
}

TEST_F(UiRendererTagMigrationTest, SavedSceneCarriesNoUiTagOnTheHud) {
	const auto scene = load(g_legacyScene);
	ASSERT_NE(scene, nullptr);
	const auto saved = SceneSerializer(scene).serializeToString();
	EXPECT_NE(saved.find(std::format("FormatVersion: {}", SceneSerializer::format().currentVersion())),
			  std::string::npos);
	const auto reloaded = load(saved);
	ASSERT_NE(reloaded, nullptr);
	EXPECT_FALSE(findByTag(*reloaded, "Hud").hasComponent<component::RendererTag>());
	EXPECT_FALSE(findByTag(*reloaded, "Label").hasComponent<component::RendererTag>());
}

TEST_F(UiRendererTagMigrationTest, OnlyTaggedHudNeedsTheMigration) {
	const auto canRead = [](const std::string& iYaml) -> bool {
		const core::YamlDocument document{iYaml};
		return canReadWithoutMigration(document.getRoot(), SceneSerializer::format());
	};
	EXPECT_FALSE(canRead(g_legacyScene));
	EXPECT_TRUE(canRead("Scene: a\nFormatVersion: 4\nEntities:\n  - Entity: 1\n    RendererTag: {Name: ui}\n"));
	EXPECT_TRUE(canRead("Scene: a\nFormatVersion: 4\nEntities:\n  - Entity: 1\n    Canvas: {scaleMode: "
						"ScaleWithHeight}\n"));
}
