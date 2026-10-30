/**
 * @file PcgGraph.h
 * @author Silmaen
 * @date 28/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/UUID.h"
#include "core/expected.h"
#include "math/vectors.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <map>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace owl::scene::pcg {

/// File extension of a procedural generation graph.
inline constexpr std::string_view g_PcgGraphExtension = ".owlpcg";

/// Tile value of a cell no output of the graph wrote: the tilemap keeps its tile.
inline constexpr int32_t g_UntouchedTile = std::numeric_limits<int32_t>::min();

/**
 * @brief
 *  Data carried by a pin.
 */
enum struct PinType : uint8_t {
	Field,///< One number per cell of the region, a mask when it holds 0 and 1.
	Points///< A set of cells of the region.
};

/**
 * @brief
 *  Kind of a node parameter.
 */
enum struct ParamType : uint8_t {
	Float,///< A number.
	Int,///< An integer.
	Choice///< One of the `choices`.
};

/**
 * @brief
 *  A pin of a node type.
 */
struct OWL_API PinSpec {
	/// Name, unique on its side of the node.
	std::string name;
	/// Carried data.
	PinType type{PinType::Field};
};

/**
 * @brief
 *  A parameter of a node type, edited in the inspector and saved in the node values.
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
	/// One-sentence description, shown as the tooltip.
	std::string description;
};

/**
 * @brief
 *  A kind of node: its pins and parameters.
 */
struct OWL_API NodeType {
	/// Stable identifier saved in the graph (`source.noise`).
	std::string id;
	/// Title shown on the node.
	std::string title;
	/// Palette group (`Sources`, `Fields`, `Points`, `Outputs`).
	std::string category;
	/// One-sentence description.
	std::string description;
	/// Input pins, in order.
	std::vector<PinSpec> inputs;
	/// Output pins, in order.
	std::vector<PinSpec> outputs;
	/// Parameters, in order.
	std::vector<ParamSpec> params;
};

/**
 * @brief
 *  A node of a graph.
 */
struct OWL_API GraphNode {
	/// Node identifier, also part of the seed of its random streams.
	core::UUID id;
	/// Identifier of its `NodeType`.
	std::string type;
	/// Top-left corner on the canvas; outputs apply from the top one down.
	std::array<float, 2> position{0.f, 0.f};
	/// Parameter values by name (defaults when absent).
	std::map<std::string, std::string> values;

	/**
	 * @brief
	 *  Comparison operator.
	 * @param[in] iOther The other node.
	 * @return True when both nodes are the same.
	 */
	auto operator==(const GraphNode& iOther) const -> bool = default;
};

/**
 * @brief
 *  A link from an output pin to an input pin.
 */
struct OWL_API GraphLink {
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

	/**
	 * @brief
	 *  Comparison operator.
	 * @param[in] iOther The other link.
	 * @return True when both links are the same.
	 */
	auto operator==(const GraphLink& iOther) const -> bool = default;
};

/**
 * @brief
 *  A procedural generation graph (`.owlpcg`): sources of noise and rules, operations on fields and points, and
 *  outputs writing tiles. Non-destructive: the graph is kept, its result regenerated from a seed.
 */
struct OWL_API Graph {
	/// Nodes.
	std::vector<GraphNode> nodes;
	/// Links; an input takes at most one.
	std::vector<GraphLink> links;

	/**
	 * @brief
	 *  Find a node.
	 * @param[in] iId The node.
	 * @return The node, null when absent.
	 */
	[[nodiscard]] auto findNode(core::UUID iId) const -> const GraphNode*;

	/**
	 * @brief
	 *  Find a node.
	 * @param[in] iId The node.
	 * @return The node, null when absent.
	 */
	[[nodiscard]] auto findNode(core::UUID iId) -> GraphNode*;

	/**
	 * @brief
	 *  Remove a node and its links.
	 * @param[in] iId The node.
	 */
	void removeNode(core::UUID iId);

	/**
	 * @brief
	 *  Value of a node parameter, its default when the node does not set it.
	 * @param[in] iNode The node.
	 * @param[in] iParam The parameter name.
	 * @return The value as text, empty for an unknown parameter.
	 */
	[[nodiscard]] static auto getValue(const GraphNode& iNode, std::string_view iParam) -> std::string;

	/**
	 * @brief
	 *  Comparison operator.
	 * @param[in] iOther The other graph.
	 * @return True when both graphs are the same.
	 */
	auto operator==(const Graph& iOther) const -> bool = default;
};

/**
 * @brief
 *  Failure of a graph read or evaluation.
 */
enum struct PcgError : uint8_t {
	InvalidYaml,///< The text is not YAML.
	UnsupportedVersion,///< Written by a newer engine, or a migration failed.
	InvalidContent,///< YAML without a `PcgGraph` map, or a value of the wrong kind.
	Cycle,///< A node depends on itself.
	EmptyRegion///< The region has no cell.
};

/**
 * @brief
 *  One number per cell of a region, row-major from the top-left.
 */
struct OWL_API Field {
	/// Size of the region in cells.
	math::vec2ui size{0, 0};
	/// Values, `size.x() * size.y()` of them.
	std::vector<float> values;

	/**
	 * @brief
	 *  Value of a cell.
	 * @param[in] iX Column.
	 * @param[in] iY Row.
	 * @return The value, 0 outside the region.
	 */
	[[nodiscard]] auto get(uint32_t iX, uint32_t iY) const -> float;

	/**
	 * @brief
	 *  Comparison operator.
	 * @param[in] iOther The other field.
	 * @return True when both fields are the same.
	 */
	auto operator==(const Field& iOther) const -> bool = default;
};

/**
 * @brief
 *  A set of cells of a region, in the order they were picked.
 */
struct OWL_API Points {
	/// The cells.
	std::vector<math::vec2ui> cells;

	/**
	 * @brief
	 *  Comparison operator.
	 * @param[in] iOther The other set.
	 * @return True when both sets are the same.
	 */
	auto operator==(const Points& iOther) const -> bool = default;
};

/// Data produced on an output pin.
using PinValue = std::variant<Field, Points>;

/**
 * @brief
 *  Where and from which seed a graph is evaluated.
 */
struct OWL_API Region {
	/// Size in cells.
	math::vec2ui size{0, 0};
	/// Seed of every random stream of the graph: the same seed gives the same result.
	uint64_t seed{0};
};

/**
 * @brief
 *  Result of an evaluation.
 */
struct OWL_API Result {
	/// Size of the region in cells.
	math::vec2ui size{0, 0};
	/// Tile per cell, row-major from the top-left: a tile index, -1 to erase, `g_UntouchedTile` to keep the cell.
	std::vector<int32_t> tiles;
	/// Value of each output pin, by node and pin name (previews of the editor).
	std::map<core::UUID, std::map<std::string, PinValue>> values;
	/// Problems met (unknown node, unlinked input), one sentence each.
	std::vector<std::string> warnings;
	/// Heights written by an `output.height` node (the lowest one on the canvas wins), an empty field without one.
	Field height;
};

/**
 * @brief
 *  Every node type, in palette order.
 * @return The node types.
 */
[[nodiscard]] OWL_API auto getNodeTypes() -> const std::vector<NodeType>&;

/**
 * @brief
 *  Find a node type.
 * @param[in] iId The type identifier.
 * @return The type, null when unknown.
 */
[[nodiscard]] OWL_API auto findNodeType(std::string_view iId) -> const NodeType*;

/**
 * @brief
 *  Whether an output pin may feed an input pin.
 * @param[in] iFrom The output type.
 * @param[in] iTo The input type.
 * @return True when the types match.
 */
[[nodiscard]] OWL_API auto canConnect(PinType iFrom, PinType iTo) -> bool;

/**
 * @brief
 *  Evaluate a graph over a region: every node reachable from an output, each output writing its tiles from the top
 *  one on the canvas down, so a lower output wins.
 * @param[in] iGraph The graph.
 * @param[in] iRegion The region and the seed.
 * @return The tiles and the pin values, `Cycle` or `EmptyRegion`.
 */
[[nodiscard]] OWL_API auto evaluate(const Graph& iGraph, const Region& iRegion) -> expected<Result, PcgError>;

/**
 * @brief
 *  Write a graph as YAML.
 * @param[in] iGraph The graph.
 * @return The text.
 */
[[nodiscard]] OWL_API auto serialize(const Graph& iGraph) -> std::string;

/**
 * @brief
 *  Read a graph from YAML.
 * @param[in] iText The text.
 * @param[in] iSourceName Name used in the warnings.
 * @return The graph, or why it cannot be read.
 */
[[nodiscard]] OWL_API auto deserialize(std::string_view iText, std::string_view iSourceName)
		-> expected<Graph, PcgError>;

/**
 * @brief
 *  Read a graph file.
 * @param[in] iPath The file.
 * @return The graph, or why it cannot be read.
 */
[[nodiscard]] OWL_API auto loadFromFile(const std::filesystem::path& iPath) -> expected<Graph, PcgError>;

/**
 * @brief
 *  Write a graph file. Atomic: a failed write keeps the previous file.
 * @param[in] iGraph The graph.
 * @param[in] iPath The file.
 * @return True on success.
 */
[[nodiscard]] OWL_API auto saveToFile(const Graph& iGraph, const std::filesystem::path& iPath) -> bool;

}// namespace owl::scene::pcg
