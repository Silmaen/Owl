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
	GetVariable,///< Reads a graph variable.
	SetVariable///< Writes a graph variable.
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
	 *  Grouped pin it belongs to: its own name for a grouped pin (`rgba`), the group name for a component of a
	 *  split one (`r`), empty otherwise.
	 */
	std::string group;
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
	/// Lua code of the node: hook name, function to call, or expression with `{0}`, `{1}` for the inputs.
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
 *  A graph variable: a script property the inspector shows and the scene saves.
 */
struct GraphVariable {
	/// Lua name of the property.
	std::string name;
	/// Property type: `Float`, `Int`, `String`, `Bool` or `Entity`.
	ScriptPropertyType type{ScriptPropertyType::Float};
	/// Default value, as text.
	std::string defaultValue;

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
 *  A visual script: variables, nodes and links.
 */
struct VisualScript {
	/// Variables, in declaration order.
	std::vector<GraphVariable> variables;
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
 * The number parameters `x, y [, z]` and `r, g, b [, a]` of a binding form one grouped pin (`xyz` of type `Vector3`,
 * `rgba` of type `Color`), shown as its components when the node value `split.<group>` is `true`.
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
 *  Compile a visual script to Lua.
 * @param[in] iScript The script.
 * @return The Lua source, or every error found.
 */
[[nodiscard]] OWL_API auto compile(const VisualScript& iScript)
		-> expected<std::string, std::vector<CompileDiagnostic>>;

/**
 * @brief
 *  Read and compile the text of a `.owlvs` file, logging every error.
 * @param[in] iText The YAML text.
 * @param[in] iSourceName File name for the log messages.
 * @return The Lua source, or an empty optional on failure.
 */
[[nodiscard]] OWL_API auto compileText(std::string_view iText, std::string_view iSourceName)
		-> std::optional<std::string>;

}// namespace owl::script::visual
