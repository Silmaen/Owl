/**
 * @file BehaviourTree.cpp
 * @author Silmaen
 * @date 10/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "scene/ai/BehaviourTree.h"

#include "core/FormatVersionYaml.h"
#include "core/external/yaml.h"
#include "data/assets/AssetSearchPaths.h"
#include "platform/AtomicFile.h"
#include "platform/FileUtils.h"

#include <magic_enum/magic_enum.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <exception>
#include <format>
#include <limits>
#include <set>
#include <string>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace owl::scene::ai {

namespace {

constexpr std::array<core::MigrationStep, 0> g_treeMigrations{};
constexpr core::DocumentFormat g_treeFormat{.name = "BehaviourTree", .migrations = g_treeMigrations};

auto param(std::string iName, const ParamType iType, std::string iDefault, std::string iDescription,
		   const float iMin = 0.f, const float iMax = 1.f) -> ParamSpec {
	return {.name = std::move(iName),
			.type = iType,
			.defaultValue = std::move(iDefault),
			.min = iMin,
			.max = iMax,
			.choices = {},
			.description = std::move(iDescription)};
}

auto choice(std::string iName, std::vector<std::string> iChoices, std::string iDescription) -> ParamSpec {
	auto defaultValue = iChoices.front();
	return {.name = std::move(iName),
			.type = ParamType::Choice,
			.defaultValue = std::move(defaultValue),
			.min = 0.f,
			.max = 0.f,
			.choices = std::move(iChoices),
			.description = std::move(iDescription)};
}

auto keyParam(std::string iDescription) -> ParamSpec {
	return param("key", ParamType::Key, "", std::move(iDescription));
}

auto buildNodeTypes() -> std::vector<NodeType> {
	using enum ParamType;
	std::vector<NodeType> types;
	const auto add = [&types](std::string iId, std::string iTitle, const NodeKind iKind, std::string iDescription,
							  std::vector<ParamSpec> iParams = {}) -> void {
		types.push_back({.id = std::move(iId),
						 .title = std::move(iTitle),
						 .kind = iKind,
						 .description = std::move(iDescription),
						 .params = std::move(iParams)});
	};
	add("root", "Root", NodeKind::Root, "The entry of the tree: runs its child again each time it ends.");
	add("composite.sequence", "Sequence", NodeKind::Composite,
		"Runs its children from top to bottom while they succeed: fails at the first that fails.");
	add("composite.selector", "Selector", NodeKind::Composite,
		"Runs its children from top to bottom until one succeeds: fails when all fail.",
		{param("reactive", Bool, "false",
			   "Try the children above again every tick: one that succeeds or runs stops the running one "
			   "(an enemy that sees the player drops its patrol).")});
	add("composite.parallel", "Parallel", NodeKind::Composite, "Runs every child at each tick.",
		{choice("policy", {"all", "one"},
				"all: succeeds when every child succeeded, fails at the first failure. one: succeeds at the first "
				"success, fails when every child failed.")});
	add("composite.random", "Random", NodeKind::Composite,
		"Runs its children in a random order until one succeeds (a new order each time it starts).");
	add("decorator.inverter", "Inverter", NodeKind::Decorator, "Success becomes failure and failure success.");
	add("decorator.succeeder", "Succeeder", NodeKind::Decorator, "Succeeds whatever its child returns.");
	add("decorator.repeat", "Repeat", NodeKind::Decorator,
		"Runs its child again each time it succeeds, one run per tick; fails when it fails.",
		{param("count", Int, "0", "Runs before it succeeds; 0: forever.", 0.f, 10000.f)});
	add("decorator.retry", "Retry", NodeKind::Decorator,
		"Runs its child again each time it fails, one run per tick; succeeds when it succeeds.",
		{param("count", Int, "3", "Runs before it fails; 0: until it succeeds.", 0.f, 10000.f)});
	add("decorator.cooldown", "Cooldown", NodeKind::Decorator,
		"Fails until a time has passed since its child last ended.",
		{param("seconds", Float, "2", "Time after the end of the child during which it fails.", 0.f, 3600.f)});
	add("decorator.timeout", "Timeout", NodeKind::Decorator, "Stops its child and fails when it runs for too long.",
		{param("seconds", Float, "5", "Longest run of the child.", 0.f, 3600.f)});
	add("decorator.guard", "Guard", NodeKind::Decorator,
		"Runs its child while a value of the blackboard holds, checked at every tick: fails and stops the child "
		"when it no longer holds.",
		{keyParam("Key of the blackboard compared."),
		 choice("op", {"==", "!=", "<", "<=", ">", ">=", "is set", "is not set"}, "Comparison."),
		 param("value", Text, "", "Value compared with, written like the key (true, 2.5, a text, x, y, z).")});
	add("condition.compare", "Compare", NodeKind::Condition, "Succeeds when a value of the blackboard holds.",
		{keyParam("Key of the blackboard compared."),
		 choice("op", {"==", "!=", "<", "<=", ">", ">=", "is set", "is not set"},
				"Comparison; is set: true, not 0, not empty, an entity."),
		 param("value", Text, "", "Value compared with, written like the key (true, 2.5, a text, x, y, z).")});
	add("condition.distance", "Distance", NodeKind::Condition,
		"Succeeds when the entity held by a key (or the point) is closer, or farther, than a distance.",
		{keyParam("Key holding an entity or a point."), choice("op", {"closer than", "farther than"}, "Comparison."),
		 param("distance", Float, "5", "Distance, in world units.", 0.f, 100000.f),
		 choice("space", {"3D", "ground", "plane"},
				"Where it is measured: 3D, on the ground (x, z: the height of a player's eye does not count) or on "
				"the plane (x, y) of a 2D scene.")});
	add("condition.chance", "Chance", NodeKind::Condition, "Succeeds at random, with a probability.",
		{param("probability", Float, "0.5", "Probability of success, from 0 to 1.", 0.f, 1.f)});
	add("condition.lua", "Lua Condition", NodeKind::Condition,
		"Calls a Lua function of the script of the entity (else of the scene script) with the entity and the "
		"tick duration: succeeds when it returns a true value.",
		{param("function", Text, "", "Name of the Lua function.")});
	add("action.wait", "Wait", NodeKind::Action, "Runs for a time, then succeeds.",
		{param("seconds", Float, "1", "Time to wait.", 0.f, 3600.f),
		 param("spread", Float, "0", "Random time added, up to this many seconds.", 0.f, 3600.f)});
	add("action.set", "Set Value", NodeKind::Action, "Writes a value on the blackboard, then succeeds.",
		{keyParam("Key of the blackboard written."),
		 param("value", Text, "", "Value written, like the key (true, 2.5, a text, x, y, z).")});
	add("action.move_to", "Move To", NodeKind::Action,
		"Walks to the entity or the point held by a key, with the Nav Agent of the entity (else its Steering Agent); "
		"runs until there, fails when there is no path.",
		{keyParam("Key holding an entity or a point."),
		 param("distance", Float, "0.5", "Distance at which it is there.", 0.f, 1000.f)});
	add("action.steer", "Steer", NodeKind::Action,
		"Sets a behaviour of the Steering Agent of the entity, on the entity or the point held by a key, then "
		"succeeds.",
		{choice("behaviour", {"seek", "flee", "arrive", "pursue", "evade", "wander", "stop"},
				"Behaviour set; stop sets the six of them to 0."),
		 param("key", Key, "", "Key holding the target entity or point; empty keeps the target."),
		 param("weight", Float, "1", "Weight of the behaviour.", 0.f, 100.f),
		 param("exclusive", Bool, "true", "Set the five other target behaviours to 0.")});
	add("action.find_player", "Find Player", NodeKind::Action,
		"Writes the player entity on the blackboard; fails when the scene has no player.",
		{keyParam("Key written (an entity).")});
	add("action.lua", "Lua Action", NodeKind::Action,
		"Calls a Lua function of the script of the entity (else of the scene script) with the entity and the tick "
		"duration at every tick: true or nothing succeeds, false fails, \"running\" runs on.",
		{param("function", Text, "", "Name of the Lua function.")});
	add("action.log", "Log", NodeKind::Action, "Writes a message to the log, then succeeds.",
		{param("message", Text, "", "The message.")});
	return types;
}

void emitValue(YAML::Emitter& ioOut, const BlackboardValue& iValue) {
	std::visit(
			[&ioOut]<typename T>(const T& iHeld) -> void {
				if constexpr (std::is_same_v<T, core::UUID>)
					ioOut << static_cast<uint64_t>(iHeld);
				else if constexpr (std::is_same_v<T, math::vec3>)
					ioOut << YAML::Flow << YAML::BeginSeq << iHeld.x() << iHeld.y() << iHeld.z() << YAML::EndSeq;
				else
					ioOut << iHeld;
			},
			iValue);
}

auto readValue(const BlackboardType iType, const YAML::Node& iNode) -> BlackboardValue {
	switch (iType) {
		case BlackboardType::Bool:
			return iNode.as<bool>(false);
		case BlackboardType::Number:
			return iNode.as<double>(0.0);
		case BlackboardType::String:
			return iNode.as<std::string>("");
		case BlackboardType::Entity:
			return core::UUID{iNode.as<uint64_t>(0)};
		case BlackboardType::Vector:
			if (iNode.IsSequence() && iNode.size() == 3)
				return math::vec3{iNode[0].as<float>(), iNode[1].as<float>(), iNode[2].as<float>()};
			return math::vec3{0.f, 0.f, 0.f};
	}
	return 0.0;
}

auto parseTree(const YAML::Node& iRoot) -> BehaviourTree {
	BehaviourTree tree;
	for (const auto& entry: iRoot["blackboard"]) {
		const auto type = magic_enum::enum_cast<BlackboardType>(entry["type"].as<std::string>("Number"))
								  .value_or(BlackboardType::Number);
		tree.blackboard.push_back({.name = entry["name"].as<std::string>(),
								   .value = readValue(type, entry["value"]),
								   .description = entry["description"].as<std::string>("")});
	}
	for (const auto& entry: iRoot["nodes"]) {
		TreeNode node{.id = core::UUID{entry["id"].as<uint64_t>()},
					  .type = entry["type"].as<std::string>(),
					  .position = {0.f, 0.f},
					  .values = {},
					  .comment = entry["comment"].as<std::string>("")};
		if (const auto pos = entry["position"]; pos && pos.IsSequence() && pos.size() >= 2)
			node.position = {pos[0].as<float>(), pos[1].as<float>()};
		if (const auto values = entry["values"]; values && values.IsMap())
			for (const auto& item: values) node.values[item.first.as<std::string>()] = item.second.as<std::string>();
		tree.nodes.push_back(std::move(node));
	}
	for (const auto& entry: iRoot["links"])
		tree.links.push_back({.parent = core::UUID{entry["parent"].as<uint64_t>()},
							  .child = core::UUID{entry["child"].as<uint64_t>()}});
	return tree;
}

auto isKeyParam(const ParamSpec& iSpec) -> bool { return iSpec.type == ParamType::Key; }

}// namespace

auto getBlackboardType(const BlackboardValue& iValue) -> BlackboardType {
	return static_cast<BlackboardType>(iValue.index());
}

auto getBlackboardTypeName(const BlackboardType iType) -> std::string_view { return magic_enum::enum_name(iType); }

auto parseBlackboardType(const std::string_view iName) -> std::optional<BlackboardType> {
	return magic_enum::enum_cast<BlackboardType>(iName);
}

auto getDefaultValue(const BlackboardType iType) -> BlackboardValue {
	switch (iType) {
		case BlackboardType::Bool:
			return false;
		case BlackboardType::Number:
			return 0.0;
		case BlackboardType::String:
			return std::string{};
		case BlackboardType::Entity:
			return core::UUID{0};
		case BlackboardType::Vector:
			return math::vec3{0.f, 0.f, 0.f};
	}
	return 0.0;
}

auto toText(const BlackboardValue& iValue) -> std::string {
	return std::visit(
			[]<typename T>(const T& iHeld) -> std::string {
				if constexpr (std::is_same_v<T, bool>)
					return iHeld ? "true" : "false";
				else if constexpr (std::is_same_v<T, double>)
					return std::format("{}", iHeld);
				else if constexpr (std::is_same_v<T, std::string>)
					return iHeld;
				else if constexpr (std::is_same_v<T, core::UUID>)
					return std::format("{}", static_cast<uint64_t>(iHeld));
				else
					return std::format("{}, {}, {}", iHeld.x(), iHeld.y(), iHeld.z());
			},
			iValue);
}

auto parseValue(const BlackboardType iType, const std::string_view iText) -> std::optional<BlackboardValue> {
	const auto trim = [](std::string_view iPart) -> std::string_view {
		while (!iPart.empty() && iPart.front() == ' ') iPart.remove_prefix(1);
		while (!iPart.empty() && iPart.back() == ' ') iPart.remove_suffix(1);
		return iPart;
	};
	const auto number = [&trim](const std::string_view iPart) -> std::optional<double> {
		const auto part = trim(iPart);
		double value = 0.0;
		const auto [end, error] = std::from_chars(part.data(), part.data() + part.size(), value);
		if (error != std::errc{} || end != part.data() + part.size() || part.empty())
			return std::nullopt;
		return value;
	};
	switch (iType) {
		case BlackboardType::Bool:
			if (trim(iText) == "true")
				return true;
			if (trim(iText) == "false")
				return false;
			return std::nullopt;
		case BlackboardType::Number:
			if (const auto value = number(iText); value.has_value())
				return *value;
			return std::nullopt;
		case BlackboardType::String:
			return std::string{iText};
		case BlackboardType::Entity:
			{
				const auto part = trim(iText);
				uint64_t value = 0;
				if (const auto [end, error] = std::from_chars(part.data(), part.data() + part.size(), value);
					error != std::errc{} || end != part.data() + part.size())
					return std::nullopt;
				return core::UUID{value};
			}
		case BlackboardType::Vector:
			{
				std::array<float, 3> axes{};
				std::string_view rest = iText;
				for (size_t axis = 0; axis < axes.size(); ++axis) {
					const auto comma = rest.find(',');
					if ((comma == std::string_view::npos) != (axis == 2))
						return std::nullopt;
					const auto value = number(rest.substr(0, comma));
					if (!value.has_value())
						return std::nullopt;
					axes[axis] = static_cast<float>(*value);
					rest = comma == std::string_view::npos ? std::string_view{} : rest.substr(comma + 1);
				}
				return math::vec3{axes[0], axes[1], axes[2]};
			}
	}
	return std::nullopt;
}

auto BehaviourTree::findNode(const core::UUID iId) const -> const TreeNode* {
	const auto found = std::ranges::find(nodes, iId, &TreeNode::id);
	return found == nodes.end() ? nullptr : &*found;
}

auto BehaviourTree::findNode(const core::UUID iId) -> TreeNode* {
	const auto found = std::ranges::find(nodes, iId, &TreeNode::id);
	return found == nodes.end() ? nullptr : &*found;
}

void BehaviourTree::removeNode(const core::UUID iId) {
	std::erase_if(nodes, [iId](const TreeNode& iNode) -> bool { return iNode.id == iId; });
	std::erase_if(links, [iId](const TreeLink& iLink) -> bool { return iLink.parent == iId || iLink.child == iId; });
}

auto BehaviourTree::getRoot() const -> const TreeNode* {
	const auto found = std::ranges::find(nodes, std::string_view{"root"}, &TreeNode::type);
	return found == nodes.end() ? nullptr : &*found;
}

auto BehaviourTree::getChildren(const core::UUID iId) const -> std::vector<core::UUID> {
	std::vector<std::tuple<float, float, uint64_t>> sorted;
	for (const auto& link: links) {
		if (link.parent != iId)
			continue;
		if (const auto* child = findNode(link.child); child != nullptr)
			sorted.emplace_back(child->position[1], child->position[0], static_cast<uint64_t>(child->id));
	}
	std::ranges::sort(sorted);
	std::vector<core::UUID> children;
	children.reserve(sorted.size());
	for (const auto& [y, x, id]: sorted) children.emplace_back(id);
	return children;
}

auto BehaviourTree::getParent(const core::UUID iId) const -> std::optional<core::UUID> {
	const auto found = std::ranges::find(links, iId, &TreeLink::child);
	return found == links.end() ? std::nullopt : std::optional{found->parent};
}

auto BehaviourTree::findKey(const std::string_view iName) const -> const BlackboardKey* {
	const auto found = std::ranges::find(blackboard, iName, &BlackboardKey::name);
	return found == blackboard.end() ? nullptr : &*found;
}

auto BehaviourTree::getValue(const TreeNode& iNode, const std::string_view iParam) -> std::string {
	if (const auto found = iNode.values.find(std::string{iParam}); found != iNode.values.end())
		return found->second;
	if (const auto* type = findNodeType(iNode.type); type != nullptr) {
		if (const auto spec = std::ranges::find(type->params, iParam, &ParamSpec::name); spec != type->params.end())
			return spec->defaultValue;
	}
	return {};
}

auto BehaviourTree::validate() const -> std::vector<std::string> {
	std::vector<std::string> problems;
	const auto roots = std::ranges::count(nodes, std::string_view{"root"}, &TreeNode::type);
	if (roots == 0)
		problems.emplace_back("The tree has no Root node.");
	else if (roots > 1)
		problems.emplace_back("The tree has several Root nodes: only the first one runs.");
	std::set<core::UUID> reached;
	if (const auto* root = getRoot(); root != nullptr) {
		std::vector<core::UUID> open{root->id};
		while (!open.empty()) {
			const auto current = open.back();
			open.pop_back();
			if (!reached.insert(current).second)
				continue;
			for (const auto child: getChildren(current)) open.push_back(child);
		}
	}
	for (const auto& node: nodes) {
		const auto* type = findNodeType(node.type);
		const std::string title = type != nullptr ? type->title : node.type;
		if (type == nullptr) {
			problems.push_back(std::format("Unknown node type '{}'.", node.type));
			continue;
		}
		const auto children = getChildren(node.id).size();
		if (children > getMaxChildren(type->kind))
			problems.push_back(
					std::format("{} has {} children, at most {}.", title, children, getMaxChildren(type->kind)));
		if ((type->kind == NodeKind::Composite || type->kind == NodeKind::Decorator || type->kind == NodeKind::Root) &&
			children == 0)
			problems.push_back(std::format("{} has no child.", title));
		if (type->kind != NodeKind::Root && !reached.contains(node.id))
			problems.push_back(std::format("{} is not linked to the Root: it never runs.", title));
		for (const auto& spec: type->params) {
			const auto value = getValue(node, spec.name);
			if (isKeyParam(spec) && !value.empty() && findKey(value) == nullptr)
				problems.push_back(std::format("{} reads the key '{}' the blackboard does not declare.", title, value));
			if (spec.name == "function" && value.empty())
				problems.push_back(std::format("{} names no Lua function.", title));
		}
	}
	return problems;
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

auto getMaxChildren(const NodeKind iKind) -> size_t {
	switch (iKind) {
		case NodeKind::Root:
		case NodeKind::Decorator:
			return 1;
		case NodeKind::Composite:
			return std::numeric_limits<size_t>::max();
		case NodeKind::Condition:
		case NodeKind::Action:
			break;
	}
	return 0;
}

auto serialize(const BehaviourTree& iTree) -> std::string {
	YAML::Emitter out;
	out << YAML::BeginMap;
	core::emitFormatVersion(out, g_treeFormat);
	out << YAML::Key << "BehaviourTree" << YAML::Value << YAML::BeginMap;
	out << YAML::Key << "blackboard" << YAML::Value << YAML::BeginSeq;
	for (const auto& key: iTree.blackboard) {
		out << YAML::BeginMap;
		out << YAML::Key << "name" << YAML::Value << key.name;
		out << YAML::Key << "type" << YAML::Value << std::string{magic_enum::enum_name(getBlackboardType(key.value))};
		out << YAML::Key << "value" << YAML::Value;
		emitValue(out, key.value);
		if (!key.description.empty())
			out << YAML::Key << "description" << YAML::Value << key.description;
		out << YAML::EndMap;
	}
	out << YAML::EndSeq;
	out << YAML::Key << "nodes" << YAML::Value << YAML::BeginSeq;
	for (const auto& node: iTree.nodes) {
		out << YAML::BeginMap;
		out << YAML::Key << "id" << YAML::Value << static_cast<uint64_t>(node.id);
		out << YAML::Key << "type" << YAML::Value << node.type;
		out << YAML::Key << "position" << YAML::Value << YAML::Flow << YAML::BeginSeq << node.position[0]
			<< node.position[1] << YAML::EndSeq;
		if (!node.values.empty()) {
			out << YAML::Key << "values" << YAML::Value << YAML::BeginMap;
			for (const auto& [key, value]: node.values) out << YAML::Key << key << YAML::Value << value;
			out << YAML::EndMap;
		}
		if (!node.comment.empty())
			out << YAML::Key << "comment" << YAML::Value << node.comment;
		out << YAML::EndMap;
	}
	out << YAML::EndSeq;
	out << YAML::Key << "links" << YAML::Value << YAML::BeginSeq;
	for (const auto& link: iTree.links) {
		out << YAML::Flow << YAML::BeginMap;
		out << YAML::Key << "parent" << YAML::Value << static_cast<uint64_t>(link.parent);
		out << YAML::Key << "child" << YAML::Value << static_cast<uint64_t>(link.child);
		out << YAML::EndMap;
	}
	out << YAML::EndSeq;
	out << YAML::EndMap;
	out << YAML::EndMap;
	return std::string{out.c_str()} + "\n";
}

auto deserialize(const std::string_view iText, const std::string_view iSourceName)
		-> expected<BehaviourTree, TreeError> {
	YAML::Node root;
	try {
		root = YAML::Load(std::string{iText});
	} catch (const std::exception& iError) {
		OWL_CORE_WARN("BehaviourTree: Cannot parse '{}': {}.", iSourceName, iError.what())
		return unexpected{TreeError::InvalidYaml};
	}
	if (!root.IsMap() || !root["BehaviourTree"] || !root["BehaviourTree"].IsMap()) {
		OWL_CORE_WARN("BehaviourTree: '{}' has no BehaviourTree map.", iSourceName)
		return unexpected{TreeError::InvalidContent};
	}
	if (!core::upgradeYamlDocument(g_treeFormat, root, iSourceName)) {
		OWL_CORE_WARN("BehaviourTree: Cannot read the format of '{}'.", iSourceName)
		return unexpected{TreeError::UnsupportedVersion};
	}
	try {
		return parseTree(root["BehaviourTree"]);
	} catch (const std::exception& iError) {
		OWL_CORE_WARN("BehaviourTree: Invalid content in '{}': {}.", iSourceName, iError.what())
		return unexpected{TreeError::InvalidContent};
	}
}

auto loadFromFile(const std::filesystem::path& iPath) -> expected<BehaviourTree, TreeError> {
	if (std::error_code error; !std::filesystem::is_regular_file(iPath, error)) {
		OWL_CORE_WARN("BehaviourTree: Cannot read '{}'.", iPath.string())
		return unexpected{TreeError::InvalidContent};
	}
	return deserialize(platform::fileToString(iPath), iPath.string());
}

auto loadAsset(const std::filesystem::path& iAsset) -> expected<BehaviourTree, TreeError> {
	const auto bytes = data::assets::readAssetBytes(iAsset);
	if (!bytes) {
		OWL_CORE_WARN("BehaviourTree: '{}' not found in the pack nor the asset folders.", iAsset.string())
		return unexpected{TreeError::InvalidContent};
	}
	return deserialize({reinterpret_cast<const char*>(bytes->data()), bytes->size()}, iAsset.string());
}

auto saveToFile(const BehaviourTree& iTree, const std::filesystem::path& iPath) -> bool {
	if (const auto written = platform::writeFileAtomic(iPath, serialize(iTree)); !written) {
		OWL_CORE_ERROR("BehaviourTree: Failed to write '{}'.", iPath.string())
		return false;
	}
	return true;
}

}// namespace owl::scene::ai
