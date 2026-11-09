/**
 * @file ScriptTypeChecker_test.cpp
 * @author Silmaen
 * @date 09/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <script/ScriptInstance.h>
#include <script/ScriptTypeChecker.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

using namespace owl;
using namespace owl::script;

namespace {

class ScriptTypeCheckerFixture : public testing::Test {
protected:
	static void SetUpTestSuite() {
		core::Log::init(core::Log::Level::Off);
		s_checker = mkUniq<ScriptTypeChecker>();
	}

	static void TearDownTestSuite() {
		s_checker.reset();
		core::Log::invalidate();
	}

	static auto errors(const std::string_view iSource) -> std::vector<ScriptDiagnostic> {
		auto all = s_checker->check(iSource, "test.lua");
		std::erase_if(all, [](const ScriptDiagnostic& iDiagnostic) -> bool {
			return iDiagnostic.severity != ScriptDiagnostic::Severity::Error;
		});
		return all;
	}

	static auto describe(const std::vector<ScriptDiagnostic>& iDiagnostics) -> std::string {
		std::string text;
		for (const auto& diagnostic: iDiagnostics)
			text += std::format("\n  {}:{}: {}", diagnostic.line, diagnostic.column, diagnostic.message);
		return text;
	}

	inline static uniq<ScriptTypeChecker> s_checker;
};

auto readText(const std::filesystem::path& iPath) -> std::string {
	std::ifstream file{iPath, std::ios::binary};
	return {std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
}

}// namespace

TEST(ScriptTypeChecker, DefinitionsDeclareEveryTable) {
	const auto definitions = getScriptDefinitions();
	EXPECT_NE(definitions.find("declare transform: {"), std::string::npos);
	EXPECT_NE(definitions.find("\tget_position: ("), std::string::npos);
	EXPECT_NE(definitions.find(") -> (number, number, number),"), std::string::npos);
	EXPECT_NE(definitions.find("declare __owl_math: {"), std::string::npos);
	EXPECT_NE(definitions.find("declare entity_id: number"), std::string::npos);
}

TEST_F(ScriptTypeCheckerFixture, ACorrectScriptHasNoError) {
	ASSERT_TRUE(s_checker->isReady());
	const auto found = errors(R"(properties = {
	{ name = "speed", type = "float", default = 2 },
}

local function step(dt: number)
	local x, y, z = transform.get_position(entity_id)
	transform.set_position(entity_id, x + speed * dt, y, z)
	gamestate.set("x", math.sqrt(x * x) + math.random(1, 3))
	local keys = table.sorted_keys({ a = 1 })
	log.info(tr("hello") .. #keys)
end

function on_update(dt)
	step(dt)
end
)");
	EXPECT_TRUE(found.empty()) << describe(found);
}

TEST_F(ScriptTypeCheckerFixture, MisusesAreReportedWhereTheyAre) {
	const auto unknown = errors("function on_create()\n\tgamestat.set(\"x\", 1)\nend\n");
	ASSERT_EQ(unknown.size(), 1u) << describe(unknown);
	EXPECT_EQ(unknown.front().line, 2u);
	EXPECT_EQ(unknown.front().column, 2u);
	EXPECT_NE(unknown.front().message.find("gamestat"), std::string::npos);

	const auto field = errors("function on_create()\n\ttransform.get_positon(entity_id)\nend\n");
	ASSERT_EQ(field.size(), 1u) << describe(field);
	EXPECT_NE(field.front().message.find("get_positon"), std::string::npos);

	const auto argument = errors("function on_create()\n\ttransform.get_position(\"door\")\nend\n");
	EXPECT_EQ(argument.size(), 1u) << describe(argument);

	const auto count = errors("function on_create()\n\ttransform.set_position(entity_id, 1)\nend\n");
	EXPECT_EQ(count.size(), 1u) << describe(count);

	const auto strict = errors("--!strict\nlocal n: number = \"text\"\n");
	EXPECT_EQ(strict.size(), 1u) << describe(strict);

	const auto syntax = errors("function on_create(\n");
	EXPECT_FALSE(syntax.empty());
}

TEST_F(ScriptTypeCheckerFixture, LintWarningsAreWarnings) {
	const auto all = s_checker->check("local unused = 1\nfunction on_create()\nend\n", "lint.lua");
	EXPECT_TRUE(std::ranges::any_of(all, [](const ScriptDiagnostic& iDiagnostic) -> bool {
		return iDiagnostic.severity == ScriptDiagnostic::Severity::Warning && iDiagnostic.line == 1;
	}));
	EXPECT_TRUE(errors("local unused = 1\n").empty());
}

TEST_F(ScriptTypeCheckerFixture, EveryGlobalOfTheEngineIsDeclared) {
	const std::string script = "function on_create()\n"
							   "\tlocal names = {}\n"
							   "\tfor name in pairs(_G) do table.insert(names, name) end\n"
							   "\tglobals = table.concat(names, \" \")\n"
							   "end\n";
	const std::vector<uint8_t> data(script.begin(), script.end());
	const ScriptInstance instance;
	ASSERT_TRUE(instance.createFromBuffer(data, "globals", 0));
	instance.onCreate();
	const auto names = instance.getPropertyString("globals").value_or("");
	ASSERT_FALSE(names.empty());
	std::istringstream stream{names};
	size_t checked = 0;
	for (std::string name; stream >> name;) {
		if (name == "globals" || name == "on_create" || name == "_G")
			continue;
		const auto found = errors(std::format("local _ = {}\n", name));
		EXPECT_TRUE(found.empty()) << name << describe(found);
		++checked;
	}
	EXPECT_GT(checked, 30u);
}

TEST_F(ScriptTypeCheckerFixture, ShippedScriptsHaveNoTypeError) {
	const auto root = test::getRootPath();
	s_checker->addModuleFolder(root / "engine_demo");
	for (const auto& sample: std::filesystem::directory_iterator(root / "samples"))
		if (sample.is_directory())
			s_checker->addModuleFolder(sample.path());
	size_t checked = 0;
	for (const auto* folder: {"engine_demo", "samples", "engine_assets"}) {
		for (const auto& entry: std::filesystem::recursive_directory_iterator(root / folder)) {
			const auto& path = entry.path();
			// Editor scripts run in the editor console, against its `editor` module, not in a game.
			if (path.extension() != ".lua" || path.generic_string().find("/editor/") != std::string::npos)
				continue;
			const auto found = errors(readText(path));
			EXPECT_TRUE(found.empty()) << path.lexically_relative(root).generic_string() << describe(found);
			++checked;
		}
	}
	EXPECT_GT(checked, 50u);
}
