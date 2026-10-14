/**
 * @file Macro_test.cpp
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "commands/Macro.h"
#include "commands/CommandJournal.h"
#include "nestTestHelper.h"
#include "testHelper.h"

#include <scene/component/Tag.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <vector>

using namespace owl;
using namespace owl::nest;
using namespace owl::nest::commands;

namespace {

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wweak-vtables")
class MacroTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_registry.setJournal(&m_journal);
	}

	void TearDown() override {
		for (const auto& file: m_files) std::filesystem::remove(file);
		core::Log::invalidate();
	}

	auto context(const CommandSource iSource = CommandSource::Console) -> CommandContext {
		return {.scene = m_scene, .undo = &m_undo, .playing = false, .source = iSource};
	}

	auto run(const std::string& iName, const CommandArgs& iArgs, const CommandSource iSource = CommandSource::Console)
			-> expected<CommandResult, CommandFailure> {
		return m_registry.execute(iName, iArgs, context(iSource));
	}

	auto tempFile(const std::string& iName) -> std::filesystem::path {
		auto path = std::filesystem::temp_directory_path() /
					std::format("owl_macro_{}_{}", static_cast<uint64_t>(core::UUID{}), iName);
		m_files.push_back(path);
		return path;
	}

	[[nodiscard]] auto count() const -> size_t { return m_scene->registry.view<scene::component::Tag>().size(); }

	/// Registry running the commands.
	CommandRegistry m_registry;
	/// Journal of the commands.
	CommandJournal m_journal;
	/// Scene changed by the commands.
	shared<scene::Scene> m_scene = mkShared<scene::Scene>();
	/// Undo history of the scene.
	SceneUndoManager m_undo;
	/// Files to remove.
	std::vector<std::filesystem::path> m_files;
};
OWL_DIAG_POP

auto crates() -> Macro {
	return Macro{{"# A crate and its lid.", "entity.create name=\"Crate\"", "", "entity.create name=\"Lid\" parent=$1",
				  "entity.set_transform entity=$_ translation=(0, 1, 0)  # on top"}};
}

}// namespace

TEST_F(MacroTest, JournalRecordsEverySourceAndFailure) {
	ASSERT_TRUE(run("entity.create", {{"name", std::string{"A"}}}, CommandSource::Gesture).has_value());
	ASSERT_TRUE(run("entity.create", {{"name", std::string{"B"}}}, CommandSource::Palette).has_value());
	EXPECT_FALSE(
			run("entity.rename", {{"entity", std::string{"Nobody"}}, {"name", std::string{"X"}}}, CommandSource::Mcp)
					.has_value());
	const CommandTarget target{.registry = &m_registry, .scene = m_scene, .undo = &m_undo};
	ASSERT_TRUE(target.execute("entity.create", {}).has_value());
	const auto entries = m_journal.getEntries();
	ASSERT_EQ(entries.size(), 4u);
	EXPECT_EQ(entries[0].index, 1u);
	EXPECT_EQ(entries[0].source, CommandSource::Gesture);
	EXPECT_EQ(entries[0].line, "entity.create name=\"A\"");
	EXPECT_EQ(entries[1].source, CommandSource::Palette);
	EXPECT_EQ(entries[2].source, CommandSource::Mcp);
	EXPECT_FALSE(entries[2].succeeded);
	EXPECT_EQ(entries[2].failure.error, CommandError::UnknownEntity);
	EXPECT_EQ(entries[3].source, CommandSource::Gesture);
	EXPECT_EQ(m_journal.filter(CommandSource::Gesture).size(), 2u);
	EXPECT_EQ(m_journal.filter(CommandSource::Replay).size(), 0u);
	EXPECT_EQ(m_journal.resolveReference(1).value(), entries[0].result.entity);
	EXPECT_EQ(m_journal.resolveReference(std::nullopt).value(), entries[3].result.entity);
	EXPECT_FALSE(m_journal.resolveReference(3).has_value());
	EXPECT_FALSE(m_journal.resolveReference(9).has_value());
	EXPECT_EQ(getSourceName(CommandSource::Mcp), "mcp");
	m_journal.clear();
	EXPECT_TRUE(m_journal.getEntries().empty());
	EXPECT_EQ(m_journal.getNextIndex(), 5u);
}

TEST_F(MacroTest, ReplayIsOneUndoStep) {
	ASSERT_TRUE(run("entity.create", {{"name", std::string{"Before"}}}).has_value());
	const auto report = crates().replay(m_registry, context(), "Macro crates");
	ASSERT_TRUE(report.has_value()) << report.error().message;
	EXPECT_EQ(report->commands, 3u);
	EXPECT_EQ(count(), 3u);
	const auto lid = m_scene->findEntityByUUID(report->lastEntity);
	ASSERT_TRUE(lid);
	EXPECT_EQ(lid.getName(), "Lid");
	EXPECT_FLOAT_EQ(lid.getComponent<scene::component::Transform>().transform.translation().y(), 1.f);
	EXPECT_EQ(m_scene->findEntityByUUID(owl::nest::test::parentOf(lid)).getName(), "Crate");
	EXPECT_EQ(m_journal.filter(CommandSource::Replay).size(), 3u);
	EXPECT_EQ(m_undo.undoDescription(), "Macro crates");

	m_undo.undo(*m_scene);
	EXPECT_EQ(count(), 1u);
	EXPECT_EQ(m_undo.undoDescription(), "Create 'Before'");
	m_undo.redo(*m_scene);
	EXPECT_EQ(count(), 3u);
	EXPECT_TRUE(m_scene->findEntityByUUID(report->lastEntity));
}

TEST_F(MacroTest, FailingLineStopsAndUndoesTheReplay) {
	ASSERT_TRUE(run("entity.create", {{"name", std::string{"Before"}}}).has_value());
	const Macro broken{{"entity.create name=\"One\"", "entity.create name=\"Two\"", "# skipped",
						R"(entity.rename entity="Nobody" name="X")", R"(entity.create name="Never")"}};
	const auto report = broken.replay(m_registry, context(), "Macro broken");
	ASSERT_FALSE(report.has_value());
	EXPECT_EQ(report.error().error, MacroError::CommandFailed);
	EXPECT_EQ(report.error().line, 4u);
	EXPECT_TRUE(report.error().message.starts_with("line 4:")) << report.error().message;
	EXPECT_EQ(count(), 1u);
	EXPECT_EQ(m_undo.undoDescription(), "Create 'Before'");
	EXPECT_FALSE(m_undo.isGrouping());

	const Macro unreadable{{"entity.create name=\"One\"", "entity.create colour=#fff"}};
	const auto invalid = unreadable.replay(m_registry, context(), "Macro unreadable");
	ASSERT_FALSE(invalid.has_value());
	EXPECT_EQ(invalid.error().error, MacroError::InvalidLine);
	EXPECT_EQ(invalid.error().line, 2u);
	EXPECT_EQ(count(), 1u);

	const Macro dangling{{"entity.delete entity=$3"}};
	EXPECT_EQ(dangling.replay(m_registry, context(), "Macro dangling").error().error, MacroError::InvalidLine);
	EXPECT_EQ(Macro{}.replay(m_registry, {.scene = nullptr, .undo = nullptr}, "none").error().error,
			  MacroError::NoScene);
}

TEST_F(MacroTest, AssetRoundTripFromTheJournal) {
	ASSERT_TRUE(run("entity.create", {{"name", std::string{"Tree"}}}).has_value());
	const auto tree = m_journal.getEntries().back().result.entity;
	ASSERT_TRUE(run("entity.create",
					{{"name", std::string{"Leaf"}}, {"parent", static_cast<int64_t>(static_cast<uint64_t>(tree))}})
						.has_value());
	const auto leaf = m_journal.getEntries().back().result.entity;
	EXPECT_FALSE(run("entity.rename", {{"entity", std::string{"Nobody"}}, {"name", std::string{"X"}}}).has_value());
	ASSERT_TRUE(run("entity.set_transform", {{"entity", static_cast<int64_t>(static_cast<uint64_t>(leaf))},
											 {"scale", math::vec3{2.f, 2.f, 2.f}}})
						.has_value());
	std::vector<const JournalEntry*> selection;
	for (const auto& entry: m_journal.getEntries()) selection.push_back(&entry);
	const auto macro = Macro::fromJournal(selection, m_registry);
	ASSERT_EQ(macro.getLines().size(), 3u);
	EXPECT_EQ(macro.getLines()[0], "entity.create name=\"Tree\"");
	EXPECT_EQ(macro.getLines()[1], "entity.create name=\"Leaf\" parent=$1");
	EXPECT_EQ(macro.getLines()[2], "entity.set_transform entity=$2 scale=(2, 2, 2)");

	const auto path = tempFile(std::string{"tree"} + std::string{Macro::extension});
	ASSERT_TRUE(macro.save(path).has_value());
	const auto loaded = Macro::load(path);
	ASSERT_TRUE(loaded.has_value());
	EXPECT_EQ(loaded->getLines(), macro.getLines());

	const auto fresh = mkShared<scene::Scene>();
	SceneUndoManager freshUndo;
	const auto report = loaded->replay(m_registry, {.scene = fresh, .undo = &freshUndo}, "Macro tree");
	ASSERT_TRUE(report.has_value()) << report.error().message;
	const auto copy = fresh->findEntityByUUID(report->lastEntity);
	ASSERT_TRUE(copy);
	EXPECT_EQ(copy.getName(), "Leaf");
	EXPECT_EQ(fresh->findEntityByUUID(owl::nest::test::parentOf(copy)).getName(), "Tree");
	EXPECT_FLOAT_EQ(copy.getComponent<scene::component::Transform>().transform.scale().x(), 2.f);

	EXPECT_EQ(Macro::load(tempFile("missing.owlmacro")).error().error, MacroError::FileNotFound);
}

TEST_F(MacroTest, MacroRunCommandReplaysAFile) {
	const auto path = tempFile("crates.owlmacro");
	ASSERT_TRUE(crates().save(path).has_value());
	const auto result = run("macro.run", {{"path", path.string()}});
	ASSERT_TRUE(result.has_value()) << result.error().message;
	EXPECT_EQ(count(), 2u);
	EXPECT_EQ(result->data.at("commands"), ArgValue{int64_t{3}});
	EXPECT_EQ(m_journal.getEntries().back().name, "macro.run");
	EXPECT_EQ(m_journal.getEntries().back().source, CommandSource::Console);
	EXPECT_EQ(m_journal.resolveReference(std::nullopt).value(), result->entity);
	ASSERT_TRUE(run("edit.undo", {}).has_value());
	EXPECT_EQ(count(), 0u);

	const auto self = tempFile("self.owlmacro");
	ASSERT_TRUE(Macro{{std::format("macro.run path=\"{}\"", self.generic_string())}}.save(self).has_value());
	const auto loop = run("macro.run", {{"path", self.string()}});
	ASSERT_FALSE(loop.has_value());
	EXPECT_TRUE(loop.error().message.contains("deeper")) << loop.error().message;
	EXPECT_FALSE(m_undo.isGrouping());
	EXPECT_FALSE(run("macro.run", {{"path", std::string{"no/such.owlmacro"}}}).has_value());
	EXPECT_EQ(run("macro.run", {{"path", path.string()}}, CommandSource::Mcp).value().data.at("commands"),
			  ArgValue{int64_t{3}});
	EXPECT_EQ(m_registry
					  .execute("macro.run", {{"path", path.string()}},
							   {.scene = m_scene, .undo = &m_undo, .playing = true})
					  .error()
					  .error,
			  CommandError::RefusedDuringPlay);
}

TEST(UndoGroup, GroupsNestAndStayInsideTheirFloor) {
	core::Log::init(core::Log::Level::Off);
	const CommandRegistry registry;
	auto scene = mkShared<scene::Scene>();
	SceneUndoManager undo;
	const auto create = [&](const std::string& iName) -> void {
		ASSERT_TRUE(registry.execute("entity.create", {{"name", iName}}, scene, undo).has_value());
	};
	create("Outside");
	undo.beginGroup("Outer");
	create("A");
	EXPECT_FALSE(registry.execute("edit.redo", {}, scene, undo).has_value());
	ASSERT_TRUE(registry.execute("edit.undo", {}, scene, undo).has_value());
	EXPECT_FALSE(registry.execute("edit.undo", {}, scene, undo).has_value());
	ASSERT_TRUE(registry.execute("edit.redo", {}, scene, undo).has_value());
	undo.beginGroup("Inner");
	create("B");
	create("C");
	undo.endGroup();
	undo.beginGroup("Empty");
	undo.endGroup();
	undo.endGroup();
	EXPECT_EQ(scene->getAllEntities().size(), 4u);
	EXPECT_EQ(undo.undoDescription(), "Outer");
	undo.undo(*scene);
	EXPECT_EQ(scene->getAllEntities().size(), 1u);
	EXPECT_EQ(undo.undoDescription(), "Create 'Outside'");
	undo.redo(*scene);
	EXPECT_EQ(scene->getAllEntities().size(), 4u);
	undo.endGroup();
	undo.cancelGroup(*scene);
	EXPECT_EQ(scene->getAllEntities().size(), 4u);
	core::Log::invalidate();
}
