/**
 * @file BehaviourTree.h
 * @author Silmaen
 * @date 10/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/Macros.h"
#include "core/UUID.h"
#include "core/expected.h"
#include "math/vectors.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace owl::scene::ai {

/// File extension of a behaviour tree.
inline constexpr std::string_view g_BehaviourTreeExtension = ".owlbt";

/**
 * @brief
 *  Kind of a value of the blackboard.
 */
enum struct BlackboardType : uint8_t {
	Bool,///< A flag.
	Number,///< A number.
	String,///< A text.
	Entity,///< An entity, by UUID (0: none).
	Vector///< A point or a direction.
};

/// A value of the blackboard, in the order of `BlackboardType`.
using BlackboardValue = std::variant<bool, double, std::string, core::UUID, math::vec3>;

/**
 * @brief
 *  The kind of a value.
 * @param[in] iValue The value.
 * @return Its kind.
 */
[[nodiscard]] OWL_API auto getBlackboardType(const BlackboardValue& iValue) -> BlackboardType;

/// Every kind of blackboard value, in order.
inline constexpr std::array g_BlackboardTypes{BlackboardType::Bool, BlackboardType::Number, BlackboardType::String,
											  BlackboardType::Entity, BlackboardType::Vector};

/**
 * @brief
 *  Name of a kind of value, as the files and the commands write it (`Number`).
 * @param[in] iType The kind.
 * @return The name.
 */
[[nodiscard]] OWL_API auto getBlackboardTypeName(BlackboardType iType) -> std::string_view;

/**
 * @brief
 *  The kind of value of a name (the reverse of `getBlackboardTypeName`).
 * @param[in] iName The name.
 * @return The kind, none for an unknown name.
 */
[[nodiscard]] OWL_API auto parseBlackboardType(std::string_view iName) -> std::optional<BlackboardType>;

/**
 * @brief
 *  The value a key of a kind holds before anything sets it: false, 0, empty, none, the origin.
 * @param[in] iType The kind.
 * @return The value.
 */
[[nodiscard]] OWL_API auto getDefaultValue(BlackboardType iType) -> BlackboardValue;

/**
 * @brief
 *  A value as text: `true`, `2.5`, the text itself, the UUID, `x, y, z`.
 * @param[in] iValue The value.
 * @return The text.
 */
[[nodiscard]] OWL_API auto toText(const BlackboardValue& iValue) -> std::string;

/**
 * @brief
 *  Read a value of a kind from its text (the reverse of `toText`).
 * @param[in] iType The kind.
 * @param[in] iText The text.
 * @return The value, none when the text is not one of that kind.
 */
[[nodiscard]] OWL_API auto parseValue(BlackboardType iType, std::string_view iText) -> std::optional<BlackboardValue>;

/**
 * @brief
 *  A key of the blackboard a tree declares, with the value it starts with.
 */
struct OWL_API BlackboardKey {
	/// Name, unique in the tree.
	std::string name;
	/// Value at the start; its kind is the kind of the key.
	BlackboardValue value{0.0};
	/// One sentence shown in the editor.
	std::string description;

	OWL_DIAG_PUSH
	OWL_DIAG_DISABLE_CLANG("-Wfloat-equal")
	/**
	 * @brief
	 *  Compare two keys.
	 * @param[in] iOther The other key.
	 * @return True when equal.
	 */
	auto operator==(const BlackboardKey& iOther) const -> bool = default;
	OWL_DIAG_POP
};

/**
 * @brief
 *  Role of a node in the tree.
 */
enum struct NodeKind : uint8_t {
	Root,///< The entry: one child, one root per tree.
	Composite,///< Runs its children in an order: sequence, selector, parallel.
	Decorator,///< Changes what its only child does or returns.
	Condition,///< A leaf that checks and returns at once.
	Action///< A leaf that acts, possibly over several ticks.
};

/**
 * @brief
 *  Kind of value of a node parameter.
 */
enum struct ParamType : uint8_t {
	Float,///< A number.
	Int,///< A whole number.
	Bool,///< A flag.
	Text,///< A free text (a Lua function, a message).
	Choice,///< One of the `choices`.
	Key///< The name of a blackboard key.
};

/**
 * @brief
 *  A parameter of a node type.
 */
struct OWL_API ParamSpec {
	/// Name, the key of the node value.
	std::string name;
	/// Kind of value.
	ParamType type{ParamType::Float};
	/// Value of a node that does not set it.
	std::string defaultValue;
	/// Smallest value of a number.
	float min{0.f};
	/// Largest value of a number.
	float max{1.f};
	/// Values of a choice.
	std::vector<std::string> choices;
	/// One sentence, shown as the tooltip.
	std::string description;
};

/**
 * @brief
 *  A kind of node of the palette.
 */
struct OWL_API NodeType {
	/// Stable identifier saved in the tree (`composite.sequence`).
	std::string id;
	/// Title shown on the node.
	std::string title;
	/// Role in the tree.
	NodeKind kind{NodeKind::Action};
	/// One sentence.
	std::string description;
	/// Parameters, in order.
	std::vector<ParamSpec> params;
};

/**
 * @brief
 *  A node of a tree.
 */
struct OWL_API TreeNode {
	/// Identifier.
	core::UUID id;
	/// Identifier of its `NodeType`.
	std::string type;
	/// Top-left corner on the canvas; the children of a node run from top to bottom.
	std::array<float, 2> position{0.f, 0.f};
	/// Parameter values by name (defaults when absent).
	std::map<std::string, std::string> values;
	/// Note shown under the title in the editor.
	std::string comment;

	OWL_DIAG_PUSH
	OWL_DIAG_DISABLE_CLANG("-Wfloat-equal")
	/**
	 * @brief
	 *  Compare two nodes.
	 * @param[in] iOther The other node.
	 * @return True when equal.
	 */
	auto operator==(const TreeNode& iOther) const -> bool = default;
	OWL_DIAG_POP
};

/**
 * @brief
 *  A link from a node to one of its children.
 */
struct OWL_API TreeLink {
	/// The parent.
	core::UUID parent{0};
	/// The child; a node has at most one parent.
	core::UUID child{0};

	OWL_DIAG_PUSH
	OWL_DIAG_DISABLE_CLANG("-Wfloat-equal")
	/**
	 * @brief
	 *  Compare two links.
	 * @param[in] iOther The other link.
	 * @return True when equal.
	 */
	auto operator==(const TreeLink& iOther) const -> bool = default;
	OWL_DIAG_POP
};

/**
 * @brief
 *  A behaviour tree: nodes, the links from parents to children, the blackboard keys (`.owlbt`).
 */
struct OWL_API BehaviourTree {
	/// Nodes.
	std::vector<TreeNode> nodes;
	/// Links from parents to children.
	std::vector<TreeLink> links;
	/// Keys of the blackboard of every agent running the tree.
	std::vector<BlackboardKey> blackboard;

	/**
	 * @brief
	 *  Find a node.
	 * @param[in] iId Its identifier.
	 * @return The node, null when absent.
	 */
	[[nodiscard]] auto findNode(core::UUID iId) const -> const TreeNode*;

	/**
	 * @brief
	 *  Find a node.
	 * @param[in] iId Its identifier.
	 * @return The node, null when absent.
	 */
	[[nodiscard]] auto findNode(core::UUID iId) -> TreeNode*;

	/**
	 * @brief
	 *  Remove a node and its links.
	 * @param[in] iId Its identifier.
	 */
	void removeNode(core::UUID iId);

	/**
	 * @brief
	 *  The root node: the first node of type `root`.
	 * @return The root, null when the tree has none.
	 */
	[[nodiscard]] auto getRoot() const -> const TreeNode*;

	/**
	 * @brief
	 *  The children of a node, in their run order: from top to bottom on the canvas, then from left to right.
	 * @param[in] iId The parent.
	 * @return The identifiers of its children.
	 */
	[[nodiscard]] auto getChildren(core::UUID iId) const -> std::vector<core::UUID>;

	/**
	 * @brief
	 *  The parent of a node.
	 * @param[in] iId The node.
	 * @return The parent, none for a node without one.
	 */
	[[nodiscard]] auto getParent(core::UUID iId) const -> std::optional<core::UUID>;

	/**
	 * @brief
	 *  Find a key of the blackboard.
	 * @param[in] iName Its name.
	 * @return The key, null when the tree does not declare it.
	 */
	[[nodiscard]] auto findKey(std::string_view iName) const -> const BlackboardKey*;

	/**
	 * @brief
	 *  The value of a node parameter: the one set on the node, else the default of its type.
	 * @param[in] iNode The node.
	 * @param[in] iParam The name of the parameter.
	 * @return The value as text, empty for an unknown parameter.
	 */
	[[nodiscard]] static auto getValue(const TreeNode& iNode, std::string_view iParam) -> std::string;

	/**
	 * @brief
	 *  The problems of the tree, one sentence each: no root, a child too many, an unknown node or key.
	 * @return The problems; empty for a tree that runs as drawn.
	 */
	[[nodiscard]] auto validate() const -> std::vector<std::string>;

	OWL_DIAG_PUSH
	OWL_DIAG_DISABLE_CLANG("-Wfloat-equal")
	/**
	 * @brief
	 *  Compare two trees.
	 * @param[in] iOther The other tree.
	 * @return True when equal.
	 */
	auto operator==(const BehaviourTree& iOther) const -> bool = default;
	OWL_DIAG_POP
};

/**
 * @brief
 *  Why a tree could not be read.
 */
enum struct TreeError : uint8_t {
	InvalidYaml,///< The text is not YAML.
	UnsupportedVersion,///< Written by a newer engine, or a migration failed.
	InvalidContent///< YAML without a `BehaviourTree` map, or a value of the wrong kind.
};

/**
 * @brief
 *  Every node type of the palette.
 * @return The node types, in palette order.
 */
[[nodiscard]] OWL_API auto getNodeTypes() -> const std::vector<NodeType>&;

/**
 * @brief
 *  Find a node type.
 * @param[in] iId Its identifier.
 * @return The type, null when unknown.
 */
[[nodiscard]] OWL_API auto findNodeType(std::string_view iId) -> const NodeType*;

/**
 * @brief
 *  The largest number of children of a node kind.
 * @param[in] iKind The kind.
 * @return 1 for the root and the decorators, no limit for the composites, 0 for the leaves.
 */
[[nodiscard]] OWL_API auto getMaxChildren(NodeKind iKind) -> size_t;

/**
 * @brief
 *  Write a tree as YAML.
 * @param[in] iTree The tree.
 * @return The text.
 */
[[nodiscard]] OWL_API auto serialize(const BehaviourTree& iTree) -> std::string;

/**
 * @brief
 *  Read a tree from YAML.
 * @param[in] iText The text.
 * @param[in] iSourceName Name of the source, for the messages.
 * @return The tree or why it could not be read.
 */
[[nodiscard]] OWL_API auto deserialize(std::string_view iText, std::string_view iSourceName)
		-> expected<BehaviourTree, TreeError>;

/**
 * @brief
 *  Read a tree file from the disk.
 * @param[in] iPath The file.
 * @return The tree or why it could not be read.
 */
[[nodiscard]] OWL_API auto loadFromFile(const std::filesystem::path& iPath) -> expected<BehaviourTree, TreeError>;

/**
 * @brief
 *  Read a tree asset from the open pack, else the asset folders.
 * @param[in] iAsset The asset path.
 * @return The tree or why it could not be read.
 */
[[nodiscard]] OWL_API auto loadAsset(const std::filesystem::path& iAsset) -> expected<BehaviourTree, TreeError>;

/**
 * @brief
 *  Write a tree file.
 * @param[in] iTree The tree.
 * @param[in] iPath The file.
 * @return True when written.
 */
[[nodiscard]] OWL_API auto saveToFile(const BehaviourTree& iTree, const std::filesystem::path& iPath) -> bool;

}// namespace owl::scene::ai
