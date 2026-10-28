/**
 * @file ScriptConsole_test.cpp
 * @author Silmaen
 * @date 28/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <script/ScriptConsole.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

using namespace owl;
using namespace owl::script;

namespace {

class ScriptConsoleTest : public testing::Test {
protected:
	void SetUp() override { core::Log::init(core::Log::Level::Off); }

	void TearDown() override { core::Log::invalidate(); }
};

}// namespace

TEST_F(ScriptConsoleTest, ExpressionsPrintTheirValueAndGlobalsStay) {
	ScriptConsole console;
	auto result = console.evaluate("x = 20");
	EXPECT_EQ(result.status, ScriptConsole::Status::Ok);
	EXPECT_TRUE(result.output.empty());
	result = console.evaluate("x + 22");
	ASSERT_EQ(result.output.size(), 1u);
	EXPECT_EQ(result.output.front(), "42");
	result = console.evaluate("print('a', 1)");
	ASSERT_FALSE(result.output.empty());
	EXPECT_EQ(result.output.front(), "a\t1");
}

TEST_F(ScriptConsoleTest, AnOpenBlockWaitsForItsEnd) {
	ScriptConsole console;
	EXPECT_EQ(console.evaluate("function twice(v)").status, ScriptConsole::Status::Incomplete);
	EXPECT_TRUE(console.hasPending());
	EXPECT_EQ(console.evaluate("return v * 2").status, ScriptConsole::Status::Incomplete);
	EXPECT_EQ(console.evaluate("end").status, ScriptConsole::Status::Ok);
	EXPECT_FALSE(console.hasPending());
	EXPECT_EQ(console.evaluate("twice(4)").output.front(), "8");
	EXPECT_EQ(console.evaluate("if true then").status, ScriptConsole::Status::Incomplete);
	console.cancelPending();
	EXPECT_FALSE(console.hasPending());
}

TEST_F(ScriptConsoleTest, ErrorsAreReportedAndTheSandboxHolds) {
	ScriptConsole console;
	auto result = console.evaluate("error('boom')");
	EXPECT_EQ(result.status, ScriptConsole::Status::Error);
	ASSERT_FALSE(result.output.empty());
	EXPECT_TRUE(result.output.front().contains("boom"));
	EXPECT_EQ(console.evaluate("x = = 1").status, ScriptConsole::Status::Error);
	EXPECT_EQ(console.evaluate("io.open('x')").status, ScriptConsole::Status::Error);
}

TEST_F(ScriptConsoleTest, HostFunctionsTakeATableAndReturnOne) {
	ScriptConsole console;
	std::map<std::string, HostValue> received;
	console.addFunction("editor.entity.create", "Create an entity.",
						[&received](const std::map<std::string, HostValue>& iArgs) -> HostResult {
							received = iArgs;
							return {.ok = true, .error = {}, .fields = {{"entity", int64_t{7}}, {"name", "Tree"}}};
						});
	console.addFunction("editor.fail", "Always fails.", [](const std::map<std::string, HostValue>&) -> HostResult {
		return {.ok = false, .error = "refused", .fields = {}};
	});
	const auto result = console.evaluate(R"(editor.entity.create{name = "Tree", size = 2.5, at = {1, 2}}.entity)");
	ASSERT_EQ(result.status, ScriptConsole::Status::Ok) << (result.output.empty() ? "" : result.output.front());
	EXPECT_EQ(result.output.front(), "7");
	EXPECT_EQ(std::get<std::string>(received.at("name")), "Tree");
	EXPECT_DOUBLE_EQ(std::get<double>(received.at("size")), 2.5);
	EXPECT_EQ(std::get<std::vector<double>>(received.at("at")).size(), 2u);
	const auto failed = console.evaluate("editor.fail{}");
	EXPECT_EQ(failed.status, ScriptConsole::Status::Error);
	EXPECT_TRUE(failed.output.front().contains("refused"));

	const auto root = console.help("");
	ASSERT_FALSE(root.empty());
	EXPECT_TRUE(root.front().contains("editor"));
	EXPECT_TRUE(console.help("editor.entity").front().contains("Create an entity."));
	EXPECT_EQ(console.evaluate("help(editor.entity)").output, console.help("editor.entity"));
	EXPECT_EQ(console.run("total = 3 print(total)", "tools/count.lua").output.front(), "3");
	EXPECT_EQ(console.run("error('x')", "tools/bad.lua").status, ScriptConsole::Status::Error);
}
