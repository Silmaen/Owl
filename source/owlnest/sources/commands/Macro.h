/**
 * @file Macro.h
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "CommandJournal.h"
#include "CommandRegistry.h"

#include <core/expected.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace owl::nest::commands {

/**
 * @brief
 *  Why a macro could not be read, written or replayed.
 */
enum struct MacroError : uint8_t {
	FileNotFound,///< No file at the path.
	ReadFailed,///< The file cannot be read.
	WriteFailed,///< The file cannot be written.
	InvalidLine,///< A line does not read as a command.
	CommandFailed,///< A command of the macro refused to run.
	NoScene,///< No scene or undo history to replay on.
	TooDeep///< Macros replaying macros nest too deep (a macro replaying itself).
};

/**
 * @brief
 *  A macro failure and its explanation.
 */
struct MacroFailure {
	/// Failure reason.
	MacroError error{MacroError::ReadFailed};
	/// Line of the macro at fault, from 1 (0 when the failure is not about a line).
	size_t line{0};
	/// Human-readable explanation, starting with the line number when there is one.
	std::string message;
};

/**
 * @brief
 *  What a replay did.
 */
struct MacroReport {
	/// Commands run.
	size_t commands{0};
	/// The last entity created or changed (0 when none).
	core::UUID lastEntity{0};
};

/**
 * @brief
 *  A recorded sequence of commands: a `.owlmacro` file, plain text, one command per line, `#` comments.
 *
 * Inside a macro, `$n` designates the result of its `n`-th command and `$_` the previous one, so a recorded sequence
 * refers to the entities it creates without hard-coded ids.
 */
class Macro final {
public:
	/// Extension of the macro files.
	static constexpr std::string_view extension = ".owlmacro";
	/// Deepest nesting of macros replaying macros.
	static constexpr size_t maxDepth = 8;

	Macro() = default;

	Macro(const Macro&) = default;

	Macro(Macro&&) = default;

	auto operator=(const Macro&) -> Macro& = default;

	auto operator=(Macro&&) -> Macro& = default;

	~Macro() = default;

	/**
	 * @brief
	 *  Build a macro from lines of the command syntax.
	 * @param[in] iLines The lines, comments and blank lines included.
	 */
	explicit Macro(std::vector<std::string> iLines) : m_lines{std::move(iLines)} {}

	/**
	 * @brief
	 *  Read a macro file.
	 * @param[in] iPath The file.
	 * @return The macro, or why it cannot be read.
	 */
	[[nodiscard]] static auto load(const std::filesystem::path& iPath) -> expected<Macro, MacroFailure>;

	/**
	 * @brief
	 *  Build a macro from journal entries: the commands that ran, in order, with the entities they created
	 *  replaced by `$n` references.
	 * @param[in] iEntries The entries kept (failed ones are skipped).
	 * @param[in] iRegistry The registry declaring the commands.
	 * @return The macro.
	 */
	[[nodiscard]] static auto fromJournal(std::span<const JournalEntry* const> iEntries,
										  const CommandRegistry& iRegistry) -> Macro;

	/**
	 * @brief
	 *  Write the macro file.
	 * @param[in] iPath The file.
	 * @return Nothing on success, or why it cannot be written.
	 */
	[[nodiscard]] auto save(const std::filesystem::path& iPath) const -> expected<void, MacroFailure>;

	/**
	 * @brief
	 *  Get the lines.
	 * @return The lines, comments and blank lines included.
	 */
	[[nodiscard]] auto getLines() const -> const std::vector<std::string>& { return m_lines; }

	/**
	 * @brief
	 *  Run every command, as one undo step; the first failing line stops the replay and undoes what it did.
	 * @param[in] iRegistry The registry running the commands (and journaling them as `Replay`).
	 * @param[in] iContext The scene, its undo history and the play state.
	 * @param[in] iDescription Description of the undo step.
	 * @return What the replay did, or the failing line and why.
	 */
	[[nodiscard]] auto replay(const CommandRegistry& iRegistry, const CommandContext& iContext,
							  const std::string& iDescription) const -> expected<MacroReport, MacroFailure>;

private:
	/// The lines, comments and blank lines included.
	std::vector<std::string> m_lines;
};

/**
 * @brief
 *  Find a macro file: an absolute path, a path relative to the working directory, else to an asset directory.
 * @param[in] iPath The path as given.
 * @return The file found, or the path as given when none exists.
 */
[[nodiscard]] auto resolveMacroPath(const std::filesystem::path& iPath) -> std::filesystem::path;

/**
 * @brief
 *  Declaration of the `macro.run` command (replay a macro file as one undo step).
 * @return The command declaration.
 */
[[nodiscard]] auto getMacroRunSpec() -> CommandSpec;

}// namespace owl::nest::commands
