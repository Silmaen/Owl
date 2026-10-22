/**
 * @file SceneSettingsCommands_test.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "UndoManager.h"
#include "commands/CommandRegistry.h"
#include "commands/SceneSettingsCommands.h"
#include "nestTestHelper.h"
#include "testHelper.h"

#include <string>

using namespace owl;
using namespace owl::nest;
using namespace owl::nest::commands;
using namespace owl::nest::test;

namespace {

auto renderersYaml(const std::string& iName, const bool iEnabled) -> std::string {
	renderer::EnabledRenderersConfig config;
	config.entries.push_back({.name = iName, .enabled = iEnabled, .overrides = {}});
	return config.toYaml();
}

}// namespace

using SceneSettingsCommandsTest = NestTest;

TEST_F(SceneSettingsCommandsTest, ModifyRenderersUndoRedo) {
	const auto after = renderersYaml("Voxel", false);
	m_undo.execute(mkUniq<ModifyEnabledRenderersCommand>("", after), m_scene);
	EXPECT_EQ(m_undo.undoDescription(), "Edit scene renderers");
	ASSERT_EQ(m_scene.getEnabledRenderers().entries.size(), 1u);
	EXPECT_EQ(m_scene.getEnabledRenderers().entries[0].name, "Voxel");
	EXPECT_FALSE(m_scene.getEnabledRenderers().entries[0].enabled);

	m_undo.undo(m_scene);
	EXPECT_TRUE(m_scene.getEnabledRenderers().isEmpty());

	m_undo.redo(m_scene);
	EXPECT_EQ(m_scene.getEnabledRenderers().entries.size(), 1u);
}

TEST_F(SceneSettingsCommandsTest, ModifyRenderersMergesKeepingFirstBefore) {
	const auto first = renderersYaml("Voxel", true);
	const auto second = renderersYaml("Voxel", false);
	m_undo.execute(mkUniq<ModifyEnabledRenderersCommand>("", first), m_scene);
	m_undo.execute(mkUniq<ModifyEnabledRenderersCommand>(first, second), m_scene);
	EXPECT_FALSE(m_scene.getEnabledRenderers().entries[0].enabled);

	m_undo.undo(m_scene);
	EXPECT_TRUE(m_scene.getEnabledRenderers().isEmpty());
	EXPECT_FALSE(m_undo.canUndo());
}

TEST_F(SceneSettingsCommandsTest, SetRenderersCommandUndoesAndRefusesANoOp) {
	const CommandRegistry registry;
	auto scene = mkShared<scene::Scene>();
	SceneUndoManager undo;
	ASSERT_NE(registry.find("scene.set_renderers"), nullptr);
	const auto yaml = renderersYaml("Voxel", false);
	ASSERT_TRUE(registry.execute("scene.set_renderers", {{"config", yaml}}, scene, undo));
	ASSERT_EQ(scene->getEnabledRenderers().entries.size(), 1u);
	EXPECT_FALSE(scene->getEnabledRenderers().entries[0].enabled);
	const auto again = registry.execute("scene.set_renderers", {{"config", yaml}}, scene, undo);
	ASSERT_FALSE(again);
	EXPECT_EQ(again.error().error, CommandError::NotApplicable);
	undo.undo(*scene);
	EXPECT_TRUE(scene->getEnabledRenderers().isEmpty());
}

TEST_F(SceneSettingsCommandsTest, SetSceneScriptCommandUndoesAndRefusesBadInput) {
	const CommandRegistry registry;
	auto scene = mkShared<scene::Scene>();
	SceneUndoManager undo;
	ASSERT_NE(registry.find("scene.set_script"), nullptr);
	const std::string yaml = "scriptPath: scripts/director.lua\nproperties:\n  - name: limit\n    type: int\n    value: 4\n";
	ASSERT_TRUE(registry.execute("scene.set_script", {{"script", yaml}}, scene, undo));
	EXPECT_EQ(scene->getSceneScript().scriptPath, "scripts/director.lua");
	ASSERT_EQ(scene->getSceneScript().properties.size(), 1u);
	const auto same = registry.execute("scene.set_script", {{"script", yaml}}, scene, undo);
	ASSERT_FALSE(same);
	EXPECT_EQ(same.error().error, CommandError::NotApplicable);
	const auto wrong = registry.execute("scene.set_script", {{"script", std::string{"scriptPath: a.txt\n"}}}, scene, undo);
	ASSERT_FALSE(wrong);
	EXPECT_EQ(wrong.error().error, CommandError::InvalidArgument);
	ASSERT_TRUE(registry.execute("scene.set_script", {{"script", std::string{}}}, scene, undo));
	EXPECT_TRUE(scene->getSceneScript().scriptPath.empty());
	undo.undo(*scene);
	EXPECT_TRUE(scene->getSceneScript().scriptPath.empty());
}
