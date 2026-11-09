/**
 * @file MaterialGraphYaml.cpp
 * @author Silmaen
 * @date 09/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "MaterialGraphYaml.h"

#include <magic_enum/magic_enum.hpp>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace owl::renderer {

namespace {

void emitPins(YAML::Emitter& ioOut, const char* iKey, const std::vector<MaterialPinSpec>& iPins) {
	ioOut << YAML::Key << iKey << YAML::Value << YAML::BeginSeq;
	for (const auto& pin: iPins) {
		ioOut << YAML::Flow << YAML::BeginMap;
		ioOut << YAML::Key << "name" << YAML::Value << pin.name;
		ioOut << YAML::Key << "type" << YAML::Value << std::string{magic_enum::enum_name(pin.type)};
		if (!pin.defaultValue.empty())
			ioOut << YAML::Key << "default" << YAML::Value << pin.defaultValue;
		ioOut << YAML::EndMap;
	}
	ioOut << YAML::EndSeq;
}

void emitNodes(YAML::Emitter& ioOut, const std::vector<MaterialGraphNode>& iNodes,
			   const std::vector<MaterialGraphLink>& iLinks) {
	ioOut << YAML::Key << "nodes" << YAML::Value << YAML::BeginSeq;
	for (const auto& node: iNodes) {
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
	for (const auto& link: iLinks) {
		ioOut << YAML::Flow << YAML::BeginMap;
		ioOut << YAML::Key << "from" << YAML::Value << static_cast<uint64_t>(link.fromNode);
		ioOut << YAML::Key << "fromPin" << YAML::Value << link.fromPin;
		ioOut << YAML::Key << "to" << YAML::Value << static_cast<uint64_t>(link.toNode);
		ioOut << YAML::Key << "toPin" << YAML::Value << link.toPin;
		ioOut << YAML::EndMap;
	}
	ioOut << YAML::EndSeq;
}

auto parsePins(const YAML::Node& iPins) -> std::vector<MaterialPinSpec> {
	std::vector<MaterialPinSpec> pins;
	if (!iPins || !iPins.IsSequence())
		return pins;
	for (const auto& entry: iPins) {
		MaterialPinSpec pin{.name = entry["name"].as<std::string>(),
							.type = magic_enum::enum_cast<MaterialPinType>(entry["type"].as<std::string>())
											.value_or(MaterialPinType::Float),
							.defaultValue = {}};
		if (const auto value = entry["default"]; value)
			pin.defaultValue = value.as<std::string>();
		pins.push_back(std::move(pin));
	}
	return pins;
}

void parseNodes(const YAML::Node& iNode, std::vector<MaterialGraphNode>& oNodes,
				std::vector<MaterialGraphLink>& oLinks) {
	if (const auto nodes = iNode["nodes"]; nodes && nodes.IsSequence()) {
		for (const auto& entry: nodes) {
			MaterialGraphNode node{.id = core::UUID{entry["id"].as<uint64_t>()},
								   .type = entry["type"].as<std::string>(),
								   .position = {0.f, 0.f},
								   .values = {}};
			if (const auto pos = entry["position"]; pos && pos.IsSequence() && pos.size() >= 2)
				node.position = {pos[0].as<float>(), pos[1].as<float>()};
			if (const auto values = entry["values"]; values && values.IsMap()) {
				for (const auto& item: values)
					node.values[item.first.as<std::string>()] = item.second.as<std::string>();
			}
			oNodes.push_back(std::move(node));
		}
	}
	if (const auto links = iNode["links"]; links && links.IsSequence()) {
		for (const auto& entry: links)
			oLinks.push_back({.fromNode = core::UUID{entry["from"].as<uint64_t>()},
							  .fromPin = entry["fromPin"].as<std::string>(),
							  .toNode = core::UUID{entry["to"].as<uint64_t>()},
							  .toPin = entry["toPin"].as<std::string>()});
	}
}

}// namespace

void emitMaterialGraph(YAML::Emitter& ioOut, const MaterialGraph& iGraph) {
	ioOut << YAML::BeginMap;
	if (!iGraph.parameters.empty()) {
		ioOut << YAML::Key << "parameters" << YAML::Value << YAML::BeginSeq;
		for (const auto& parameter: iGraph.parameters) {
			ioOut << YAML::Flow << YAML::BeginMap;
			ioOut << YAML::Key << "name" << YAML::Value << parameter.name;
			ioOut << YAML::Key << "type" << YAML::Value << std::string{magic_enum::enum_name(parameter.type)};
			ioOut << YAML::Key << "default" << YAML::Value << parameter.defaultValue;
			ioOut << YAML::EndMap;
		}
		ioOut << YAML::EndSeq;
	}
	if (!iGraph.customNodes.empty()) {
		ioOut << YAML::Key << "customNodes" << YAML::Value << YAML::BeginSeq;
		for (const auto& custom: iGraph.customNodes) {
			ioOut << YAML::BeginMap;
			ioOut << YAML::Key << "name" << YAML::Value << custom.name;
			if (!custom.title.empty())
				ioOut << YAML::Key << "title" << YAML::Value << custom.title;
			emitPins(ioOut, "inputs", custom.inputs);
			emitPins(ioOut, "outputs", custom.outputs);
			ioOut << YAML::Key << "code" << YAML::Value << YAML::Literal << custom.code;
			ioOut << YAML::EndMap;
		}
		ioOut << YAML::EndSeq;
	}
	if (!iGraph.functions.empty()) {
		ioOut << YAML::Key << "functions" << YAML::Value << YAML::BeginSeq;
		for (const auto& function: iGraph.functions) {
			ioOut << YAML::BeginMap;
			ioOut << YAML::Key << "name" << YAML::Value << function.name;
			emitPins(ioOut, "inputs", function.inputs);
			emitPins(ioOut, "outputs", function.outputs);
			emitNodes(ioOut, function.nodes, function.links);
			ioOut << YAML::EndMap;
		}
		ioOut << YAML::EndSeq;
	}
	emitNodes(ioOut, iGraph.nodes, iGraph.links);
	ioOut << YAML::EndMap;
}

auto parseMaterialGraph(const YAML::Node& iNode) -> MaterialGraph {
	MaterialGraph graph;
	if (const auto parameters = iNode["parameters"]; parameters && parameters.IsSequence()) {
		for (const auto& entry: parameters)
			graph.parameters.push_back(
					{.name = entry["name"].as<std::string>(),
					 .type = magic_enum::enum_cast<MaterialParameterType>(entry["type"].as<std::string>())
									 .value_or(MaterialParameterType::Float),
					 .defaultValue = entry["default"] ? entry["default"].as<std::string>() : std::string{}});
	}
	if (const auto customs = iNode["customNodes"]; customs && customs.IsSequence()) {
		for (const auto& entry: customs) {
			auto code = entry["code"] ? entry["code"].as<std::string>() : std::string{};
			while (code.ends_with('\n')) code.pop_back();
			graph.customNodes.push_back({.name = entry["name"].as<std::string>(),
										 .title = entry["title"] ? entry["title"].as<std::string>() : std::string{},
										 .inputs = parsePins(entry["inputs"]),
										 .outputs = parsePins(entry["outputs"]),
										 .code = std::move(code)});
		}
	}
	if (const auto functions = iNode["functions"]; functions && functions.IsSequence()) {
		for (const auto& entry: functions) {
			MaterialFunction function{.name = entry["name"].as<std::string>(),
									  .inputs = parsePins(entry["inputs"]),
									  .outputs = parsePins(entry["outputs"]),
									  .nodes = {},
									  .links = {}};
			parseNodes(entry, function.nodes, function.links);
			graph.functions.push_back(std::move(function));
		}
	}
	parseNodes(iNode, graph.nodes, graph.links);
	return graph;
}

}// namespace owl::renderer
