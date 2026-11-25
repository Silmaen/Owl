/**
 * @file PluginHost_test.cpp
 * @author Silmaen
 * @date 24/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "session/PluginHost.h"
#include "UndoManager.h"
#include "commands/CommandJournal.h"
#include "commands/CommandRegistry.h"
#include "commands/DocumentCommands.h"
#include "commands/Macro.h"
#include "commands/PluginCommands.h"
#include "session/PluginUi.h"
#include "testHelper.h"

#include <app/Application.h>
#include <data/assets/pack/ExtensionRegistry.h>
#include <platform/FileUtils.h>
#include <scene/ComponentRegistry.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/ScriptedComponents.h>
#include <scene/component/Transform.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

using namespace owl;
using namespace owl::nest;
using namespace owl::nest::commands;
using namespace owl::nest::session;

namespace {

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wweak-vtables")
// Records the widgets a plugin asks for and answers the ones the test names by their label.
class FakeUi final : public PluginUi {
public:
	auto draw(const PluginWidget& iWidget) -> PluginWidgetResult override {
		drawn.push_back(iWidget);
		if (const auto answer = answers.find(iWidget.label); answer != answers.end())
			return answer->second;
		return {.value = iWidget.value, .changed = iWidget.kind == PluginWidgetKind::Section};
	}

	[[nodiscard]] auto find(const std::string& iLabel) const -> const PluginWidget* {
		const auto found = std::ranges::find(drawn, iLabel, &PluginWidget::label);
		return found == drawn.end() ? nullptr : &*found;
	}

	std::vector<PluginWidget> drawn;
	std::map<std::string, PluginWidgetResult> answers;
};

// A plugin document open in a tab, as `plugin.document_set` reaches it.
class FakeDocuments final : public DocumentHost, public PluginDocumentEditor {
public:
	auto openSoundCue(const std::filesystem::path& /*iPath*/) -> SoundCueEditor* override { return nullptr; }

	auto openLocalisation(const std::filesystem::path& /*iPath*/) -> LocalisationEditor* override { return nullptr; }

	auto openDataTable(const std::filesystem::path& /*iPath*/) -> DataTableEditor* override { return nullptr; }

	auto openDialogue(const std::filesystem::path& /*iPath*/) -> DialogueEditor* override { return nullptr; }

	auto openSequence(const std::filesystem::path& /*iPath*/) -> SequenceEditor* override { return nullptr; }

	auto openBehaviourTree(const std::filesystem::path& /*iPath*/) -> BehaviourTreeEditor* override { return nullptr; }

	auto openHudTheme(const std::filesystem::path& /*iPath*/) -> HudThemeEditor* override { return nullptr; }

	auto openMaterial(const std::filesystem::path& /*iPath*/) -> MaterialEditor* override { return nullptr; }

	auto openPluginDocument(const std::filesystem::path& iPath) -> PluginDocumentEditor* override {
		return iPath == path ? this : nullptr;
	}

	[[nodiscard]] auto getText() const -> const std::string& override { return text; }

	auto setText(const std::string& iText, const std::string& /*iDescription*/) -> bool override {
		if (iText == text)
			return false;
		text = iText;
		return true;
	}

	std::filesystem::path path;
	std::string text;
};

class PluginHostTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_app = mkShared<app::Application>(app::AppParams{.name = "plugin host",
														  .renderer = renderer::gpu::RenderAPI::Type::Null,
														  .hasGui = false,
														  .isDummy = true});
		m_root = std::filesystem::temp_directory_path() / "owl_plugin_host";
		std::filesystem::remove_all(m_root);
		std::filesystem::create_directories(m_root);
		m_host.setContextProvider([this]() -> CommandContext { return context(); });
		OWL_REQUIRE_MODULE(SCRIPT);
	}

	void TearDown() override {
		m_host.unload();
		scene::ScriptedComponents::clear();
		std::filesystem::remove_all(m_root);
		app::Application::invalidate();
		m_app.reset();
		core::Log::invalidate();
	}

	[[nodiscard]] auto context() -> CommandContext {
		return {.scene = m_scene,
				.undo = &m_undo,
				.playing = false,
				.documents = &m_documents,
				.projectDirectory = m_root};
	}

	auto run(const std::string& iName, const CommandArgs& iArgs) -> expected<CommandResult, CommandFailure> {
		return m_registry.execute(iName, iArgs, context());
	}

	// Writes a Script pack with an Owl Nest entry script and returns its folder.
	auto writePack(const std::string& iId, const std::string& iScript, const std::string& iKind = "Script") const
			-> std::filesystem::path {
		const auto folder = m_root / "extensions" / iId;
		std::filesystem::create_directories(folder / "editor");
		std::ofstream{folder / "extension.yml"} << "FormatVersion: 1\nOwlExtension:\n  id: " << iId
												<< "\n  kind: " << iKind << "\n  editorScript: editor/main.lua\n";
		std::ofstream{folder / "editor" / "main.lua"} << iScript;
		return folder;
	}

	auto load(const std::vector<std::filesystem::path>& iPacks) -> size_t {
		m_extensions.clear();
		std::ignore = m_extensions.mount(iPacks);
		std::ignore = scene::ScriptedComponents::sync(m_extensions);
		return m_host.load(m_extensions);
	}

	[[nodiscard]] static auto errorsOf(const PluginInfo* iPlugin) -> std::string {
		std::string text;
		if (iPlugin != nullptr)
			for (const auto& error: iPlugin->errors) text += error + "\n";
		return text;
	}

	/// The application the packs are read under.
	shared<app::Application> m_app;
	/// Temporary project.
	std::filesystem::path m_root;
	/// Registry the plugins fill.
	CommandRegistry m_registry;
	/// Scene the commands edit.
	shared<scene::Scene> m_scene = mkShared<scene::Scene>();
	/// Undo history of the scene.
	SceneUndoManager m_undo;
	/// Documents `plugin.document_set` reaches.
	FakeDocuments m_documents;
	/// Mounted packs.
	data::assets::pack::ExtensionRegistry m_extensions;
	/// Host under test.
	PluginHost m_host{m_registry};
};
OWL_DIAG_POP

constexpr auto g_rowScript = R"(
nest.command{name = "t.row.plant", description = "Plant a row of markers", handler = "plant"}
nest.argument{command = "t.row.plant", name = "count", type = "integer", default = 3, description = "Markers."}
nest.argument{command = "t.row.plant", name = "fail", type = "bool", default = false}
nest.argument{command = "t.row.plant", name = "mode", type = "enum", choices = "line, circle", default = "line"}
function plant(args)
    local last = 0
    for i = 1, args.count do
        last = editor.entity.create{name = "Marker " .. i}.entity
    end
    if args.fail then error("asked to fail") end
    return {entity = last, planted = args.count, mode = args.mode, at = {1, 2, 3}}
end
nest.panel{id = "t.row.panel", title = "Row", handler = "draw_panel"}
count = 2
function draw_panel(args)
    if ui.section{label = "Row"}.open then
        ui.value{label = "Entities", value = args.entity_count}
        count = ui.integer{label = "Count", value = count, min = 1, max = 9}.value
        if ui.button{label = "Plant"}.clicked then editor.t.row.plant{count = count} end
    end
    if ui.section{label = "Closed"}.open then
        ui.text{text = "hidden"}
    end
    if args.selection ~= nil then ui.value{label = "Selected", value = args.selection_name} end
end
)";

}// namespace

TEST_F(PluginHostTest, HostCommandsFollowTheHost) {
	EXPECT_NE(m_registry.find("plugin.list"), nullptr);
	EXPECT_NE(m_registry.find("asset.import"), nullptr);
	{
		CommandRegistry registry;
		const auto revision = registry.getRevision();
		{
			const PluginHost host{registry};
			EXPECT_NE(registry.find("plugin.reload"), nullptr);
			EXPECT_GT(registry.getRevision(), revision);
		}
		EXPECT_EQ(registry.find("plugin.reload"), nullptr);
		EXPECT_EQ(registry.find("plugin.list"), nullptr);
		EXPECT_FALSE(registry.remove("plugin.list"));
	}
	EXPECT_EQ(run("plugin.reload", {}).error().error, CommandError::NotApplicable);
	const auto listed = run("plugin.list", {});
	ASSERT_TRUE(listed.has_value());
	EXPECT_EQ(std::get<int64_t>(listed->data.at("count")), 0);
	EXPECT_EQ(getContributionKindName(PluginContributionKind::DocumentType), "document");
	EXPECT_EQ(getContributionKindName(PluginContributionKind::Drawer), "drawer");
}

TEST_F(PluginHostTest, APackWithoutEditorScriptIsNoPlugin) {
	const auto folder = m_root / "extensions" / "plain";
	std::filesystem::create_directories(folder);
	std::ofstream{folder / "extension.yml"} << "OwlExtension:\n  id: plain\n";
	EXPECT_EQ(load({folder}), 0u);
	EXPECT_TRUE(m_host.getPlugins().empty());
}

TEST_F(PluginHostTest, LoadErrorsAreKeptPerPlugin) {
	const auto syntax = writePack("t.syntax", "nest.command{name = \"t.syntax.go\", handler = \"go\"}\nthis is no lua");
	const auto prefix = writePack("t.prefix", R"(nest.command{name = "other.go", handler = "go"})");
	const auto missing = writePack("t.missing", R"(nest.panel{id = "t.missing.panel", handler = "nowhere"})");
	const auto data = writePack("t.data", R"(nest.panel{id = "t.data.panel", handler = "draw"})", "Data");
	const auto early = writePack("t.early", R"(editor.entity.create{name = "Too early"})");
	const auto twice =
			writePack("t.twice", "function d() end\nnest.drawer{component = \"Transform\", handler = \"d\"}\n"
								 "nest.drawer{component = \"Transform\", handler = \"d\"}");
	const auto badArgs = writePack("t.args", "function go() end\nnest.command{name = \"t.args.go\", handler = \"go\"}\n"
											 "nest.argument{command = \"t.args.go\", name = \"n\", type = \"matrix\"}");
	EXPECT_EQ(load({syntax, prefix, missing, data, early, twice, badArgs}), 0u);
	ASSERT_EQ(m_host.getPlugins().size(), 7u);
	EXPECT_TRUE(m_host.find("t.syntax")->contributions.empty());
	EXPECT_EQ(m_registry.find("t.syntax.go"), nullptr);
	EXPECT_FALSE(m_host.find("t.syntax")->errors.empty());
	EXPECT_TRUE(errorsOf(m_host.find("t.prefix")).contains("must start with `t.prefix.`"));
	EXPECT_TRUE(errorsOf(m_host.find("t.missing")).contains("no function `nowhere`"));
	EXPECT_EQ(m_host.find("t.missing")->contributions.size(), 1u);
	EXPECT_TRUE(errorsOf(m_host.find("t.data")).contains("holds no"));
	EXPECT_TRUE(errorsOf(m_host.find("t.early")).contains("while it loads"));
	EXPECT_EQ(m_scene->getEntityCount(), 0u);
	EXPECT_TRUE(errorsOf(m_host.find("t.twice")).contains("already declared"));
	EXPECT_TRUE(errorsOf(m_host.find("t.args")).contains("no argument type"));
	EXPECT_EQ(m_host.find("nope"), nullptr);
	FakeUi ui;
	EXPECT_FALSE(m_host.drawPanel("t.missing.panel", ui));
	ASSERT_EQ(ui.drawn.size(), 1u);
	EXPECT_TRUE(ui.drawn.front().label.contains("The plugin failed"));
	const auto listed = run("plugin.list", {});
	ASSERT_TRUE(listed.has_value());
	EXPECT_EQ(std::get<int64_t>(listed->data.at("count")), 7);
	EXPECT_GE(std::get<int64_t>(listed->data.at("errors")), 7);
}

TEST_F(PluginHostTest, ACommandIsOneUndoStepAndLeavesWithItsPlugin) {
	const auto pack = writePack("t.row", g_rowScript);
	ASSERT_EQ(load({pack}), 1u) << errorsOf(m_host.find("t.row"));
	const auto* spec = m_registry.find("t.row.plant");
	ASSERT_NE(spec, nullptr);
	EXPECT_EQ(spec->description, "Plant a row of markers");
	ASSERT_EQ(spec->args.size(), 3u);
	EXPECT_EQ(spec->args[0].type, ArgType::Integer);
	EXPECT_EQ(spec->args[2].choices, (std::vector<std::string>{"line", "circle"}));
	const auto* command = m_host.findContribution(PluginContributionKind::Command, "t.row.plant");
	ASSERT_NE(command, nullptr);
	EXPECT_EQ(command->handler, "plant");
	EXPECT_GT(command->line, 0u);

	CommandJournal journal;
	m_registry.setJournal(&journal);
	const auto planted = run("t.row.plant", {{"count", int64_t{4}}});
	m_registry.setJournal(nullptr);
	ASSERT_TRUE(planted.has_value()) << planted.error().message;
	EXPECT_EQ(m_scene->getEntityCount(), 4u);
	EXPECT_EQ(planted->description, "Plant a row of markers");
	EXPECT_EQ(std::get<int64_t>(planted->data.at("planted")), 4);
	EXPECT_EQ(std::get<std::string>(planted->data.at("mode")), "line");
	EXPECT_TRUE(std::holds_alternative<math::vec3>(planted->data.at("at")));
	EXPECT_NE(planted->entity, core::UUID{0});
	// The four creations and the plugin command are in the journal, the creations as script commands.
	EXPECT_EQ(journal.getEntries().size(), 5u);
	// A macro built from the journal replays the plugin command once, not its four creations as well.
	std::vector<const JournalEntry*> entries;
	for (const auto& entry: journal.getEntries()) entries.push_back(&entry);
	EXPECT_EQ(std::ranges::count_if(entries, [](const JournalEntry* iEntry) -> bool { return iEntry->nested; }), 4);
	EXPECT_FALSE(journal.getEntries().back().nested);
	const auto macro = Macro::fromJournal(entries, m_registry);
	ASSERT_EQ(macro.getLines().size(), 1u);
	EXPECT_TRUE(macro.getLines().front().starts_with("t.row.plant count=4")) << macro.getLines().front();
	ASSERT_TRUE(m_undo.canUndo());
	m_undo.undo(*m_scene);
	EXPECT_EQ(m_scene->getEntityCount(), 0u);
	EXPECT_FALSE(m_undo.canUndo());

	const auto failed = run("t.row.plant", {{"fail", true}});
	ASSERT_FALSE(failed.has_value());
	EXPECT_TRUE(failed.error().message.contains("asked to fail"));
	EXPECT_EQ(m_scene->getEntityCount(), 0u);
	EXPECT_FALSE(m_undo.canUndo());
	EXPECT_TRUE(errorsOf(m_host.find("t.row")).contains("asked to fail"));
	EXPECT_EQ(run("t.row.plant", {{"mode", std::string{"square"}}}).error().error, CommandError::InvalidArgument);
	auto playing = context();
	playing.playing = true;
	EXPECT_EQ(m_registry.execute("t.row.plant", {}, playing).error().error, CommandError::RefusedDuringPlay);

	m_host.unload();
	EXPECT_EQ(m_registry.find("t.row.plant"), nullptr);
	EXPECT_TRUE(m_host.getPlugins().empty());
	EXPECT_NE(m_registry.find("plugin.list"), nullptr);
}

TEST_F(PluginHostTest, APanelDrawsThroughTheUiAndEditsThroughCommands) {
	const auto pack = writePack("t.row", g_rowScript);
	ASSERT_EQ(load({pack}), 1u) << errorsOf(m_host.find("t.row"));
	const auto panels = m_host.getContributions(PluginContributionKind::Panel);
	ASSERT_EQ(panels.size(), 1u);
	EXPECT_EQ(panels.front()->title, "Row");
	EXPECT_FALSE(panels.front()->visible);
	ASSERT_TRUE(run("plugin.show_panel", {{"id", std::string{"t.row.panel"}}}));
	EXPECT_TRUE(panels.front()->visible);
	EXPECT_EQ(run("plugin.show_panel", {{"id", std::string{"nope"}}}).error().error, CommandError::InvalidArgument);

	FakeUi ui;
	ui.answers["Closed"] = {.value = {}, .changed = false};
	ASSERT_TRUE(m_host.drawPanel("t.row.panel", ui));
	ASSERT_NE(ui.find("Count"), nullptr);
	EXPECT_EQ(ui.find("Count")->kind, PluginWidgetKind::Integer);
	EXPECT_EQ(std::get<int64_t>(ui.find("Count")->value), 2);
	EXPECT_DOUBLE_EQ(ui.find("Count")->max.value_or(0.0), 9.0);
	EXPECT_EQ(std::get<std::string>(ui.find("Entities")->value), "0");
	EXPECT_EQ(ui.find("hidden"), nullptr);
	EXPECT_EQ(ui.find("Selected"), nullptr);
	EXPECT_EQ(m_scene->getEntityCount(), 0u);

	// The count typed is kept by the script, the button runs the plugin command: one undo step.
	ui.drawn.clear();
	ui.answers["Count"] = {.value = int64_t{3}, .changed = true};
	ui.answers["Plant"] = {.value = {}, .changed = true};
	ASSERT_TRUE(m_host.drawPanel("t.row.panel", ui));
	EXPECT_EQ(m_scene->getEntityCount(), 3u);
	m_undo.undo(*m_scene);
	EXPECT_EQ(m_scene->getEntityCount(), 0u);

	const auto selected = m_scene->createEntity("Chosen");
	m_host.setSelectionProvider([&selected]() -> core::UUID { return selected.getUUID(); });
	ui.drawn.clear();
	ui.answers.clear();
	ASSERT_TRUE(m_host.drawPanel("t.row.panel", ui));
	ASSERT_NE(ui.find("Selected"), nullptr);
	EXPECT_EQ(std::get<std::string>(ui.find("Selected")->value), "Chosen");
	EXPECT_FALSE(m_host.drawPanel("nope", ui));
	EXPECT_FALSE(m_host.setPanelVisible("nope", true));

	// A reload keeps the panels shown.
	ASSERT_TRUE(run("plugin.reload", {}));
	ASSERT_EQ(m_host.getContributions(PluginContributionKind::Panel).size(), 1u);
	EXPECT_TRUE(m_host.getContributions(PluginContributionKind::Panel).front()->visible);
}

TEST_F(PluginHostTest, AHandlerReadsTheSceneButWritesThroughCommands) {
	const auto pack = writePack("t.read", R"(
nest.command{name = "t.read.scan", description = "Read the scene", handler = "scan"}
function scan(args)
    local all = nest.entities{}
    local moved = nest.entities{component = "Transform", max = 1}
    local box = nest.components{entity = "Box"}
    local fields = nest.fields{entity = "Box", component = "Transform"}
    local position = nest.field{entity = "Box", component = "Transform", field = "translation"}
    local tag = nest.field{entity = "Box", component = "Tag", field = "tag"}
    local missing = nest.field{entity = "Box", component = "Transform", field = "nope"}
    local lamp = nest.field{entity = "Box", component = "PointLight", field = "color"}
    local has_tag = false
    for _, key in ipairs(box.keys) do
        if key == "Tag" then has_tag = true end
    end
    return {count = all.count, listed = #moved.ids, matches = moved.count, first = all.names[1], has_tag = has_tag,
            field_names = table.concat(fields.names, ","), x = position.value[1], z = position.value[3],
            tag = tag.value, found = missing.found, lamp_has = lamp.has, selection = all.selection_name,
            nobody = nest.entity{entity = "Nobody"}.exists, none = nest.components{entity = "Nobody"}.exists}
end
nest.command{name = "t.read.bad", description = "Unknown component", handler = "bad"}
function bad(args) nest.entities{component = "NoSuchComponent"} end
nest.drawer{component = "Transform", handler = "draw_transform"}
function draw_transform(args)
    ui.number{label = "Z", value = args.translation[3]}
    ui.value{label = "Entity", value = args.name}
end
)");
	ASSERT_EQ(load({pack}), 1u) << errorsOf(m_host.find("t.read"));
	auto box = m_scene->createEntity("Box");
	box.getComponent<scene::component::Transform>().transform.translation() = math::vec3{1.f, 2.f, 3.f};
	std::ignore = m_scene->createEntity("Crate");
	m_host.setSelectionProvider([&box]() -> core::UUID { return box.getUUID(); });

	const auto before = scene::SceneSerializer::serializeEntityToString(box);
	const auto scanned = run("t.read.scan", {});
	ASSERT_TRUE(scanned.has_value()) << scanned.error().message;
	const auto& data = scanned->data;
	EXPECT_EQ(std::get<int64_t>(data.at("count")), 2);
	EXPECT_EQ(std::get<int64_t>(data.at("matches")), 2);
	EXPECT_EQ(std::get<int64_t>(data.at("listed")), 1);
	EXPECT_EQ(std::get<std::string>(data.at("first")), "Box");
	EXPECT_TRUE(std::get<bool>(data.at("has_tag")));
	EXPECT_EQ(std::get<std::string>(data.at("field_names")), "translation,rotation,scale");
	const auto number = [&data](const std::string& iName) -> double {
		const auto& value = data.at(iName);
		return std::holds_alternative<int64_t>(value) ? static_cast<double>(std::get<int64_t>(value))
													  : std::get<double>(value);
	};
	EXPECT_DOUBLE_EQ(number("x"), 1.0);
	EXPECT_DOUBLE_EQ(number("z"), 3.0);
	EXPECT_EQ(std::get<std::string>(data.at("tag")), "Box");
	EXPECT_FALSE(std::get<bool>(data.at("found")));
	EXPECT_FALSE(std::get<bool>(data.at("lamp_has")));
	EXPECT_EQ(std::get<std::string>(data.at("selection")), "Box");
	EXPECT_FALSE(std::get<bool>(data.at("nobody")));
	EXPECT_FALSE(std::get<bool>(data.at("none")));
	// Reading is no edit: no undo step, the entity is as it was.
	EXPECT_FALSE(m_undo.canUndo());
	EXPECT_EQ(scene::SceneSerializer::serializeEntityToString(box), before);
	const auto bad = run("t.read.bad", {});
	ASSERT_FALSE(bad.has_value());
	EXPECT_TRUE(bad.error().message.contains("NoSuchComponent"));

	// The drawer of a built-in component receives its field values.
	FakeUi ui;
	ASSERT_TRUE(m_host.drawComponent(box, "Transform", ui));
	ASSERT_NE(ui.find("Z"), nullptr);
	EXPECT_DOUBLE_EQ(std::get<double>(ui.find("Z")->value), 3.0);
	EXPECT_EQ(std::get<std::string>(ui.find("Entity")->value), "Box");
}

TEST_F(PluginHostTest, WidgetsOnlyDrawFromAHandler) {
	const auto pack =
			writePack("t.ui", "function go() ui.text{text = \"nowhere\"} end\n"
							  "nest.command{name = \"t.ui.go\", handler = \"go\", scene = false, play = true}\n"
							  "function late() nest.panel{id = \"t.ui.late\", handler = \"late\"} end\n"
							  "nest.command{name = \"t.ui.late\", handler = \"late\", scene = false}");
	ASSERT_EQ(load({pack}), 1u) << errorsOf(m_host.find("t.ui"));
	const auto drawn = m_registry.execute("t.ui.go", {}, CommandContext{});
	ASSERT_FALSE(drawn.has_value());
	EXPECT_TRUE(drawn.error().message.contains("ui.*"));
	const auto late = m_registry.execute("t.ui.late", {}, CommandContext{});
	ASSERT_FALSE(late.has_value());
	EXPECT_TRUE(late.error().message.contains("loads only"));
	EXPECT_TRUE(m_registry.find("t.ui.go")->allowedInPlay);
	EXPECT_FALSE(m_registry.find("t.ui.go")->needsScene);
}

TEST_F(PluginHostTest, TheDemoPluginAddsOneOfEachKind) {
	const auto pack = owl::test::getRootPath() / "engine_demo" / "extensions" / "demo.notes";
	ASSERT_EQ(load({pack}), 1u) << errorsOf(m_host.find("demo.notes"));
	const auto* plugin = m_host.find("demo.notes");
	ASSERT_NE(plugin, nullptr);
	EXPECT_EQ(plugin->name, "Design notes");
	EXPECT_EQ(plugin->script, "editor/demo.notes/main.lua");
	EXPECT_EQ(plugin->contributions.size(), 5u);
	for (const auto& contribution: plugin->contributions) EXPECT_GT(contribution.line, 0u) << contribution.id;
	const auto listed = run("plugin.list", {});
	ASSERT_TRUE(listed.has_value());
	EXPECT_EQ(std::get<std::string>(listed->data.at("commands")), "demo.notes.pin");
	EXPECT_EQ(std::get<std::string>(listed->data.at("panels")), "demo.notes.panel");
	EXPECT_EQ(std::get<std::string>(listed->data.at("drawers")), "demo.notes.Note");
	EXPECT_EQ(std::get<std::string>(listed->data.at("documents")), "demo.notes.document");
	EXPECT_EQ(std::get<std::string>(listed->data.at("importers")), "demo.notes.text");

	// Command: the note is pinned in one undo step.
	auto crate = m_scene->createEntity("Crate");
	const auto* note = scene::ComponentRegistry::find("demo.notes.Note");
	ASSERT_NE(note, nullptr);
	const auto pinnedNote =
			run("demo.notes.pin", {{"entity", std::string{"Crate"}}, {"text", std::string{"Paint: it red"}}});
	ASSERT_TRUE(pinnedNote.has_value()) << pinnedNote.error().message;
	ASSERT_TRUE(note->has(crate));
	EXPECT_EQ(std::get<std::string>(scene::ScriptedComponents::get(crate, "demo.notes.Note")->find("text")->value),
			  "Paint: it red");
	m_undo.undo(*m_scene);
	EXPECT_FALSE(note->has(crate));
	EXPECT_FALSE(m_undo.canUndo());
	EXPECT_FALSE(run("demo.notes.pin", {{"entity", std::string{"Nobody"}}}).has_value());

	// Panel: pins the draft on the selection.
	m_host.setSelectionProvider([&crate]() -> core::UUID { return crate.getUUID(); });
	FakeUi ui;
	ui.answers["Note"] = {.value = std::string{"From the panel"}, .changed = true};
	ui.answers["Pin"] = {.value = {}, .changed = true};
	ASSERT_TRUE(m_host.drawPanel("demo.notes.panel", ui));
	ASSERT_TRUE(note->has(crate));
	const auto* pinned = scene::ScriptedComponents::get(crate, "demo.notes.Note");
	EXPECT_EQ(std::get<std::string>(pinned->find("text")->value), "From the panel");
	// The panel lists the entities that carry a note, read from the scene.
	ASSERT_NE(ui.find("Pinned"), nullptr);
	EXPECT_EQ(std::get<std::string>(ui.find("Pinned")->value), "1");
	ASSERT_NE(ui.find("Crate"), nullptr);
	EXPECT_EQ(std::get<std::string>(ui.find("Crate")->value), "From the panel");

	// Drawer: the fields of the component reach the handler, an edit is a command.
	ui = {};
	ui.answers["Done"] = {.value = true, .changed = true};
	EXPECT_FALSE(m_host.drawComponent(crate, "Transform", ui));
	ASSERT_TRUE(m_host.drawComponent(crate, "demo.notes.Note", ui));
	ASSERT_NE(ui.find("Note"), nullptr);
	EXPECT_EQ(std::get<std::string>(ui.find("Note")->value), "From the panel");
	EXPECT_TRUE(std::get<bool>(scene::ScriptedComponents::get(crate, "demo.notes.Note")->find("done")->value));
	m_undo.undo(*m_scene);
	EXPECT_FALSE(std::get<bool>(scene::ScriptedComponents::get(crate, "demo.notes.Note")->find("done")->value));

	// Document type: ticking a note rewrites the text through `plugin.document_set`.
	m_documents.path = m_root / "level.owlnote";
	m_documents.text = "- [ ] Place the pickups\n- [x] Block out\n";
	const auto* type = m_host.findDocumentType(m_documents.path);
	ASSERT_NE(type, nullptr);
	EXPECT_EQ(type->id, "demo.notes.document");
	EXPECT_EQ(m_host.findDocumentType("level.txt"), nullptr);
	ui = {};
	ui.answers["Place the pickups"] = {.value = true, .changed = true};
	ASSERT_TRUE(m_host.drawDocument(type->id, m_documents.path, m_documents.text, ui));
	EXPECT_EQ(m_documents.text, "- [x] Place the pickups\n- [x] Block out\n");
	ui = {};
	ui.answers["Remove done"] = {.value = {}, .changed = true};
	ASSERT_TRUE(m_host.drawDocument(type->id, m_documents.path, m_documents.text, ui));
	EXPECT_EQ(m_documents.text, "\n");
	EXPECT_FALSE(m_host.drawDocument("nope", m_documents.path, m_documents.text, ui));
	EXPECT_EQ(run("plugin.document_set", {{"path", std::string{"other.owlnote"}}, {"text", std::string{"x"}}})
					  .error()
					  .error,
			  CommandError::NotApplicable);
	EXPECT_EQ(run("plugin.document_set", {{"path", m_documents.path.string()}, {"text", std::string{"\n"}}})
					  .error()
					  .error,
			  CommandError::NotApplicable);

	// Importer: a text file becomes a `.owlnote` beside it; undo removes it.
	const auto source = m_root / "todo.txt";
	std::ofstream{source} << "Fix the door\n\n  \nAdd a lamp\n";
	EXPECT_NE(m_host.findImporter(source), nullptr);
	EXPECT_EQ(m_host.findImporter(source, "other.importer"), nullptr);
	EXPECT_EQ(m_host.findImporter("todo.md"), nullptr);
	const auto imported = run("asset.import", {{"path", std::string{"todo.txt"}}});
	ASSERT_TRUE(imported.has_value()) << imported.error().message;
	const auto target = m_root / "todo.owlnote";
	EXPECT_EQ(std::get<std::string>(imported->data.at("path")), target.generic_string());
	EXPECT_EQ(platform::fileToString(target), "- [ ] Fix the door\n- [ ] Add a lamp\n");
	m_undo.undo(*m_scene);
	EXPECT_FALSE(std::filesystem::exists(target));
	m_undo.redo(*m_scene);
	EXPECT_TRUE(std::filesystem::exists(target));
	std::ofstream{source} << "Only one\n";
	ASSERT_TRUE(run("asset.import", {{"path", source.string()}, {"importer", std::string{"demo.notes.text"}}}));
	EXPECT_EQ(platform::fileToString(target), "- [ ] Only one\n");
	m_undo.undo(*m_scene);
	EXPECT_EQ(platform::fileToString(target), "- [ ] Fix the door\n- [ ] Add a lamp\n");
	EXPECT_EQ(run("asset.import", {{"path", std::string{"missing.txt"}}}).error().error, CommandError::InvalidArgument);
	EXPECT_EQ(run("asset.import", {{"path", target.string()}}).error().error, CommandError::InvalidArgument);
	EXPECT_FALSE(m_host.runImporter("nope", source, "x").has_value());
}

TEST_F(PluginHostTest, TheTemplateScriptIsAWorkingPlugin) {
	ASSERT_TRUE(run("extension.create", {{"id", std::string{"my.tools"}}}));
	const auto created = run("extension.script", {{"path", std::string{"my.tools"}}, {"editor", true}});
	ASSERT_TRUE(created.has_value()) << created.error().message;
	const auto folder = m_root / "extensions" / "my.tools";
	EXPECT_TRUE(std::filesystem::exists(folder / "editor" / "my.tools" / "main.lua"));
	EXPECT_FALSE(std::filesystem::exists(folder / "scripts"));
	ASSERT_TRUE(run("extension.set", {{"path", folder.string()}, {"editor_script", std::string{}}}));
	EXPECT_EQ(load({folder}), 0u);
	EXPECT_TRUE(m_host.getPlugins().empty());
	ASSERT_TRUE(run("extension.set",
					{{"path", folder.string()}, {"editor_script", std::string{"editor/my.tools/main.lua"}}}));
	ASSERT_EQ(load({folder}), 1u) << errorsOf(m_host.find("my.tools"));
	FakeUi ui;
	ui.answers["Hello"] = {.value = {}, .changed = true};
	ASSERT_TRUE(m_host.drawPanel("my.tools.panel", ui));
	EXPECT_EQ(m_scene->getEntityCount(), 1u);
	ASSERT_TRUE(run("my.tools.hello", {}));
	EXPECT_EQ(m_scene->getEntityCount(), 2u);
}

TEST_F(PluginHostTest, AnImporterStaysInTheProject) {
	const auto pack =
			writePack("t.out", "nest.importer{id = \"t.out.up\", extension = \".up\", handler = \"up\"}\n"
							   "function up(args) return {path = \"../outside.txt\", text = \"x\"} end\n"
							   "nest.importer{id = \"t.out.none\", extension = \".none\", handler = \"none\"}\n"
							   "function none(args) return {} end\n"
							   "nest.importer{id = \"t.out.same\", extension = \".same\", handler = \"same\"}\n"
							   "function same(args) return {path = args.path, text = \"x\"} end\n");
	ASSERT_EQ(load({pack}), 1u) << errorsOf(m_host.find("t.out"));
	for (const auto* name: {"a.up", "a.none", "a.same"}) {
		std::ofstream{m_root / name} << "source";
		EXPECT_EQ(run("asset.import", {{"path", std::string{name}}}).error().error, CommandError::NotApplicable)
				<< name;
	}
	EXPECT_FALSE(std::filesystem::exists(m_root.parent_path() / "outside.txt"));
	EXPECT_EQ(platform::fileToString(m_root / "a.same"), "source");
	EXPECT_FALSE(m_undo.canUndo());
}

TEST_F(PluginHostTest, WriteTextFileCommandRestoresTheFile) {
	const auto file = m_root / "written.txt";
	WriteTextFileCommand created{file, std::nullopt, "after"};
	created.redo(*m_scene);
	EXPECT_EQ(platform::fileToString(file), "after");
	EXPECT_TRUE(created.description().contains("written.txt"));
	created.undo(*m_scene);
	EXPECT_FALSE(std::filesystem::exists(file));
	WriteTextFileCommand replaced{file, std::string{"before"}, "after"};
	replaced.redo(*m_scene);
	replaced.undo(*m_scene);
	EXPECT_EQ(platform::fileToString(file), "before");
}
