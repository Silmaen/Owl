/**
 * @file CommandForm.h
 * @author Silmaen
 * @date 13/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "CommandRegistry.h"

#include <core/expected.h>

#include <string>
#include <vector>

namespace owl::nest::commands {

/**
 * @brief
 *  One argument of a command form: its declaration and what the user entered.
 */
struct FormField {
	/// The argument declaration.
	ArgSpec spec;
	/// Whether the argument is given (always true for a required one).
	bool set{false};
	/// The value entered, of the alternative matching the declared type.
	ArgValue value;
};

/**
 * @brief
 *  State of the argument form of a command, kept apart from its widgets so it can be tested.
 *
 * Each field starts from the declared default value (given), else from a neutral value of its type (not given for an
 * optional argument). `buildArgs` checks the form before the command runs.
 */
class CommandForm final {
public:
	/**
	 * @brief
	 *  Build an empty form, bound to no command.
	 */
	CommandForm() = default;

	/**
	 * @brief
	 *  Build the form of a command.
	 * @param[in] iSpec The command declaration.
	 */
	explicit CommandForm(const CommandSpec& iSpec);

	/**
	 * @brief
	 *  Name of the command the form runs.
	 * @return The command name, empty for an empty form.
	 */
	[[nodiscard]] auto getCommandName() const -> const std::string& { return m_command; }

	/**
	 * @brief
	 *  The fields, in declaration order.
	 * @return The fields.
	 */
	[[nodiscard]] auto getFields() const -> const std::vector<FormField>& { return m_fields; }

	/**
	 * @brief
	 *  The fields, for the widgets to edit.
	 * @return The fields.
	 */
	[[nodiscard]] auto getFields() -> std::vector<FormField>& { return m_fields; }

	/**
	 * @brief
	 *  Give a value to every entity field not given yet (the selection, when the form opens).
	 * @param[in] iEntity The entity UUID.
	 */
	void setDefaultEntity(core::UUID iEntity);

	/**
	 * @brief
	 *  Check the form and turn it into command arguments.
	 * @return The arguments of the given fields, or why the form cannot run (missing entity, empty required text,
	 *  value outside the choices).
	 */
	[[nodiscard]] auto buildArgs() const -> expected<CommandArgs, CommandFailure>;

	/**
	 * @brief
	 *  Neutral value of a type: false, 0, empty text, zero vector, opaque white, first choice.
	 * @param[in] iSpec The argument declaration.
	 * @return The value.
	 */
	[[nodiscard]] static auto initialValue(const ArgSpec& iSpec) -> ArgValue;

	/**
	 * @brief
	 *  One-line text of the result data of a command (`entity: 42, path: a.owl`).
	 * @param[in] iData The data.
	 * @return The text, empty when there is no data.
	 */
	[[nodiscard]] static auto describeData(const ResultData& iData) -> std::string;

	/**
	 * @brief
	 *  Whether a command can run in the current editor state.
	 * @param[in] iSpec The command declaration.
	 * @param[in] iPlaying True while the scene plays.
	 * @return False for an edit while playing.
	 */
	[[nodiscard]] static auto isAvailable(const CommandSpec& iSpec, const bool iPlaying) -> bool {
		return !iPlaying || iSpec.allowedInPlay;
	}

private:
	/// Command name.
	std::string m_command;
	/// One field per declared argument.
	std::vector<FormField> m_fields;
};

}// namespace owl::nest::commands
