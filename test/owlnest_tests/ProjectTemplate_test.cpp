/**
 * @file ProjectTemplate_test.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "ProjectTemplate.h"
#include "Project.h"
#include "panel/NewProjectDialog.h"
#include "testHelper.h"

#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/component/components.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace owl;
using namespace owl::nest;

namespace {

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wweak-vtables")
class ProjectTemplateTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_dir = std::filesystem::temp_directory_path() / "owl_project_template_test";
		std::filesystem::remove_all(m_dir);
		std::filesystem::create_directories(m_dir);
		m_templates = listProjectTemplates(test::getRootPath() / "engine_assets" / "project_templates");
	}

	void TearDown() override {
		std::filesystem::remove_all(m_dir);
		core::Log::invalidate();
	}

	std::filesystem::path m_dir;
	std::vector<ProjectTemplate> m_templates;
};
OWL_DIAG_POP

}// namespace

TEST_F(ProjectTemplateTest, BundledTemplatesAreListedInOrder) {
	std::vector<std::string> ids;
	for (const auto& tpl: m_templates) {
		ids.push_back(tpl.id);
		EXPECT_FALSE(tpl.name.empty());
		EXPECT_FALSE(tpl.description.empty());
	}
	EXPECT_EQ(ids, (std::vector<std::string>{"empty_2d", "raycast", "voxel", "mixed"}));
	EXPECT_TRUE(listProjectTemplates(m_dir / "missing").empty());
}

TEST_F(ProjectTemplateTest, EveryTemplateCreatesAProjectWhoseFirstSceneLoads) {
	for (const auto& tpl: m_templates) {
		const auto dest = m_dir / tpl.id;
		ASSERT_TRUE(createProjectFromTemplate(tpl, dest, "Game " + tpl.id)) << tpl.id;
		EXPECT_FALSE(exists(dest / "template.yml")) << tpl.id;
		Project project;
		ASSERT_TRUE(project.loadFromFile(dest / "owl_project.yml")) << tpl.id;
		EXPECT_EQ(project.name, "Game " + tpl.id);
		EXPECT_FALSE(project.rendererStack.isEmpty()) << tpl.id;
		const auto scn = mkShared<scene::Scene>();
		ASSERT_TRUE(scene::SceneSerializer(scn).deserialize(dest / project.firstScene)) << tpl.id;
		EXPECT_FALSE(scn->getAllEntities().empty()) << tpl.id;
		for (const auto& enabled: scn->getEnabledRenderers().entries)
			EXPECT_NE(project.rendererStack.find(enabled.name), nullptr) << tpl.id << ": " << enabled.name;
		if (project.rendererStack.find("ui") != nullptr) {
			const auto entities = scn->getAllEntities();
			EXPECT_TRUE(std::ranges::any_of(entities,
											[](const scene::Entity& iEntity) -> bool {
												return iEntity.hasComponent<scene::component::UiText>();
											}))
					<< tpl.id << ": the HUD has no text";
		}
	}
}

TEST_F(ProjectTemplateTest, TemplateAssetsAreCopied) {
	for (const auto& tpl: m_templates) {
		if (tpl.id != "raycast")
			continue;
		ASSERT_TRUE(createProjectFromTemplate(tpl, m_dir / "r", "R"));
		for (const auto* file:
			 {"scripts/player.lua", "tilemaps/room.owltilemap", "tilesets/walls.owltileset", "textures/tiles.png"})
			EXPECT_TRUE(exists(m_dir / "r" / file)) << file;
	}
}

TEST_F(ProjectTemplateTest, RefusesANonEmptyFolderAndAMissingTemplate) {
	ASSERT_FALSE(m_templates.empty());
	std::filesystem::create_directories(m_dir / "busy");
	std::ofstream(m_dir / "busy" / "keep.txt") << "mine";
	const auto busy = createProjectFromTemplate(m_templates.front(), m_dir / "busy", "X");
	ASSERT_FALSE(busy);
	EXPECT_EQ(busy.error(), ProjectTemplateError::DestinationNotEmpty);
	EXPECT_FALSE(exists(m_dir / "busy" / "owl_project.yml"));
	const ProjectTemplate missing{.id = "x", .name = "x", .description = {}, .order = 0, .directory = m_dir / "none"};
	const auto absent = createProjectFromTemplate(missing, m_dir / "new", "X");
	ASSERT_FALSE(absent);
	EXPECT_EQ(absent.error(), ProjectTemplateError::MissingTemplate);
	EXPECT_FALSE(describe(absent.error()).empty());
}

TEST_F(ProjectTemplateTest, DialogBuildsTheRequestFromItsFields) {
	panel::NewProjectDialog dialog;
	dialog.open(m_templates, m_dir);
	EXPECT_TRUE(dialog.isOpen());
	dialog.setFields("Quest", m_dir.string(), 1);
	const auto request = dialog.makeRequest();
	ASSERT_TRUE(request.has_value());
	EXPECT_EQ(request->directory, m_dir / "Quest");
	EXPECT_EQ(request->name, "Quest");
	ASSERT_TRUE(request->projectTemplate.has_value());
	EXPECT_EQ(request->projectTemplate->id, "raycast");
	dialog.setFields("", m_dir.string(), 0);
	EXPECT_FALSE(dialog.makeRequest().has_value());
	dialog.setFields("Bare", m_dir.string(), 99);
	EXPECT_FALSE(dialog.makeRequest()->projectTemplate.has_value());
}
