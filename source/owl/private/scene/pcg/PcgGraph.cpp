/**
 * @file PcgGraph.cpp
 * @author Silmaen
 * @date 28/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "scene/pcg/PcgGraph.h"

#include "core/FormatVersionYaml.h"
#include "core/external/yaml.h"
#include "platform/AtomicFile.h"
#include "platform/FileUtils.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <exception>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace owl::scene::pcg {

namespace {

constexpr std::array<core::MigrationStep, 0> g_pcgMigrations{};
constexpr core::DocumentFormat g_pcgFormat{.name = "PcgGraph", .migrations = g_pcgMigrations};

auto floatParam(std::string iName, std::string iDefault, const float iMin, const float iMax, std::string iDescription)
		-> ParamSpec {
	return {.name = std::move(iName),
			.type = ParamType::Float,
			.defaultValue = std::move(iDefault),
			.min = iMin,
			.max = iMax,
			.choices = {},
			.description = std::move(iDescription)};
}

auto intParam(std::string iName, std::string iDefault, const float iMin, const float iMax, std::string iDescription)
		-> ParamSpec {
	return {.name = std::move(iName),
			.type = ParamType::Int,
			.defaultValue = std::move(iDefault),
			.min = iMin,
			.max = iMax,
			.choices = {},
			.description = std::move(iDescription)};
}

auto choiceParam(std::string iName, std::vector<std::string> iChoices, std::string iDescription) -> ParamSpec {
	auto defaultValue = iChoices.front();
	return {.name = std::move(iName),
			.type = ParamType::Choice,
			.defaultValue = std::move(defaultValue),
			.min = 0.f,
			.max = 0.f,
			.choices = std::move(iChoices),
			.description = std::move(iDescription)};
}

auto field(std::string iName) -> PinSpec { return {.name = std::move(iName), .type = PinType::Field}; }

auto points(std::string iName) -> PinSpec { return {.name = std::move(iName), .type = PinType::Points}; }

auto seedParam() -> ParamSpec {
	return intParam("seed", "0", 0.f, 1000000.f,
					"Added to the seed of the generation: two nodes of the same seed and "
					"settings still differ, the node identifier is part of the stream.");
}

auto buildNodeTypes() -> std::vector<NodeType> {
	std::vector<NodeType> types;
	types.push_back(
			{.id = "source.noise",
			 .title = "Noise",
			 .category = "Sources",
			 .description = "Smooth value noise in [0, 1], summed over octaves (hills, biomes, caves).",
			 .inputs = {},
			 .outputs = {field("Field")},
			 .params = {floatParam("scale", "8", 1.f, 256.f, "Size of the features in cells."),
						intParam("octaves", "3", 1.f, 8.f, "Layers of finer noise added."),
						floatParam("persistence", "0.5", 0.f, 1.f, "Weight of each finer layer."), seedParam()}});
	types.push_back({.id = "source.random",
					 .title = "Random",
					 .category = "Sources",
					 .description = "An independent random value in [0, 1] per cell (the start of a cave automaton).",
					 .inputs = {},
					 .outputs = {field("Field")},
					 .params = {seedParam()}});
	types.push_back({.id = "source.constant",
					 .title = "Constant",
					 .category = "Sources",
					 .description = "The same value on every cell.",
					 .inputs = {},
					 .outputs = {field("Field")},
					 .params = {floatParam("value", "1", -1000.f, 1000.f, "The value.")}});
	types.push_back({.id = "source.gradient",
					 .title = "Gradient",
					 .category = "Sources",
					 .description = "From 0 to 1 across the region: left to right, top to bottom, or from the centre "
									"out (islands).",
					 .inputs = {},
					 .outputs = {field("Field")},
					 .params = {choiceParam("direction", {"horizontal", "vertical", "radial"},
											"Direction the value grows along.")}});
	types.push_back({.id = "field.threshold",
					 .title = "Threshold",
					 .category = "Fields",
					 .description = "A mask: 1 where the value lies in [min, max], 0 elsewhere.",
					 .inputs = {field("Field")},
					 .outputs = {field("Mask")},
					 .params = {floatParam("min", "0.5", -1000.f, 1000.f, "Lowest value kept."),
								floatParam("max", "1", -1000.f, 1000.f, "Highest value kept.")}});
	types.push_back({.id = "field.combine",
					 .title = "Combine",
					 .category = "Fields",
					 .description = "Two fields merged cell by cell.",
					 .inputs = {field("A"), field("B")},
					 .outputs = {field("Field")},
					 .params = {choiceParam("operation", {"add", "subtract", "multiply", "min", "max", "average"},
											"How the two values merge.")}});
	types.push_back({.id = "field.invert",
					 .title = "Invert",
					 .category = "Fields",
					 .description = "1 minus the value: a mask turned inside out.",
					 .inputs = {field("Field")},
					 .outputs = {field("Field")},
					 .params = {}});
	types.push_back({.id = "field.blur",
					 .title = "Blur",
					 .category = "Fields",
					 .description = "Each value averaged with its neighbours over a square of the radius.",
					 .inputs = {field("Field")},
					 .outputs = {field("Field")},
					 .params = {intParam("radius", "1", 1.f, 16.f, "Half side of the square in cells.")}});
	types.push_back(
			{.id = "field.cellular",
			 .title = "Cellular Automaton",
			 .category = "Fields",
			 .description = "Cave rule on a mask: a cell becomes solid with at least `birth` solid "
							"neighbours, stays solid with at least `survival`, repeated.",
			 .inputs = {field("Mask")},
			 .outputs = {field("Mask")},
			 .params = {intParam("iterations", "4", 0.f, 16.f, "Steps of the rule."),
						intParam("birth", "5", 0.f, 8.f, "Solid neighbours that fill an empty cell."),
						intParam("survival", "4", 0.f, 8.f, "Solid neighbours that keep a solid cell."),
						choiceParam("edges", {"solid", "open"}, "Whether the cells past the region count as solid.")}});
	types.push_back({.id = "field.edge",
					 .title = "Edge",
					 .category = "Fields",
					 .description = "The cells of a mask that touch a cell outside it (walls around rooms).",
					 .inputs = {field("Mask")},
					 .outputs = {field("Mask")},
					 .params = {choiceParam("neighbours", {"8", "4"}, "Diagonal neighbours count or not.")}});
	types.push_back({.id = "points.scatter",
					 .title = "Scatter",
					 .category = "Points",
					 .description = "Random cells of a mask, kept apart by a minimum distance (trees, rocks, chests).",
					 .inputs = {field("Mask")},
					 .outputs = {points("Points")},
					 .params = {floatParam("density", "0.1", 0.f, 1.f, "Share of the mask cells tried."),
								intParam("spacing", "1", 0.f, 64.f, "Smallest distance between two points, in cells."),
								seedParam()}});
	types.push_back({.id = "points.filter",
					 .title = "Filter Points",
					 .category = "Points",
					 .description = "The points where a field lies in [min, max].",
					 .inputs = {points("Points"), field("Field")},
					 .outputs = {points("Points")},
					 .params = {floatParam("min", "0.5", -1000.f, 1000.f, "Lowest value kept."),
								floatParam("max", "1", -1000.f, 1000.f, "Highest value kept.")}});
	types.push_back({.id = "points.to_mask",
					 .title = "Points to Mask",
					 .category = "Points",
					 .description = "A mask of the points, each grown to a square of the radius.",
					 .inputs = {points("Points")},
					 .outputs = {field("Mask")},
					 .params = {intParam("radius", "0", 0.f, 16.f, "Half side of the square in cells.")}});
	types.push_back({.id = "output.tiles",
					 .title = "Write Tiles",
					 .category = "Outputs",
					 .description = "Writes a tile on the cells where the mask is at least 0.5.",
					 .inputs = {field("Mask")},
					 .outputs = {},
					 .params = {intParam("tile", "0", 0.f, 65535.f, "Tile index in the tileset.")}});
	types.push_back({.id = "output.points",
					 .title = "Write Tiles at Points",
					 .category = "Outputs",
					 .description = "Writes a tile on each point.",
					 .inputs = {points("Points")},
					 .outputs = {},
					 .params = {intParam("tile", "0", 0.f, 65535.f, "Tile index in the tileset.")}});
	types.push_back({.id = "output.erase",
					 .title = "Erase Tiles",
					 .category = "Outputs",
					 .description = "Empties the cells where the mask is at least 0.5.",
					 .inputs = {field("Mask")},
					 .outputs = {},
					 .params = {}});
	return types;
}

void emitGraph(YAML::Emitter& ioOut, const Graph& iGraph) {
	ioOut << YAML::Key << "nodes" << YAML::Value << YAML::BeginSeq;
	for (const auto& node: iGraph.nodes) {
		ioOut << YAML::BeginMap;
		ioOut << YAML::Key << "id" << YAML::Value << static_cast<uint64_t>(node.id);
		ioOut << YAML::Key << "type" << YAML::Value << node.type;
		ioOut << YAML::Key << "position" << YAML::Value << YAML::Flow << YAML::BeginSeq << node.position[0]
			  << node.position[1] << YAML::EndSeq;
		if (!node.values.empty()) {
			ioOut << YAML::Key << "values" << YAML::Value << YAML::BeginMap;
			for (const auto& [key, value]: node.values) ioOut << YAML::Key << key << YAML::Value << value;
			ioOut << YAML::EndMap;
		}
		ioOut << YAML::EndMap;
	}
	ioOut << YAML::EndSeq;
	ioOut << YAML::Key << "links" << YAML::Value << YAML::BeginSeq;
	for (const auto& link: iGraph.links) {
		ioOut << YAML::Flow << YAML::BeginMap;
		ioOut << YAML::Key << "from" << YAML::Value << static_cast<uint64_t>(link.fromNode);
		ioOut << YAML::Key << "fromPin" << YAML::Value << link.fromPin;
		ioOut << YAML::Key << "to" << YAML::Value << static_cast<uint64_t>(link.toNode);
		ioOut << YAML::Key << "toPin" << YAML::Value << link.toPin;
		if (!link.waypoints.empty()) {
			ioOut << YAML::Key << "waypoints" << YAML::Value << YAML::BeginSeq;
			for (const auto& [x, y]: link.waypoints) ioOut << YAML::BeginSeq << x << y << YAML::EndSeq;
			ioOut << YAML::EndSeq;
		}
		ioOut << YAML::EndMap;
	}
	ioOut << YAML::EndSeq;
}

auto parseWaypoints(const YAML::Node& iLink) -> std::vector<std::array<float, 2>> {
	std::vector<std::array<float, 2>> waypoints;
	for (const auto& point: iLink["waypoints"])
		if (point.IsSequence() && point.size() == 2)
			waypoints.push_back({point[0].as<float>(), point[1].as<float>()});
	return waypoints;
}

auto parseGraph(const YAML::Node& iRoot) -> Graph {
	Graph graph;
	if (const auto nodes = iRoot["nodes"]; nodes && nodes.IsSequence()) {
		for (const auto& entry: nodes) {
			GraphNode node{.id = core::UUID{entry["id"].as<uint64_t>()},
						   .type = entry["type"].as<std::string>(),
						   .position = {0.f, 0.f},
						   .values = {}};
			if (const auto pos = entry["position"]; pos && pos.IsSequence() && pos.size() >= 2)
				node.position = {pos[0].as<float>(), pos[1].as<float>()};
			if (const auto values = entry["values"]; values && values.IsMap()) {
				for (const auto& item: values)
					node.values[item.first.as<std::string>()] = item.second.as<std::string>();
			}
			graph.nodes.push_back(std::move(node));
		}
	}
	if (const auto links = iRoot["links"]; links && links.IsSequence()) {
		for (const auto& entry: links)
			graph.links.push_back({.fromNode = core::UUID{entry["from"].as<uint64_t>()},
								   .fromPin = entry["fromPin"].as<std::string>(),
								   .toNode = core::UUID{entry["to"].as<uint64_t>()},
								   .toPin = entry["toPin"].as<std::string>(),
								   .waypoints = parseWaypoints(entry)});
	}
	return graph;
}

}// namespace

auto Graph::findNode(const core::UUID iId) const -> const GraphNode* {
	const auto found = std::ranges::find(nodes, iId, &GraphNode::id);
	return found == nodes.end() ? nullptr : &*found;
}

auto Graph::findNode(const core::UUID iId) -> GraphNode* {
	const auto found = std::ranges::find(nodes, iId, &GraphNode::id);
	return found == nodes.end() ? nullptr : &*found;
}

void Graph::removeNode(const core::UUID iId) {
	std::erase_if(nodes, [iId](const GraphNode& iNode) -> bool { return iNode.id == iId; });
	std::erase_if(links,
				  [iId](const GraphLink& iLink) -> bool { return iLink.fromNode == iId || iLink.toNode == iId; });
}

auto Graph::getValue(const GraphNode& iNode, const std::string_view iParam) -> std::string {
	if (const auto found = iNode.values.find(std::string{iParam}); found != iNode.values.end())
		return found->second;
	if (const auto* type = findNodeType(iNode.type); type != nullptr) {
		if (const auto param = std::ranges::find(type->params, iParam, &ParamSpec::name); param != type->params.end())
			return param->defaultValue;
	}
	return {};
}

auto Field::get(const uint32_t iX, const uint32_t iY) const -> float {
	if (iX >= size.x() || iY >= size.y())
		return 0.f;
	return values[static_cast<size_t>(iY) * size.x() + iX];
}

auto getNodeTypes() -> const std::vector<NodeType>& {
	static const auto types = buildNodeTypes();
	return types;
}

auto findNodeType(const std::string_view iId) -> const NodeType* {
	const auto& types = getNodeTypes();
	const auto found = std::ranges::find(types, iId, &NodeType::id);
	return found == types.end() ? nullptr : &*found;
}

auto canConnect(const PinType iFrom, const PinType iTo) -> bool { return iFrom == iTo; }

auto serialize(const Graph& iGraph) -> std::string {
	YAML::Emitter out;
	out << YAML::BeginMap;
	core::emitFormatVersion(out, g_pcgFormat);
	out << YAML::Key << "PcgGraph" << YAML::Value << YAML::BeginMap;
	emitGraph(out, iGraph);
	out << YAML::EndMap;
	out << YAML::EndMap;
	return std::string{out.c_str()} + "\n";
}

auto deserialize(const std::string_view iText, const std::string_view iSourceName) -> expected<Graph, PcgError> {
	YAML::Node root;
	try {
		root = YAML::Load(std::string{iText});
	} catch (const std::exception& iError) {
		OWL_CORE_WARN("PcgGraph: Cannot parse '{}': {}.", iSourceName, iError.what())
		return unexpected{PcgError::InvalidYaml};
	}
	if (!root.IsMap() || !root["PcgGraph"] || !root["PcgGraph"].IsMap()) {
		OWL_CORE_WARN("PcgGraph: '{}' has no PcgGraph map.", iSourceName)
		return unexpected{PcgError::InvalidContent};
	}
	if (!core::upgradeYamlDocument(g_pcgFormat, root, iSourceName)) {
		OWL_CORE_WARN("PcgGraph: Cannot read the format of '{}'.", iSourceName)
		return unexpected{PcgError::UnsupportedVersion};
	}
	try {
		return parseGraph(root["PcgGraph"]);
	} catch (const std::exception& iError) {
		OWL_CORE_WARN("PcgGraph: Invalid content in '{}': {}.", iSourceName, iError.what())
		return unexpected{PcgError::InvalidContent};
	}
}

auto loadFromFile(const std::filesystem::path& iPath) -> expected<Graph, PcgError> {
	if (std::error_code error; !std::filesystem::is_regular_file(iPath, error)) {
		OWL_CORE_WARN("PcgGraph: Cannot read '{}'.", iPath.string())
		return unexpected{PcgError::InvalidContent};
	}
	return deserialize(platform::fileToString(iPath), iPath.string());
}

auto saveToFile(const Graph& iGraph, const std::filesystem::path& iPath) -> bool {
	if (const auto written = platform::writeFileAtomic(iPath, serialize(iGraph)); !written) {
		OWL_CORE_ERROR("PcgGraph: Failed to write '{}'.", iPath.string())
		return false;
	}
	return true;
}

}// namespace owl::scene::pcg
