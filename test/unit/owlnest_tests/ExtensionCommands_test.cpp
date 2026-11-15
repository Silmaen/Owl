/**
 * @file ExtensionCommands_test.cpp
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "commands/ExtensionCommands.h"
#include "Project.h"
#include "UndoManager.h"
#include "commands/CommandRegistry.h"
#include "panel/ExtensionPanel.h"
#include "testHelper.h"

#include <app/Application.h>
#include <data/assets/pack/ExtensionManifest.h>
#include <data/assets/pack/ExtensionPack.h>
#include <platform/FileUtils.h>
#include <scene/Scene.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <variant>
#include <vector>

using namespace owl;
using namespace owl::nest;
using namespace owl::nest::commands;
using data::assets::pack::ExtensionKind;
using data::assets::pack::ExtensionManifest;

namespace {

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wweak-vtables")
class ExtensionCommandsTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_app = mkShared<app::Application>(app::AppParams{.name = "extension commands",
														  .renderer = renderer::gpu::RenderAPI::Type::Null,
														  .hasGui = false,
														  .isDummy = true});
		m_root = std::filesystem::temp_directory_path() / "owl_extension_commands";
		std::filesystem::remove_all(m_root);
		std::filesystem::create_directories(m_root);
		m_app->addAssetDirectory({.title = "project", .assetsPath = m_root});
	}

	void TearDown() override {
		m_app->removeAssetDirectory(m_root);
		std::filesystem::remove_all(m_root);
		app::Application::invalidate();
		m_app.reset();
		core::Log::invalidate();
	}

	auto run(const std::string& iName, const CommandArgs& iArgs) -> expected<CommandResult, CommandFailure> {
		return m_registry.execute(
				iName, iArgs,
				CommandContext{.scene = m_scene, .undo = &m_undo, .playing = false, .projectDirectory = m_root});
	}

	[[nodiscard]] auto manifestOf(const std::string& iId) const -> ExtensionManifest {
		const auto folder = getExtensionsFolder(m_root) / iId;
		return data::assets::pack::ExtensionPack::readManifest(folder).value_or(ExtensionManifest{});
	}

	/// The application owning the asset directories.
	shared<app::Application> m_app;
	/// Temporary project.
	std::filesystem::path m_root;
	/// Registry under test.
	CommandRegistry m_registry;
	/// Scene whose history records the changes.
	shared<scene::Scene> m_scene = mkShared<scene::Scene>();
	/// Undo history of the scene.
	SceneUndoManager m_undo;
};
OWL_DIAG_POP

}// namespace

TEST_F(ExtensionCommandsTest, ProjectFolders) {
	EXPECT_EQ(getExtensionsFolder("/p"), std::filesystem::path("/p/extensions"));
	EXPECT_EQ(getExtensionExportFile("/p", "a.b"), std::filesystem::path("/p/exports/a.b.owlmod"));
}

TEST_F(ExtensionCommandsTest, CreateAndList) {
	const auto empty = run("extension.list", {});
	ASSERT_TRUE(empty.has_value());
	EXPECT_EQ(std::get<int64_t>(empty->data.at("count")), 0);
	ASSERT_TRUE(run("extension.create", {{"id", std::string{"demo.recolour"}},
										 {"name", std::string{"Recolour"}},
										 {"kind", std::string{"Script"}}}));
	EXPECT_EQ(manifestOf("demo.recolour").name, "Recolour");
	EXPECT_EQ(manifestOf("demo.recolour").kind, ExtensionKind::Script);
	ASSERT_TRUE(run("extension.create", {{"id", std::string{"other"}}}));
	const auto listed = run("extension.list", {});
	ASSERT_TRUE(listed.has_value());
	EXPECT_EQ(std::get<int64_t>(listed->data.at("count")), 2);
	EXPECT_EQ(std::get<std::string>(listed->data.at("ids")), "demo.recolour,other");
	EXPECT_EQ(run("extension.create", {{"id", std::string{"other"}}}).error().error, CommandError::NotApplicable);
	EXPECT_EQ(run("extension.create", {{"id", std::string{"Bad Id"}}}).error().error, CommandError::InvalidArgument);
	EXPECT_EQ(run("extension.create", {{"id", std::string{"v"}}, {"version", std::string{"x"}}}).error().error,
			  CommandError::InvalidArgument);
	const auto noProject = m_registry.execute("extension.list", {}, CommandContext{});
	EXPECT_EQ(noProject.error().error, CommandError::NotApplicable);
	EXPECT_EQ(m_registry.execute("extension.create", {{"id", std::string{"x"}}}, CommandContext{}).error().error,
			  CommandError::NotApplicable);
}

TEST_F(ExtensionCommandsTest, SetIsUndoable) {
	ASSERT_TRUE(run("extension.create", {{"id", std::string{"mod"}}}));
	ASSERT_TRUE(run("extension.create", {{"id", std::string{"base"}}}));
	const auto set = run("extension.set", {{"path", std::string{"mod"}},
										   {"version", std::string{"2.1"}},
										   {"load_order", int64_t{3}},
										   {"author", std::string{"Me"}},
										   {"description", std::string{"A mod."}},
										   {"dependencies", std::string{"base@1.0, extra"}}});
	ASSERT_TRUE(set.has_value());
	EXPECT_EQ(std::get<std::string>(set->data.at("dependencies")), "base@1.0.0, extra@0.0.0");
	auto manifest = manifestOf("mod");
	EXPECT_EQ(manifest.version.toString(), "2.1.0");
	EXPECT_EQ(manifest.loadOrder, 3);
	EXPECT_EQ(manifest.author, "Me");
	EXPECT_EQ(manifest.description, "A mod.");
	ASSERT_EQ(manifest.dependencies.size(), 2u);
	m_undo.undo(*m_scene);
	EXPECT_EQ(manifestOf("mod").version.toString(), "1.0.0");
	EXPECT_TRUE(manifestOf("mod").dependencies.empty());
	m_undo.redo(*m_scene);
	EXPECT_EQ(manifestOf("mod").loadOrder, 3);
	const auto absolute = (getExtensionsFolder(m_root) / "mod" / "extension.yml").string();
	ASSERT_TRUE(run("extension.set", {{"path", absolute}, {"name", std::string{"Named"}}}));
	EXPECT_EQ(manifestOf("mod").name, "Named");
	ASSERT_TRUE(run("extension.set", {{"path", std::string{"extensions/mod"}}, {"dependencies", std::string{}}}));
	EXPECT_TRUE(manifestOf("mod").dependencies.empty());
	EXPECT_EQ(run("extension.set", {{"path", std::string{"mod"}}, {"name", std::string{"Named"}}}).error().error,
			  CommandError::NotApplicable);
	EXPECT_EQ(run("extension.set", {{"path", std::string{"missing"}}}).error().error, CommandError::InvalidArgument);
	EXPECT_EQ(run("extension.set", {{"path", std::string{"mod"}}, {"version", std::string{"no"}}}).error().error,
			  CommandError::InvalidArgument);
	EXPECT_EQ(run("extension.set", {{"path", std::string{"mod"}}, {"dependencies", std::string{"a@x"}}}).error().error,
			  CommandError::InvalidArgument);
	EXPECT_EQ(run("extension.set", {{"path", std::string{"mod"}}, {"dependencies", std::string{"mod"}}}).error().error,
			  CommandError::InvalidArgument);
	{
		std::ofstream broken(getExtensionsFolder(m_root) / "base" / "extension.yml");
		broken << "OwlExtension: [";
	}
	EXPECT_EQ(run("extension.set", {{"path", std::string{"base"}}, {"name", std::string{"B"}}}).error().error,
			  CommandError::NotApplicable);
	const SetExtensionManifestCommand command{m_root / "none" / "extension.yml", "a", "b"};
	EXPECT_NE(command.description().find("none"), std::string::npos);
}

TEST_F(ExtensionCommandsTest, ExportWritesAnOwlmod) {
	ASSERT_TRUE(run("extension.create", {{"id", std::string{"mod"}}}));
	{
		std::filesystem::create_directories(getExtensionsFolder(m_root) / "mod" / "textures");
		std::ofstream texture(getExtensionsFolder(m_root) / "mod" / "textures" / "hero.png");
		texture << "pixels";
	}
	const auto exported = run("extension.export", {{"path", std::string{"mod"}}});
	ASSERT_TRUE(exported.has_value());
	EXPECT_EQ(std::get<int64_t>(exported->data.at("entries")), 1);
	const auto file = getExtensionExportFile(m_root, "mod");
	EXPECT_EQ(std::filesystem::path{std::get<std::string>(exported->data.at("path"))}, file);
	const auto pack = data::assets::pack::ExtensionPack::open(file);
	ASSERT_TRUE(pack.has_value());
	EXPECT_TRUE(pack->contains("textures/hero.png"));
	ASSERT_TRUE(run("extension.export", {{"path", std::string{"mod"}}, {"output", std::string{"out/m.owlmod"}}}));
	EXPECT_TRUE(std::filesystem::exists(m_root / "out" / "m.owlmod"));
	EXPECT_EQ(run("extension.export", {{"path", std::string{"none"}}}).error().error, CommandError::InvalidArgument);
	EXPECT_EQ(run("extension.export", {{"path", std::string{"mod"}}, {"output", std::string{"out"}}}).error().error,
			  CommandError::NotApplicable);
	const auto absolute = (getExtensionsFolder(m_root) / "mod").string();
	EXPECT_EQ(m_registry.execute("extension.export", {{"path", absolute}}, CommandContext{}).error().error,
			  CommandError::MissingArgument);
}

TEST_F(ExtensionCommandsTest, EnableKeepsTheDisabledListOfTheProject) {
	ASSERT_TRUE(run("extension.create", {{"id", std::string{"mod"}}}));
	ASSERT_TRUE(run("extension.create", {{"id", std::string{"other"}}}));
	EXPECT_EQ(run("extension.enable", {{"path", std::string{"mod"}}}).error().error, CommandError::NotApplicable);
	std::vector<std::string> disabled;
	int changes = 0;
	const CommandContext context{.scene = m_scene,
								 .undo = &m_undo,
								 .playing = false,
								 .projectDirectory = m_root,
								 .disabledExtensions = &disabled,
								 .onExtensionsChanged = [&changes]() -> void { ++changes; }};
	const auto disable =
			m_registry.execute("extension.enable", {{"path", std::string{"mod"}}, {"enabled", false}}, context);
	ASSERT_TRUE(disable.has_value());
	EXPECT_EQ(disabled, std::vector<std::string>{"mod"});
	EXPECT_EQ(changes, 1);
	EXPECT_EQ(m_registry.execute("extension.enable", {{"path", std::string{"mod"}}, {"enabled", false}}, context)
					  .error()
					  .error,
			  CommandError::NotApplicable);
	EXPECT_EQ(getEnabledExtensionSources(m_root, disabled),
			  std::vector<std::filesystem::path>{getExtensionsFolder(m_root) / "other"});
	EXPECT_TRUE(getEnabledExtensionSources({}, disabled).empty());
	const auto listed = m_registry.execute("extension.list", {}, context);
	ASSERT_TRUE(listed.has_value());
	EXPECT_EQ(std::get<std::string>(listed->data.at("enabled")), "other");
	ASSERT_TRUE(m_registry.execute("extension.create", {{"id", std::string{"third"}}}, context));
	EXPECT_EQ(changes, 2);
	ASSERT_TRUE(m_registry.execute("extension.enable", {{"path", std::string{"mod"}}}, context));
	EXPECT_TRUE(disabled.empty());
	EXPECT_EQ(m_registry.execute("extension.enable", {{"path", std::string{"none"}}}, context).error().error,
			  CommandError::InvalidArgument);

	panel::ExtensionPanel panel;
	panel.setCommandRegistry(&m_registry);
	panel.setProjectDirectory(m_root);
	panel.setContextProvider([&context]() -> CommandContext { return CommandContext{context}; });
	panel.setDisabledExtensions(&disabled);
	panel.refresh();
	ASSERT_TRUE(panel.select("other"));
	EXPECT_TRUE(panel.isEnabled("other"));
	ASSERT_TRUE(panel.run("extension.enable", {{"enabled", false}}));
	EXPECT_FALSE(panel.isEnabled("other"));
	ASSERT_TRUE(panel.run("extension.set", {{"name", std::string{"Other"}}}));
	EXPECT_EQ(changes, 5);
}

TEST_F(ExtensionCommandsTest, ProjectKeepsItsDisabledPacks) {
	ASSERT_TRUE(run("extension.create", {{"id", std::string{"mod"}}}));
	ASSERT_TRUE(run("extension.create", {{"id", std::string{"off"}}}));
	Project project;
	project.name = "Packs";
	project.firstScene = "scenes/a.owl";
	project.projectDirectory = m_root;
	project.disabledExtensions = {"off"};
	EXPECT_EQ(project.getEnabledExtensions(), std::vector<std::filesystem::path>{getExtensionsFolder(m_root) / "mod"});
	EXPECT_EQ(project.makeExportSettings(m_root / "out", m_root / "runner").extensionSources,
			  project.getEnabledExtensions());
	ASSERT_TRUE(project.saveToFile(m_root / "owl_project.yml"));
	Project loaded;
	ASSERT_TRUE(loaded.loadFromFile(m_root / "owl_project.yml"));
	EXPECT_EQ(loaded.disabledExtensions, std::vector<std::string>{"off"});
	EXPECT_TRUE(Project{}.getEnabledExtensions().empty());
}

TEST_F(ExtensionCommandsTest, PanelEditsThroughCommands) {
	panel::ExtensionPanel panel;
	EXPECT_FALSE(panel.isVisible());
	panel.toggle();
	EXPECT_TRUE(panel.isVisible());
	panel.setCommandRegistry(&m_registry);
	panel.setProjectDirectory(m_root);
	panel.refresh();
	EXPECT_TRUE(panel.getEntries().empty());
	EXPECT_EQ(panel.getSelected(), nullptr);
	ASSERT_TRUE(panel.run("extension.create", {{"id", std::string{"mod"}}}));
	ASSERT_EQ(panel.getEntries().size(), 1u);
	ASSERT_NE(panel.getSelected(), nullptr);
	EXPECT_EQ(panel.getSelected()->manifest.id, "mod");
	EXPECT_FALSE(panel.run("extension.set", {{"name", std::string{"No scene"}}}));
	panel.setScene(m_scene);
	panel.setUndoManager(&m_undo);
	ASSERT_TRUE(panel.run("extension.set", {{"name", std::string{"Named"}}}));
	EXPECT_EQ(panel.getSelected()->manifest.name, "Named");
	ASSERT_TRUE(panel.run("extension.export", {}));
	EXPECT_TRUE(std::filesystem::exists(getExtensionExportFile(m_root, "mod")));
	EXPECT_FALSE(panel.run("extension.set", {{"version", std::string{"bad"}}}));
	EXPECT_FALSE(panel.select("missing"));
	EXPECT_TRUE(panel.select("mod"));
	panel.setProjectDirectory({});
	panel.refresh();
	EXPECT_TRUE(panel.getEntries().empty());
	panel.toggle();
	EXPECT_FALSE(panel.isVisible());
}
