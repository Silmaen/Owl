/**
 * @file LuaBindingRegistry_test.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <core/external/lua.h>
#include <script/LuaBindings.h>
#include <script/LuaEngine.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

using namespace owl;
using namespace owl::script;

namespace {
auto readText(const std::filesystem::path& iPath) -> std::string {
	const std::ifstream file(iPath, std::ios::binary);
	std::ostringstream text;
	text << file.rdbuf();
	return text.str();
}

auto isBound(const std::string& iTable, const std::string& iName) -> bool {
	return std::ranges::any_of(getLuaBindings(), [&](const LuaBinding& iBinding) -> bool {
		return iBinding.table == iTable && iBinding.name == iName;
	});
}
}// namespace

TEST(LuaBindingRegistry, EveryBindingIsDeclaredOnceAndDocumented) {
	std::set<std::pair<std::string_view, std::string_view>> seen;
	for (const auto& binding: getLuaBindings()) {
		EXPECT_TRUE(seen.emplace(binding.table, binding.name).second) << binding.table << "." << binding.name;
		EXPECT_NE(binding.function, nullptr) << binding.name;
		EXPECT_FALSE(binding.description.empty()) << binding.table << "." << binding.name;
		EXPECT_TRUE(std::ranges::any_of(getLuaTables(),
										[&](const LuaTable& iTable) -> bool { return iTable.name == binding.table; }))
				<< "table " << binding.table << " is not declared";
	}
	EXPECT_EQ(getLuaTables().size(), 15u);
}

TEST(LuaBindingRegistry, RegistersEveryDeclaredFunction) {
	core::Log::init(core::Log::Level::Off);
	const LuaEngine engine;
	ASSERT_TRUE(engine.isValid());
	registerBindings(engine.getState());
	auto* state = engine.getState();
	for (const auto& binding: getLuaBindings()) {
		lua_getglobal(state, std::string{binding.table}.c_str());
		ASSERT_EQ(lua_type(state, -1), LUA_TTABLE) << binding.table;
		lua_getfield(state, -1, std::string{binding.name}.c_str());
		EXPECT_EQ(lua_type(state, -1), LUA_TFUNCTION) << binding.table << "." << binding.name;
		lua_pop(state, 2);
	}
	core::Log::invalidate();
}

TEST(LuaBindingRegistry, ReferencePageMatchesTheRegistry) {
	const auto page = generateLuaReference();
	for (const auto& binding: getLuaBindings())
		EXPECT_NE(page.find(std::format("`{}.{}(", binding.table, binding.name)), std::string::npos) << binding.name;
	if (const auto committed = readText(owl::test::getRootPath() / "doc" / "pages" / "lua-api.md"); committed != page) {
		const auto expected = std::filesystem::temp_directory_path() / "lua-api.md";
		std::ofstream(expected, std::ios::binary) << page;
		FAIL() << "doc/pages/lua-api.md is out of date: copy " << expected.string() << " over it.";
	}
}

TEST(LuaBindingRegistry, ScriptingGuideCallsOnlyBoundFunctions) {
	const auto guide = readText(owl::test::getRootPath() / "doc" / "pages" / "scripting.md");
	ASSERT_FALSE(guide.empty());
	std::string tables;
	for (const auto& table: getLuaTables()) tables += (tables.empty() ? "" : "|") + std::string{table.name};
	const std::regex call{std::format(R"(\b({})\.([a-z_]+)\()", tables)};
	size_t calls = 0;
	for (auto it = std::sregex_iterator(guide.begin(), guide.end(), call); it != std::sregex_iterator(); ++it) {
		++calls;
		EXPECT_TRUE(isBound((*it)[1].str(), (*it)[2].str())) << (*it)[0].str() << " is documented but not bound";
	}
	EXPECT_GT(calls, 0u);
}
