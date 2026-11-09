/**
 * @file ScriptTypeChecker.cpp
 * @author Silmaen
 * @date 09/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "script/ScriptTypeChecker.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace owl::script {

struct ScriptTypeChecker::Impl {};

ScriptTypeChecker::ScriptTypeChecker() : mp_impl{mkUniq<Impl>()} {}

ScriptTypeChecker::~ScriptTypeChecker() = default;

auto ScriptTypeChecker::isReady() const -> bool { return false; }

void ScriptTypeChecker::addModuleFolder([[maybe_unused]] const std::filesystem::path& iFolder) {}

auto ScriptTypeChecker::check([[maybe_unused]] const std::string_view iSource,
							  [[maybe_unused]] const std::string_view iChunkName) -> std::vector<ScriptDiagnostic> {
	return {};
}

auto getScriptDefinitions() -> std::string { return {}; }

}// namespace owl::script
