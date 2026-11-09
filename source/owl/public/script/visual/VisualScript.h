/**
 * @file VisualScript.h
 * @author Silmaen
 * @date 27/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/UUID.h"
#include "core/expected.h"
#include "script/ScriptEngine.h"

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/**
 * @brief
 *  Visual scripts: event graphs saved as `.owlvs` and compiled to Lua.
 */
namespace owl::script::visual {

/// File extension of a visual script.
inline constexpr std::string_view g_VisualScriptExtension = ".owlvs";

/**
 * @brief
 *  Type carried by a pin.
 */
enum struct PinType : uint8_t {
	Exec,///< Execution flow, no value.
	Boolean,///< `true` / `false`.
	Integer,///< Lua integer.
	Number,///< Lua number.
	String,///< Lua string.
	Table,///< Lua table.
	Entity,///< Entity UUID (integer), the script's own entity when left empty.
	Vector2,///< Table `{x, y}`.
	Vector3,///< Table `{x, y, z}`.
	Color,///< Table `{r, g, b, a}`.
	Any///< Any value.
};

/// Prefix of the node value that splits a grouped pin into its components (`split.rgba` = `true`).
inline constexpr std::string_view g_SplitPrefix = "split.";

/**
 * @brief
 *  How the compiler turns a node into Lua.
 */
enum struct NodeKind : uint8_t {
	Event,///< Lua hook the engine calls (`on_update`), its parameters as outputs.
	CustomEvent,///< Lua function of a chosen name (trigger or HUD callback), one argument.
	Call,///< Call of an engine binding (`transform.get_position`).
	Operator,///< Pure Lua expression of its inputs.
	Branch,///< `if` on a boolean.
	Sequence,///< Runs its outputs one after the other.
	ForLoop,///< Numeric `for` loop.
	ForEach,///< `for` loop over the items of a list (`ipairs`).
	Statement,///< Lua statement of its inputs (`t[k] = v`), with execution pins.
	GetVariable,///< Reads a graph variable.
	SetVariable,///< Writes a graph variable.
	FunctionEntry,///< Start of a graph function: its parameters as outputs.
	FunctionReturn,///< End of a graph function: its results as inputs, and the execution output a macro leaves by.
	FunctionCall,///< Call of a graph function, of this graph or of a library graph.
	CustomCall,///< Call of a custom node of the graph: Lua written by the author of the graph.
	LuaCall///< Call of an annotated function of a Lua module.
};

/**
 * @brief
 *  A pin of a node type.
 */
struct PinSpec {
	/// Pin name, unique on its side of the node, shown on the canvas.
	std::string name;
	/// Carried type.
	PinType type{PinType::Any};
	/// Value of an unlinked input without a value of its own.
	std::string defaultValue;
	/// Whether an unlinked, empty input passes `nil`.
	bool optional{false};
	/**
	 * @brief
	 *  Grouped pin it belongs to: its own name for a grouped pin (`rgba`, `velocity`), the group name for a
	 *  component of a split one (`r`, `vx`), empty otherwise.
	 */
	std::string group;
	/// Pins a grouped pin stands for, in order (`vx`, `vy`); empty for any other pin.
	std::vector<std::string> components;
	/// Whether the input is a value typed on the node that takes no link (the value of a constant).
	bool field{false};
};

/**
 * @brief
 *  A kind of node the palette offers.
 */
struct NodeType {
	/// Stable identifier saved in the graph (`call.transform.get_position`).
	std::string id;
	/// Title shown on the node.
	std::string title;
	/// Palette group (`Events`, `Flow`, `transform`).
	std::string category;
	/// One-sentence description.
	std::string description;
	/// How the node compiles.
	NodeKind kind{NodeKind::Operator};
	/**
	 * @brief
	 *  Lua code of the node: hook name, function to call, or expression (statement) with `{0}`, `{1}` for the inputs.
	 *
	 * In a template, `[{n}]` becomes `.name` when the input is a string literal that is a Lua name, and an optional
	 * input left empty at the end of a call or a table (`, {n})`, `, {n} }`) is left out.
	 */
	std::string lua;
	/// Input pins, in order.
	std::vector<PinSpec> inputs;
	/// Output pins, in order.
	std::vector<PinSpec> outputs;
	/// Whether the node has no execution pins and is evaluated where its outputs are read.
	bool pure{false};
};

/**
 * @brief
 *  A node of a graph.
 */
struct GraphNode {
	/// Node identifier.
	core::UUID id;
	/// Identifier of its NodeType.
	std::string type;
	/// Top-left corner on the canvas.
	std::array<float, 2> position{0.f, 0.f};
	/// Values of the unlinked inputs by pin name, and the node settings (`variable`, `function`).
	std::map<std::string, std::string> values;

	/**
	 * @brief
	 *  Equality operator.
	 * @param[in] iOther Node to compare to.
	 * @return True when both nodes are identical.
	 */
	auto operator==(const GraphNode& iOther) const -> bool = default;
};

/**
 * @brief
 *  A link from an output pin to an input pin.
 */
struct GraphLink {
	/// Source node.
	core::UUID fromNode{0};
	/// Output pin name on the source node.
	std::string fromPin;
	/// Destination node.
	core::UUID toNode{0};
	/// Input pin name on the destination node.
	std::string toPin;
	/// Points the link passes through in the editor, canvas units, from the output to the input.
	std::vector<std::array<float, 2>> waypoints;
	/// Colour name of an execution link in the editor (`Red`, `Green`...), empty for the default one.
	std::string color{};// NOLINT(readability-redundant-member-init) lets designated initializers omit it

	/**
	 * @brief
	 *  Equality operator.
	 * @param[in] iOther Link to compare to.
	 * @return True when both links are identical.
	 */
	auto operator==(const GraphLink& iOther) const -> bool = default;
};

/**
 * @brief
 *  A graph variable: a script property the inspector shows and the scene saves, or the internal state of the script.
 */
struct GraphVariable {
	/// Lua name of the property.
	std::string name;
	/// Property type: `Float`, `Int`, `String`, `Bool`, `Entity`, a vector, a colour, or a list (`Numbers`, `Strings`).
	ScriptPropertyType type{ScriptPropertyType::Float};
	/// Default value, as text.
	std::string defaultValue;
	/// Smallest value the inspector accepts, when bounded.
	std::optional<float> min;
	/// Largest value the inspector accepts, when bounded.
	std::optional<float> max;
	/// Step of the inspector field, when set.
	std::optional<float> step;
	/// True for a script property; false for internal state, set to its default when the script loads.
	bool exposed{true};

	/**
	 * @brief
	 *  Equality operator.
	 * @param[in] iOther Variable to compare to.
	 * @return True when both variables are identical.
	 */
	auto operator==(const GraphVariable& iOther) const -> bool = default;
};

/**
 * @brief
 *  A typed parameter or result of a graph function.
 */
struct GraphParameter {
	/// Name, shown on the pin.
	std::string name;
	/// Carried type.
	PinType type{PinType::Number};

	/**
	 * @brief
	 *  Equality operator.
	 * @param[in] iOther Parameter to compare to.
	 * @return True when both parameters are identical.
	 */
	auto operator==(const GraphParameter& iOther) const -> bool = default;
};

/**
 * @brief
 *  A function of a graph, callable from this graph and, through a library, from any other one.
 *
 * Its body starts at its `func.entry` node and ends at `func.return` nodes. A macro is a function with several
 * execution outputs: each return node names the one the call continues by.
 */
struct GraphFunction {
	/// Lua name, unique in the graph.
	std::string name;
	/// Parameters, outputs of the entry node and inputs of the calls.
	std::vector<GraphParameter> inputs;
	/// Results, inputs of the return nodes and outputs of the calls.
	std::vector<GraphParameter> outputs;
	/// Shown in the Macros category of the palette, with its exits.
	bool macro{false};
	/// Execution outputs of the calls: `Then` for a function, one per exit for a macro.
	std::vector<std::string> exits{"Then"};
	/// A global the engine calls by name and reads the results of (dialogue condition); else local to the graph.
	bool exported{false};

	/**
	 * @brief
	 *  Equality operator.
	 * @param[in] iOther Function to compare to.
	 * @return True when both functions are identical.
	 */
	auto operator==(const GraphFunction& iOther) const -> bool = default;
};

/**
 * @brief
 *  A node the author of the graph writes in Lua: its title, its typed pins and the body of the function it runs.
 *
 * The compiler turns it into a local function of the graph's Lua, its inputs as parameters named after them; the body
 * returns the outputs in order. A pure custom node has no execution pins and runs where its outputs are read.
 */
struct CustomNode {
	/// Lua name, unique in the graph.
	std::string name;
	/// Title shown on the node.
	std::string title;
	/// Inputs, parameters of the function.
	std::vector<GraphParameter> inputs;
	/// Outputs, the values the body returns.
	std::vector<GraphParameter> outputs;
	/// Lua body of the function.
	std::string code;
	/// Whether the node has no execution pins.
	bool pure{false};

	/**
	 * @brief
	 *  Equality operator.
	 * @param[in] iOther Node to compare to.
	 * @return True when both nodes are identical.
	 */
	auto operator==(const CustomNode& iOther) const -> bool = default;
};

/**
 * @brief
 *  A visual script: variables, functions, custom nodes, nodes and links.
 */
struct VisualScript {
	/// Variables, in declaration order.
	std::vector<GraphVariable> variables;
	/// Functions and macros, in declaration order.
	std::vector<GraphFunction> functions;
	/// Custom nodes, in declaration order.
	std::vector<CustomNode> customNodes;
	/// Nodes.
	std::vector<GraphNode> nodes;
	/// Links.
	std::vector<GraphLink> links;

	/**
	 * @brief
	 *  Find a node.
	 * @param[in] iId Node identifier.
	 * @return The node, or nullptr.
	 */
	[[nodiscard]] OWL_API auto findNode(core::UUID iId) const -> const GraphNode*;

	/**
	 * @brief
	 *  Find a node.
	 * @param[in] iId Node identifier.
	 * @return The node, or nullptr.
	 */
	[[nodiscard]] OWL_API auto findNode(core::UUID iId) -> GraphNode*;

	/**
	 * @brief
	 *  Find a variable.
	 * @param[in] iName Variable name.
	 * @return The variable, or nullptr.
	 */
	[[nodiscard]] OWL_API auto findVariable(std::string_view iName) const -> const GraphVariable*;

	/**
	 * @brief
	 *  Find a function or a macro.
	 * @param[in] iName Function name.
	 * @return The function, or nullptr.
	 */
	[[nodiscard]] OWL_API auto findFunction(std::string_view iName) const -> const GraphFunction*;

	/**
	 * @brief
	 *  Find a custom node.
	 * @param[in] iName Custom node name.
	 * @return The custom node, or nullptr.
	 */
	[[nodiscard]] OWL_API auto findCustomNode(std::string_view iName) const -> const CustomNode*;

	/**
	 * @brief
	 *  Remove a node and every link attached to it.
	 * @param[in] iId Node identifier.
	 */
	OWL_API void removeNode(core::UUID iId);

	/**
	 * @brief
	 *  Equality operator.
	 * @param[in] iOther Script to compare to.
	 * @return True when both scripts are identical.
	 */
	auto operator==(const VisualScript& iOther) const -> bool = default;
};

/**
 * @brief
 *  Pins of one node, its type resolved against the graph (a variable node takes the type of its variable).
 */
struct NodePins {
	/// Input pins.
	std::vector<PinSpec> inputs;
	/// Output pins.
	std::vector<PinSpec> outputs;
};

/**
 * @brief
 *  Why a visual script could not be read.
 */
enum struct VisualScriptError : uint8_t {
	InvalidYaml,///< The text is not YAML.
	UnsupportedVersion,///< Written by a newer engine, or a migration failed.
	InvalidContent///< YAML without a `VisualScript` map.
};

/**
 * @brief
 *  A compilation error, attached to the node that causes it.
 */
struct CompileDiagnostic {
	/// Faulty node, 0 for the whole graph.
	core::UUID node{0};
	/// What is wrong, in one sentence.
	std::string message;
};

/**
 * @brief
 *  Every node type: built-in nodes, then one node per engine binding.
 * @return The palette.
 */
[[nodiscard]] OWL_API auto getNodeTypes() -> const std::vector<NodeType>&;

/**
 * @brief
 *  Find a node type.
 * @param[in] iId Type identifier.
 * @return The type, or nullptr when unknown.
 */
[[nodiscard]] OWL_API auto findNodeType(std::string_view iId) -> const NodeType*;

/**
 * @brief
 *  Pins of a node.
 *
 * The number parameters `x, y [, z]` and `r, g, b [, a]` of a binding, also with a prefix (`vx, vy`, `cell_x, cell_y`),
 * form one grouped pin (`xyz` or `velocity` of type `Vector3` / `Vector2`, `rgba` of type `Color`), shown as its
 * components when the node value `split.<group>` is `true`.
 * @param[in] iScript The graph holding the node.
 * @param[in] iNode The node.
 * @return Its pins, empty when its type is unknown.
 */
[[nodiscard]] OWL_API auto getNodePins(const VisualScript& iScript, const GraphNode& iNode) -> NodePins;

/**
 * @brief
 *  Whether an output of a type may feed an input of another.
 * @param[in] iFrom Output type.
 * @param[in] iTo Input type.
 * @return True when the link is allowed.
 */
[[nodiscard]] OWL_API auto canConnect(PinType iFrom, PinType iTo) -> bool;

/**
 * @brief
 *  Pin type of a graph variable.
 * @param[in] iType Property type of the variable.
 * @return Its pin type, `Table` for the lists of numbers or strings.
 */
[[nodiscard]] OWL_API auto getVariablePinType(ScriptPropertyType iType) -> PinType;

/**
 * @brief
 *  Component names of a grouped pin type.
 * @param[in] iType Pin type.
 * @return `x, y`, `x, y, z` or `r, g, b, a`; empty for a type that is not a group.
 */
[[nodiscard]] OWL_API auto getPinComponents(PinType iType) -> std::vector<std::string>;

/**
 * @brief
 *  Whether a name can be a graph variable or a custom event: a Lua identifier that is no keyword, no engine global
 *  and does not start with two underscores.
 * @param[in] iName Candidate name.
 * @return True when the name is usable.
 */
[[nodiscard]] OWL_API auto isValidName(std::string_view iName) -> bool;

/**
 * @brief
 *  Reads the text of a library graph or a Lua module from its asset path (`scripts/lib/maths.owlvs`).
 */
using LibraryReader = std::function<std::optional<std::string>(std::string_view)>;

/**
 * @brief
 *  Replace how the library graphs and the Lua modules are read; by default from the open pack, then the asset folders.
 * @param[in] iReader The reader, or empty for the default one.
 */
OWL_API void setLibraryReader(LibraryReader iReader);

/**
 * @brief
 *  Forget the library graphs and the Lua modules read so far, so that the next use reads them again (after a library is saved).
 */
OWL_API void clearLibraryCache();

/**
 * @brief
 *  A library graph, read once and kept.
 * @param[in] iPath Asset path of the `.owlvs`.
 * @return The graph, or nullptr when it cannot be read.
 */
[[nodiscard]] OWL_API auto findLibrary(std::string_view iPath) -> const VisualScript*;

/**
 * @brief
 *  Function a `func.*` node stands for: of its graph, or of the library its `library` value names.
 * @param[in] iScript The graph holding the node.
 * @param[in] iNode The node.
 * @return The function, or nullptr when unknown.
 */
[[nodiscard]] OWL_API auto findNodeFunction(const VisualScript& iScript, const GraphNode& iNode)
		-> const GraphFunction*;

/**
 * @brief
 *  Custom node a `custom.*` node stands for.
 * @param[in] iScript The graph holding the node.
 * @param[in] iNode The node, its `custom` value naming the custom node.
 * @return The custom node, or nullptr when unknown.
 */
[[nodiscard]] OWL_API auto findNodeCustom(const VisualScript& iScript, const GraphNode& iNode) -> const CustomNode*;

/**
 * @brief
 *  A function of a Lua module, annotated so that graphs call it.
 *
 * Its annotations are comment lines right above `function M.name(...)`: `--- text` the description,
 * `---\@param name type` each parameter, `---\@return type name` each result, `---\@pure` a function without side
 * effect, called where its results are read. The types are the pin type names (`number`, `vector3`, `entity`...).
 */
struct LuaFunction {
	/// Function name in its module table.
	std::string name;
	/// Description, from the comment lines above the annotations.
	std::string description;
	/// Parameters, inputs of the node.
	std::vector<GraphParameter> inputs;
	/// Results, outputs of the node.
	std::vector<GraphParameter> outputs;
	/// Whether the node has no execution pins.
	bool pure{false};

	/**
	 * @brief
	 *  Equality operator.
	 * @param[in] iOther Function to compare to.
	 * @return True when both functions are identical.
	 */
	auto operator==(const LuaFunction& iOther) const -> bool = default;
};

/**
 * @brief
 *  The annotated functions of a Lua module, in the order of its source.
 *
 * A function with no annotation line stays out: graphs see only what its author typed.
 * @param[in] iSource The Lua source of the module.
 * @return Its annotated functions.
 */
[[nodiscard]] OWL_API auto parseLuaFunctions(std::string_view iSource) -> std::vector<LuaFunction>;

/**
 * @brief
 *  Asset path of a module `require` loads.
 * @param[in] iModule Module name, words joined by dots (`scripts.lib.maths`).
 * @return Its path (`scripts/lib/maths.lua`).
 */
[[nodiscard]] OWL_API auto getLuaModulePath(std::string_view iModule) -> std::string;

/**
 * @brief
 *  The annotated functions of a Lua module, read once with the library reader and kept until `clearLibraryCache`.
 * @param[in] iModule Module name (`scripts.lib.maths`).
 * @return Its functions, or nullptr when the module cannot be read.
 */
[[nodiscard]] OWL_API auto findLuaModule(std::string_view iModule) -> const std::vector<LuaFunction>*;

/**
 * @brief
 *  Lua function a `lua.*` node stands for.
 * @param[in] iNode The node, its `module` and `function` values naming the function.
 * @return The function, or nullptr when unknown.
 */
[[nodiscard]] OWL_API auto findNodeLuaFunction(const GraphNode& iNode) -> const LuaFunction*;

/**
 * @brief
 *  Name of a pin type, as the graph files write it (`number`, `vector3`).
 * @param[in] iType The type.
 * @return Its name.
 */
[[nodiscard]] OWL_API auto getPinTypeKey(PinType iType) -> std::string_view;

/**
 * @brief
 *  Pin type of a name written by `getPinTypeKey`.
 * @param[in] iName The name.
 * @return The type, or nothing when unknown.
 */
[[nodiscard]] OWL_API auto parsePinTypeKey(std::string_view iName) -> std::optional<PinType>;

/**
 * @brief
 *  Whether a path names a visual script.
 * @param[in] iPath File path or chunk name.
 * @return True for a `.owlvs` file.
 */
[[nodiscard]] OWL_API auto isVisualScriptPath(std::string_view iPath) -> bool;

/**
 * @brief
 *  Write a visual script as YAML.
 * @param[in] iScript The script.
 * @return The YAML text.
 */
[[nodiscard]] OWL_API auto serialize(const VisualScript& iScript) -> std::string;

/**
 * @brief
 *  Read a visual script from YAML.
 * @param[in] iText The YAML text.
 * @param[in] iSourceName File name for the log messages.
 * @return The script, or why it could not be read.
 */
[[nodiscard]] OWL_API auto deserialize(std::string_view iText, std::string_view iSourceName)
		-> expected<VisualScript, VisualScriptError>;

/**
 * @brief
 *  What a compilation is for.
 */
enum struct CompileMode : uint8_t {
	Game,///< Plain Lua, as a game runs it.
	Debug///< With value probes: every wire keeps its last value in the global table `__owl_probes`.
};

/**
 * @brief
 *  A visual script compiled with its value probes.
 */
struct DebugBuild {
	/// The Lua source.
	std::string lua;
	/// Lines of the Lua source each node runs, from 1: a breakpoint on a node is one on these lines.
	std::map<core::UUID, std::vector<int>> nodeLines;
};

/**
 * @brief
 *  Key of a wire in the probe table: the output pin the wire leaves.
 * @param[in] iNode Node of the output.
 * @param[in] iPin Name of the output pin.
 * @return The key, `<node>:<pin>`.
 */
[[nodiscard]] OWL_API auto getProbeKey(core::UUID iNode, std::string_view iPin) -> std::string;

/**
 * @brief
 *  Compile a visual script to Lua.
 * @param[in] iScript The script.
 * @param[in] iMode Game or debug output.
 * @return The Lua source, or every error found.
 */
[[nodiscard]] OWL_API auto compile(const VisualScript& iScript, CompileMode iMode = CompileMode::Game)
		-> expected<std::string, std::vector<CompileDiagnostic>>;

/**
 * @brief
 *  Compile a visual script with its value probes and the Lua lines of every node.
 * @param[in] iScript The script.
 * @return The build, or every error found.
 */
[[nodiscard]] OWL_API auto compileForDebug(const VisualScript& iScript)
		-> expected<DebugBuild, std::vector<CompileDiagnostic>>;

/**
 * @brief
 *  Read and compile the text of a `.owlvs` file, logging every error.
 * @param[in] iText The YAML text.
 * @param[in] iSourceName File name for the log messages.
 * @param[in] iMode Game or debug output.
 * @return The Lua source, or an empty optional on failure.
 */
[[nodiscard]] OWL_API auto compileText(std::string_view iText, std::string_view iSourceName,
									   CompileMode iMode = CompileMode::Game) -> std::optional<std::string>;

}// namespace owl::script::visual
