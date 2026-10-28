/**
 * @file ScriptConsole.cpp
 * @author Silmaen
 * @date 28/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "script/ScriptConsole.h"

#include <string>
#include <string_view>
#include <vector>

namespace owl::script {

struct ScriptConsole::Impl {};

ScriptConsole::ScriptConsole() : mp_impl{mkUniq<Impl>()} {}

ScriptConsole::~ScriptConsole() = default;

void ScriptConsole::addFunction([[maybe_unused]] const std::string_view iPath,
								[[maybe_unused]] const std::string_view iDescription,
								[[maybe_unused]] HostFunction iFunction) {}

auto ScriptConsole::evaluate([[maybe_unused]] const std::string_view iLine) -> Result {
	return {.status = Status::Error, .output = {"Lua is not built in (OWL_MODULE_SCRIPT is off)."}};
}

auto ScriptConsole::run([[maybe_unused]] const std::string_view iSource, [[maybe_unused]] const std::string_view iName)
		-> Result {
	return {.status = Status::Error, .output = {"Lua is not built in (OWL_MODULE_SCRIPT is off)."}};
}

auto ScriptConsole::help([[maybe_unused]] const std::string_view iPath) const -> std::vector<std::string> { return {}; }

}// namespace owl::script
