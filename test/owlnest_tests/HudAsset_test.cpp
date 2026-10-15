/**
 * @file HudAsset_test.cpp
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "AssetKind.h"
#include "Project.h"
#include "UndoManager.h"
#include "commands/CommandRegistry.h"
#include "testHelper.h"

#include <core/Log.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

using namespace owl;
using namespace owl::nest;
using namespace owl::nest::commands;

namespace {

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wweak-vtables")
class HudAssetTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_dir = std::filesystem::temp_directory_path() /
				("owl_hud_asset_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
		std::filesystem::create_directories(m_dir / "huds");
	}

	void TearDown() override {
		std::error_code ec;
		std::filesystem::remove_all(m_dir, ec);
		core::Log::invalidate();
	}

	std::filesystem::path m_dir;
	CommandRegistry m_registry;
	shared<scene::Scene> m_scene = mkShared<scene::Scene>();
	SceneUndoManager m_undo;
};
OWL_DIAG_POP

}// namespace

TEST(HudAsset, ClassifiedAndListed) {
	EXPECT_EQ(classifyAsset("huds/gameplay.owlhud"), AssetKind::Hud);
	const auto dir = std::filesystem::temp_directory_path() / "owl_list_assets_test";
	std::filesystem::remove_all(dir);
	std::filesystem::create_directories(dir / "huds");
	for (const auto* name: {"huds/b.owlhud", "a.owlhud", "scene.owl"}) std::ofstream{dir / name} << "x";
	const auto found = listAssets(dir, ".owlhud");
	ASSERT_EQ(found.size(), 2u);
	EXPECT_EQ(found[0], "a.owlhud");
	EXPECT_EQ(found[1], "huds/b.owlhud");
	EXPECT_TRUE(listAssets(dir / "missing", ".owlhud").empty());
	std::filesystem::remove_all(dir);
}

TEST_F(HudAssetTest, SetHudIsACommandThatUndoes) {
	ASSERT_NE(m_registry.find("scene.set_hud"), nullptr);
	EXPECT_FALSE(m_scene->getHudPath().has_value());
	ASSERT_TRUE(m_registry.execute("scene.set_hud", {{"path", std::string{"huds/gameplay.owlhud"}}}, m_scene, m_undo));
	EXPECT_EQ(m_scene->getHudPath(), std::string{"huds/gameplay.owlhud"});
	ASSERT_TRUE(m_registry.execute("scene.set_hud", {{"path", std::string{}}}, m_scene, m_undo));
	ASSERT_TRUE(m_scene->getHudPath().has_value());
	EXPECT_TRUE(m_scene->getHudPath()->empty());
	ASSERT_TRUE(m_registry.execute("scene.set_hud", {{"use_default", true}}, m_scene, m_undo));
	EXPECT_FALSE(m_scene->getHudPath().has_value());

	m_undo.undo(*m_scene);
	ASSERT_TRUE(m_scene->getHudPath().has_value());
	EXPECT_TRUE(m_scene->getHudPath()->empty());
	m_undo.undo(*m_scene);
	EXPECT_EQ(m_scene->getHudPath(), std::string{"huds/gameplay.owlhud"});
	m_undo.redo(*m_scene);
	EXPECT_TRUE(m_scene->getHudPath()->empty());
}

TEST_F(HudAssetTest, SetHudRefusesAWrongFileAndANoOp) {
	const auto wrong =
			m_registry.execute("scene.set_hud", {{"path", std::string{"scenes/level.owl"}}}, m_scene, m_undo);
	ASSERT_FALSE(wrong);
	EXPECT_EQ(wrong.error().error, CommandError::InvalidArgument);
	const auto same = m_registry.execute("scene.set_hud", {{"use_default", true}}, m_scene, m_undo);
	ASSERT_FALSE(same);
	EXPECT_EQ(same.error().error, CommandError::NotApplicable);
	EXPECT_FALSE(m_undo.canUndo());
}

TEST_F(HudAssetTest, ProjectDefaultHudRoundTripsAndFeedsTheExport) {
	Project project;
	project.name = "Huds";
	project.firstScene = "scenes/level.owl";
	project.defaultHud = "huds/gameplay.owlhud";
	project.projectDirectory = m_dir;
	const auto file = m_dir / "owl_project.yml";
	ASSERT_TRUE(project.saveToFile(file));
	Project loaded;
	ASSERT_TRUE(loaded.loadFromFile(file));
	EXPECT_EQ(loaded.defaultHud, "huds/gameplay.owlhud");
	EXPECT_EQ(loaded.makeExportSettings(m_dir / "out", m_dir / "runner").defaultHud, "huds/gameplay.owlhud");
}

TEST_F(HudAssetTest, ASceneOpenedAsHudSavesAsHud) {
	const auto hud = mkShared<scene::Scene>();
	hud->createEntity("Canvas");
	const auto path = m_dir / "huds" / "gameplay.owlhud";
	ASSERT_TRUE(scene::SceneSerializer(hud, scene::DocumentKind::Hud).serialize(path));
	const auto reopened = mkShared<scene::Scene>();
	ASSERT_TRUE(scene::SceneSerializer(reopened, scene::documentKindOf(path)).deserialize(path).has_value());
	EXPECT_EQ(reopened->getDocumentKind(), scene::DocumentKind::Hud);
	// A plain serializer follows the document the scene came from.
	EXPECT_TRUE(scene::SceneSerializer(reopened).serializeToString().starts_with("Hud:"));
}
