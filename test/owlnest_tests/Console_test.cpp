/**
 * @file Console_test.cpp
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include "nestTestHelper.h"
#include "session/Console.h"
#include "session/HeadlessSession.h"

#include <app/Application.h>
#include <scene/Entity.h>
#include <scene/component/CircleRenderer.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

using namespace owl;
using namespace owl::nest;
using namespace owl::nest::session;

namespace {

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wweak-vtables")
class ConsoleTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_app = mkShared<app::Application>(app::AppParams{.args = nullptr,
														  .frameLogFrequency = 0,
														  .name = "consoleTest",
														  .assetsPattern = "",
														  .icon = "",
														  .width = 0,
														  .height = 0,
														  .argCount = 0,
														  .renderer = renderer::gpu::RenderAPI::Type::Null,
														  .hasGui = false,
														  .useDebugging = false,
														  .isDummy = true});
	}

	void TearDown() override {
		app::Application::invalidate();
		m_app.reset();
		core::Log::invalidate();
	}

	static auto samplePath() -> std::filesystem::path {
		return owl::test::getRootPath() / "sample_project" / "scenes" / "platformer_house.owl";
	}

	static auto last(const std::vector<ConsoleOutput>& iOutput) -> ConsoleOutput {
		return iOutput.empty() ? ConsoleOutput{} : iOutput.back();
	}

	/// Dummy application on the Null renderer.
	shared<app::Application> m_app;
};
OWL_DIAG_POP

}// namespace

TEST_F(ConsoleTest, RunsLinesWithResultReferences) {
	HeadlessSession session;
	session.newScene();
	Console console{session};
	const auto created = console.submit("entity.create name=\"Tree\"");
	ASSERT_EQ(created.size(), 2u);
	EXPECT_EQ(created[0].kind, ConsoleLineKind::Input);
	EXPECT_EQ(created[1].kind, ConsoleLineKind::Result);
	EXPECT_TRUE(created[1].text.starts_with("$1  Create 'Tree'")) << created[1].text;
	EXPECT_EQ(created[1].journalIndex, 1u);
	const auto leaf = last(console.submit("entity.create name=Leaf parent=$1"));
	ASSERT_EQ(leaf.kind, ConsoleLineKind::Result) << leaf.text;
	const auto& scene = *session.getSessionScene();
	const auto tree = scene.findEntityByUUID(session.getJournal().resolveReference(1).value());
	const auto child = scene.findEntityByUUID(session.getJournal().resolveReference(std::nullopt).value());
	EXPECT_EQ(owl::nest::test::parentOf(child), tree.getUUID());

	EXPECT_EQ(last(console.submit("entity.rename entity=$7 name=X")).kind, ConsoleLineKind::Error);
	const auto failed = last(console.submit("entity.rename entity=\"Nobody\" name=X"));
	EXPECT_EQ(failed.kind, ConsoleLineKind::Error);
	EXPECT_EQ(failed.journalIndex, 3u);
	EXPECT_TRUE(console.submit("# only a comment").size() == 1u);
	EXPECT_TRUE(console.submit("   ").empty());
	EXPECT_EQ(session.getJournal().filter(commands::CommandSource::Console).size(), 3u);

	const auto childId = child.getUUID();
	ASSERT_EQ(last(console.submit("edit.undo")).kind, ConsoleLineKind::Result);
	EXPECT_FALSE(scene.findEntityByUUID(childId));
}

TEST_F(ConsoleTest, PrintsTheHelp) {
	HeadlessSession session;
	Console console{session};
	const auto all = console.submit("help");
	EXPECT_EQ(all.size(), session.getRegistry().getCommands().size() + 2);
	const auto one = last(console.submit("help entity.create"));
	EXPECT_EQ(one.kind, ConsoleLineKind::Help);
	EXPECT_TRUE(one.text.starts_with("entity.create [name=<text>]")) << one.text;
	EXPECT_EQ(last(console.submit("help entity.explode")).kind, ConsoleLineKind::Error);
}

TEST_F(ConsoleTest, CompletesNamesArgumentsAndChoices) {
	HeadlessSession session;
	const Console console{session};
	const auto commands = console.complete("enti");
	EXPECT_EQ(commands.completed, "entity.");
	EXPECT_EQ(commands.candidates.size(), 6u);
	EXPECT_EQ(console.complete("entity.cr").completed, "entity.create ");
	EXPECT_EQ(console.complete("he").completed, "help ");
	EXPECT_EQ(console.complete("help macro").completed, "help macro.run ");
	EXPECT_EQ(console.complete("entity.create na").completed, "entity.create name=");
	EXPECT_EQ(console.complete("entity.create name=\"A\" ").candidates, std::vector<std::string>{"parent="});
	EXPECT_EQ(console.complete("entity.delete entity=1 children=t").completed, "entity.delete entity=1 children=true ");
	EXPECT_TRUE(console.complete("nothing.at all").candidates.empty());
	EXPECT_EQ(console.complete("nothing.at all").completed, "nothing.at all");
}

TEST_F(ConsoleTest, KeepsTheHistory) {
	HeadlessSession session;
	Console console{session};
	EXPECT_FALSE(console.previousHistory().has_value());
	std::ignore = console.submit("help");
	std::ignore = console.submit("help");
	std::ignore = console.submit("help entity.create");
	EXPECT_EQ(console.getHistory().size(), 2u);
	EXPECT_EQ(console.previousHistory(), "help entity.create");
	EXPECT_EQ(console.previousHistory(), "help");
	EXPECT_EQ(console.previousHistory(), "help");
	EXPECT_EQ(console.nextHistory(), "help entity.create");
	EXPECT_EQ(console.nextHistory(), "");
	EXPECT_FALSE(console.nextHistory().has_value());
	std::vector<std::string> restored;
	for (size_t i = 0; i < Console::maxHistory + 5; ++i) restored.push_back(std::format("line {}", i));
	console.setHistory(restored);
	EXPECT_EQ(console.getHistory().size(), Console::maxHistory);
	EXPECT_EQ(console.previousHistory(), std::format("line {}", Console::maxHistory + 4));
}

TEST_F(ConsoleTest, ReportsCommandsRefusedDuringPlay) {
	HeadlessSession session;
	ASSERT_TRUE(session.loadScene(samplePath()).has_value());
	Console console{session};
	ASSERT_TRUE(session.startPlay({.frames = 1, .stopAtEnd = false}).has_value());
	ASSERT_TRUE(session.isScenePlaying());
	const auto refused = last(console.submit("entity.create name=Crate"));
	EXPECT_EQ(refused.kind, ConsoleLineKind::Refused);
	EXPECT_TRUE(refused.text.starts_with("refused during Play")) << refused.text;
	const commands::Macro macro{{"entity.create"}};
	EXPECT_FALSE(session.replay(macro, "Macro").has_value());
	ASSERT_TRUE(session.stopPlay().has_value());
}

TEST_F(ConsoleTest, HeadlessReplayOfAMacroFile) {
	HeadlessSession session;
	ASSERT_TRUE(session.loadScene(samplePath()).has_value());
	const auto before = session.getSessionScene()->getAllEntities().size();
	const auto path = std::filesystem::temp_directory_path() /
					  std::format("owl_console_{}.owlmacro", static_cast<uint64_t>(core::UUID{}));
	const commands::Macro macro{{"entity.create name=\"Crate\"",
								 "component.add entity=$1 component=\"Circle Renderer\"",
								 "entity.set_transform entity=$1 translation=(2, 3, 0)"}};
	ASSERT_TRUE(macro.save(path).has_value());
	Console console{session};
	const auto result = last(console.submit(std::format("macro.run path=\"{}\"", path.generic_string())));
	std::filesystem::remove(path);
	ASSERT_EQ(result.kind, ConsoleLineKind::Result) << result.text;
	const auto& scene = *session.getSessionScene();
	EXPECT_EQ(scene.getAllEntities().size(), before + 1);
	const auto crate = scene.findEntityByUUID(session.getJournal().resolveReference(std::nullopt).value());
	ASSERT_TRUE(crate);
	EXPECT_TRUE(crate.hasComponent<scene::component::CircleRenderer>());
	EXPECT_FLOAT_EQ(scene.getWorldTransform(crate).translation().x(), 2.f);
	EXPECT_EQ(session.getJournal().filter(commands::CommandSource::Replay).size(), 3u);

	ASSERT_TRUE(session.execute("edit.undo", {}).has_value());
	EXPECT_EQ(scene.getAllEntities().size(), before);
	EXPECT_EQ(session.getJournal().getEntries().back().source, commands::CommandSource::Mcp);
	ASSERT_TRUE(session.replay(macro, "Macro again").has_value());
	EXPECT_EQ(scene.getAllEntities().size(), before + 1);
}
