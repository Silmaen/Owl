/**
 * @file VisualScriptDemo_test.cpp
 * @author Silmaen
 * @date 08/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <script/ScriptEngine.h>
#include <script/ScriptTypeChecker.h>
#include <script/visual/VisualScript.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <string>
#include <vector>

using namespace owl;
using namespace owl::script;
using namespace owl::script::visual;

namespace {

auto readText(const std::filesystem::path& iPath) -> std::string {
	std::ifstream file(iPath, std::ios::binary);
	return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

auto findDemoGraphs() -> std::vector<std::filesystem::path> {
	std::vector<std::filesystem::path> graphs;
	for (const auto& entry: std::filesystem::recursive_directory_iterator(test::getRootPath() / "engine_demo"))
		if (entry.is_regular_file() && entry.path().extension() == g_VisualScriptExtension)
			graphs.push_back(entry.path());
	return graphs;
}

}// namespace

TEST(VisualScriptDemo, EveryDemoGraphCompiles) {
	const auto graphs = findDemoGraphs();
	ASSERT_FALSE(graphs.empty());
	for (const auto& path: graphs) {
		SCOPED_TRACE(path.string());
		const auto script = deserialize(readText(path), path.string());
		ASSERT_TRUE(script.has_value());
		const auto lua = compile(*script);
		if (!lua) {
			for (const auto& diagnostic: lua.error())
				ADD_FAILURE() << "Node " << static_cast<uint64_t>(diagnostic.node) << ": " << diagnostic.message;
			continue;
		}
		EXPECT_EQ(ScriptEngine::extractProperties(path).size(),
				  static_cast<size_t>(std::ranges::count(script->variables, true, &GraphVariable::exposed)));
	}
}

TEST(VisualScriptDemo, EveryDemoGraphPassesTheCheck) {
	const auto root = test::getRootPath();
	ScriptTypeChecker checker;
	checker.addModuleFolder(root / "engine_demo");
	const auto graphs = findDemoGraphs();
	ASSERT_FALSE(graphs.empty());
	for (const auto& path: graphs) {
		SCOPED_TRACE(path.string());
		const auto script = deserialize(readText(path), path.string());
		ASSERT_TRUE(script.has_value());
		for (const auto& found: checkTypes(*script, checker))
			ADD_FAILURE() << "Node " << static_cast<uint64_t>(found.node) << ": " << found.message;
	}
}
