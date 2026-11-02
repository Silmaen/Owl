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

#include <input/ActionMap.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/component/components.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
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
		EXPECT_FALSE(tpl.tags.empty()) << tpl.id;
		EXPECT_FALSE(tpl.contents.empty()) << tpl.id;
		EXPECT_FALSE(tpl.preview.empty()) << tpl.id << ": no preview.png";
	}
	EXPECT_EQ(ids, (std::vector<std::string>{"empty_2d", "platformer_2d", "mesh_3d", "isometric", "raycast", "voxel",
											 "mixed"}));
	EXPECT_TRUE(listProjectTemplates(m_dir / "missing").empty());
}

TEST_F(ProjectTemplateTest, EveryTemplateCreatesAProjectWhoseFirstSceneLoads) {
	for (const auto& tpl: m_templates) {
		const auto dest = m_dir / tpl.id;
		ASSERT_TRUE(createProjectFromTemplate(tpl, dest, "Game " + tpl.id)) << tpl.id;
		EXPECT_FALSE(exists(dest / "template.yml")) << tpl.id;
		EXPECT_FALSE(exists(dest / "preview.png")) << tpl.id;
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
			ASSERT_TRUE(scn->getHudPath().has_value()) << tpl.id << ": the first scene names no HUD";
			const auto hud = mkShared<scene::Scene>();
			ASSERT_TRUE(scene::SceneSerializer(hud, scene::DocumentKind::Hud).deserialize(dest / *scn->getHudPath()))
					<< tpl.id;
			const auto entities = hud->getAllEntities();
			EXPECT_TRUE(std::ranges::any_of(entities,
											[](const scene::Entity& iEntity) -> bool {
												return iEntity.hasComponent<scene::component::UiText>();
											}))
					<< tpl.id << ": the HUD has no text";
		}
	}
}

TEST_F(ProjectTemplateTest, EveryTemplateShipsTheBuiltInActionMap) {
	for (const auto& tpl: m_templates) {
		const auto dest = m_dir / ("actions_" + tpl.id);
		ASSERT_TRUE(createProjectFromTemplate(tpl, dest, "Game"));
		const auto map = input::ActionMap::loadFile(dest / input::ActionMap::fileName);
		ASSERT_TRUE(map) << tpl.id;
		EXPECT_EQ(*map, input::ActionMap::createDefault()) << tpl.id;
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
	const ProjectTemplate missing{.id = "x",
								  .name = "x",
								  .description = {},
								  .order = 0,
								  .directory = m_dir / "none",
								  .tags = {},
								  .contents = {},
								  .preview = {}};
	const auto absent = createProjectFromTemplate(missing, m_dir / "new", "X");
	ASSERT_FALSE(absent);
	EXPECT_EQ(absent.error(), ProjectTemplateError::MissingTemplate);
	EXPECT_FALSE(describe(absent.error()).empty());
}

TEST_F(ProjectTemplateTest, DialogBuildsTheRequestFromItsFields) {
	panel::NewProjectDialog dialog;
	dialog.open(m_templates, m_dir);
	EXPECT_TRUE(dialog.isOpen());
	dialog.setFields("Quest", m_dir.string(), 4);
	const auto request = dialog.makeRequest();
	ASSERT_TRUE(request.has_value());
	EXPECT_EQ(request->directory, m_dir / "Quest");
	EXPECT_EQ(request->name, "Quest");
	ASSERT_TRUE(request->projectTemplate.has_value());
	EXPECT_EQ(request->projectTemplate->id, "raycast");
	dialog.setFields("", m_dir.string(), 0);
	EXPECT_FALSE(dialog.makeRequest().has_value());
	EXPECT_EQ(dialog.getIssue(), panel::NewProjectIssue::EmptyName);
	dialog.setFields("Bare", m_dir.string(), 99);
	EXPECT_FALSE(dialog.makeRequest()->projectTemplate.has_value());
}

TEST_F(ProjectTemplateTest, DialogProposesAFreeName) {
	std::filesystem::create_directories(m_dir / "MyGame");
	std::ofstream(m_dir / "MyGame" / "keep.txt") << "mine";
	panel::NewProjectDialog dialog;
	dialog.open(m_templates, m_dir);
	const auto request = dialog.makeRequest();
	ASSERT_TRUE(request.has_value());
	EXPECT_EQ(request->name, "MyGame2");
	EXPECT_EQ(dialog.getSelected(), 0u);
}

TEST_F(ProjectTemplateTest, NameAndLocationAreChecked) {
	using panel::NewProjectIssue;
	const auto parent = m_dir.string();
	EXPECT_EQ(panel::validateNewProject("Quest", parent), NewProjectIssue::None);
	EXPECT_EQ(panel::validateNewProject("", parent), NewProjectIssue::EmptyName);
	for (const auto* bad: {"a/b", "a\\b", "what?", "..", ".", " lead", "trail ", "dot.", "a:b", "<x>"})
		EXPECT_EQ(panel::validateNewProject(bad, parent), NewProjectIssue::InvalidName) << bad;
	EXPECT_EQ(panel::validateNewProject("Quest", ""), NewProjectIssue::EmptyLocation);
	EXPECT_EQ(panel::validateNewProject("Quest", (m_dir / "missing").string()), NewProjectIssue::MissingLocation);
	std::filesystem::create_directories(m_dir / "Empty");
	EXPECT_EQ(panel::validateNewProject("Empty", parent), NewProjectIssue::None);
	std::filesystem::create_directories(m_dir / "Busy");
	std::ofstream(m_dir / "Busy" / "keep.txt") << "mine";
	EXPECT_EQ(panel::validateNewProject("Busy", parent), NewProjectIssue::DestinationNotEmpty);
	std::ofstream(m_dir / "File") << "a file";
	EXPECT_EQ(panel::validateNewProject("File", parent), NewProjectIssue::DestinationNotEmpty);
	for (const auto issue: {NewProjectIssue::EmptyName, NewProjectIssue::InvalidName, NewProjectIssue::EmptyLocation,
							NewProjectIssue::MissingLocation, NewProjectIssue::DestinationNotEmpty})
		EXPECT_FALSE(panel::describe(issue).empty());
	EXPECT_TRUE(panel::describe(NewProjectIssue::None).empty());
}

TEST(NewProjectDialog, GridMovesStayInside) {
	// 7 cards on 3 columns: rows {0 1 2} {3 4 5} {6}.
	EXPECT_EQ(panel::moveInGrid(0, 7, 3, 1, 0), 1u);
	EXPECT_EQ(panel::moveInGrid(0, 7, 3, -1, 0), 0u);
	EXPECT_EQ(panel::moveInGrid(1, 7, 3, 0, 1), 4u);
	EXPECT_EQ(panel::moveInGrid(1, 7, 3, 0, -1), 1u);
	EXPECT_EQ(panel::moveInGrid(5, 7, 3, 0, 1), 6u);
	EXPECT_EQ(panel::moveInGrid(6, 7, 3, 0, 1), 6u);
	EXPECT_EQ(panel::moveInGrid(6, 7, 3, 1, 0), 6u);
	EXPECT_EQ(panel::moveInGrid(4, 7, 0, 1, 0), 5u);
	EXPECT_EQ(panel::moveInGrid(3, 0, 3, 1, 1), 0u);
}

TEST_F(ProjectTemplateTest, DialogFiltersByTagAndWalksTheShownCards) {
	panel::NewProjectDialog dialog;
	dialog.open(m_templates, m_dir);
	const auto tags = dialog.getTags();
	for (const auto* tag: {"2D", "3D", "Iso", "Raycast", "Voxel", "Mixed"})
		EXPECT_NE(std::ranges::find(tags, tag), tags.end()) << tag;
	EXPECT_EQ(dialog.getVisibleTemplates().size(), m_templates.size());
	dialog.setTagFilter("3D");
	const auto shown = dialog.getVisibleTemplates();
	ASSERT_GE(shown.size(), 3u);
	for (const auto index: shown)
		EXPECT_NE(std::ranges::find(m_templates[index].tags, "3D"), m_templates[index].tags.end());
	EXPECT_EQ(dialog.getSelected(), shown.front());
	dialog.moveSelection(2, 1, 0);
	EXPECT_EQ(dialog.getSelected(), shown[1]);
	dialog.moveSelection(2, 0, 1);
	EXPECT_EQ(dialog.getSelected(), shown[std::min<size_t>(3, shown.size() - 1)]);
	dialog.setTagFilter("");
	EXPECT_EQ(dialog.getVisibleTemplates().size(), m_templates.size());
}

TEST_F(ProjectTemplateTest, EveryTemplateHasAScenario) {
	std::string scenarios;
	for (const auto& entry: std::filesystem::directory_iterator(test::getRootPath() / "test" / "unit" / "scenarios")) {
		if (entry.path().extension() != ".owltest")
			continue;
		std::ifstream file{entry.path()};
		scenarios.append(std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{});
	}
	for (const auto& tpl: m_templates)
		EXPECT_NE(scenarios.find(std::format("project: ../../../engine_assets/project_templates/{}\n", tpl.id)),
				  std::string::npos)
				<< "no scenario plays the template " << tpl.id;
}

TEST_F(ProjectTemplateTest, MixedTemplateMixesStylesInOneScene) {
	const auto mixed = std::ranges::find(m_templates, std::string{"mixed"}, &ProjectTemplate::id);
	ASSERT_NE(mixed, m_templates.end());
	Project project;
	ASSERT_TRUE(project.loadFromFile(mixed->directory / "owl_project.yml"));
	const auto scn = mkShared<scene::Scene>();
	ASSERT_TRUE(scene::SceneSerializer(scn).deserialize(mixed->directory / project.firstScene));
	std::vector<std::string> kinds;
	for (const auto& entry: scn->getEnabledRenderers().entries) {
		if (!entry.enabled || entry.name == "ui")
			continue;
		const auto* layer = project.rendererStack.find(entry.name);
		ASSERT_NE(layer, nullptr) << entry.name;
		kinds.push_back(layer->typeKey + ":" + layer->defaultConfig);
	}
	// A mesh layer and at least two differently configured 2D layers in the first scene.
	EXPECT_GE(kinds.size(), 3u);
	EXPECT_NE(project.rendererStack.find("mesh_world"), nullptr);
	EXPECT_TRUE(std::ranges::any_of(
			kinds, [](const std::string& iKind) -> bool { return iKind.starts_with("RendererMesh"); }));
}
