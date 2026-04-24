/**
 * @file NodeCanvasSerializer.cpp
 * @author Silmaen
 * @date 24/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "gui/widgets/NodeCanvasSerializer.h"

#include "core/external/yaml.h"

#include <fstream>
#include <sstream>
#include <unordered_map>

namespace owl::gui::widgets {

namespace {

constexpr int g_currentVersion = 1;

void emitPin(YAML::Emitter& ioOut, const NodePin& iPin) {
	ioOut << YAML::Flow << YAML::BeginMap;
	ioOut << YAML::Key << "id" << YAML::Value << static_cast<uint64_t>(iPin.id);
	ioOut << YAML::Key << "label" << YAML::Value << iPin.label;
	ioOut << YAML::Key << "typeTag" << YAML::Value << iPin.typeTag;
	ioOut << YAML::EndMap;
}

void emitNode(YAML::Emitter& ioOut, const Node& iNode) {
	ioOut << YAML::BeginMap;
	ioOut << YAML::Key << "id" << YAML::Value << static_cast<uint64_t>(iNode.id);
	ioOut << YAML::Key << "title" << YAML::Value << iNode.title;
	ioOut << YAML::Key << "position" << YAML::Value << YAML::Flow << YAML::BeginSeq << iNode.position.x()
		  << iNode.position.y() << YAML::EndSeq;
	ioOut << YAML::Key << "titleColor" << YAML::Value << YAML::Flow << YAML::BeginSeq << iNode.titleColor.x()
		  << iNode.titleColor.y() << iNode.titleColor.z() << iNode.titleColor.w() << YAML::EndSeq;
	ioOut << YAML::Key << "inputs" << YAML::Value << YAML::BeginSeq;
	for (const auto& pin: iNode.inputs)
		emitPin(ioOut, pin);
	ioOut << YAML::EndSeq;
	ioOut << YAML::Key << "outputs" << YAML::Value << YAML::BeginSeq;
	for (const auto& pin: iNode.outputs)
		emitPin(ioOut, pin);
	ioOut << YAML::EndSeq;
	if (!iNode.customData.empty())
		ioOut << YAML::Key << "customData" << YAML::Value << YAML::Literal << iNode.customData;
	ioOut << YAML::EndMap;
}

void emitLink(YAML::Emitter& ioOut, const Link& iLink) {
	ioOut << YAML::Flow << YAML::BeginMap;
	ioOut << YAML::Key << "id" << YAML::Value << static_cast<uint64_t>(iLink.id);
	ioOut << YAML::Key << "from" << YAML::Value << static_cast<uint64_t>(iLink.fromPin);
	ioOut << YAML::Key << "to" << YAML::Value << static_cast<uint64_t>(iLink.toPin);
	ioOut << YAML::EndMap;
}

auto parsePin(const YAML::Node& iPin, PinKind iKind) -> std::optional<NodePin> {
	if (!iPin["id"])
		return std::nullopt;
	NodePin pin;
	pin.id = core::UUID{iPin["id"].as<uint64_t>()};
	pin.label = iPin["label"] ? iPin["label"].as<std::string>() : std::string{};
	pin.typeTag = iPin["typeTag"] ? iPin["typeTag"].as<std::string>() : std::string{};
	pin.kind = iKind;
	return pin;
}

auto parseNode(const YAML::Node& iNode) -> std::optional<Node> {
	if (!iNode["id"])
		return std::nullopt;
	Node node;
	node.id = core::UUID{iNode["id"].as<uint64_t>()};
	node.title = iNode["title"] ? iNode["title"].as<std::string>() : std::string{};
	if (const auto pos = iNode["position"]; pos && pos.IsSequence() && pos.size() >= 2)
		node.position = {pos[0].as<float>(), pos[1].as<float>()};
	if (const auto col = iNode["titleColor"]; col && col.IsSequence() && col.size() >= 4)
		node.titleColor = {col[0].as<float>(), col[1].as<float>(), col[2].as<float>(), col[3].as<float>()};
	if (const auto ins = iNode["inputs"]; ins && ins.IsSequence()) {
		for (const auto& raw: ins)
			if (auto pin = parsePin(raw, PinKind::Input))
				node.inputs.push_back(std::move(*pin));
	}
	if (const auto outs = iNode["outputs"]; outs && outs.IsSequence()) {
		for (const auto& raw: outs)
			if (auto pin = parsePin(raw, PinKind::Output))
				node.outputs.push_back(std::move(*pin));
	}
	if (const auto cd = iNode["customData"]; cd)
		node.customData = cd.as<std::string>();
	return node;
}

void emitCanvas(YAML::Emitter& ioOut, const NodeCanvas& iCanvas, std::string_view iName) {
	ioOut << YAML::BeginMap;
	ioOut << YAML::Key << "NodeGraph" << YAML::Value << std::string{iName};
	ioOut << YAML::Key << "Version" << YAML::Value << g_currentVersion;
	ioOut << YAML::Key << "Nodes" << YAML::Value << YAML::BeginSeq;
	for (const auto& node: iCanvas.nodes())
		emitNode(ioOut, node);
	ioOut << YAML::EndSeq;
	ioOut << YAML::Key << "Links" << YAML::Value << YAML::BeginSeq;
	for (const auto& link: iCanvas.links())
		emitLink(ioOut, link);
	ioOut << YAML::EndSeq;
	ioOut << YAML::EndMap;
}

}// namespace

auto NodeCanvasSerializer::serializeToString(const NodeCanvas& iCanvas, std::string_view iName) -> std::string {
	YAML::Emitter out;
	emitCanvas(out, iCanvas, iName);
	return std::string{out.c_str()};
}

auto NodeCanvasSerializer::deserializeFromString(NodeCanvas& ioCanvas, std::string_view iYaml) -> bool {
	YAML::Node root;
	try {
		root = YAML::Load(std::string{iYaml});
	} catch (const YAML::Exception& ex) {
		OWL_CORE_ERROR("NodeCanvasSerializer: YAML parse error: {}", ex.what())
		return false;
	}
	if (!root || !root.IsMap())
		return false;
	if (const auto version = root["Version"]; version && version.as<int>() > g_currentVersion) {
		OWL_CORE_WARN("NodeCanvasSerializer: document version {} is newer than supported {}", version.as<int>(),
					  g_currentVersion)
	}

	ioCanvas.clear();
	if (const auto nodes = root["Nodes"]; nodes && nodes.IsSequence()) {
		for (const auto& raw: nodes) {
			if (auto node = parseNode(raw))
				ioCanvas.addNode(std::move(*node));
		}
	}
	if (const auto links = root["Links"]; links && links.IsSequence()) {
		for (const auto& raw: links) {
			if (!raw["from"] || !raw["to"])
				continue;
			const core::UUID fromPin{raw["from"].as<uint64_t>()};
			const core::UUID toPin{raw["to"].as<uint64_t>()};
			ioCanvas.addLink(fromPin, toPin);
		}
	}
	return true;
}

auto NodeCanvasSerializer::serializeToFile(const NodeCanvas& iCanvas, const std::filesystem::path& iPath,
										   std::string_view iName) -> bool {
	std::ofstream out(iPath);
	if (!out)
		return false;
	out << serializeToString(iCanvas, iName);
	return out.good();
}

auto NodeCanvasSerializer::deserializeFromFile(NodeCanvas& ioCanvas, const std::filesystem::path& iPath) -> bool {
	const std::ifstream in(iPath);
	if (!in)
		return false;
	std::stringstream ss;
	ss << in.rdbuf();
	return deserializeFromString(ioCanvas, ss.str());
}

auto NodeCanvasSerializer::serializeSubset(const NodeCanvas& iCanvas, std::span<const core::UUID> iNodeIds)
		-> std::string {
	if (iNodeIds.empty())
		return {};

	// Gather the subset into a temporary canvas to reuse the full emitter. The copy is cheap — we
	// only store the nodes/links we need — and it keeps the YAML format identical to a full save.
	NodeCanvas subset;
	std::unordered_map<uint64_t, bool> idSet;
	for (const auto id: iNodeIds)
		idSet.emplace(static_cast<uint64_t>(id), true);
	std::unordered_map<uint64_t, bool> pinsInSubset;

	for (const auto& node: iCanvas.nodes()) {
		if (!idSet.contains(static_cast<uint64_t>(node.id)))
			continue;
		subset.addNode(node);
		for (const auto& pin: node.inputs)
			pinsInSubset.emplace(static_cast<uint64_t>(pin.id), true);
		for (const auto& pin: node.outputs)
			pinsInSubset.emplace(static_cast<uint64_t>(pin.id), true);
	}
	for (const auto& link: iCanvas.links()) {
		if (pinsInSubset.contains(static_cast<uint64_t>(link.fromPin)) &&
			pinsInSubset.contains(static_cast<uint64_t>(link.toPin))) {
			subset.addLink(link.fromPin, link.toPin);
		}
	}
	return serializeToString(subset);
}

auto NodeCanvasSerializer::pasteSubset(NodeCanvas& ioCanvas, std::string_view iYaml) -> std::vector<core::UUID> {
	YAML::Node root;
	try {
		root = YAML::Load(std::string{iYaml});
	} catch (const YAML::Exception&) {
		return {};
	}
	if (!root || !root.IsMap())
		return {};

	// Two-pass: first assign fresh UUIDs to every node and every pin (recording the old→new map),
	// then push nodes and rewrite link endpoints to reference the new pin UUIDs.
	std::unordered_map<uint64_t, core::UUID> pinIdMap;
	std::vector<core::UUID> createdNodeIds;

	if (const auto nodes = root["Nodes"]; nodes && nodes.IsSequence()) {
		for (const auto& raw: nodes) {
			auto parsed = parseNode(raw);
			if (!parsed)
				continue;
			// Fresh UUID for the node.
			parsed->id = core::UUID{};
			// Fresh UUIDs for every pin, keep the mapping for link rewriting.
			for (auto& pin: parsed->inputs) {
				const auto oldId = static_cast<uint64_t>(pin.id);
				pin.id = core::UUID{};
				pinIdMap.emplace(oldId, pin.id);
			}
			for (auto& pin: parsed->outputs) {
				const auto oldId = static_cast<uint64_t>(pin.id);
				pin.id = core::UUID{};
				pinIdMap.emplace(oldId, pin.id);
			}
			createdNodeIds.push_back(ioCanvas.addNode(std::move(*parsed)));
		}
	}

	if (const auto links = root["Links"]; links && links.IsSequence()) {
		for (const auto& raw: links) {
			if (!raw["from"] || !raw["to"])
				continue;
			const auto oldFrom = raw["from"].as<uint64_t>();
			const auto oldTo = raw["to"].as<uint64_t>();
			const auto fromIt = pinIdMap.find(oldFrom);
			const auto toIt = pinIdMap.find(oldTo);
			if (fromIt == pinIdMap.end() || toIt == pinIdMap.end())
				continue;
			ioCanvas.addLink(fromIt->second, toIt->second);
		}
	}
	return createdNodeIds;
}

}// namespace owl::gui::widgets
