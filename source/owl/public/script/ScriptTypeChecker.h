/**
 * @file ScriptTypeChecker.h
 * @author Silmaen
 * @date 09/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace owl::script {

/**
 * @brief
 *  A problem the type checker found in a script.
 */
struct OWL_API ScriptDiagnostic {
	/**
	 * @brief
	 *  How serious a problem is.
	 */
	enum struct Severity : uint8_t {
		Error,///< A type error: the script misuses a value or calls something that does not exist.
		Warning///< A lint warning: an unused local, a shadowed name, unreachable code.
	};
	/// How serious it is.
	Severity severity{Severity::Error};
	/// First line, from 1.
	uint32_t line{1};
	/// First column, from 1.
	uint32_t column{1};
	/// Last line, from 1.
	uint32_t endLine{1};
	/// Column past the end, from 1.
	uint32_t endColumn{1};
	/// What is wrong, one sentence.
	std::string message;
};

/**
 * @brief
 *  Type checking of the Luau scripts of a game, against the declarations of the engine bindings.
 *
 * The checker knows the Luau standard library, every table of the binding registry (`getScriptDefinitions`), the
 * globals the engine sets (`entity_id`, `tr`, the HUD globals) and the properties a script declares. A script is
 * non-strict unless it starts with `--!strict`. Building the checker loads the declarations once; each `check`
 * takes a few milliseconds.
 */
class OWL_API ScriptTypeChecker final {
public:
	ScriptTypeChecker(const ScriptTypeChecker&) = delete;
	ScriptTypeChecker(ScriptTypeChecker&&) = delete;
	auto operator=(const ScriptTypeChecker&) -> ScriptTypeChecker& = delete;
	auto operator=(ScriptTypeChecker&&) -> ScriptTypeChecker& = delete;

	/**
	 * @brief
	 *  Constructor: loads the standard library and the engine declarations.
	 */
	ScriptTypeChecker();

	/**
	 * @brief
	 *  Destructor.
	 */
	~ScriptTypeChecker();

	/**
	 * @brief
	 *  Check a script.
	 * @param[in] iSource The script.
	 * @param[in] iChunkName Name of the script in the messages (its path).
	 * @return The problems, by position; empty without the script module.
	 */
	[[nodiscard]] auto check(std::string_view iSource, std::string_view iChunkName) -> std::vector<ScriptDiagnostic>;

	/**
	 * @brief
	 *  Check whether the declarations of the engine loaded.
	 * @return True when scripts are checked against them.
	 */
	[[nodiscard]] auto isReady() const -> bool;

	/**
	 * @brief
	 *  Search a folder for the modules a script requires, after the asset folders of the application.
	 * @param[in] iFolder A folder holding `a/b.lua` for `require("a.b")`.
	 */
	void addModuleFolder(const std::filesystem::path& iFolder);

private:
	/// The Luau frontend and its resolvers.
	struct Impl;
	/// Implementation.
	uniq<Impl> mp_impl;
};

/**
 * @brief
 *  The Luau declarations of the engine globals: one `declare` per table of the binding registry, typed from its
 *  documented parameters and results, and the globals the engine sets.
 * @return The definition file text, empty without the script module.
 */
[[nodiscard]] OWL_API auto getScriptDefinitions() -> std::string;

}// namespace owl::script
