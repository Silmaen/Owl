/**
 * @file ScriptConsole.cpp
 * @author Silmaen
 * @date 28/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "script/ScriptConsole.h"

#include "core/external/lua.h"
#include "script/LuaEngine.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <format>
#include <map>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace owl::script {

struct ScriptConsole::Impl {
	LuaEngine engine;
	std::vector<HostFunction> functions;
	std::map<std::string, std::string, std::less<>> descriptions;
	std::vector<std::string>* output{nullptr};
};

namespace {

constexpr std::string_view g_chunkName = "=console";

auto readValue(lua_State* iState, const int iIndex) -> HostValue {
	switch (lua_type(iState, iIndex)) {
		case LUA_TBOOLEAN:
			return lua_toboolean(iState, iIndex) != 0;
		case LUA_TNUMBER:
			if (lua_isinteger(iState, iIndex) != 0)
				return static_cast<int64_t>(lua_tointeger(iState, iIndex));
			return static_cast<double>(lua_tonumber(iState, iIndex));
		case LUA_TSTRING:
			return std::string{lua_tostring(iState, iIndex)};
		case LUA_TTABLE:
			{
				std::vector<double> numbers;
				const auto size = static_cast<int64_t>(lua_rawlen(iState, iIndex));
				for (int64_t i = 1; i <= size; ++i) {
					lua_rawgeti(iState, iIndex, i);
					numbers.push_back(static_cast<double>(lua_tonumber(iState, -1)));
					lua_pop(iState, 1);
				}
				return numbers;
			}
		default:
			return std::monostate{};
	}
}

void pushValue(lua_State* iState, const HostValue& iValue) {
	std::visit(
			[iState]<typename T>(const T& iItem) -> void {
				if constexpr (std::is_same_v<T, std::monostate>)
					lua_pushnil(iState);
				else if constexpr (std::is_same_v<T, bool>)
					lua_pushboolean(iState, iItem ? 1 : 0);
				else if constexpr (std::is_same_v<T, int64_t>)
					lua_pushinteger(iState, static_cast<lua_Integer>(iItem));
				else if constexpr (std::is_same_v<T, double>)
					lua_pushnumber(iState, static_cast<lua_Number>(iItem));
				else if constexpr (std::is_same_v<T, std::string>)
					lua_pushlstring(iState, iItem.data(), iItem.size());
				else {
					lua_createtable(iState, static_cast<int>(iItem.size()), 0);
					for (size_t i = 0; i < iItem.size(); ++i) {
						lua_pushnumber(iState, static_cast<lua_Number>(iItem[i]));
						lua_rawseti(iState, -2, static_cast<lua_Integer>(i) + 1);
					}
				}
			},
			iValue);
}

// Runs the host function; false leaves the error message on the stack.
auto callHost(lua_State* iState) -> bool {
	auto* impl = static_cast<ScriptConsole::Impl*>(lua_touserdata(iState, lua_upvalueindex(1)));
	const auto index = static_cast<size_t>(lua_tointeger(iState, lua_upvalueindex(2)));
	std::map<std::string, HostValue> args;
	if (lua_istable(iState, 1)) {
		lua_pushnil(iState);
		while (lua_next(iState, 1) != 0) {
			if (lua_type(iState, -2) == LUA_TSTRING)
				args.emplace(lua_tostring(iState, -2), readValue(iState, lua_gettop(iState)));
			lua_pop(iState, 1);
		}
	}
	HostResult result;
	try {
		result = impl->functions[index](args);
	} catch (const std::exception& exception) { result = {.ok = false, .error = exception.what(), .fields = {}}; }
	if (!result.ok) {
		lua_pushstring(iState, result.error.c_str());
		return false;
	}
	lua_createtable(iState, 0, static_cast<int>(result.fields.size()));
	for (const auto& [key, value]: result.fields) {
		pushValue(iState, value);
		lua_setfield(iState, -2, key.c_str());
	}
	return true;
}

auto hostTrampoline(lua_State* iState) -> int { return callHost(iState) ? 1 : lua_error(iState); }

auto printToConsole(lua_State* iState) -> int {
	auto* impl = static_cast<ScriptConsole::Impl*>(lua_touserdata(iState, lua_upvalueindex(1)));
	std::string line;
	for (int i = 1; i <= lua_gettop(iState); ++i) {
		if (i > 1)
			line += '\t';
		size_t size = 0;
		const char* text = luaL_tolstring(iState, i, &size);
		line.append(text, size);
		lua_pop(iState, 1);
	}
	if (impl->output != nullptr)
		impl->output->push_back(std::move(line));
	return 0;
}

// The first line of a message: Lua appends its traceback after it.
auto firstLine(const std::string& iMessage) -> std::string { return iMessage.substr(0, iMessage.find('\n')); }

}// namespace

ScriptConsole::ScriptConsole() : mp_impl{mkUniq<Impl>()} {
	auto* state = mp_impl->engine.getState();
	if (state == nullptr)
		return;
	lua_pushlightuserdata(state, mp_impl.get());
	lua_pushcclosure(state, printToConsole, 1);
	lua_setglobal(state, "print");
}

ScriptConsole::~ScriptConsole() = default;

void ScriptConsole::addFunction(const std::string_view iPath, const std::string_view iDescription,
								HostFunction iFunction) {
	auto* state = mp_impl->engine.getState();
	if (state == nullptr || iPath.empty())
		return;
	mp_impl->functions.push_back(std::move(iFunction));
	mp_impl->descriptions.emplace(std::string{iPath}, std::string{iDescription});
	lua_pushglobaltable(state);
	std::string_view rest = iPath;
	for (auto dot = rest.find('.'); dot != std::string_view::npos; dot = rest.find('.')) {
		const std::string part{rest.substr(0, dot)};
		lua_pushlstring(state, part.data(), part.size());
		lua_rawget(state, -2);
		if (!lua_istable(state, -1)) {
			lua_pop(state, 1);
			lua_newtable(state);
			lua_pushlstring(state, part.data(), part.size());
			lua_pushvalue(state, -2);
			lua_rawset(state, -4);
		}
		lua_remove(state, -2);
		rest.remove_prefix(dot + 1);
	}
	lua_pushlightuserdata(state, mp_impl.get());
	lua_pushinteger(state, static_cast<lua_Integer>(mp_impl->functions.size() - 1));
	lua_pushlstring(state, rest.data(), rest.size());
	lua_insert(state, -3);
	lua_pushcclosure(state, hostTrampoline, 2);
	lua_rawset(state, -3);
	lua_pop(state, 1);
}

auto ScriptConsole::evaluate(const std::string_view iLine) -> Result {
	Result result;
	const auto trimmed = iLine.substr(0, iLine.find_last_not_of(" \t\r\n") + 1);
	if (m_pending.empty() && trimmed.starts_with("help")) {
		const auto rest = trimmed.substr(4);
		if (rest.empty() || rest.front() == ' ' || rest.front() == '(') {
			std::string topic{rest};
			std::erase_if(topic, [](const char iChar) -> bool {
				return iChar == ' ' || iChar == '(' || iChar == ')' || iChar == '"' || iChar == '\'';
			});
			result.output = help(topic);
			return result;
		}
	}
	m_pending += m_pending.empty() ? std::string{iLine} : std::format("\n{}", iLine);
	mp_impl->output = &result.output;
	auto chunk = mp_impl->engine.runChunk(std::format("return {}", m_pending), std::string{g_chunkName});
	if (!chunk.compiled)
		chunk = mp_impl->engine.runChunk(m_pending, std::string{g_chunkName});
	mp_impl->output = nullptr;
	if (!chunk.compiled && chunk.message.ends_with("<eof>")) {
		result.status = Status::Incomplete;
		return result;
	}
	m_pending.clear();
	if (!chunk.ok) {
		result.status = Status::Error;
		result.output.push_back(firstLine(chunk.message));
		return result;
	}
	if (!chunk.values.empty()) {
		std::string values;
		for (const auto& value: chunk.values) values += values.empty() ? value : std::format("\t{}", value);
		result.output.push_back(std::move(values));
	}
	return result;
}

auto ScriptConsole::run(const std::string_view iSource, const std::string_view iName) -> Result {
	Result result;
	mp_impl->output = &result.output;
	const auto chunk = mp_impl->engine.runChunk(iSource, std::format("@{}", iName));
	mp_impl->output = nullptr;
	if (!chunk.ok) {
		result.status = Status::Error;
		result.output.push_back(firstLine(chunk.message));
	}
	return result;
}

auto ScriptConsole::help(const std::string_view iPath) const -> std::vector<std::string> {
	std::vector<std::string> lines;
	if (const auto exact = mp_impl->descriptions.find(iPath); exact != mp_impl->descriptions.end()) {
		lines.push_back(std::format("{}{{...}}: {}", exact->first, exact->second));
		return lines;
	}
	const auto prefix = iPath.empty() ? std::string{} : std::format("{}.", iPath);
	std::vector<std::string> tables;
	for (const auto& [path, description]: mp_impl->descriptions) {
		if (!path.starts_with(prefix))
			continue;
		const auto rest = std::string_view{path}.substr(prefix.size());
		if (const auto dot = rest.find('.'); dot != std::string_view::npos) {
			const auto table = std::format("{}{}", prefix, rest.substr(0, dot));
			if (!std::ranges::contains(tables, table))
				tables.push_back(table);
			continue;
		}
		lines.push_back(std::format("{}{{...}}: {}", path, description));
	}
	for (const auto& table: tables) lines.push_back(std::format("{}: table, help({}) lists it", table, table));
	if (lines.empty())
		lines.push_back(iPath.empty() ? std::string{"Nothing to list."} : std::format("No help on '{}'.", iPath));
	return lines;
}

}// namespace owl::script
