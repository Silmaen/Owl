/**
 * @file SceneSettingsCommands_test.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

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
	YAML::Emitter emitter;
	emitter << config.toYaml();
	return emitter.c_str();
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
