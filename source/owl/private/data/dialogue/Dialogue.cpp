/**
 * @file Dialogue.cpp
 * @author Silmaen
 * @date 30/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "data/dialogue/Dialogue.h"

#include "core/FormatVersionYaml.h"
#include "core/external/yaml.h"
#include "data/assets/AssetReference.h"
#include "data/assets/AssetSearchPaths.h"
#include "platform/AtomicFile.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <span>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>

namespace owl::data::dialogue {

namespace {

constexpr std::array<core::MigrationStep, 0> g_dialogueMigrations{};
constexpr core::DocumentFormat g_dialogueFormat{.name = "Dialogue", .migrations = g_dialogueMigrations};

void emitText(YAML::Emitter& ioOut, const std::string_view iName, const DialogueText& iText) {
	if (iText.text.empty() && iText.key.empty())
		return;
	ioOut << YAML::Key << std::string{iName} << YAML::Value << YAML::Flow << YAML::BeginMap;
	if (!iText.text.empty())
		ioOut << YAML::Key << "text" << YAML::Value << iText.text;
	if (!iText.key.empty())
		ioOut << YAML::Key << "key" << YAML::Value << iText.key;
	ioOut << YAML::EndMap;
}

auto readText(const YAML::Node& iNode) -> DialogueText {
	if (!iNode || !iNode.IsMap())
		return {};
	return {.text = iNode["text"].as<std::string>(""), .key = iNode["key"].as<std::string>("")};
}

auto readId(const YAML::Node& iNode) -> core::UUID { return core::UUID{iNode.as<uint64_t>(0)}; }

// The nodes a node leads to, null ids left out.
auto successorsOf(const DialogueNode& iNode) -> std::vector<core::UUID> {
	std::vector<core::UUID> next;
	for (size_t output = 0; output < Dialogue::getOutputCount(iNode); ++output)
		if (const auto target = Dialogue::getOutput(iNode, output); target != core::UUID{0})
			next.push_back(target);
	return next;
}

auto hasSpeech(const NodeKind iKind) -> bool { return iKind == NodeKind::Line || iKind == NodeKind::Choice; }

}// namespace

auto getNodeKindName(const NodeKind iKind) -> std::string_view { return magic_enum::enum_name(iKind); }

auto parseNodeKind(const std::string_view iName) -> std::optional<NodeKind> {
	return magic_enum::enum_cast<NodeKind>(iName);
}

auto getNodeKinds() -> std::span<const NodeKind> {
	static constexpr auto s_kinds = magic_enum::enum_values<NodeKind>();
	return s_kinds;
}

auto Dialogue::isDialogue(const std::filesystem::path& iPath) -> bool { return iPath.extension() == fileExtension(); }

auto Dialogue::format() -> const core::DocumentFormat& { return g_dialogueFormat; }

auto Dialogue::makeDefault() -> Dialogue {
	Dialogue dialogue;
	DialogueNode start;
	start.id = core::UUID{};
	start.kind = NodeKind::Start;
	DialogueNode end;
	end.id = core::UUID{};
	end.kind = NodeKind::End;
	end.position = {320.f, 0.f};
	start.next = end.id;
	dialogue.nodes.push_back(std::move(start));
	dialogue.nodes.push_back(std::move(end));
	return dialogue;
}

auto Dialogue::serializeToString() const -> std::string {
	YAML::Emitter out;
	out << YAML::BeginMap;
	out << YAML::Key << "Dialogue" << YAML::Value << "";
	core::emitFormatVersion(out, g_dialogueFormat);
	out << YAML::Key << "nodes" << YAML::Value << YAML::BeginSeq;
	for (const auto& node: nodes) {
		out << YAML::BeginMap;
		out << YAML::Key << "id" << YAML::Value << static_cast<uint64_t>(node.id);
		out << YAML::Key << "kind" << YAML::Value << std::string{getNodeKindName(node.kind)};
		out << YAML::Key << "position" << YAML::Value << YAML::Flow << YAML::BeginSeq << node.position.x()
			<< node.position.y() << YAML::EndSeq;
		if (hasSpeech(node.kind)) {
			emitText(out, "speaker", node.speaker);
			emitText(out, "text", node.text);
			if (!node.sound.empty())
				out << YAML::Key << "sound" << YAML::Value << data::assets::toAssetReference(node.sound);
		}
		if (!node.function.empty())
			out << YAML::Key << "function" << YAML::Value << node.function;
		if (node.kind == NodeKind::Choice) {
			out << YAML::Key << "options" << YAML::Value << YAML::BeginSeq;
			for (const auto& option: node.options) {
				out << YAML::BeginMap;
				emitText(out, "text", option.text);
				if (!option.condition.empty())
					out << YAML::Key << "condition" << YAML::Value << option.condition;
				out << YAML::Key << "next" << YAML::Value << static_cast<uint64_t>(option.next);
				out << YAML::EndMap;
			}
			out << YAML::EndSeq;
		}
		if (node.next != core::UUID{0})
			out << YAML::Key << "next" << YAML::Value << static_cast<uint64_t>(node.next);
		if (node.nextFalse != core::UUID{0})
			out << YAML::Key << "nextFalse" << YAML::Value << static_cast<uint64_t>(node.nextFalse);
		out << YAML::EndMap;
	}
	out << YAML::EndSeq;
	if (!routes.empty()) {
		out << YAML::Key << "routes" << YAML::Value << YAML::BeginSeq;
		for (const auto& route: routes) {
			out << YAML::Flow << YAML::BeginMap;
			out << YAML::Key << "node" << YAML::Value << static_cast<uint64_t>(route.node);
			out << YAML::Key << "output" << YAML::Value << route.output;
			out << YAML::Key << "points" << YAML::Value << YAML::BeginSeq;
			for (const auto& point: route.points)
				out << YAML::Flow << YAML::BeginSeq << point.x() << point.y() << YAML::EndSeq;
			out << YAML::EndSeq;
			if (!route.color.empty())
				out << YAML::Key << "color" << YAML::Value << route.color;
			out << YAML::EndMap;
		}
		out << YAML::EndSeq;
	}
	out << YAML::EndMap;
	return std::string{out.c_str()};
}

auto Dialogue::deserializeFromString(const std::string_view iYaml) -> std::optional<Dialogue> {
	YAML::Node root;
	try {
		root = YAML::Load(std::string{iYaml});
	} catch (const YAML::Exception& e) {
		OWL_CORE_ERROR("Dialogue: Failed to parse YAML: {}.", e.what())
		return std::nullopt;
	}
	if (!root || !root.IsMap() || !root["Dialogue"]) {
		OWL_CORE_WARN("Dialogue: Document is not a dialogue.")
		return std::nullopt;
	}
	if (!core::upgradeYamlDocument(g_dialogueFormat, root, "<buffer>")) {
		OWL_CORE_ERROR("Dialogue: Document cannot be read.")
		return std::nullopt;
	}
	try {
		Dialogue dialogue;
		for (const auto& item: root["nodes"]) {
			DialogueNode node;
			node.id = readId(item["id"]);
			if (node.id == core::UUID{0} || dialogue.findNode(node.id) != nullptr) {
				OWL_CORE_WARN("Dialogue: Node {} skipped (no id or twice).", static_cast<uint64_t>(node.id))
				continue;
			}
			if (const auto kind = parseNodeKind(item["kind"].as<std::string>("Line")); kind.has_value())
				node.kind = *kind;
			if (const auto& position = item["position"]; position && position.IsSequence() && position.size() == 2)
				node.position = {position[0].as<float>(0.f), position[1].as<float>(0.f)};
			node.speaker = readText(item["speaker"]);
			node.text = readText(item["text"]);
			node.sound = data::assets::toAssetPath(item["sound"].as<std::string>(""));
			node.function = item["function"].as<std::string>("");
			for (const auto& option: item["options"])
				node.options.push_back({.text = readText(option["text"]),
										.condition = option["condition"].as<std::string>(""),
										.next = readId(option["next"])});
			node.next = readId(item["next"]);
			node.nextFalse = readId(item["nextFalse"]);
			dialogue.nodes.push_back(std::move(node));
		}
		for (const auto& item: root["routes"]) {
			DialogueRoute route{.node = readId(item["node"]), .output = item["output"].as<uint32_t>(0), .points = {}};
			const auto* node = dialogue.findNode(route.node);
			if (node == nullptr || route.output >= getOutputCount(*node) ||
				dialogue.findRoute(route.node, route.output) != nullptr)
				continue;
			for (const auto& point: item["points"])
				if (point.IsSequence() && point.size() == 2)
					route.points.emplace_back(point[0].as<float>(0.f), point[1].as<float>(0.f));
			route.color = item["color"].as<std::string>("");
			dialogue.routes.push_back(std::move(route));
		}
		return dialogue;
	} catch (const YAML::Exception& e) {
		OWL_CORE_ERROR("Dialogue: Invalid value: {}.", e.what())
		return std::nullopt;
	}
}

auto Dialogue::saveToFile(const std::filesystem::path& iPath) const -> bool {
	if (const auto written = platform::writeFileAtomic(iPath, serializeToString()); !written) {
		OWL_CORE_ERROR("Dialogue: Failed to write '{}': {}.", iPath.string(), describe(written.error()))
		return false;
	}
	return true;
}

auto Dialogue::loadFile(const std::filesystem::path& iPath) -> std::optional<Dialogue> {
	const std::ifstream file(iPath);
	if (!file) {
		OWL_CORE_WARN("Dialogue: Cannot open '{}'.", iPath.string())
		return std::nullopt;
	}
	std::stringstream text;
	text << file.rdbuf();
	return deserializeFromString(text.str());
}

auto Dialogue::load(const std::filesystem::path& iAsset) -> std::optional<Dialogue> {
	const auto bytes = data::assets::readAssetBytes(iAsset);
	if (!bytes) {
		OWL_CORE_WARN("Dialogue: '{}' not found in the pack nor the asset folders.", iAsset.string())
		return std::nullopt;
	}
	return deserializeFromString({reinterpret_cast<const char*>(bytes->data()), bytes->size()});
}

auto Dialogue::findNode(const core::UUID iId) const -> const DialogueNode* {
	const auto found = std::ranges::find(nodes, iId, &DialogueNode::id);
	return found == nodes.end() ? nullptr : &*found;
}

auto Dialogue::findNode(const core::UUID iId) -> DialogueNode* {
	const auto found = std::ranges::find(nodes, iId, &DialogueNode::id);
	return found == nodes.end() ? nullptr : &*found;
}

auto Dialogue::getOutputCount(const DialogueNode& iNode) -> size_t {
	switch (iNode.kind) {
		case NodeKind::Start:
		case NodeKind::Line:
		case NodeKind::Event:
			return 1;
		case NodeKind::Branch:
			return 2;
		case NodeKind::Choice:
			return iNode.options.size();
		case NodeKind::End:
			return 0;
	}
	return 0;
}

auto Dialogue::getOutput(const DialogueNode& iNode, const size_t iOutput) -> core::UUID {
	if (iOutput >= getOutputCount(iNode))
		return core::UUID{0};
	if (iNode.kind == NodeKind::Choice)
		return iNode.options[iOutput].next;
	return iOutput == 0 ? iNode.next : iNode.nextFalse;
}

auto Dialogue::setOutput(DialogueNode& ioNode, const size_t iOutput, const core::UUID iTarget) -> bool {
	if (iOutput >= getOutputCount(ioNode))
		return false;
	if (ioNode.kind == NodeKind::Choice)
		ioNode.options[iOutput].next = iTarget;
	else if (iOutput == 0)
		ioNode.next = iTarget;
	else
		ioNode.nextFalse = iTarget;
	return true;
}

auto Dialogue::findRoute(const core::UUID iNode, const size_t iOutput) const -> const DialogueRoute* {
	const auto found = std::ranges::find_if(routes, [iNode, iOutput](const DialogueRoute& iRoute) -> bool {
		return iRoute.node == iNode && iRoute.output == iOutput;
	});
	return found == routes.end() ? nullptr : &*found;
}

auto Dialogue::findStart() const -> const DialogueNode* {
	const auto found = std::ranges::find(nodes, NodeKind::Start, &DialogueNode::kind);
	return found == nodes.end() ? nullptr : &*found;
}

auto Dialogue::getProblems() const -> std::vector<DialogueProblem> {
	std::vector<DialogueProblem> problems;
	const auto starts = std::ranges::count(nodes, NodeKind::Start, &DialogueNode::kind);
	if (starts == 0)
		problems.push_back({.node = 0, .message = "The dialogue has no Start node."});
	else if (starts > 1)
		problems.push_back({.node = 0, .message = "The dialogue has several Start nodes: the first one is used."});
	for (const auto& node: nodes) {
		for (const auto next: successorsOf(node))
			if (findNode(next) == nullptr)
				problems.push_back({.node = node.id, .message = "A link leads to a node that does not exist."});
		if (node.kind == NodeKind::Choice && node.options.empty())
			problems.push_back({.node = node.id, .message = "The choice has no option."});
		if ((node.kind == NodeKind::Branch || node.kind == NodeKind::Event) && node.function.empty())
			problems.push_back({.node = node.id,
								.message = node.kind == NodeKind::Branch ? "The branch names no condition function."
																		 : "The event names no function."});
	}
	if (const auto* start = findStart(); start != nullptr) {
		std::unordered_set<uint64_t> reached{start->id};
		std::vector<core::UUID> pending{start->id};
		while (!pending.empty()) {
			const auto* node = findNode(pending.back());
			pending.pop_back();
			if (node == nullptr)
				continue;
			for (const auto next: successorsOf(*node))
				if (reached.insert(next).second)
					pending.push_back(next);
		}
		for (const auto& node: nodes)
			if (node.kind != NodeKind::Start && !reached.contains(node.id))
				problems.push_back({.node = node.id, .message = "The node cannot be reached from the Start node."});
	}
	return problems;
}

auto Dialogue::getTextKeys() const -> std::vector<std::string> {
	std::vector<std::string> keys;
	const auto add = [&keys](const DialogueText& iText) -> void {
		if (!iText.key.empty() && std::ranges::find(keys, iText.key) == keys.end())
			keys.push_back(iText.key);
	};
	for (const auto& node: nodes) {
		add(node.speaker);
		add(node.text);
		for (const auto& option: node.options) add(option.text);
	}
	return keys;
}

auto Dialogue::getFunctions() const -> std::vector<std::string> {
	std::vector<std::string> functions;
	const auto add = [&functions](const std::string& iName) -> void {
		if (!iName.empty() && std::ranges::find(functions, iName) == functions.end())
			functions.push_back(iName);
	};
	for (const auto& node: nodes) {
		add(node.function);
		for (const auto& option: node.options) add(option.condition);
	}
	return functions;
}

}// namespace owl::data::dialogue
