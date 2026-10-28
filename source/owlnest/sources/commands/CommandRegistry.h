/**
 * @file CommandRegistry.h
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "../UndoManager.h"

#include <core/expected.h>
#include <owl.h>
#include <scene/Entity.h>
#include <scene/Scene.h>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <initializer_list>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace owl::nest::commands {

class CommandJournal;
class CommandRegistry;

/**
 * @brief
 *  Who asked for a command: the journal records it and filters on it.
 */
enum struct CommandSource : uint8_t {
	Gesture,///< A panel, a menu, a shortcut or the viewport.
	Console,///< A line typed in the console.
	Palette,///< The command palette and its argument form.
	Mcp,///< An external tool (the MCP server).
	Replay,///< A line of a replayed macro.
	Scenario,///< A step of a headless scenario.
	Script///< A Lua editor script or a Lua line of the console.
};

/**
 * @brief
 *  Name of a command source, as shown and filtered by the console.
 * @param[in] iSource The source.
 * @return The lower-case name (`gesture`, `console`...).
 */
[[nodiscard]] auto getSourceName(CommandSource iSource) -> std::string_view;

/**
 * @brief
 *  Type of a command argument.
 */
enum struct ArgType : uint8_t {
	Bool,///< `true` / `false`.
	Integer,///< Signed integer.
	Number,///< Floating-point number (an integer is accepted).
	Text,///< String.
	Entity,///< An entity: its UUID (integer) or its tag (text).
	Vec3,///< Three numbers.
	Vec2,///< Two numbers.
	Vec4,///< Four numbers.
	Color,///< A colour: three or four numbers in [0, 1] (alpha 1 when absent), or `#rrggbb` / `#rrggbbaa`.
	Enum///< One text among the declared choices.
};

/**
 * @brief
 *  Value of a command argument.
 */
using ArgValue = std::variant<bool, int64_t, double, std::string, math::vec3, math::vec2, math::vec4>;

/**
 * @brief
 *  Declaration of one argument of a command.
 */
struct ArgSpec {
	/// Argument name, as written by the caller.
	std::string name;
	/// Expected type.
	ArgType type{ArgType::Text};
	/// Whether the command refuses to run without it.
	bool required{false};
	/// One-line description, shown by tools.
	std::string description;
	/// Value the command receives when the caller omits the argument (none: the argument stays absent).
	std::optional<ArgValue> defaultValue{};// NOLINT(readability-redundant-member-init) callers may omit it
	/// Accepted values of an `Enum` argument.
	std::vector<std::string> choices{};// NOLINT(readability-redundant-member-init) callers may omit it
	/// Whether the `Text` argument is a file path: remote callers (MCP) must keep it inside the project.
	bool projectPath{false};
};

/**
 * @brief
 *  Why a command did not run.
 */
enum struct CommandError : uint8_t {
	UnknownCommand,///< No command has this name.
	MissingArgument,///< A required argument is absent.
	WrongArgumentType,///< An argument has another type than declared.
	UnknownArgument,///< An argument the command does not declare.
	UnknownEntity,///< No entity matches the UUID or tag given.
	InvalidArgument,///< The value is of the right type but cannot be used (unknown component, missing file...).
	NotApplicable,///< The scene refused the change (e.g. a cycle in the hierarchy, nothing to undo).
	RefusedDuringPlay///< The command edits the scene and the scene is playing.
};

/**
 * @brief
 *  A command failure and its explanation.
 */
struct CommandFailure {
	/// Failure reason.
	CommandError error{CommandError::InvalidArgument};
	/// Human-readable explanation, naming the command and the argument.
	std::string message;
};

/**
 * @brief
 *  Named arguments given to a command.
 */
class CommandArgs final {
public:
	CommandArgs() = default;

	/**
	 * @brief
	 *  Build the arguments from name / value pairs.
	 * @param[in] iValues The arguments.
	 */
	CommandArgs(std::initializer_list<std::pair<const std::string, ArgValue>> iValues) : m_values{iValues} {}

	/**
	 * @brief
	 *  Set an argument.
	 * @param[in] iName Argument name.
	 * @param[in] iValue Argument value.
	 */
	void set(const std::string& iName, ArgValue iValue) { m_values.insert_or_assign(iName, std::move(iValue)); }

	/**
	 * @brief
	 *  Check whether an argument is given.
	 * @param[in] iName Argument name.
	 * @return True when the argument is set.
	 */
	[[nodiscard]] auto has(const std::string& iName) const -> bool { return m_values.contains(iName); }

	/**
	 * @brief
	 *  Read a boolean argument.
	 * @param[in] iName Argument name.
	 * @param[in] iDefault Value when the argument is absent.
	 * @return The value.
	 */
	[[nodiscard]] auto getBool(const std::string& iName, bool iDefault = false) const -> bool;

	/**
	 * @brief
	 *  Read a text argument.
	 * @param[in] iName Argument name.
	 * @param[in] iDefault Value when the argument is absent.
	 * @return The value.
	 */
	[[nodiscard]] auto getText(const std::string& iName, const std::string& iDefault = {}) const -> std::string;

	/**
	 * @brief
	 *  Read a vector argument.
	 * @param[in] iName Argument name.
	 * @return The value, or nothing when the argument is absent.
	 */
	[[nodiscard]] auto getVec3(const std::string& iName) const -> std::optional<math::vec3>;

	/**
	 * @brief
	 *  Read an argument of any type.
	 * @tparam T Alternative of `ArgValue` expected.
	 * @param[in] iName Argument name.
	 * @return The value, or nothing when the argument is absent or of another type.
	 */
	template<typename T>
	[[nodiscard]] auto get(const std::string& iName) const -> std::optional<T> {
		const auto found = m_values.find(iName);
		if (found == m_values.end())
			return std::nullopt;
		const auto* value = std::get_if<T>(&found->second);
		return value != nullptr ? std::optional<T>{*value} : std::nullopt;
	}

	/**
	 * @brief
	 *  Raw access to every argument.
	 * @return The arguments by name.
	 */
	[[nodiscard]] auto getValues() const -> const std::map<std::string, ArgValue>& { return m_values; }

private:
	/// Arguments by name.
	std::map<std::string, ArgValue> m_values;
};

/**
 * @brief
 *  Structured data returned by a command, by field name (sorted, hence deterministic).
 */
using ResultData = std::map<std::string, ArgValue>;

/**
 * @brief
 *  What a command did.
 */
struct CommandResult {
	/// The entity created or changed (0 when none), to select it.
	core::UUID entity{0};
	/// Description of the undo entry (or of the entry undone / redone).
	std::string description;
	/// Fields a tool can read: `entity` (UUID) when an entity is concerned, plus what the command adds.
	ResultData data{};// NOLINT(readability-redundant-member-init) callers may omit it
};

/**
 * @brief
 *  A command ready for the undo manager, built by a command factory.
 */
struct PreparedCommand {
	/// The undoable command.
	uniq<SceneUndoCommand> command;
	/**
	 * True when the factory already applied the change (creation, duplication, component edits: the command
	 * records it); false when the undo manager applies it by running the command.
	 */
	bool applied{false};
	/// The entity created or changed (0 when none).
	core::UUID entity{0};
	/// Extra result fields (see CommandResult::data).
	ResultData data{};// NOLINT(readability-redundant-member-init) callers may omit it
};

/**
 * @brief
 *  Everything a command may act on: the scene, its undo history, the play state and the scene file.
 */
struct CommandContext {
	/// Callback signature: told the path of the file written.
	using SaveCallback = std::function<void(const std::filesystem::path& iPath)>;
	/// The scene changed.
	shared<scene::Scene> scene;
	/// The undo manager of the scene (non-owning).
	SceneUndoManager* undo{nullptr};
	/// True while the scene plays: commands that edit it are refused.
	bool playing{false};
	/// File of the scene, used by `scene.save` without `path` (empty when never saved).
	std::filesystem::path scenePath{};
	/// Called after `scene.save` wrote the file, with its path (the editor updates the document).
	SaveCallback onSaved{};// NOLINT(readability-redundant-member-init) callers may omit it
	/// Render stack of the open project, edited by `project.add_render_layer` (non-owning, null without a project).
	renderer::RendererStackConfig* projectStack{nullptr};
	/// Called after `project.add_render_layer` changed the stack (the editor saves the project and rebuilds it).
	std::function<void()> onProjectStackChanged{};// NOLINT(readability-redundant-member-init) callers may omit it
	/// Who asks for the command, recorded by the journal.
	CommandSource source{CommandSource::Gesture};
	/// The registry running the command, set by `CommandRegistry::execute` (commands that run other commands).
	const CommandRegistry* registry{nullptr};
};

/**
 * @brief
 *  Declaration of a command: its name, documentation, arguments and factory.
 */
struct CommandSpec {
	/// Factory signature: build the command for a scene, from validated arguments.
	using Factory = std::function<expected<PreparedCommand, CommandFailure>(const shared<scene::Scene>& ioScene,
																			const CommandArgs& iArgs)>;
	/// Runner signature: act directly on the context (commands that are not an undo entry: save, undo, redo).
	using Runner = std::function<expected<CommandResult, CommandFailure>(const CommandContext& iContext,
																		 const CommandArgs& iArgs)>;
	/// Dotted command name (`entity.create`).
	std::string name;
	/// One-line description, shown by tools.
	std::string description;
	/// Declared arguments.
	std::vector<ArgSpec> args;
	/// Command factory, for a command recorded in the undo history.
	Factory factory;
	/// Command runner, used when there is no factory.
	Runner run{};// NOLINT(readability-redundant-member-init) callers may omit it
	/// Whether the command may run while the scene plays (only commands that do not edit it).
	bool allowedInPlay{false};
};

/**
 * @brief
 *  Registry of the editor commands, addressable by name with typed arguments.
 *
 * Every scene change of the editor (hierarchy, inspector menus, shortcuts), of the headless runner (`command:`
 * scenario steps) and of the tests goes through `execute`, which validates the arguments, builds the command and
 * hands it to the scene `UndoManager`: the change is undoable whoever asked for it.
 */
class CommandRegistry final {
public:
	CommandRegistry(const CommandRegistry&) = delete;

	CommandRegistry(CommandRegistry&&) = default;

	auto operator=(const CommandRegistry&) -> CommandRegistry& = delete;

	auto operator=(CommandRegistry&&) -> CommandRegistry& = default;

	/**
	 * @brief
	 *  Build a registry holding the built-in scene commands.
	 */
	CommandRegistry();

	~CommandRegistry() = default;

	/**
	 * @brief
	 *  Register a command, replacing any command of the same name.
	 * @param[in] iSpec The command declaration.
	 */
	void add(CommandSpec iSpec);

	/**
	 * @brief
	 *  Find a command by name.
	 * @param[in] iName Command name.
	 * @return The command declaration, or nullptr when unknown.
	 */
	[[nodiscard]] auto find(std::string_view iName) const -> const CommandSpec*;

	/**
	 * @brief
	 *  Every registered command, in registration order.
	 * @return The command declarations.
	 */
	[[nodiscard]] auto getCommands() const -> const std::vector<CommandSpec>& { return m_commands; }

	/**
	 * @brief
	 *  Validate the arguments, build the command and run it through the undo manager.
	 * @param[in] iName Command name.
	 * @param[in] iArgs Command arguments.
	 * @param[in] ioScene The scene changed.
	 * @param[in,out] ioUndo The undo manager of the scene, which records the command.
	 * @return What the command did, or why it did not run (nothing changed then).
	 */
	[[nodiscard]] auto execute(std::string_view iName, const CommandArgs& iArgs, const shared<scene::Scene>& ioScene,
							   SceneUndoManager& ioUndo) const -> expected<CommandResult, CommandFailure>;

	/**
	 * @brief
	 *  Validate the arguments, fill the defaults and run the command on a context.
	 * @param[in] iName Command name.
	 * @param[in] iArgs Command arguments.
	 * @param[in] iContext The scene, its undo manager, the play state and the scene file.
	 * @return What the command did, or why it did not run (nothing changed then).
	 */
	[[nodiscard]] auto execute(std::string_view iName, const CommandArgs& iArgs, const CommandContext& iContext) const
			-> expected<CommandResult, CommandFailure>;

	/**
	 * @brief
	 *  Record every command executed from now on (whatever its source, failures included) into a journal.
	 * @param[in] ioJournal The journal (non-owning, outlives the registry), null to stop recording.
	 */
	void setJournal(CommandJournal* ioJournal) { mp_journal = ioJournal; }

	/**
	 * @brief
	 *  Get the journal the registry records into.
	 * @return The journal, null when none.
	 */
	[[nodiscard]] auto getJournal() const -> CommandJournal* { return mp_journal; }

	/**
	 * @brief
	 *  Convert a textual value (scenario files, tools) into an argument of the given type.
	 * @param[in] iType Declared type.
	 * @param[in] iText The value as text; a vector is its numbers separated by commas or spaces, a colour three or
	 *  four numbers or `#rrggbb[aa]`, an enum its choice (checked against the choices when the command runs).
	 * @return The value, or nothing when the text does not fit the type.
	 */
	[[nodiscard]] static auto parseArg(ArgType iType, const std::string& iText) -> std::optional<ArgValue>;

	/**
	 * @brief
	 *  Find the entity an `Entity` argument designates.
	 * @param[in] iScene The scene searched.
	 * @param[in] iValue A UUID (integer) or a tag (text, first match).
	 * @return The entity, or nothing when none matches.
	 */
	[[nodiscard]] static auto resolveEntity(const scene::Scene& iScene, const ArgValue& iValue)
			-> std::optional<scene::Entity>;

private:
	/**
	 * @brief
	 *  Check the arguments against a declaration.
	 * @param[in] iSpec The declaration.
	 * @param[in] iArgs The arguments.
	 * @return The failure, or nothing when the arguments fit.
	 */
	[[nodiscard]] static auto validate(const CommandSpec& iSpec, const CommandArgs& iArgs)
			-> std::optional<CommandFailure>;

	/**
	 * @brief
	 *  Run a command without recording it (see `execute`).
	 * @param[in] iName Command name.
	 * @param[in] iArgs Command arguments.
	 * @param[in] iContext The context.
	 * @return What the command did, or why it did not run.
	 */
	[[nodiscard]] auto run(std::string_view iName, const CommandArgs& iArgs, const CommandContext& iContext) const
			-> expected<CommandResult, CommandFailure>;

	/// Registered commands.
	std::vector<CommandSpec> m_commands;
	/// Journal recording the executed commands (non-owning, may be null).
	CommandJournal* mp_journal{nullptr};
};

/**
 * @brief
 *  A registry bound to a scene and its undo manager: what a panel needs to issue commands.
 */
struct CommandTarget {
	/// The command registry (non-owning).
	const CommandRegistry* registry{nullptr};
	/// The scene changed.
	shared<scene::Scene> scene;
	/// The undo manager of the scene (non-owning).
	SceneUndoManager* undo{nullptr};
	/// Who issues the commands, recorded by the journal.
	CommandSource source{CommandSource::Gesture};

	/**
	 * @brief
	 *  Check that the registry, the scene and the undo manager are all set.
	 * @return True when commands can run.
	 */
	[[nodiscard]] auto isValid() const -> bool { return registry != nullptr && scene && undo != nullptr; }

	/**
	 * @brief
	 *  Run a command (see CommandRegistry::execute); refused when the target is incomplete.
	 * @param[in] iName Command name.
	 * @param[in] iArgs Command arguments.
	 * @return What the command did, or why it did not run.
	 */
	[[nodiscard]] auto execute(std::string_view iName, const CommandArgs& iArgs) const
			-> expected<CommandResult, CommandFailure>;
};

}// namespace owl::nest::commands
