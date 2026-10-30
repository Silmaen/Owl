/**
 * @file DialogueCommands.cpp
 * @author Silmaen
 * @date 30/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "DialogueCommands.h"

#include "CommandRegistry.h"
#include "DocumentCommands.h"

#include <gui/widgets/NodeStyle.h>

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <iterator>
#include <map>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace owl::nest::commands {

DialogueEditor::~DialogueEditor() = default;

namespace {

using data::dialogue::Dialogue;
using data::dialogue::DialogueNode;
using data::dialogue::DialogueOption;
using data::dialogue::DialogueRoute;
using data::dialogue::NodeKind;

auto fail(const CommandError iError, std::string iMessage) -> unexpected<CommandFailure> {
	return unexpected<CommandFailure>{CommandFailure{.error = iError, .message = std::move(iMessage)}};
}

auto unchanged() -> unexpected<CommandFailure> { return fail(CommandError::NotApplicable, "nothing changes"); }

auto trimmed(std::string_view iText) -> std::string_view {
	while (!iText.empty() && iText.front() == ' ') iText.remove_prefix(1);
	while (!iText.empty() && iText.back() == ' ') iText.remove_suffix(1);
	return iText;
}

auto parseId(const std::string_view iText) -> std::optional<core::UUID> {
	const auto text = trimmed(iText);
	uint64_t value = 0;
	if (const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
		error != std::errc{} || end != text.data() + text.size())
		return std::nullopt;
	return core::UUID{value};
}

auto formatId(const core::UUID iId) -> std::string { return std::to_string(static_cast<uint64_t>(iId)); }

// What a command does to the dialogue: the description of its undo entry, or why it cannot.
struct Edit {
	// Description of the undo entry.
	std::string description;
	// Whether it merges with the previous entry of the same description.
	bool merge{false};
	// Fields given back to the caller (`node`, `nodes`).
	ResultData data{};// NOLINT(readability-redundant-member-init) most edits give nothing back
};

using DialogueEdit = auto (*)(Dialogue&, const CommandArgs&) -> expected<Edit, CommandFailure>;

// Find the document of `path`, edit a copy of its dialogue, then hand it the result as one undo entry.
auto dialogueRunner(DialogueEdit iEdit) -> CommandSpec::Runner {
	return [edit = iEdit](const CommandContext& iContext,
						  const CommandArgs& iArgs) -> expected<CommandResult, CommandFailure> {
		const std::filesystem::path path{iArgs.getText("path")};
		if (!Dialogue::isDialogue(path))
			return fail(CommandError::InvalidArgument, "`path` must end with `.owldlg`");
		auto* editor = iContext.documents != nullptr ? iContext.documents->openDialogue(path) : nullptr;
		if (editor == nullptr)
			return fail(CommandError::NotApplicable, std::format("cannot open {} in an editor tab", path.string()));
		auto next = editor->dialogue();
		auto done = edit(next, iArgs);
		if (!done)
			return unexpected<CommandFailure>{done.error()};
		if (!editor->apply(std::move(next), done->description, done->merge))
			return unchanged();
		done->data.emplace("path", path.generic_string());
		return CommandResult{.entity = core::UUID{0}, .description = done->description, .data = std::move(done->data)};
	};
}

auto nodeOf(Dialogue& ioDialogue, const CommandArgs& iArgs, const std::string& iName = "node")
		-> expected<DialogueNode*, CommandFailure> {
	const auto text = iArgs.getText(iName);
	const auto id = parseId(text);
	auto* node = id ? ioDialogue.findNode(*id) : nullptr;
	if (node == nullptr)
		return fail(CommandError::InvalidArgument, std::format("no node `{}`", text));
	return node;
}

auto nodesOf(const Dialogue& iDialogue, const CommandArgs& iArgs) -> expected<std::vector<core::UUID>, CommandFailure> {
	std::vector<core::UUID> ids;
	for (const auto part: std::string_view{iArgs.getText("nodes")} | std::views::split(',')) {
		const std::string_view text{part.begin(), part.end()};
		if (trimmed(text).empty())
			continue;
		const auto id = parseId(text);
		if (!id || iDialogue.findNode(*id) == nullptr)
			return fail(CommandError::InvalidArgument, std::format("no node `{}`", trimmed(text)));
		if (std::ranges::find(ids, *id) == ids.end())
			ids.push_back(*id);
	}
	if (ids.empty())
		return fail(CommandError::InvalidArgument, "`nodes` names no node");
	return ids;
}

auto optionOf(DialogueNode& ioNode, const CommandArgs& iArgs) -> expected<size_t, CommandFailure> {
	if (ioNode.kind != NodeKind::Choice)
		return fail(CommandError::InvalidArgument, "the node is not a choice");
	const auto index = iArgs.get<int64_t>("option").value_or(-1);
	if (index < 0 || std::cmp_greater_equal(index, ioNode.options.size()))
		return fail(CommandError::InvalidArgument, std::format("no option {}", index));
	return static_cast<size_t>(index);
}

// The bends of a node's outputs follow its options when they move: `iOrder[new] = old`.
void reorderRoutes(Dialogue& ioDialogue, const core::UUID iNode, const std::vector<size_t>& iOrder) {
	std::erase_if(ioDialogue.routes, [&](DialogueRoute& ioRoute) -> bool {
		if (ioRoute.node != iNode)
			return false;
		const auto found = std::ranges::find(iOrder, static_cast<size_t>(ioRoute.output));
		if (found == iOrder.end())
			return true;
		ioRoute.output = static_cast<uint32_t>(std::distance(iOrder.begin(), found));
		return false;
	});
}

auto createDialogue(const CommandContext& /*iContext*/, const CommandArgs& iArgs)
		-> expected<CommandResult, CommandFailure> {
	const std::filesystem::path path{iArgs.getText("path")};
	if (!Dialogue::isDialogue(path))
		return fail(CommandError::InvalidArgument, "`path` must end with `.owldlg`");
	if (exists(path))
		return fail(CommandError::InvalidArgument, std::format("{} exists already", path.string()));
	if (!Dialogue::makeDefault().saveToFile(path))
		return fail(CommandError::NotApplicable, std::format("cannot write {}", path.string()));
	return CommandResult{.entity = core::UUID{0},
						 .description = "Create dialogue",
						 .data = {{"path", path.generic_string()}}};
}

auto addNode(Dialogue& ioDialogue, const CommandArgs& iArgs) -> expected<Edit, CommandFailure> {
	const auto kind = data::dialogue::parseNodeKind(iArgs.getText("kind"));
	if (!kind)
		return fail(CommandError::InvalidArgument, std::format("no node kind `{}`", iArgs.getText("kind")));
	DialogueNode node;
	node.id = core::UUID{};
	node.kind = *kind;
	const auto position = iArgs.get<math::vec2>("position").value_or(math::vec2{0.f, 0.f});
	node.position = {position.x(), position.y()};
	if (*kind == NodeKind::Choice)
		node.options.push_back({.text = {.text = "Answer", .key = {}}, .condition = {}, .next = core::UUID{0}});
	if (iArgs.has("from")) {
		const auto from = nodeOf(ioDialogue, iArgs, "from");
		if (!from)
			return unexpected<CommandFailure>{from.error()};
		if (!Dialogue::setOutput(**from,
								 static_cast<size_t>(std::max<int64_t>(0, iArgs.get<int64_t>("output").value_or(0))),
								 node.id))
			return fail(CommandError::InvalidArgument, "the `from` node has no such output");
	}
	const auto id = formatId(node.id);
	ioDialogue.nodes.push_back(std::move(node));
	return Edit{.description = std::format("Add {} node", data::dialogue::getNodeKindName(*kind)),
				.merge = false,
				.data = {{"node", id}}};
}

auto removeNodes(Dialogue& ioDialogue, const CommandArgs& iArgs) -> expected<Edit, CommandFailure> {
	const auto ids = nodesOf(ioDialogue, iArgs);
	if (!ids)
		return unexpected<CommandFailure>{ids.error()};
	const auto removed = [&ids](const core::UUID iId) -> bool { return std::ranges::find(*ids, iId) != ids->end(); };
	std::erase_if(ioDialogue.nodes, [&removed](const DialogueNode& iNode) -> bool { return removed(iNode.id); });
	for (auto& node: ioDialogue.nodes) {
		for (size_t output = 0; output < Dialogue::getOutputCount(node); ++output) {
			if (!removed(Dialogue::getOutput(node, output)))
				continue;
			std::ignore = Dialogue::setOutput(node, output, core::UUID{0});
			std::erase_if(ioDialogue.routes, [&node, output](const DialogueRoute& iRoute) -> bool {
				return iRoute.node == node.id && iRoute.output == output;
			});
		}
	}
	std::erase_if(ioDialogue.routes, [&removed](const DialogueRoute& iRoute) -> bool { return removed(iRoute.node); });
	return Edit{.description = ids->size() > 1 ? "Remove nodes" : "Remove node"};
}

auto duplicateNodes(Dialogue& ioDialogue, const CommandArgs& iArgs) -> expected<Edit, CommandFailure> {
	const auto ids = nodesOf(ioDialogue, iArgs);
	if (!ids)
		return unexpected<CommandFailure>{ids.error()};
	std::map<uint64_t, core::UUID> copies;
	for (const auto id: *ids) copies.emplace(id, core::UUID{});
	const auto offset = iArgs.get<math::vec2>("offset").value_or(math::vec2{40.f, 40.f});
	std::vector<DialogueNode> added;
	std::vector<DialogueRoute> routes;
	for (const auto id: *ids) {
		auto copy = *ioDialogue.findNode(id);
		copy.id = copies.at(id);
		copy.position = {copy.position.x() + offset.x(), copy.position.y() + offset.y()};
		// A link inside the copied group follows the copies, a link out of it keeps its target.
		for (size_t output = 0; output < Dialogue::getOutputCount(copy); ++output)
			if (const auto found = copies.find(Dialogue::getOutput(copy, output)); found != copies.end())
				std::ignore = Dialogue::setOutput(copy, output, found->second);
		for (const auto& route: ioDialogue.routes) {
			if (route.node != id)
				continue;
			auto moved = route;
			moved.node = copy.id;
			for (auto& point: moved.points) point = {point.x() + offset.x(), point.y() + offset.y()};
			routes.push_back(std::move(moved));
		}
		added.push_back(std::move(copy));
	}
	std::string list;
	for (const auto& node: added) list += (list.empty() ? "" : ",") + formatId(node.id);
	ioDialogue.nodes.insert(ioDialogue.nodes.end(), added.begin(), added.end());
	ioDialogue.routes.insert(ioDialogue.routes.end(), routes.begin(), routes.end());
	return Edit{.description = ids->size() > 1 ? "Duplicate nodes" : "Duplicate node",
				.merge = false,
				.data = {{"nodes", list}}};
}

auto moveNodes(Dialogue& ioDialogue, const CommandArgs& iArgs) -> expected<Edit, CommandFailure> {
	const auto ids = nodesOf(ioDialogue, iArgs);
	if (!ids)
		return unexpected<CommandFailure>{ids.error()};
	const auto offset = iArgs.get<math::vec2>("offset").value_or(math::vec2{0.f, 0.f});
	for (const auto id: *ids) {
		auto* node = ioDialogue.findNode(id);
		node->position = {node->position.x() + offset.x(), node->position.y() + offset.y()};
	}
	return Edit{.description = ids->size() > 1 ? "Move nodes" : "Move node"};
}

auto linkNodes(Dialogue& ioDialogue, const CommandArgs& iArgs) -> expected<Edit, CommandFailure> {
	const auto from = nodeOf(ioDialogue, iArgs, "from");
	if (!from)
		return unexpected<CommandFailure>{from.error()};
	core::UUID target{0};
	if (const auto text = iArgs.getText("to"); !trimmed(text).empty()) {
		const auto to = nodeOf(ioDialogue, iArgs, "to");
		if (!to)
			return unexpected<CommandFailure>{to.error()};
		if ((*to)->kind == NodeKind::Start)
			return fail(CommandError::InvalidArgument, "no link leads into a Start node");
		target = (*to)->id;
	}
	const auto output = static_cast<size_t>(std::max<int64_t>(0, iArgs.get<int64_t>("output").value_or(0)));
	const auto id = (*from)->id;
	if (!Dialogue::setOutput(**from, output, target))
		return fail(CommandError::InvalidArgument, std::format("the node has no output {}", output));
	std::erase_if(ioDialogue.routes, [id, output](const DialogueRoute& iRoute) -> bool {
		return iRoute.node == id && iRoute.output == output;
	});
	return Edit{.description = target == core::UUID{0} ? "Unlink nodes" : "Link nodes"};
}

auto setNode(Dialogue& ioDialogue, const CommandArgs& iArgs) -> expected<Edit, CommandFailure> {
	const auto found = nodeOf(ioDialogue, iArgs);
	if (!found)
		return unexpected<CommandFailure>{found.error()};
	auto& node = **found;
	const auto set = [&iArgs](const char* iName, std::string& ioValue) -> void {
		if (iArgs.has(iName))
			ioValue = iArgs.getText(iName);
	};
	set("speaker", node.speaker.text);
	set("speaker_key", node.speaker.key);
	set("text", node.text.text);
	set("text_key", node.text.key);
	set("sound", node.sound);
	set("function", node.function);
	return Edit{.description = "Edit node", .merge = iArgs.getBool("merge")};
}

auto addOption(Dialogue& ioDialogue, const CommandArgs& iArgs) -> expected<Edit, CommandFailure> {
	const auto found = nodeOf(ioDialogue, iArgs);
	if (!found)
		return unexpected<CommandFailure>{found.error()};
	if ((*found)->kind != NodeKind::Choice)
		return fail(CommandError::InvalidArgument, "the node is not a choice");
	(*found)->options.push_back(
			{.text = {.text = iArgs.getText("text", "Answer"), .key = {}}, .condition = {}, .next = core::UUID{0}});
	return Edit{.description = "Add option"};
}

auto removeOption(Dialogue& ioDialogue, const CommandArgs& iArgs) -> expected<Edit, CommandFailure> {
	const auto found = nodeOf(ioDialogue, iArgs);
	if (!found)
		return unexpected<CommandFailure>{found.error()};
	auto& node = **found;
	const auto index = optionOf(node, iArgs);
	if (!index)
		return unexpected<CommandFailure>{index.error()};
	std::vector<size_t> order;
	for (size_t option = 0; option < node.options.size(); ++option)
		if (option != *index)
			order.push_back(option);
	node.options.erase(node.options.begin() + static_cast<std::ptrdiff_t>(*index));
	reorderRoutes(ioDialogue, node.id, order);
	return Edit{.description = "Remove option"};
}

auto moveOption(Dialogue& ioDialogue, const CommandArgs& iArgs) -> expected<Edit, CommandFailure> {
	const auto found = nodeOf(ioDialogue, iArgs);
	if (!found)
		return unexpected<CommandFailure>{found.error()};
	auto& node = **found;
	const auto index = optionOf(node, iArgs);
	if (!index)
		return unexpected<CommandFailure>{index.error()};
	const bool down = iArgs.getBool("down");
	if ((down && *index + 1 >= node.options.size()) || (!down && *index == 0))
		return unchanged();
	const size_t other = down ? *index + 1 : *index - 1;
	std::swap(node.options[*index], node.options[other]);
	std::vector<size_t> order(node.options.size());
	for (size_t option = 0; option < order.size(); ++option) order[option] = option;
	std::swap(order[*index], order[other]);
	reorderRoutes(ioDialogue, node.id, order);
	return Edit{.description = "Move option"};
}

auto setOption(Dialogue& ioDialogue, const CommandArgs& iArgs) -> expected<Edit, CommandFailure> {
	const auto found = nodeOf(ioDialogue, iArgs);
	if (!found)
		return unexpected<CommandFailure>{found.error()};
	const auto index = optionOf(**found, iArgs);
	if (!index)
		return unexpected<CommandFailure>{index.error()};
	auto& option = (*found)->options[*index];
	if (iArgs.has("text"))
		option.text.text = iArgs.getText("text");
	if (iArgs.has("text_key"))
		option.text.key = iArgs.getText("text_key");
	if (iArgs.has("condition"))
		option.condition = iArgs.getText("condition");
	return Edit{.description = "Edit option", .merge = iArgs.getBool("merge")};
}

// Replaces the route of an output; a route with no point and the default colour is not kept.
void storeRoute(Dialogue& ioDialogue, DialogueRoute iRoute) {
	std::erase_if(ioDialogue.routes, [&iRoute](const DialogueRoute& iOther) -> bool {
		return iOther.node == iRoute.node && iOther.output == iRoute.output;
	});
	if (!iRoute.points.empty() || !iRoute.color.empty())
		ioDialogue.routes.push_back(std::move(iRoute));
}

auto setRoute(Dialogue& ioDialogue, const CommandArgs& iArgs) -> expected<Edit, CommandFailure> {
	const auto found = nodeOf(ioDialogue, iArgs);
	if (!found)
		return unexpected<CommandFailure>{found.error()};
	const auto output = static_cast<uint32_t>(std::max<int64_t>(0, iArgs.get<int64_t>("output").value_or(0)));
	if (output >= Dialogue::getOutputCount(**found))
		return fail(CommandError::InvalidArgument, std::format("the node has no output {}", output));
	DialogueRoute route{.node = (*found)->id, .output = output, .points = {}};
	for (const auto part: std::string_view{iArgs.getText("points")} | std::views::split(';')) {
		const auto text = trimmed({part.begin(), part.end()});
		if (text.empty())
			continue;
		const auto comma = text.find(',');
		float x = 0.f;
		float y = 0.f;
		const auto first = trimmed(text.substr(0, comma));
		const auto second = comma == std::string_view::npos ? std::string_view{} : trimmed(text.substr(comma + 1));
		if (std::from_chars(first.data(), first.data() + first.size(), x).ec != std::errc{} ||
			std::from_chars(second.data(), second.data() + second.size(), y).ec != std::errc{})
			return fail(CommandError::InvalidArgument, std::format("`{}` is not a point `x,y`", text));
		route.points.emplace_back(x, y);
	}
	if (const auto* existing = ioDialogue.findRoute((*found)->id, output); existing != nullptr)
		route.color = existing->color;
	storeRoute(ioDialogue, std::move(route));
	return Edit{.description = "Edit link waypoints"};
}

auto setLinkColor(Dialogue& ioDialogue, const CommandArgs& iArgs) -> expected<Edit, CommandFailure> {
	const auto found = nodeOf(ioDialogue, iArgs);
	if (!found)
		return unexpected<CommandFailure>{found.error()};
	const auto output = static_cast<uint32_t>(std::max<int64_t>(0, iArgs.get<int64_t>("output").value_or(0)));
	if (output >= Dialogue::getOutputCount(**found))
		return fail(CommandError::InvalidArgument, std::format("the node has no output {}", output));
	if (Dialogue::getOutput(**found, output) == core::UUID{0})
		return fail(CommandError::NotApplicable, std::format("output {} of the node has no link", output));
	auto color = iArgs.getText("color");
	if (!color.empty() && !gui::widgets::findLinkColor(color).has_value())
		return fail(CommandError::InvalidArgument, std::format("`{}` is not a link colour", color));
	const auto* existing = ioDialogue.findRoute((*found)->id, output);
	DialogueRoute route{.node = (*found)->id,
						.output = output,
						.points = existing != nullptr ? existing->points : std::vector<math::vec2f>{},
						.color = std::move(color)};
	storeRoute(ioDialogue, std::move(route));
	return Edit{.description = "Set link colour"};
}

auto pathSpec() -> ArgSpec {
	return {.name = "path",
			.type = ArgType::Text,
			.required = true,
			.description = "The `.owldlg` file, opened in a tab when it is not.",
			.projectPath = true};
}

auto text(const char* iName, const char* iDescription, const bool iRequired = false) -> ArgSpec {
	return {.name = iName, .type = ArgType::Text, .required = iRequired, .description = iDescription};
}

auto integer(const char* iName, const char* iDescription, const bool iRequired = false) -> ArgSpec {
	return {.name = iName, .type = ArgType::Integer, .required = iRequired, .description = iDescription};
}

auto flag(const char* iName, const char* iDescription) -> ArgSpec {
	return {.name = iName, .type = ArgType::Bool, .description = iDescription, .defaultValue = ArgValue{false}};
}

auto kinds() -> std::vector<std::string> {
	std::vector<std::string> names;
	for (const auto kind: data::dialogue::getNodeKinds()) names.emplace_back(data::dialogue::getNodeKindName(kind));
	return names;
}

}// namespace

void addDialogueCommands(CommandRegistry& ioRegistry) {
	const auto add = [&ioRegistry](const char* iName, const char* iDescription, std::vector<ArgSpec> iArgs,
								   DialogueEdit iEdit) -> void {
		iArgs.insert(iArgs.begin(), pathSpec());
		ioRegistry.add({.name = iName,
						.description = iDescription,
						.args = std::move(iArgs),
						.factory = {},
						.run = dialogueRunner(iEdit),
						.allowedInPlay = true,
						.needsScene = false});
	};
	ioRegistry.add({.name = "dialogue.create",
					.description = "Create a `.owldlg` dialogue holding a Start node linked to an End node.",
					.args = {{.name = "path",
							  .type = ArgType::Text,
							  .required = true,
							  .description = "Dialogue file to write (`.owldlg`, must not exist).",
							  .projectPath = true}},
					.factory = {},
					.run = createDialogue,
					.allowedInPlay = true,
					.needsScene = false});
	add("dialogue.add_node", "Add a node to a dialogue, linked from an output of another node when `from` is given.",
		{{.name = "kind",
		  .type = ArgType::Enum,
		  .required = true,
		  .description = "What the node does.",
		  .choices = kinds()},
		 {.name = "position", .type = ArgType::Vec2, .description = "Top-left corner in the graph."},
		 text("from", "Id of the node whose output leads to the new one."),
		 integer("output", "Output of `from` (0 the first; a Branch: 0 true, 1 false; a Choice: its option).")},
		addNode);
	add("dialogue.remove_nodes", "Remove nodes of a dialogue and the links leading to them.",
		{text("nodes", "Ids of the nodes (`a,b`).", true)}, removeNodes);
	add("dialogue.duplicate_nodes", "Copy nodes of a dialogue, with the links between them, beside the originals.",
		{text("nodes", "Ids of the nodes (`a,b`).", true),
		 {.name = "offset", .type = ArgType::Vec2, .description = "Shift of the copies (40, 40 when absent)."}},
		duplicateNodes);
	add("dialogue.move_nodes", "Move nodes of a dialogue in the graph.",
		{text("nodes", "Ids of the nodes (`a,b`).", true),
		 {.name = "offset", .type = ArgType::Vec2, .required = true, .description = "Shift of the nodes."}},
		moveNodes);
	add("dialogue.link", "Link an output of a node to another node, or unlink it.",
		{text("from", "Id of the node the link leaves.", true),
		 integer("output", "Its output (0 the first; a Branch: 0 true, 1 false; a Choice: its option)."),
		 text("to", "Id of the node it leads to; empty unlinks the output.")},
		linkNodes);
	add("dialogue.set_node", "Edit the texts, sound or function of a node of a dialogue; absent fields are kept.",
		{text("node", "Id of the node.", true), text("speaker", "Who speaks."),
		 text("speaker_key", "Localisation key of the speaker."), text("text", "What is said."),
		 text("text_key", "Localisation key of the text."), text("sound", "Voice line played with the text."),
		 text("function", "Lua function: the condition of a Branch, the action of an Event."),
		 flag("merge", "Merge with the previous edit of the node (typing).")},
		setNode);
	add("dialogue.add_option", "Add an answer at the end of a Choice node.",
		{text("node", "Id of the Choice node.", true), text("text", "Text of the answer.")}, addOption);
	add("dialogue.remove_option", "Remove an answer of a Choice node.",
		{text("node", "Id of the Choice node.", true), integer("option", "Index of the answer, from 0.", true)},
		removeOption);
	add("dialogue.move_option", "Move an answer of a Choice node one place up or down.",
		{text("node", "Id of the Choice node.", true), integer("option", "Index of the answer, from 0.", true),
		 flag("down", "Move down instead of up.")},
		moveOption);
	add("dialogue.set_option", "Edit an answer of a Choice node; absent fields are kept.",
		{text("node", "Id of the Choice node.", true), integer("option", "Index of the answer, from 0.", true),
		 text("text", "Text of the answer."), text("text_key", "Localisation key of the text."),
		 text("condition", "Lua function returning true when the answer is offered; empty: always."),
		 flag("merge", "Merge with the previous edit of the answer (typing).")},
		setOption);
	add("dialogue.set_route", "Set the bends of the link of an output; no point makes it straight.",
		{text("node", "Id of the node the link leaves.", true), integer("output", "Its output."),
		 text("points", "Waypoints `x,y;x,y` in graph space.")},
		setRoute);
	add("dialogue.set_link_color",
		"Give the link of an output a colour of the graph palette; empty restores the default.",
		{text("node", "Id of the node the link leaves.", true), integer("output", "Its output."),
		 text("color", "Colour name (Red, Orange, Yellow, Green, Teal, Blue, Violet, Pink), empty for the default.")},
		setLinkColor);
}

}// namespace owl::nest::commands
