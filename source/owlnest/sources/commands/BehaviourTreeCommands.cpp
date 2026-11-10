/**
 * @file BehaviourTreeCommands.cpp
 * @author Silmaen
 * @date 10/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "BehaviourTreeCommands.h"

#include "CommandRegistry.h"
#include "DocumentCommands.h"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <map>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace owl::nest::commands {

BehaviourTreeEditor::~BehaviourTreeEditor() = default;

namespace {

using scene::ai::BehaviourTree;

auto fail(const CommandError iError, std::string iMessage) -> unexpected<CommandFailure> {
	return unexpected<CommandFailure>{CommandFailure{.error = iError, .message = std::move(iMessage)}};
}

auto unchanged() -> unexpected<CommandFailure> { return fail(CommandError::NotApplicable, "nothing changes"); }

// What a command does to the tree: the description of its undo entry, or why it cannot.
struct Edit {
	// Description of the undo entry.
	std::string description;
	// Whether it merges with the previous entry of the same description.
	bool merge{false};
	// Fields given back to the caller (`node`, `nodes`).
	ResultData data{};// NOLINT(readability-redundant-member-init) most edits give nothing back
};

using TreeEdit = auto (*)(BehaviourTree&, const CommandArgs&) -> expected<Edit, CommandFailure>;

auto isTreePath(const std::filesystem::path& iPath) -> bool {
	return iPath.extension() == scene::ai::g_BehaviourTreeExtension;
}

// Find the document of `path`, edit a copy of its tree, then hand it the result as one undo entry.
auto treeRunner(TreeEdit iEdit) -> CommandSpec::Runner {
	return [edit = iEdit](const CommandContext& iContext,
						  const CommandArgs& iArgs) -> expected<CommandResult, CommandFailure> {
		const std::filesystem::path path{iArgs.getText("path")};
		if (!isTreePath(path))
			return fail(CommandError::InvalidArgument, "`path` must end with `.owlbt`");
		auto* editor = iContext.documents != nullptr ? iContext.documents->openBehaviourTree(path) : nullptr;
		if (editor == nullptr)
			return fail(CommandError::NotApplicable, std::format("cannot open {} in an editor tab", path.string()));
		auto next = editor->tree();
		auto done = edit(next, iArgs);
		if (!done)
			return unexpected<CommandFailure>{done.error()};
		if (!editor->apply(std::move(next), done->description, done->merge))
			return unchanged();
		done->data.emplace("path", path.generic_string());
		return CommandResult{.entity = core::UUID{0}, .description = done->description, .data = std::move(done->data)};
	};
}

auto parseId(std::string_view iText) -> std::optional<core::UUID> {
	while (!iText.empty() && iText.front() == ' ') iText.remove_prefix(1);
	while (!iText.empty() && iText.back() == ' ') iText.remove_suffix(1);
	uint64_t value = 0;
	if (const auto [end, error] = std::from_chars(iText.data(), iText.data() + iText.size(), value);
		error != std::errc{} || end != iText.data() + iText.size() || iText.empty())
		return std::nullopt;
	return core::UUID{value};
}

auto nodeArg(const BehaviourTree& iTree, const CommandArgs& iArgs, const std::string& iName)
		-> expected<core::UUID, CommandFailure> {
	const auto text = iArgs.getText(iName);
	const auto id = parseId(text);
	if (!id || iTree.findNode(*id) == nullptr)
		return fail(CommandError::InvalidArgument, std::format("no node `{}`", text));
	return *id;
}

auto nodesArg(const BehaviourTree& iTree, const CommandArgs& iArgs)
		-> expected<std::vector<core::UUID>, CommandFailure> {
	std::vector<core::UUID> nodes;
	for (const auto part: std::string_view{iArgs.getText("nodes")} | std::views::split(',')) {
		const std::string_view text{part.begin(), part.end()};
		if (text.find_first_not_of(' ') == std::string_view::npos)
			continue;
		const auto id = parseId(text);
		if (!id || iTree.findNode(*id) == nullptr)
			return fail(CommandError::InvalidArgument, std::format("no node `{}`", text));
		if (!std::ranges::contains(nodes, *id))
			nodes.push_back(*id);
	}
	if (nodes.empty())
		return fail(CommandError::InvalidArgument, "`nodes` names no node");
	return nodes;
}

auto idsText(const std::vector<core::UUID>& iIds) -> std::string {
	std::string text;
	for (const auto id: iIds) text += std::format("{}{}", text.empty() ? "" : ",", static_cast<uint64_t>(id));
	return text;
}

auto kindOf(const BehaviourTree& iTree, const core::UUID iNode) -> std::optional<scene::ai::NodeKind> {
	const auto* node = iTree.findNode(iNode);
	const auto* type = node != nullptr ? scene::ai::findNodeType(node->type) : nullptr;
	return type != nullptr ? std::optional{type->kind} : std::nullopt;
}

// Whether `iNode` is `iAncestor` or below it.
auto isBelow(const BehaviourTree& iTree, core::UUID iNode, const core::UUID iAncestor) -> bool {
	for (size_t depth = 0; depth <= iTree.nodes.size(); ++depth) {
		if (iNode == iAncestor)
			return true;
		const auto parent = iTree.getParent(iNode);
		if (!parent)
			return false;
		iNode = *parent;
	}
	return true;
}

// Link a child under a parent: its former parent and, under a parent of one child, the former child are unlinked.
auto linkNodes(BehaviourTree& ioTree, const core::UUID iParent, const core::UUID iChild)
		-> expected<void, CommandFailure> {
	const auto parentKind = kindOf(ioTree, iParent);
	const auto childKind = kindOf(ioTree, iChild);
	if (!parentKind || !childKind)
		return fail(CommandError::InvalidArgument, "a node of unknown type cannot be linked");
	const size_t most = scene::ai::getMaxChildren(*parentKind);
	if (most == 0)
		return fail(CommandError::NotApplicable, "a condition or an action has no child");
	if (*childKind == scene::ai::NodeKind::Root)
		return fail(CommandError::NotApplicable, "the Root has no parent");
	if (isBelow(ioTree, iParent, iChild))
		return fail(CommandError::NotApplicable, "the link would make a node its own ancestor");
	std::erase_if(ioTree.links, [iChild](const scene::ai::TreeLink& iLink) -> bool { return iLink.child == iChild; });
	if (most == 1)
		std::erase_if(ioTree.links,
					  [iParent](const scene::ai::TreeLink& iLink) -> bool { return iLink.parent == iParent; });
	ioTree.links.push_back({.parent = iParent, .child = iChild});
	return {};
}

auto createTree(const CommandContext& /*iContext*/, const CommandArgs& iArgs)
		-> expected<CommandResult, CommandFailure> {
	const std::filesystem::path path{iArgs.getText("path")};
	if (!isTreePath(path))
		return fail(CommandError::InvalidArgument, "`path` must end with `.owlbt`");
	if (exists(path))
		return fail(CommandError::InvalidArgument, std::format("{} exists already", path.string()));
	BehaviourTree tree;
	const core::UUID root;
	const core::UUID selector;
	const core::UUID wait;
	tree.nodes.push_back({.id = root, .type = "root", .position = {0.f, 0.f}, .values = {}, .comment = {}});
	tree.nodes.push_back(
			{.id = selector, .type = "composite.selector", .position = {220.f, 0.f}, .values = {}, .comment = {}});
	tree.nodes.push_back({.id = wait, .type = "action.wait", .position = {440.f, 0.f}, .values = {}, .comment = {}});
	tree.links = {{.parent = root, .child = selector}, {.parent = selector, .child = wait}};
	if (!scene::ai::saveToFile(tree, path))
		return fail(CommandError::NotApplicable, std::format("cannot write {}", path.string()));
	return CommandResult{.entity = core::UUID{0},
						 .description = "Create behaviour tree",
						 .data = {{"path", path.generic_string()}}};
}

auto addNode(BehaviourTree& ioTree, const CommandArgs& iArgs) -> expected<Edit, CommandFailure> {
	const auto typeName = iArgs.getText("type");
	const auto* type = scene::ai::findNodeType(typeName);
	if (type == nullptr)
		return fail(CommandError::InvalidArgument, std::format("no node type `{}`", typeName));
	if (type->kind == scene::ai::NodeKind::Root && ioTree.getRoot() != nullptr)
		return fail(CommandError::NotApplicable, "the tree has a Root already");
	const auto position = iArgs.get<math::vec2>("position").value_or(math::vec2{0.f, 0.f});
	const core::UUID id;
	ioTree.nodes.push_back(
			{.id = id, .type = typeName, .position = {position.x(), position.y()}, .values = {}, .comment = {}});
	if (iArgs.has("parent")) {
		const auto parent = nodeArg(ioTree, iArgs, "parent");
		if (!parent)
			return unexpected<CommandFailure>{parent.error()};
		if (const auto linked = linkNodes(ioTree, *parent, id); !linked)
			return unexpected<CommandFailure>{linked.error()};
	}
	return Edit{.description = std::format("Add {}", type->title),
				.merge = false,
				.data = {{"node", std::format("{}", static_cast<uint64_t>(id))}}};
}

auto removeNodes(BehaviourTree& ioTree, const CommandArgs& iArgs) -> expected<Edit, CommandFailure> {
	const auto nodes = nodesArg(ioTree, iArgs);
	if (!nodes)
		return unexpected<CommandFailure>{nodes.error()};
	for (const auto id: *nodes) ioTree.removeNode(id);
	return Edit{.description = nodes->size() == 1 ? "Delete node" : std::format("Delete {} nodes", nodes->size())};
}

auto duplicateNodes(BehaviourTree& ioTree, const CommandArgs& iArgs) -> expected<Edit, CommandFailure> {
	const auto nodes = nodesArg(ioTree, iArgs);
	if (!nodes)
		return unexpected<CommandFailure>{nodes.error()};
	const auto offset = iArgs.get<math::vec2>("offset").value_or(math::vec2{40.f, 40.f});
	std::map<core::UUID, core::UUID> copies;
	std::vector<core::UUID> created;
	for (const auto id: *nodes) {
		auto copy = *ioTree.findNode(id);
		if (copy.type == "root")
			continue;
		copy.id = core::UUID{};
		copy.position = {copy.position[0] + offset.x(), copy.position[1] + offset.y()};
		copies[id] = copy.id;
		created.push_back(copy.id);
		ioTree.nodes.push_back(std::move(copy));
	}
	if (created.empty())
		return fail(CommandError::NotApplicable, "the Root cannot be duplicated");
	// The links inside the selection are copied with it.
	const auto links = ioTree.links;
	for (const auto& link: links)
		if (copies.contains(link.parent) && copies.contains(link.child))
			ioTree.links.push_back({.parent = copies[link.parent], .child = copies[link.child]});
	return Edit{.description =
						created.size() == 1 ? "Duplicate node" : std::format("Duplicate {} nodes", created.size()),
				.merge = false,
				.data = {{"nodes", idsText(created)}}};
}

auto moveNodes(BehaviourTree& ioTree, const CommandArgs& iArgs) -> expected<Edit, CommandFailure> {
	const auto nodes = nodesArg(ioTree, iArgs);
	if (!nodes)
		return unexpected<CommandFailure>{nodes.error()};
	const auto offset = iArgs.get<math::vec2>("offset").value_or(math::vec2{0.f, 0.f});
	for (const auto id: *nodes) {
		auto* node = ioTree.findNode(id);
		node->position = {node->position[0] + offset.x(), node->position[1] + offset.y()};
	}
	return Edit{.description = nodes->size() == 1 ? "Move node" : "Move nodes", .merge = iArgs.getBool("merge")};
}

auto link(BehaviourTree& ioTree, const CommandArgs& iArgs) -> expected<Edit, CommandFailure> {
	const auto parent = nodeArg(ioTree, iArgs, "parent");
	if (!parent)
		return unexpected<CommandFailure>{parent.error()};
	const auto child = nodeArg(ioTree, iArgs, "child");
	if (!child)
		return unexpected<CommandFailure>{child.error()};
	if (const auto linked = linkNodes(ioTree, *parent, *child); !linked)
		return unexpected<CommandFailure>{linked.error()};
	return Edit{.description = "Link nodes"};
}

auto unlink(BehaviourTree& ioTree, const CommandArgs& iArgs) -> expected<Edit, CommandFailure> {
	const auto child = nodeArg(ioTree, iArgs, "child");
	if (!child)
		return unexpected<CommandFailure>{child.error()};
	if (std::erase_if(ioTree.links,
					  [&child](const scene::ai::TreeLink& iLink) -> bool { return iLink.child == *child; }) == 0)
		return unchanged();
	return Edit{.description = "Unlink node"};
}

auto setValue(BehaviourTree& ioTree, const CommandArgs& iArgs) -> expected<Edit, CommandFailure> {
	const auto id = nodeArg(ioTree, iArgs, "node");
	if (!id)
		return unexpected<CommandFailure>{id.error()};
	auto* node = ioTree.findNode(*id);
	const auto* type = scene::ai::findNodeType(node->type);
	const auto name = iArgs.getText("key");
	const auto spec = type != nullptr ? std::ranges::find(type->params, name, &scene::ai::ParamSpec::name)
									  : std::vector<scene::ai::ParamSpec>::const_iterator{};
	if (type == nullptr || spec == type->params.end())
		return fail(CommandError::InvalidArgument, std::format("the node has no parameter `{}`", name));
	const auto value = iArgs.getText("value");
	if (spec->type == scene::ai::ParamType::Choice && !std::ranges::contains(spec->choices, value))
		return fail(CommandError::InvalidArgument, std::format("`{}` is not a choice of `{}`", value, name));
	node->values[name] = value;
	return Edit{.description = std::format("Set {}", name), .merge = iArgs.getBool("merge")};
}

auto setComment(BehaviourTree& ioTree, const CommandArgs& iArgs) -> expected<Edit, CommandFailure> {
	const auto id = nodeArg(ioTree, iArgs, "node");
	if (!id)
		return unexpected<CommandFailure>{id.error()};
	ioTree.findNode(*id)->comment = iArgs.getText("comment");
	return Edit{.description = "Set node comment", .merge = iArgs.getBool("merge")};
}

auto addKey(BehaviourTree& ioTree, const CommandArgs& iArgs) -> expected<Edit, CommandFailure> {
	const auto name = iArgs.getText("name");
	if (name.empty() || ioTree.findKey(name) != nullptr)
		return fail(CommandError::InvalidArgument, std::format("the key `{}` is empty or exists", name));
	const auto type = scene::ai::parseBlackboardType(iArgs.getText("type", "Number"));
	if (!type)
		return fail(CommandError::InvalidArgument, "unknown key type");
	auto value = scene::ai::getDefaultValue(*type);
	if (iArgs.has("value")) {
		const auto parsed = scene::ai::parseValue(*type, iArgs.getText("value"));
		if (!parsed)
			return fail(CommandError::InvalidArgument, std::format("`{}` is not a {}", iArgs.getText("value"),
																   scene::ai::getBlackboardTypeName(*type)));
		value = *parsed;
	}
	ioTree.blackboard.push_back({.name = name, .value = std::move(value), .description = iArgs.getText("description")});
	return Edit{.description = "Add blackboard key"};
}

auto setKey(BehaviourTree& ioTree, const CommandArgs& iArgs) -> expected<Edit, CommandFailure> {
	const auto name = iArgs.getText("name");
	const auto found = std::ranges::find(ioTree.blackboard, name, &scene::ai::BlackboardKey::name);
	if (found == ioTree.blackboard.end())
		return fail(CommandError::InvalidArgument, std::format("no key `{}`", name));
	if (iArgs.has("type")) {
		const auto type = scene::ai::parseBlackboardType(iArgs.getText("type"));
		if (!type)
			return fail(CommandError::InvalidArgument, "unknown key type");
		if (*type != scene::ai::getBlackboardType(found->value))
			found->value = scene::ai::getDefaultValue(*type);
	}
	if (iArgs.has("value")) {
		const auto parsed = scene::ai::parseValue(scene::ai::getBlackboardType(found->value), iArgs.getText("value"));
		if (!parsed)
			return fail(CommandError::InvalidArgument,
						std::format("`{}` does not fit the key", iArgs.getText("value")));
		found->value = *parsed;
	}
	if (iArgs.has("description"))
		found->description = iArgs.getText("description");
	if (iArgs.has("rename")) {
		const auto rename = iArgs.getText("rename");
		if (rename.empty() || (rename != name && ioTree.findKey(rename) != nullptr))
			return fail(CommandError::InvalidArgument, std::format("the key `{}` is empty or exists", rename));
		found->name = rename;
		// The nodes reading the key follow it.
		for (auto& node: ioTree.nodes) {
			const auto* type = scene::ai::findNodeType(node.type);
			if (type == nullptr)
				continue;
			for (const auto& spec: type->params)
				if (spec.type == scene::ai::ParamType::Key && BehaviourTree::getValue(node, spec.name) == name)
					node.values[spec.name] = rename;
		}
	}
	return Edit{.description = "Edit blackboard key", .merge = iArgs.getBool("merge")};
}

auto removeKey(BehaviourTree& ioTree, const CommandArgs& iArgs) -> expected<Edit, CommandFailure> {
	const auto name = iArgs.getText("name");
	if (std::erase_if(ioTree.blackboard,
					  [&name](const scene::ai::BlackboardKey& iKey) -> bool { return iKey.name == name; }) == 0)
		return fail(CommandError::InvalidArgument, std::format("no key `{}`", name));
	return Edit{.description = "Remove blackboard key"};
}

auto pathSpec() -> ArgSpec {
	return {.name = "path",
			.type = ArgType::Text,
			.required = true,
			.description = "The `.owlbt` file, opened in a tab when it is not.",
			.projectPath = true};
}

auto text(const char* iName, const char* iDescription, const bool iRequired = false) -> ArgSpec {
	return {.name = iName, .type = ArgType::Text, .required = iRequired, .description = iDescription};
}

auto vec2(const char* iName, const char* iDescription) -> ArgSpec {
	return {.name = iName, .type = ArgType::Vec2, .description = iDescription};
}

auto flag(const char* iName, const char* iDescription) -> ArgSpec {
	return {.name = iName, .type = ArgType::Bool, .description = iDescription, .defaultValue = ArgValue{false}};
}

auto keyTypes() -> ArgSpec {
	std::vector<std::string> names;
	names.reserve(scene::ai::g_BlackboardTypes.size());
	for (const auto type: scene::ai::g_BlackboardTypes) names.emplace_back(scene::ai::getBlackboardTypeName(type));
	return {.name = "type", .type = ArgType::Enum, .description = "Kind of the value.", .choices = std::move(names)};
}

}// namespace

void addBehaviourTreeCommands(CommandRegistry& ioRegistry) {
	const auto add = [&ioRegistry](const char* iName, const char* iDescription, std::vector<ArgSpec> iArgs,
								   TreeEdit iEdit) -> void {
		iArgs.insert(iArgs.begin(), pathSpec());
		ioRegistry.add({.name = iName,
						.description = iDescription,
						.args = std::move(iArgs),
						.factory = {},
						.run = treeRunner(iEdit),
						.allowedInPlay = true,
						.needsScene = false});
	};
	ioRegistry.add({.name = "bt.create",
					.description = "Create a `.owlbt` behaviour tree: a Root, a Selector and a Wait.",
					.args = {{.name = "path",
							  .type = ArgType::Text,
							  .required = true,
							  .description = "Tree file to write (`.owlbt`, must not exist).",
							  .projectPath = true}},
					.factory = {},
					.run = createTree,
					.allowedInPlay = true,
					.needsScene = false});
	const auto nodes = text("nodes", "Identifiers of the nodes (`12,34`).", true);
	add("bt.add_node", "Add a node to a behaviour tree, under a parent when one is given.",
		{text("type", "Node type (`composite.sequence`, `action.wait`...).", true),
		 vec2("position", "Top-left corner on the canvas."), text("parent", "Identifier of the parent node.")},
		addNode);
	add("bt.remove_nodes", "Remove nodes of a behaviour tree and their links.", {nodes}, removeNodes);
	add("bt.duplicate_nodes", "Duplicate nodes of a behaviour tree with the links between them (not the Root).",
		{nodes, vec2("offset", "Shift of the copies on the canvas (40, 40 when absent).")}, duplicateNodes);
	add("bt.move_nodes", "Move nodes on the canvas; the children of a node run from top to bottom.",
		{nodes, vec2("offset", "Shift on the canvas."), flag("merge", "Merge with the previous move (dragging).")},
		moveNodes);
	add("bt.link", "Make a node the child of another; a Root or a decorator keeps one child, the old one is unlinked.",
		{text("parent", "Identifier of the parent node.", true), text("child", "Identifier of the child node.", true)},
		link);
	add("bt.unlink", "Unlink a node from its parent.", {text("child", "Identifier of the node.", true)}, unlink);
	add("bt.set_value", "Set a parameter of a node.",
		{text("node", "Identifier of the node.", true), text("key", "Name of the parameter.", true),
		 text("value", "Value, as text.", true), flag("merge", "Merge with the previous edit (typing, dragging).")},
		setValue);
	add("bt.set_comment", "Set the note shown under the title of a node.",
		{text("node", "Identifier of the node.", true), text("comment", "The note; empty for none."),
		 flag("merge", "Merge with the previous edit (typing).")},
		setComment);
	add("bt.add_key", "Declare a key of the blackboard of a behaviour tree.",
		{text("name", "Name of the key.", true), keyTypes(), text("value", "Value at the start, as text."),
		 text("description", "One sentence shown in the editor.")},
		addKey);
	add("bt.set_key", "Edit a key of the blackboard; a new name is followed by the nodes reading it.",
		{text("name", "Name of the key.", true), text("rename", "New name."), keyTypes(),
		 text("value", "Value at the start, as text."), text("description", "One sentence shown in the editor."),
		 flag("merge", "Merge with the previous edit (typing).")},
		setKey);
	add("bt.remove_key", "Remove a key of the blackboard.", {text("name", "Name of the key.", true)}, removeKey);
}

}// namespace owl::nest::commands
