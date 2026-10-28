/**
 * @file ScriptConsole.h
 * @author Silmaen
 * @date 28/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace owl::script {

/**
 * @brief
 *  A value passed between a ScriptConsole and its host functions: nil, a boolean, an integer, a number, a string, or
 *  a list of numbers (a vector, a colour).
 */
using HostValue = std::variant<std::monostate, bool, int64_t, double, std::string, std::vector<double>>;

/**
 * @brief
 *  What a host function returns: its fields, or why it failed.
 */
struct HostResult {
	/// Whether the call succeeded.
	bool ok{true};
	/// Why it failed, raised as a Lua error.
	std::string error;
	/// Fields of the table returned to Lua.
	std::map<std::string, HostValue> fields;
};

/**
 * @brief
 *  A function the host gives to the scripts of a console; it takes the fields of the table passed as only argument.
 */
using HostFunction = std::function<HostResult(const std::map<std::string, HostValue>&)>;

/**
 * @brief
 *  A Lua state of its own for tools (the Owl Nest console and editor scripts): an interactive evaluator that keeps its
 *  globals between lines, whole scripts, and host functions grouped in tables.
 *
 * The sandbox and the quotas are the ones of the game scripts; no scene is bound, so the engine bindings that need
 * one fail. `print` writes to the output of the evaluation.
 */
class OWL_API ScriptConsole final {
public:
	/**
	 * @brief
	 *  Outcome of an evaluation.
	 */
	enum struct Status : uint8_t {
		Ok,///< Ran; the output holds what it printed and the value of an expression.
		Incomplete,///< The input opens a block not closed yet: the next line continues it.
		Error///< Did not compile or failed; the output holds the message.
	};

	/**
	 * @brief
	 *  What an evaluation gives back.
	 */
	struct Result {
		/// Outcome.
		Status status{Status::Ok};
		/// Printed lines, then the values of an expression, or the error message.
		std::vector<std::string> output;
	};

	/**
	 * @brief
	 *  Create the console and its Lua state.
	 */
	ScriptConsole();

	/**
	 * @brief
	 *  Destructor.
	 */
	~ScriptConsole();

	ScriptConsole(const ScriptConsole&) = delete;

	ScriptConsole(ScriptConsole&&) = delete;

	auto operator=(const ScriptConsole&) -> ScriptConsole& = delete;

	auto operator=(ScriptConsole&&) -> ScriptConsole& = delete;

	/**
	 * @brief
	 *  Give the scripts a function, `table.name{...}`; the tables of a dotted path are created as needed.
	 * @param[in] iPath Dotted path of the function (`editor.entity.create`).
	 * @param[in] iDescription One sentence shown by `help`.
	 * @param[in] iFunction The function.
	 */
	void addFunction(std::string_view iPath, std::string_view iDescription, HostFunction iFunction);

	/**
	 * @brief
	 *  Evaluate one line typed in a console, like the standalone `lua`: an expression prints its value, a statement
	 *  runs, an open block waits for the next lines.
	 * @param[in] iLine The line.
	 * @return The outcome and the output.
	 */
	auto evaluate(std::string_view iLine) -> Result;

	/**
	 * @brief
	 *  Forget the lines of an open block.
	 */
	void cancelPending() { m_pending.clear(); }

	/**
	 * @brief
	 *  Whether lines of an open block wait for the end of the block.
	 * @return True while a block is open.
	 */
	[[nodiscard]] auto hasPending() const -> bool { return !m_pending.empty(); }

	/**
	 * @brief
	 *  Run a whole script in the state of the console.
	 * @param[in] iSource The Lua source.
	 * @param[in] iName Chunk name for the error messages (the script path).
	 * @return The outcome and what it printed.
	 */
	auto run(std::string_view iSource, std::string_view iName) -> Result;

	/**
	 * @brief
	 *  Help on a function or a table of the console, as `help(x)` prints it.
	 * @param[in] iPath Dotted path, empty for the root tables.
	 * @return The lines.
	 */
	[[nodiscard]] auto help(std::string_view iPath) const -> std::vector<std::string>;

	/// Forward-declared implementation, shared with the C functions of the Lua state.
	struct Impl;

private:
	/// The implementation.
	uniq<Impl> mp_impl;
	/// Lines of the block being typed.
	std::string m_pending;
};

}// namespace owl::script
