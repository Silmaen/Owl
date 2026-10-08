/**
 * @file ScenarioTest_test.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include "ScenarioTest.h"

#include <input/KeyCodes.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/component/components.h>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <string>

using namespace owl;
using namespace owl::nest::runner;

namespace {

class ScenarioFile : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_dir = std::filesystem::temp_directory_path() / "owl_scenario_test";
		std::filesystem::remove_all(m_dir);
		std::filesystem::create_directories(m_dir);
		std::ofstream(m_dir / "level.owl") << "Scene: untitled\n";
	}

	void TearDown() override {
		std::filesystem::remove_all(m_dir);
		core::Log::invalidate();
	}

	[[nodiscard]] auto write(const std::string& iBody) const -> std::filesystem::path {
		const auto file = m_dir / "case.owltest";
		std::ofstream(file) << "scene: level.owl\n" << iBody;
		return file;
	}

	std::filesystem::path m_dir;
};

auto makeCheck(const std::string& iField, const ScenarioCheck::Op iOp, const double iValue) -> ScenarioCheck {
	ScenarioCheck check;
	check.field = iField;
	check.op = iOp;
	check.number = iValue;
	return check;
}

}// namespace

TEST(ScenarioTest, ParseKeyNamesAndCodes) {
	EXPECT_EQ(ScenarioTest::parseKey("A"), input::key::A);
	EXPECT_EQ(ScenarioTest::parseKey("Z"), input::key::Z);
	EXPECT_EQ(ScenarioTest::parseKey("D7"), input::key::D7);
	EXPECT_EQ(ScenarioTest::parseKey("Right"), input::key::Right);
	EXPECT_EQ(ScenarioTest::parseKey("Escape"), input::key::Escape);
	EXPECT_EQ(ScenarioTest::parseKey("262"), 262u);
	EXPECT_FALSE(ScenarioTest::parseKey("Hyper").has_value());
	EXPECT_FALSE(ScenarioTest::parseKey("").has_value());
	EXPECT_FALSE(ScenarioTest::parseKey("a").has_value());
}

TEST_F(ScenarioFile, LoadsEveryStepKind) {
	const auto scenario = ScenarioTest::load(write(R"(timestep_ms: 10
steps:
  - frames: 3
  - input: {keys: [Right, Space], mouse: Left, mouse_pos: [10, 20], frames: 2}
  - expect: {player: true, translation.x: {greater: 1.5}, exists: true}
  - expect: {entity: Coin, translation.y: {equals: 2, tolerance: 0.5}}
  - expect: {gamestate: level, equals: castle}
)"));
	ASSERT_TRUE(scenario.has_value()) << scenario.error();
	EXPECT_EQ(scenario->getScene().filename(), "level.owl");
	EXPECT_NEAR(scenario->getTimeStep().getMilliseconds(), 10.0, 1e-3);
	const auto& steps = scenario->getSteps();
	ASSERT_EQ(steps.size(), 5u);
	EXPECT_EQ(steps[0].frames, 3u);
	EXPECT_EQ(steps[1].frames, 2u);
	EXPECT_EQ(steps[1].keys.size(), 2u);
	EXPECT_EQ(steps[1].mouseButtons.size(), 1u);
	ASSERT_TRUE(steps[1].mousePos.has_value());
	EXPECT_TRUE(steps[2].player);
	EXPECT_EQ(steps[2].checks.size(), 2u);
	EXPECT_EQ(steps[3].entity, "Coin");
	EXPECT_NEAR(steps[3].checks.front().tolerance, 0.5, 1e-9);
	EXPECT_EQ(steps[4].gameState, "level");
	EXPECT_EQ(steps[4].checks.front().text, std::optional<std::string>{"castle"});
}

TEST_F(ScenarioFile, RejectsBrokenFiles) {
	EXPECT_FALSE(ScenarioTest::load(m_dir / "missing.owltest").has_value());
	EXPECT_FALSE(ScenarioTest::load(write("steps: []\n")).has_value());
	EXPECT_FALSE(ScenarioTest::load(write("steps:\n  - input: {key: Hyper}\n")).has_value());
	EXPECT_FALSE(ScenarioTest::load(write("steps:\n  - expect: {translation.x: 1}\n")).has_value());
	EXPECT_FALSE(ScenarioTest::load(write("steps:\n  - expect: {entity: A, translation.x: {near: 1}}\n")).has_value());
	EXPECT_FALSE(ScenarioTest::load(write("steps:\n  - jump: 3\n")).has_value());
	std::ofstream(m_dir / "noscene.owltest") << "steps:\n  - frames: 1\n";
	EXPECT_FALSE(ScenarioTest::load(m_dir / "noscene.owltest").has_value());
	std::ofstream(m_dir / "badscene.owltest") << "scene: nowhere.owl\nsteps:\n  - frames: 1\n";
	EXPECT_FALSE(ScenarioTest::load(m_dir / "badscene.owltest").has_value());
}

TEST(ScenarioTest, ChecksEntitiesAndGameState) {
	core::Log::init(core::Log::Level::Off);
	scene::Scene scn;
	auto coin = scn.createEntity("Coin");
	coin.getComponent<scene::component::Transform>().transform.translation() = {2.f, 3.f, 0.f};
	scn.getGameState().set("score", int64_t{10});
	scn.getGameState().set("level", std::string{"castle"});
	const auto expect = [](const std::string& iEntity, const std::string& iField, const ScenarioCheck::Op iOp,
						   const double iValue) -> ScenarioStep {
		ScenarioStep step;
		step.entity = iEntity;
		step.checks.push_back(makeCheck(iField, iOp, iValue));
		return step;
	};
	EXPECT_EQ(ScenarioTest::check(expect("Coin", "translation.x", ScenarioCheck::Op::Equals, 2.0), scn), "");
	EXPECT_EQ(ScenarioTest::check(expect("Coin", "translation.y", ScenarioCheck::Op::Greater, 2.5), scn), "");
	EXPECT_EQ(ScenarioTest::check(expect("Coin", "exists", ScenarioCheck::Op::Equals, 1.0), scn), "");
	EXPECT_EQ(ScenarioTest::check(expect("Ghost", "exists", ScenarioCheck::Op::Equals, 0.0), scn), "");
	EXPECT_NE(ScenarioTest::check(expect("Coin", "translation.x", ScenarioCheck::Op::Less, 1.0), scn), "");
	EXPECT_NE(ScenarioTest::check(expect("Ghost", "translation.x", ScenarioCheck::Op::Equals, 0.0), scn), "");
	EXPECT_NE(ScenarioTest::check(expect("Coin", "colour.r", ScenarioCheck::Op::Equals, 0.0), scn), "");
	EXPECT_NE(ScenarioTest::check(expect("Nobody", "translation.x", ScenarioCheck::Op::Equals, 0.0), scn)
					  .find("not found"),
			  std::string::npos);

	ScenarioStep score;
	score.gameState = "score";
	score.checks.push_back(makeCheck("value", ScenarioCheck::Op::Equals, 10.0));
	EXPECT_EQ(ScenarioTest::check(score, scn), "");
	score.checks.front().op = ScenarioCheck::Op::Greater;
	EXPECT_NE(ScenarioTest::check(score, scn), "");
	ScenarioStep level;
	level.gameState = "level";
	level.checks.push_back(makeCheck("value", ScenarioCheck::Op::Equals, 0.0));
	level.checks.front().text = "castle";
	EXPECT_EQ(ScenarioTest::check(level, scn), "");
	level.checks.front().text = "dungeon";
	EXPECT_NE(ScenarioTest::check(level, scn), "");
	level.gameState = "unset";
	EXPECT_NE(ScenarioTest::check(level, scn), "");
	core::Log::invalidate();
}

TEST_F(ScenarioFile, RunnerFailsOnABrokenExpectation) {
#ifndef OWL_RUNNER_EXECUTABLE
	GTEST_SKIP() << "OwlRunner is not built (OWL_BUILD_NEST=OFF).";
#else
	const auto scene = test::getRootPath() / "sample_project" / "scenes" / "platformer_house.owl";
	const auto file = m_dir / "fail.owltest";
	std::ofstream(file) << std::format(
			"scene: {}\nsteps:\n  - frames: 2\n  - expect: {{entity: NoSuchEntity, exists: true}}\n",
			scene.generic_string());
	auto command = std::format(R"("{}" --scenario "{}")", OWL_RUNNER_EXECUTABLE, file.string());
#ifdef OWL_PLATFORM_WINDOWS
	command = std::format(R"("{}")", command);
#endif
	// NOLINTNEXTLINE(bugprone-command-processor) The scenario is played by the runner process itself.
	EXPECT_NE(std::system(command.c_str()), 0);
#endif
}
