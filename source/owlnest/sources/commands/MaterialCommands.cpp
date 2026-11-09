/**
 * @file MaterialCommands.cpp
 * @author Silmaen
 * @date 09/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "MaterialCommands.h"

#include "CommandRegistry.h"
#include "DocumentCommands.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <map>
#include <optional>
#include <ranges>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace owl::nest::commands {

MaterialEditor::~MaterialEditor() = default;

namespace {

using renderer::MaterialAsset;
using renderer::MaterialCustomNode;
using renderer::MaterialFunction;
using renderer::MaterialGraph;
using renderer::MaterialGraphLink;
using renderer::MaterialGraphNode;
using renderer::MaterialGraphParameter;
using renderer::MaterialParameterType;
using renderer::MaterialPinSpec;
using renderer::MaterialPinType;

constexpr std::array g_pinTypes{MaterialPinType::Float, MaterialPinType::Float2, MaterialPinType::Float3,
								MaterialPinType::Float4, MaterialPinType::Any};
constexpr std::array g_parameterTypes{MaterialParameterType::Float, MaterialParameterType::Vector,
									  MaterialParameterType::Color, MaterialParameterType::Texture};

auto fail(const CommandError iError, std::string iMessage) -> unexpected<CommandFailure> {
	return unexpected<CommandFailure>{CommandFailure{.error = iError, .message = std::move(iMessage)}};
}

auto invalid(std::string iMessage) -> unexpected<CommandFailure> {
	return fail(CommandError::InvalidArgument, std::move(iMessage));
}

auto unchanged() -> unexpected<CommandFailure> { return fail(CommandError::NotApplicable, "nothing changes"); }

auto trim(std::string_view iText) -> std::string_view {
	while (!iText.empty() && std::isspace(static_cast<unsigned char>(iText.front())) != 0) iText.remove_prefix(1);
	while (!iText.empty() && std::isspace(static_cast<unsigned char>(iText.back())) != 0) iText.remove_suffix(1);
	return iText;
}

auto isIdentifier(const std::string_view iName) -> bool {
	if (iName.empty() || std::isdigit(static_cast<unsigned char>(iName.front())) != 0)
		return false;
	return std::ranges::all_of(iName, [](const char iChar) -> bool {
		return std::isalnum(static_cast<unsigned char>(iChar)) != 0 || iChar == '_';
	});
}

auto parseUuid(const std::string_view iText) -> std::optional<core::UUID> {
	const auto text = trim(iText);
	uint64_t value = 0;
	const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
	if (error != std::errc{} || end != text.data() + text.size() || value == 0)
		return std::nullopt;
	return core::UUID{value};
}

auto formatUuid(const core::UUID iId) -> std::string { return std::to_string(static_cast<uint64_t>(iId)); }

auto parseNodeList(const std::string_view iText) -> std::optional<std::vector<core::UUID>> {
	std::vector<core::UUID> nodes;
	std::string token;
	const auto flush = [&]() -> bool {
		if (token.empty())
			return true;
		const auto id = parseUuid(token);
		token.clear();
		if (!id)
			return false;
		nodes.push_back(*id);
		return true;
	};
	for (const char character: iText) {
		if (character == ' ' || character == ',' || character == ';') {
			if (!flush())
				return std::nullopt;
		} else {
			token.push_back(character);
		}
	}
	if (!flush())
		return std::nullopt;
	return nodes;
}

// The nodes and links a command edits: the graph itself or the body of one of its functions.
struct Scope {
	std::vector<MaterialGraphNode>* nodes{nullptr};
	std::vector<MaterialGraphLink>* links{nullptr};
	const MaterialFunction* function{nullptr};
};

auto scopeOf(MaterialAsset& ioAsset, const CommandArgs& iArgs) -> expected<Scope, CommandFailure> {
	if (!ioAsset.graph.has_value())
		return fail(CommandError::NotApplicable, "the material has no graph (run `material.convert_to_graph`)");
	auto& graph = *ioAsset.graph;
	const auto name = iArgs.getText("function");
	if (name.empty())
		return Scope{.nodes = &graph.nodes, .links = &graph.links, .function = nullptr};
	const auto found = std::ranges::find(graph.functions, name, &MaterialFunction::name);
	if (found == graph.functions.end())
		return invalid(std::format("no function `{}`", name));
	return Scope{.nodes = &found->nodes, .links = &found->links, .function = &*found};
}

auto findNode(std::vector<MaterialGraphNode>& ioNodes, const core::UUID iId) -> MaterialGraphNode* {
	const auto found = std::ranges::find(ioNodes, iId, &MaterialGraphNode::id);
	return found == ioNodes.end() ? nullptr : &*found;
}

auto nodeArg(const Scope& iScope, const CommandArgs& iArgs, const std::string& iName)
		-> expected<MaterialGraphNode*, CommandFailure> {
	const auto id = parseUuid(iArgs.getText(iName));
	if (!id)
		return invalid(std::format("`{}` is not a node identifier", iName));
	auto* node = findNode(*iScope.nodes, *id);
	if (node == nullptr)
		return invalid(std::format("no node {} in the graph", static_cast<uint64_t>(*id)));
	return node;
}

auto hasPin(const std::vector<MaterialPinSpec>& iPins, const std::string& iName) -> bool {
	return std::ranges::contains(iPins, iName, &MaterialPinSpec::name);
}

// Drops the links whose nodes or pins are gone, in the graph and in every function body.
void pruneLinks(MaterialGraph& ioGraph) {
	const auto prune = [&ioGraph](std::vector<MaterialGraphNode>& ioNodes, std::vector<MaterialGraphLink>& ioLinks,
								  const MaterialFunction* iFunction) -> void {
		std::erase_if(ioLinks, [&](const MaterialGraphLink& iLink) -> bool {
			const auto* from = findNode(ioNodes, iLink.fromNode);
			const auto* to = findNode(ioNodes, iLink.toNode);
			if (from == nullptr || to == nullptr)
				return true;
			return !hasPin(renderer::getMaterialNodePins(ioGraph, *from, iFunction).outputs, iLink.fromPin) ||
				   !hasPin(renderer::getMaterialNodePins(ioGraph, *to, iFunction).inputs, iLink.toPin);
		});
	};
	prune(ioGraph.nodes, ioGraph.links, nullptr);
	for (auto& function: ioGraph.functions) prune(function.nodes, function.links, &function);
}

// The setting naming a graph item on a node type, empty when the type names none.
auto referenceSetting(const std::string& iType) -> std::string_view {
	if (iType == "param.get")
		return "parameter";
	if (iType == "custom")
		return "custom";
	if (iType == "function.call")
		return "function";
	if (iType.starts_with("texture."))
		return "texture";
	return {};
}

void renameReferences(MaterialGraph& ioGraph, const std::string_view iSetting, const std::string& iOld,
					  const std::string& iNew) {
	const auto rename = [&](std::vector<MaterialGraphNode>& ioNodes) -> void {
		for (auto& node: ioNodes) {
			if (referenceSetting(node.type) != iSetting)
				continue;
			if (const auto value = node.values.find(std::string{iSetting});
				value != node.values.end() && value->second == iOld)
				value->second = iNew;
		}
	};
	rename(ioGraph.nodes);
	for (auto& function: ioGraph.functions) rename(function.nodes);
}

// Whether a link from `iFrom` to `iTo` closes a loop: `iFrom` already depends on `iTo`.
auto makesCycle(const std::vector<MaterialGraphLink>& iLinks, const core::UUID iFrom, const core::UUID iTo) -> bool {
	std::vector<core::UUID> pending{iTo};
	std::set<uint64_t> seen;
	while (!pending.empty()) {
		const auto current = pending.back();
		pending.pop_back();
		if (current == iFrom)
			return true;
		if (!seen.insert(static_cast<uint64_t>(current)).second)
			continue;
		for (const auto& link: iLinks)
			if (link.fromNode == current)
				pending.push_back(link.toNode);
	}
	return false;
}

template<typename T>
auto freeName(const std::vector<T>& iItems, const std::string& iBase) -> std::string {
	std::string name = iBase;
	for (uint32_t counter = 2; std::ranges::contains(iItems, name, &T::name); ++counter)
		name = std::format("{}_{}", iBase, counter);
	return name;
}

template<typename T>
auto findNamed(std::vector<T>& ioItems, const std::string& iName) -> expected<T*, CommandFailure> {
	const auto found = std::ranges::find(ioItems, iName, &T::name);
	if (found == ioItems.end())
		return invalid(std::format("no `{}` in the graph", iName));
	return &*found;
}

auto parsePinType(const std::string_view iName) -> std::optional<MaterialPinType> {
	for (const auto type: g_pinTypes)
		if (getMaterialPinTypeName(type) == iName)
			return type;
	return std::nullopt;
}

auto parseParameterType(const std::string_view iName) -> std::optional<MaterialParameterType> {
	for (const auto type: g_parameterTypes)
		if (getMaterialParameterTypeName(type) == iName)
			return type;
	return std::nullopt;
}

auto defaultOf(const MaterialParameterType iType) -> std::string {
	switch (iType) {
		case MaterialParameterType::Float:
			return "0";
		case MaterialParameterType::Vector:
			return "0 0 0 0";
		case MaterialParameterType::Color:
			return "1 1 1 1";
		case MaterialParameterType::Texture:
			return {};
	}
	return {};
}

// The material after the edit, the name of the step and what the command returns.
struct MaterialEdit {
	MaterialAsset asset;
	std::string description;
	ResultData data{};// NOLINT(readability-redundant-member-init) callers may omit it
};

// Computes an edit of the material; a function pointer fits std::function's buffer.
using MaterialChange = auto (*)(const MaterialAsset&, const CommandArgs&) -> expected<MaterialEdit, CommandFailure>;

auto openEditor(const CommandContext& iContext, const CommandArgs& iArgs) -> expected<MaterialEditor*, CommandFailure> {
	const std::filesystem::path path{iArgs.getText("path")};
	if (path.extension() != ".owlmat")
		return invalid("`path` must end with `.owlmat`");
	auto* editor = iContext.documents != nullptr ? iContext.documents->openMaterial(path) : nullptr;
	if (editor == nullptr)
		return fail(CommandError::NotApplicable, std::format("cannot open {} in an editor tab", path.string()));
	return editor;
}

auto materialRunner(MaterialChange iChange) -> CommandSpec::Runner {
	return [change = iChange](const CommandContext& iContext,
							  const CommandArgs& iArgs) -> expected<CommandResult, CommandFailure> {
		const auto editor = openEditor(iContext, iArgs);
		if (!editor)
			return unexpected<CommandFailure>{editor.error()};
		auto edit = change((*editor)->material(), iArgs);
		if (!edit)
			return unexpected<CommandFailure>{edit.error()};
		if (!(*editor)->setMaterial(edit->asset, edit->description, iArgs.getBool("merge")))
			return unchanged();
		edit->data["path"] = std::filesystem::path{iArgs.getText("path")}.generic_string();
		return CommandResult{.entity = core::UUID{0},
							 .description = std::move(edit->description),
							 .data = std::move(edit->data)};
	};
}

// --- Kinds -----------------------------------------------------------------

auto convertToGraph(const MaterialAsset& iAsset, const CommandArgs& /*iArgs*/)
		-> expected<MaterialEdit, CommandFailure> {
	if (iAsset.graph.has_value())
		return unchanged();
	if (!iAsset.parent.empty())
		return fail(CommandError::NotApplicable, "an instance takes the graph of its parent");
	MaterialEdit edit{.asset = iAsset, .description = "Convert to graph"};
	edit.asset.graph = renderer::getParameterMaterialGraph();
	return edit;
}

auto convertToParameters(const MaterialAsset& iAsset, const CommandArgs& /*iArgs*/)
		-> expected<MaterialEdit, CommandFailure> {
	if (!iAsset.graph.has_value())
		return unchanged();
	MaterialEdit edit{.asset = iAsset, .description = "Convert to parameters"};
	edit.asset.graph.reset();
	return edit;
}

// --- Nodes -----------------------------------------------------------------

auto addNode(const MaterialAsset& iAsset, const CommandArgs& iArgs) -> expected<MaterialEdit, CommandFailure> {
	MaterialEdit edit{.asset = iAsset, .description = {}};
	const auto scope = scopeOf(edit.asset, iArgs);
	if (!scope)
		return unexpected<CommandFailure>{scope.error()};
	const auto typeId = iArgs.getText("type");
	const auto* type = renderer::findMaterialNodeType(typeId);
	if (type == nullptr)
		return invalid(std::format("unknown node type `{}`", typeId));
	if ((typeId == "function.input" || typeId == "function.output") && scope->function == nullptr)
		return invalid("the function input and output nodes live in a function body");
	MaterialGraphNode node{.id = core::UUID{},
						   .type = typeId,
						   .position = {static_cast<float>(iArgs.get<double>("x").value_or(0.0)),
										static_cast<float>(iArgs.get<double>("y").value_or(0.0))},
						   .values = {}};
	if (const auto setting = referenceSetting(typeId); !setting.empty() && iArgs.has("item"))
		node.values[std::string{setting}] = iArgs.getText("item");
	edit.description = std::format("Add {}", type->title);
	edit.data["node"] = formatUuid(node.id);
	scope->nodes->push_back(std::move(node));
	return edit;
}

auto removeNodes(const MaterialAsset& iAsset, const CommandArgs& iArgs) -> expected<MaterialEdit, CommandFailure> {
	MaterialEdit edit{.asset = iAsset, .description = {}};
	const auto scope = scopeOf(edit.asset, iArgs);
	if (!scope)
		return unexpected<CommandFailure>{scope.error()};
	const auto nodes = parseNodeList(iArgs.getText("nodes"));
	if (!nodes || nodes->empty())
		return invalid("`nodes` is not a list of node identifiers");
	const auto removed = std::erase_if(*scope->nodes, [&](const MaterialGraphNode& iNode) -> bool {
		return std::ranges::contains(*nodes, iNode.id);
	});
	if (removed == 0)
		return unchanged();
	std::erase_if(*scope->links, [&](const MaterialGraphLink& iLink) -> bool {
		return std::ranges::contains(*nodes, iLink.fromNode) || std::ranges::contains(*nodes, iLink.toNode);
	});
	edit.description = removed == 1 ? std::string{"Delete node"} : std::format("Delete {} nodes", removed);
	return edit;
}

auto moveNodes(const MaterialAsset& iAsset, const CommandArgs& iArgs) -> expected<MaterialEdit, CommandFailure> {
	MaterialEdit edit{.asset = iAsset, .description = "Move nodes"};
	const auto scope = scopeOf(edit.asset, iArgs);
	if (!scope)
		return unexpected<CommandFailure>{scope.error()};
	const auto layout = iArgs.getText("layout");
	for (const auto entry: layout | std::views::split(';')) {
		const auto text = trim(std::string_view{entry.begin(), entry.end()});
		if (text.empty())
			continue;
		std::array<std::string, 3> parts;
		size_t part = 0;
		for (const char character: text) {
			if (character == ' ' || character == ',') {
				if (parts[part].empty())
					continue;
				++part;
				if (part >= parts.size())
					return invalid("`layout` entries are `node x y`");
			} else {
				parts[part].push_back(character);
			}
		}
		const auto id = parseUuid(parts[0]);
		std::array<float, 2> position{0.f, 0.f};
		const auto readFloat = [](const std::string& iText, float& oValue) -> bool {
			const auto [end, error] = std::from_chars(iText.data(), iText.data() + iText.size(), oValue);
			return error == std::errc{} && end == iText.data() + iText.size();
		};
		if (!id || !readFloat(parts[1], position[0]) || !readFloat(parts[2], position[1]))
			return invalid(std::format("`layout` entry `{}` is not `node x y`", text));
		auto* node = findNode(*scope->nodes, *id);
		if (node == nullptr)
			return invalid(std::format("no node {} in the graph", parts[0]));
		node->position = position;
	}
	return edit;
}

auto linkPins(const MaterialAsset& iAsset, const CommandArgs& iArgs) -> expected<MaterialEdit, CommandFailure> {
	MaterialEdit edit{.asset = iAsset, .description = "Link pins"};
	const auto scope = scopeOf(edit.asset, iArgs);
	if (!scope)
		return unexpected<CommandFailure>{scope.error()};
	const auto from = nodeArg(*scope, iArgs, "from_node");
	if (!from)
		return unexpected<CommandFailure>{from.error()};
	const auto to = nodeArg(*scope, iArgs, "to_node");
	if (!to)
		return unexpected<CommandFailure>{to.error()};
	const auto fromPin = iArgs.getText("from_pin");
	const auto toPin = iArgs.getText("to_pin");
	const auto& graph = *edit.asset.graph;
	if (!hasPin(renderer::getMaterialNodePins(graph, **from, scope->function).outputs, fromPin))
		return invalid(std::format("the source node has no output `{}`", fromPin));
	if (!hasPin(renderer::getMaterialNodePins(graph, **to, scope->function).inputs, toPin))
		return invalid(std::format("the destination node has no input `{}`", toPin));
	if ((*from)->id == (*to)->id || makesCycle(*scope->links, (*from)->id, (*to)->id))
		return invalid("the link would make a node depend on itself");
	const MaterialGraphLink link{.fromNode = (*from)->id, .fromPin = fromPin, .toNode = (*to)->id, .toPin = toPin};
	if (std::ranges::contains(*scope->links, link))
		return unchanged();
	std::erase_if(*scope->links, [&](const MaterialGraphLink& iLink) -> bool {
		return iLink.toNode == link.toNode && iLink.toPin == link.toPin;
	});
	scope->links->push_back(link);
	return edit;
}

auto unlinkPin(const MaterialAsset& iAsset, const CommandArgs& iArgs) -> expected<MaterialEdit, CommandFailure> {
	MaterialEdit edit{.asset = iAsset, .description = "Delete link"};
	const auto scope = scopeOf(edit.asset, iArgs);
	if (!scope)
		return unexpected<CommandFailure>{scope.error()};
	const auto to = parseUuid(iArgs.getText("to_node"));
	if (!to)
		return invalid("`to_node` is not a node identifier");
	const auto pin = iArgs.getText("to_pin");
	if (std::erase_if(*scope->links, [&](const MaterialGraphLink& iLink) -> bool {
			return iLink.toNode == *to && iLink.toPin == pin;
		}) == 0)
		return unchanged();
	return edit;
}

auto setNodeValue(const MaterialAsset& iAsset, const CommandArgs& iArgs) -> expected<MaterialEdit, CommandFailure> {
	MaterialEdit edit{.asset = iAsset, .description = {}};
	const auto scope = scopeOf(edit.asset, iArgs);
	if (!scope)
		return unexpected<CommandFailure>{scope.error()};
	const auto node = nodeArg(*scope, iArgs, "node");
	if (!node)
		return unexpected<CommandFailure>{node.error()};
	const auto name = iArgs.getText("name");
	const auto* type = renderer::findMaterialNodeType((*node)->type);
	const bool isSetting =
			type != nullptr && std::ranges::contains(type->settings, name, &renderer::MaterialSettingSpec::name);
	if (!isSetting && !hasPin(renderer::getMaterialNodePins(*edit.asset.graph, **node, scope->function).inputs, name))
		return invalid(std::format("the node has no setting nor input `{}`", name));
	if (iArgs.getBool("reset"))
		(*node)->values.erase(name);
	else
		(*node)->values[name] = iArgs.getText("value");
	edit.description = std::format("Set {}", name);
	if (isSetting && !referenceSetting((*node)->type).empty())
		pruneLinks(*edit.asset.graph);
	return edit;
}

auto pasteNodes(const MaterialAsset& iAsset, const CommandArgs& iArgs) -> expected<MaterialEdit, CommandFailure> {
	MaterialEdit edit{.asset = iAsset, .description = "Paste nodes"};
	const auto scope = scopeOf(edit.asset, iArgs);
	if (!scope)
		return unexpected<CommandFailure>{scope.error()};
	const auto clip = MaterialAsset::fromYaml(iArgs.getText("nodes"));
	if (!clip || !clip->graph.has_value() || clip->graph->nodes.empty())
		return invalid("`nodes` is not a copy of material nodes");
	auto& graph = *edit.asset.graph;
	const auto addMissing = [](auto& ioItems, const auto& iSource) -> void {
		for (const auto& item: iSource)
			if (!std::ranges::contains(ioItems, item.name, &std::remove_cvref_t<decltype(item)>::name))
				ioItems.push_back(item);
	};
	addMissing(graph.parameters, clip->graph->parameters);
	addMissing(graph.customNodes, clip->graph->customNodes);
	addMissing(graph.functions, clip->graph->functions);
	// Function bodies may have been added: the scope is looked up again.
	const auto target = scopeOf(edit.asset, iArgs);
	if (!target)
		return unexpected<CommandFailure>{target.error()};
	float minX = clip->graph->nodes.front().position[0];
	float minY = clip->graph->nodes.front().position[1];
	for (const auto& node: clip->graph->nodes) {
		minX = std::min(minX, node.position[0]);
		minY = std::min(minY, node.position[1]);
	}
	const float x = static_cast<float>(iArgs.get<double>("x").value_or(static_cast<double>(minX) + 40.0));
	const float y = static_cast<float>(iArgs.get<double>("y").value_or(static_cast<double>(minY) + 40.0));
	std::map<uint64_t, core::UUID> ids;
	std::vector<core::UUID> pasted;
	for (auto node: clip->graph->nodes) {
		if ((node.type == "function.input" || node.type == "function.output") && target->function == nullptr)
			continue;
		const core::UUID id;
		ids[static_cast<uint64_t>(node.id)] = id;
		node.id = id;
		node.position = {node.position[0] - minX + x, node.position[1] - minY + y};
		pasted.push_back(id);
		target->nodes->push_back(std::move(node));
	}
	for (auto link: clip->graph->links) {
		const auto from = ids.find(static_cast<uint64_t>(link.fromNode));
		const auto to = ids.find(static_cast<uint64_t>(link.toNode));
		if (from == ids.end() || to == ids.end())
			continue;
		link.fromNode = from->second;
		link.toNode = to->second;
		target->links->push_back(std::move(link));
	}
	pruneLinks(graph);
	edit.data["nodes"] = formatMaterialNodes(pasted);
	return edit;
}

// --- Parameters ------------------------------------------------------------

auto addParameter(const MaterialAsset& iAsset, const CommandArgs& iArgs) -> expected<MaterialEdit, CommandFailure> {
	MaterialEdit edit{.asset = iAsset, .description = "Add parameter"};
	if (!edit.asset.graph.has_value())
		return fail(CommandError::NotApplicable, "the material has no graph");
	auto& graph = *edit.asset.graph;
	const auto type = parseParameterType(iArgs.getText("type", "Float"));
	if (!type)
		return invalid("`type` is one of Float, Vector, Color, Texture");
	const auto base = iArgs.getText("name", "value");
	if (!isIdentifier(base))
		return invalid("`name` must be a Slang identifier");
	const auto name = freeName(graph.parameters, base);
	graph.parameters.push_back(
			{.name = name, .type = *type, .defaultValue = iArgs.getText("default", defaultOf(*type))});
	edit.data["name"] = name;
	return edit;
}

auto removeParameter(const MaterialAsset& iAsset, const CommandArgs& iArgs) -> expected<MaterialEdit, CommandFailure> {
	MaterialEdit edit{.asset = iAsset, .description = "Remove parameter"};
	if (!edit.asset.graph.has_value())
		return fail(CommandError::NotApplicable, "the material has no graph");
	auto& graph = *edit.asset.graph;
	if (std::erase_if(graph.parameters, [&](const MaterialGraphParameter& iParameter) -> bool {
			return iParameter.name == iArgs.getText("name");
		}) == 0)
		return invalid(std::format("no parameter `{}`", iArgs.getText("name")));
	pruneLinks(graph);
	return edit;
}

auto setParameter(const MaterialAsset& iAsset, const CommandArgs& iArgs) -> expected<MaterialEdit, CommandFailure> {
	MaterialEdit edit{.asset = iAsset, .description = "Edit parameter"};
	if (!edit.asset.graph.has_value())
		return fail(CommandError::NotApplicable, "the material has no graph");
	auto& graph = *edit.asset.graph;
	const auto name = iArgs.getText("name");
	const auto parameter = findNamed(graph.parameters, name);
	if (!parameter)
		return unexpected<CommandFailure>{parameter.error()};
	if (iArgs.has("type")) {
		const auto type = parseParameterType(iArgs.getText("type"));
		if (!type)
			return invalid("`type` is one of Float, Vector, Color, Texture");
		if (*type != (*parameter)->type) {
			(*parameter)->type = *type;
			(*parameter)->defaultValue = defaultOf(*type);
		}
	}
	if (iArgs.has("default"))
		(*parameter)->defaultValue = iArgs.getText("default");
	if (const auto newName = iArgs.getText("new_name"); !newName.empty() && newName != name) {
		if (!isIdentifier(newName) || graph.findParameter(newName) != nullptr)
			return invalid("`new_name` is not a Slang identifier or is taken");
		(*parameter)->name = newName;
		renameReferences(graph, "parameter", name, newName);
		renameReferences(graph, "texture", name, newName);
		edit.description = "Rename parameter";
	}
	pruneLinks(graph);
	return edit;
}

// --- Custom nodes and functions --------------------------------------------

auto pinsArg(const CommandArgs& iArgs, const std::string& iName, std::vector<MaterialPinSpec>& ioPins)
		-> std::optional<CommandFailure> {
	if (!iArgs.has(iName))
		return std::nullopt;
	auto pins = parseMaterialPins(iArgs.getText(iName));
	if (!pins)
		return CommandFailure{.error = CommandError::InvalidArgument,
							  .message = std::format("`{}` is not a list of `name:Type=default` pins", iName)};
	ioPins = std::move(*pins);
	return std::nullopt;
}

auto addCustomNode(const MaterialAsset& iAsset, const CommandArgs& iArgs) -> expected<MaterialEdit, CommandFailure> {
	MaterialEdit edit{.asset = iAsset, .description = "Add custom node"};
	if (!edit.asset.graph.has_value())
		return fail(CommandError::NotApplicable, "the material has no graph");
	auto& graph = *edit.asset.graph;
	const auto base = iArgs.getText("name", "custom");
	if (!isIdentifier(base))
		return invalid("`name` must be a Slang identifier");
	const auto name = freeName(graph.customNodes, base);
	graph.customNodes.push_back({.name = name,
								 .title = {},
								 .inputs = {{.name = "value", .type = MaterialPinType::Float, .defaultValue = "0"}},
								 .outputs = {{.name = "result", .type = MaterialPinType::Float, .defaultValue = "0"}},
								 .code = "result = value;"});
	edit.data["name"] = name;
	return edit;
}

auto removeCustomNode(const MaterialAsset& iAsset, const CommandArgs& iArgs) -> expected<MaterialEdit, CommandFailure> {
	MaterialEdit edit{.asset = iAsset, .description = "Remove custom node"};
	if (!edit.asset.graph.has_value())
		return fail(CommandError::NotApplicable, "the material has no graph");
	auto& graph = *edit.asset.graph;
	if (std::erase_if(graph.customNodes, [&](const MaterialCustomNode& iCustom) -> bool {
			return iCustom.name == iArgs.getText("name");
		}) == 0)
		return invalid(std::format("no custom node `{}`", iArgs.getText("name")));
	pruneLinks(graph);
	return edit;
}

auto setCustomNode(const MaterialAsset& iAsset, const CommandArgs& iArgs) -> expected<MaterialEdit, CommandFailure> {
	MaterialEdit edit{.asset = iAsset, .description = "Edit custom node"};
	if (!edit.asset.graph.has_value())
		return fail(CommandError::NotApplicable, "the material has no graph");
	auto& graph = *edit.asset.graph;
	const auto name = iArgs.getText("name");
	const auto custom = findNamed(graph.customNodes, name);
	if (!custom)
		return unexpected<CommandFailure>{custom.error()};
	if (iArgs.has("title"))
		(*custom)->title = iArgs.getText("title");
	if (iArgs.has("code"))
		(*custom)->code = iArgs.getText("code");
	for (const auto& [arg, pins]: {std::pair{"inputs", &(*custom)->inputs}, std::pair{"outputs", &(*custom)->outputs}})
		if (const auto error = pinsArg(iArgs, arg, *pins); error.has_value())
			return unexpected<CommandFailure>{*error};
	if (const auto newName = iArgs.getText("new_name"); !newName.empty() && newName != name) {
		if (!isIdentifier(newName) || graph.findCustomNode(newName) != nullptr)
			return invalid("`new_name` is not a Slang identifier or is taken");
		(*custom)->name = newName;
		renameReferences(graph, "custom", name, newName);
		edit.description = "Rename custom node";
	}
	pruneLinks(graph);
	return edit;
}

auto addFunction(const MaterialAsset& iAsset, const CommandArgs& iArgs) -> expected<MaterialEdit, CommandFailure> {
	MaterialEdit edit{.asset = iAsset, .description = "Add function"};
	if (!edit.asset.graph.has_value())
		return fail(CommandError::NotApplicable, "the material has no graph");
	auto& graph = *edit.asset.graph;
	const auto base = iArgs.getText("name", "function");
	if (!isIdentifier(base))
		return invalid("`name` must be a Slang identifier");
	MaterialFunction function{.name = freeName(graph.functions, base),
							  .inputs = {{.name = "value", .type = MaterialPinType::Float, .defaultValue = "0"}},
							  .outputs = {{.name = "result", .type = MaterialPinType::Float, .defaultValue = "0"}},
							  .nodes = {},
							  .links = {}};
	const core::UUID input;
	const core::UUID output;
	function.nodes.push_back({.id = input, .type = "function.input", .position = {0.f, 0.f}, .values = {}});
	function.nodes.push_back({.id = output, .type = "function.output", .position = {320.f, 0.f}, .values = {}});
	function.links.push_back({.fromNode = input, .fromPin = "value", .toNode = output, .toPin = "result"});
	edit.data["name"] = function.name;
	graph.functions.push_back(std::move(function));
	return edit;
}

auto removeFunction(const MaterialAsset& iAsset, const CommandArgs& iArgs) -> expected<MaterialEdit, CommandFailure> {
	MaterialEdit edit{.asset = iAsset, .description = "Remove function"};
	if (!edit.asset.graph.has_value())
		return fail(CommandError::NotApplicable, "the material has no graph");
	auto& graph = *edit.asset.graph;
	if (std::erase_if(graph.functions, [&](const MaterialFunction& iFunction) -> bool {
			return iFunction.name == iArgs.getText("name");
		}) == 0)
		return invalid(std::format("no function `{}`", iArgs.getText("name")));
	pruneLinks(graph);
	return edit;
}

auto setFunction(const MaterialAsset& iAsset, const CommandArgs& iArgs) -> expected<MaterialEdit, CommandFailure> {
	MaterialEdit edit{.asset = iAsset, .description = "Edit function"};
	if (!edit.asset.graph.has_value())
		return fail(CommandError::NotApplicable, "the material has no graph");
	auto& graph = *edit.asset.graph;
	const auto name = iArgs.getText("name");
	const auto function = findNamed(graph.functions, name);
	if (!function)
		return unexpected<CommandFailure>{function.error()};
	for (const auto& [arg, pins]:
		 {std::pair{"inputs", &(*function)->inputs}, std::pair{"outputs", &(*function)->outputs}})
		if (const auto error = pinsArg(iArgs, arg, *pins); error.has_value())
			return unexpected<CommandFailure>{*error};
	if (const auto newName = iArgs.getText("new_name"); !newName.empty() && newName != name) {
		if (!isIdentifier(newName) || graph.findFunction(newName) != nullptr)
			return invalid("`new_name` is not a Slang identifier or is taken");
		(*function)->name = newName;
		renameReferences(graph, "function", name, newName);
		edit.description = "Rename function";
	}
	pruneLinks(graph);
	return edit;
}

// --- Instances -------------------------------------------------------------

auto setGraphValue(const MaterialAsset& iAsset, const CommandArgs& iArgs) -> expected<MaterialEdit, CommandFailure> {
	MaterialEdit edit{.asset = iAsset, .description = "Override parameter"};
	const auto name = iArgs.getText("name");
	if (!isIdentifier(name))
		return invalid("`name` must be a parameter name");
	if (iArgs.getBool("reset")) {
		if (edit.asset.graphValues.erase(name) == 0)
			return unchanged();
		edit.description = "Inherit parameter";
	} else {
		edit.asset.graphValues[name] = iArgs.getText("value");
	}
	return edit;
}

auto setParentRunner(const CommandContext& iContext, const CommandArgs& iArgs)
		-> expected<CommandResult, CommandFailure> {
	const auto editor = openEditor(iContext, iArgs);
	if (!editor)
		return unexpected<CommandFailure>{editor.error()};
	const std::filesystem::path parent{iArgs.getText("parent")};
	if (!(*editor)->setMaterialParent(parent))
		return fail(CommandError::NotApplicable, "the parent is the same, not a material or the material itself");
	return CommandResult{.entity = core::UUID{0},
						 .description = parent.empty() ? "Clear material parent" : "Set material parent",
						 .data = {{"path", std::filesystem::path{iArgs.getText("path")}.generic_string()}}};
}

auto setValuesRunner(const CommandContext& iContext, const CommandArgs& iArgs)
		-> expected<CommandResult, CommandFailure> {
	const auto editor = openEditor(iContext, iArgs);
	if (!editor)
		return unexpected<CommandFailure>{editor.error()};
	const auto overrides = renderer::MaterialOverrides::fromYaml(iArgs.getText("values"));
	auto values = (*editor)->material().values;
	values.copyFields(overrides.values, overrides.fields);
	const uint32_t fields = (*editor)->material().parent.empty() ? renderer::g_allMaterialFields : overrides.fields;
	if (!(*editor)->setMaterialValues(values, fields, iArgs.getBool("merge")))
		return unchanged();
	return CommandResult{.entity = core::UUID{0},
						 .description = "Edit material",
						 .data = {{"path", std::filesystem::path{iArgs.getText("path")}.generic_string()}}};
}

// --- Specs -----------------------------------------------------------------

auto text(const char* iName, const char* iDescription, const bool iRequired = false) -> ArgSpec {
	return {.name = iName, .type = ArgType::Text, .required = iRequired, .description = iDescription};
}

auto flag(const char* iName, const char* iDescription) -> ArgSpec {
	return {.name = iName, .type = ArgType::Bool, .description = iDescription, .defaultValue = ArgValue{false}};
}

auto number(const char* iName, const char* iDescription) -> ArgSpec {
	return {.name = iName, .type = ArgType::Number, .description = iDescription};
}

auto functionArg() -> ArgSpec {
	return text("function", "Function whose body is edited, the graph itself when empty.");
}

auto mergeArg() -> ArgSpec { return flag("merge", "Join the previous step of the same name (a slider drag)."); }

auto pathArg() -> ArgSpec {
	return {.name = "path",
			.type = ArgType::Text,
			.required = true,
			.description = "The `.owlmat` file, opened in a tab when it is not.",
			.projectPath = true};
}

}// namespace

auto getMaterialPinTypeName(const renderer::MaterialPinType iType) -> std::string_view {
	switch (iType) {
		case MaterialPinType::Float:
			return "Float";
		case MaterialPinType::Float2:
			return "Float2";
		case MaterialPinType::Float3:
			return "Float3";
		case MaterialPinType::Float4:
			return "Float4";
		case MaterialPinType::Any:
			return "Any";
	}
	return "Float";
}

auto getMaterialParameterTypeName(const renderer::MaterialParameterType iType) -> std::string_view {
	switch (iType) {
		case MaterialParameterType::Float:
			return "Float";
		case MaterialParameterType::Vector:
			return "Vector";
		case MaterialParameterType::Color:
			return "Color";
		case MaterialParameterType::Texture:
			return "Texture";
	}
	return "Float";
}

auto formatMaterialPins(const std::span<const renderer::MaterialPinSpec> iPins) -> std::string {
	std::string result;
	for (const auto& pin: iPins) {
		if (!result.empty())
			result += ", ";
		result += std::format("{}:{}", pin.name, getMaterialPinTypeName(pin.type));
		if (!pin.defaultValue.empty())
			result += std::format("={}", pin.defaultValue);
	}
	return result;
}

auto parseMaterialPins(const std::string_view iText) -> std::optional<std::vector<renderer::MaterialPinSpec>> {
	std::vector<MaterialPinSpec> pins;
	for (const auto entry: iText | std::views::split(',')) {
		const auto item = trim(std::string_view{entry.begin(), entry.end()});
		if (item.empty())
			continue;
		const auto equal = item.find('=');
		const auto head = item.substr(0, equal);
		const auto colon = head.find(':');
		const auto name = trim(head.substr(0, colon));
		const auto typeName =
				colon == std::string_view::npos ? std::string_view{"Float"} : trim(head.substr(colon + 1));
		const auto type = parsePinType(typeName);
		if (!isIdentifier(name) || !type.has_value() || *type == MaterialPinType::Any ||
			std::ranges::contains(pins, name, &MaterialPinSpec::name))
			return std::nullopt;
		pins.push_back({.name = std::string{name},
						.type = *type,
						.defaultValue = equal == std::string_view::npos ? std::string{"0"}
																		: std::string{trim(item.substr(equal + 1))}});
	}
	return pins;
}

auto formatMaterialNodes(const std::span<const core::UUID> iNodes) -> std::string {
	std::string result;
	for (const auto node: iNodes) {
		if (!result.empty())
			result += ' ';
		result += formatUuid(node);
	}
	return result;
}

auto getMaterialScopeNodes(const renderer::MaterialGraph& iGraph, const std::string& iFunction)
		-> const std::vector<renderer::MaterialGraphNode>* {
	if (iFunction.empty())
		return &iGraph.nodes;
	const auto* function = iGraph.findFunction(iFunction);
	return function != nullptr ? &function->nodes : nullptr;
}

auto getMaterialScopeLinks(const renderer::MaterialGraph& iGraph, const std::string& iFunction)
		-> const std::vector<renderer::MaterialGraphLink>* {
	if (iFunction.empty())
		return &iGraph.links;
	const auto* function = iGraph.findFunction(iFunction);
	return function != nullptr ? &function->links : nullptr;
}

auto copyMaterialNodes(const renderer::MaterialGraph& iGraph, const std::string& iFunction,
					   const std::span<const core::UUID> iNodes) -> std::string {
	const auto* nodes = getMaterialScopeNodes(iGraph, iFunction);
	const auto* links = getMaterialScopeLinks(iGraph, iFunction);
	if (nodes == nullptr || links == nullptr)
		return {};
	MaterialGraph clip;
	std::set<std::string> parameters;
	std::set<std::string> customs;
	std::set<std::string> functions;
	for (const auto& node: *nodes) {
		if (!std::ranges::contains(iNodes, node.id))
			continue;
		clip.nodes.push_back(node);
		const auto setting = referenceSetting(node.type);
		const auto value = renderer::getMaterialNodeValue(node, setting);
		if (setting == "parameter" || setting == "texture")
			parameters.insert(value);
		else if (setting == "custom")
			customs.insert(value);
		else if (setting == "function")
			functions.insert(value);
	}
	if (clip.nodes.empty())
		return {};
	for (const auto& link: *links)
		if (std::ranges::contains(iNodes, link.fromNode) && std::ranges::contains(iNodes, link.toNode))
			clip.links.push_back(link);
	for (const auto& parameter: iGraph.parameters)
		if (parameters.contains(parameter.name))
			clip.parameters.push_back(parameter);
	for (const auto& custom: iGraph.customNodes)
		if (customs.contains(custom.name))
			clip.customNodes.push_back(custom);
	for (const auto& function: iGraph.functions)
		if (functions.contains(function.name))
			clip.functions.push_back(function);
	MaterialAsset asset;
	asset.graph = std::move(clip);
	return asset.toYaml();
}

void addMaterialCommands(CommandRegistry& ioRegistry) {
	const auto add = [&ioRegistry](const char* iName, const char* iDescription, std::vector<ArgSpec> iArgs,
								   CommandSpec::Runner iRunner) -> void {
		iArgs.insert(iArgs.begin(), pathArg());
		ioRegistry.add({.name = iName,
						.description = iDescription,
						.args = std::move(iArgs),
						.factory = {},
						.run = std::move(iRunner),
						.allowedInPlay = true,
						.needsScene = false});
	};
	const auto edit = [&add](const char* iName, const char* iDescription, std::vector<ArgSpec> iArgs,
							 MaterialChange iChange) -> void {
		iArgs.push_back(mergeArg());
		add(iName, iDescription, std::move(iArgs), materialRunner(iChange));
	};
	add("material.set_parent", "Make a material an instance of another `.owlmat`, or a root material again.",
		{text("parent", "Asset path of the parent `.owlmat`, empty for a root material.")}, setParentRunner);
	add("material.set_values", "Set the values of a material: the fields of a `MaterialOverrides` YAML map.",
		{text("values", "YAML map `{fields: <mask>, ...values}` as `MaterialOverrides::toYaml` writes it.", true),
		 mergeArg()},
		setValuesRunner);
	edit("material.convert_to_graph",
		 "Give a root material a graph: its parameter surface linked to the surface node, drawn the same.", {},
		 convertToGraph);
	edit("material.convert_to_parameters", "Drop the graph of a material: its values draw it again.", {},
		 convertToParameters);
	edit("material.add_node", "Add a node to the graph of a material or to the body of one of its functions.",
		 {text("type", "Node type (`math.multiply`, `texture.sample`...).", true),
		  number("x", "Canvas position of the node."), number("y", "Canvas position of the node."),
		  text("item", "Parameter, custom node or function a `param.get`, `custom` or `function.call` node names."),
		  functionArg()},
		 addNode);
	edit("material.remove_nodes", "Delete nodes of a material graph and their links.",
		 {text("nodes", "Node identifiers, separated by spaces.", true), functionArg()}, removeNodes);
	edit("material.move_nodes", "Move nodes of a material graph.",
		 {text("layout", "`node x y` entries separated by `;`.", true), functionArg()}, moveNodes);
	edit("material.link", "Link an output pin to an input pin (the input loses its previous link).",
		 {text("from_node", "Source node.", true), text("from_pin", "Output pin of the source node.", true),
		  text("to_node", "Destination node.", true), text("to_pin", "Input pin of the destination node.", true),
		  functionArg()},
		 linkPins);
	edit("material.unlink", "Remove the link of an input pin.",
		 {text("to_node", "Destination node.", true), text("to_pin", "Input pin of the destination node.", true),
		  functionArg()},
		 unlinkPin);
	edit("material.set_node_value", "Set a setting or the value of an unlinked input of a node.",
		 {text("node", "The node.", true), text("name", "Setting or input.", true),
		  text("value", "The value: numbers separated by spaces, a choice or a name."),
		  flag("reset", "Give back the default value."), functionArg()},
		 setNodeValue);
	edit("material.paste_nodes",
		 "Paste copied nodes (with the parameters, custom nodes and functions they name and the graph lacks).",
		 {text("nodes", "The copied text.", true), number("x", "Canvas position of the top-left node."),
		  number("y", "Canvas position of the top-left node."), functionArg()},
		 pasteNodes);
	edit("material.add_parameter", "Expose a value of a material graph.",
		 {text("name", "Its name, a Slang identifier; a number is added when it is taken."),
		  {.name = "type",
		   .type = ArgType::Enum,
		   .description = "Kind of value.",
		   .defaultValue = ArgValue{std::string{"Float"}},
		   .choices = {"Float", "Vector", "Color", "Texture"}},
		  text("default", "Value: numbers separated by spaces, or the asset path of a texture.")},
		 addParameter);
	edit("material.remove_parameter", "Remove a parameter of a material graph.", {text("name", "The parameter.", true)},
		 removeParameter);
	edit("material.set_parameter", "Rename a parameter of a material graph, change its kind or its default value.",
		 {text("name", "The parameter.", true),
		  text("new_name", "Its new name (its nodes follow)."),
		  {.name = "type",
		   .type = ArgType::Enum,
		   .description = "Kind of value (the default resets with it).",
		   .choices = {"Float", "Vector", "Color", "Texture"}},
		  text("default", "Default value.")},
		 setParameter);
	edit("material.add_custom_node", "Add a Slang custom node to a material graph.",
		 {text("name", "Its name, a Slang identifier; a number is added when it is taken.")}, addCustomNode);
	edit("material.remove_custom_node", "Remove a custom node of a material graph.",
		 {text("name", "The custom node.", true)}, removeCustomNode);
	edit("material.set_custom_node", "Edit a custom node: name, title, typed pins, Slang body.",
		 {text("name", "The custom node.", true), text("new_name", "Its new name (its nodes follow)."),
		  text("title", "Title shown on its nodes."),
		  text("inputs", "Inputs, `name:Type=default` separated by commas."),
		  text("outputs", "Outputs, `name:Type` separated by commas."), text("code", "Slang body.")},
		 setCustomNode);
	edit("material.add_function", "Add a material function (a reusable sub-graph) to a material graph.",
		 {text("name", "Its name, a Slang identifier; a number is added when it is taken.")}, addFunction);
	edit("material.remove_function", "Remove a material function.", {text("name", "The function.", true)},
		 removeFunction);
	edit("material.set_function", "Rename a material function or change its typed inputs and outputs.",
		 {text("name", "The function.", true), text("new_name", "Its new name (its calls follow)."),
		  text("inputs", "Inputs, `name:Type=default` separated by commas."),
		  text("outputs", "Outputs, `name:Type` separated by commas.")},
		 setFunction);
	edit("material.set_graph_value", "Override a parameter of the parent graph in a material instance.",
		 {text("name", "The parameter.", true), text("value", "Its value."),
		  flag("reset", "Stop overriding it: the parent value applies.")},
		 setGraphValue);
}

}// namespace owl::nest::commands
