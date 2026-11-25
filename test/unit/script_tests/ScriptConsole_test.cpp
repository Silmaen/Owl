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
#include <variant>
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

TEST_F(ScriptConsoleTest, CallsAGlobalFunctionWithATable) {
	ScriptConsole console;
	ASSERT_EQ(console.run("-- a comment\nfunction double(args)\n  print('doubling')\n  return {value = args.value * 2, "
						  "name = args.name}\nend\nfunction broken(args) error('boom') end\nfunction silent() end\n"
						  "answer = 42",
						  "tools/calls.lua")
					  .status,
			  ScriptConsole::Status::Ok);
	EXPECT_TRUE(console.hasFunction("double"));
	EXPECT_FALSE(console.hasFunction("answer"));
	EXPECT_FALSE(console.hasFunction("missing"));
	EXPECT_FALSE(console.hasFunction("os.exit()"));
	EXPECT_EQ(console.getFunctionLine("double"), 2u);
	EXPECT_EQ(console.getFunctionLine("print"), 0u);
	EXPECT_EQ(console.getFunctionLine("missing"), 0u);

	const auto doubled = console.call("double", {{"value", int64_t{21}}, {"name", std::string{"x"}}});
	ASSERT_TRUE(doubled.ok) << doubled.error;
	EXPECT_EQ(std::get<int64_t>(doubled.fields.at("value")), 42);
	EXPECT_EQ(std::get<std::string>(doubled.fields.at("name")), "x");
	ASSERT_EQ(doubled.output.size(), 1u);
	EXPECT_EQ(doubled.output.front(), "doubling");

	ASSERT_EQ(console.run("function names(args) return {count = #args.names, back = args.names} end", "tools/names.lua")
					  .status,
			  ScriptConsole::Status::Ok);
	const auto listed = console.call("names", {{"names", std::vector<std::string>{"Tree", "Rock"}}});
	ASSERT_TRUE(listed.ok) << listed.error;
	EXPECT_EQ(std::get<int64_t>(listed.fields.at("count")), 2);
	EXPECT_EQ(std::get<std::vector<std::string>>(listed.fields.at("back")), (std::vector<std::string>{"Tree", "Rock"}));

	const auto nothing = console.call("silent", {});
	EXPECT_TRUE(nothing.ok);
	EXPECT_TRUE(nothing.fields.empty());

	const auto failed = console.call("broken", {});
	EXPECT_FALSE(failed.ok);
	EXPECT_TRUE(failed.error.contains("boom")) << failed.error;
	const auto missing = console.call("missing", {});
	EXPECT_FALSE(missing.ok);
	EXPECT_TRUE(missing.error.contains("missing"));
	// The globals carrying the tables do not stay.
	EXPECT_EQ(console.evaluate("__owl_call_args == nil and __owl_call_result == nil").output.front(), "true");
}
