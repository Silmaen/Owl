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
#include <vector>

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
	EXPECT_EQ(getLuaTables().size(), 19u);
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

TEST(LuaBindingRegistry, EveryTableIsOnOneReferencePage) {
	for (const auto& table: getLuaTables()) {
		const auto pages = std::ranges::count_if(getLuaReferencePages(), [&](const LuaReferencePage& iPage) -> bool {
			return std::ranges::find(iPage.tables, table.name) != iPage.tables.end();
		});
		EXPECT_EQ(pages, 1) << table.name;
	}
	for (const auto& page: getLuaReferencePages()) {
		EXPECT_TRUE(page.file.starts_with("lua-api-") && page.file.ends_with(".md")) << page.file;
		for (const auto name: page.tables)
			EXPECT_TRUE(std::ranges::find(getLuaTables(), name, &LuaTable::name) != getLuaTables().end()) << name;
	}
}

TEST(LuaBindingRegistry, ReferencePageMatchesTheRegistry) {
	const auto files = generateLuaReference();
	ASSERT_EQ(files.size(), getLuaReferencePages().size() + 1);
	EXPECT_EQ(files.front().name, "lua-api.md");
	for (const auto& binding: getLuaBindings()) {
		const auto page = std::ranges::find_if(getLuaReferencePages(), [&](const LuaReferencePage& iPage) -> bool {
			return std::ranges::find(iPage.tables, binding.table) != iPage.tables.end();
		});
		ASSERT_NE(page, getLuaReferencePages().end()) << binding.table;
		const auto file = std::ranges::find(files, std::string{page->file}, &LuaReferenceFile::name);
		ASSERT_NE(file, files.end()) << page->file;
		EXPECT_NE(file->content.find(std::format("`{}.{}(", binding.table, binding.name)), std::string::npos)
				<< binding.name;
	}
	const auto pagesDir = owl::test::getRootPath() / "doc" / "pages";
	std::vector<std::string> stale;
	for (const auto& file: files) {
		if (readText(pagesDir / file.name) != file.content)
			stale.push_back(file.name);
	}
	for (const auto& entry: std::filesystem::directory_iterator(pagesDir)) {
		const auto name = entry.path().filename().string();
		if (name.starts_with("lua-api") && name.ends_with(".md") &&
			std::ranges::find(files, name, &LuaReferenceFile::name) == files.end())
			stale.push_back(name + " (not generated: delete it)");
	}
	const auto toc = readText(owl::test::getRootPath() / "doc" / "help.yml");
	for (const auto& file: files)
		EXPECT_NE(toc.find("- doc/pages/" + file.name + "\n"), std::string::npos)
				<< file.name << " not in doc/help.yml";
	if (stale.empty())
		return;
	const auto outDir = std::filesystem::current_path() / "lua-api";
	std::filesystem::create_directories(outDir);
	for (const auto& file: files) std::ofstream(outDir / file.name, std::ios::binary) << file.content;
	std::string list;
	for (const auto& name: stale) list += "\n  " + name;
	FAIL() << "The Lua API reference is out of date:" << list << "\ncopy " << outDir.string()
		   << "/*.md over doc/pages/.";
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
