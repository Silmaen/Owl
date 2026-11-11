/**
 * @file VisualScriptDocument_test.cpp
 * @author Silmaen
 * @date 27/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "document/VisualScriptDocument.h"
#include "AssetKind.h"
#include "testHelper.h"

#include <core/Log.h>
#include <platform/AtomicFile.h>
#include <platform/FileUtils.h>
#include <script/ScriptDebugger.h>
#include <script/visual/VisualScript.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

using namespace owl;
using namespace owl::nest;
namespace vs = owl::script::visual;

namespace {

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wweak-vtables")
class VisualScriptDocumentTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_root = std::filesystem::temp_directory_path() / "owl_visual_script_document_test";
		std::filesystem::remove_all(m_root);
		std::filesystem::create_directories(m_root);
		m_file = m_root / "door.owlvs";
		ASSERT_TRUE(platform::writeFileAtomic(m_file, vs::serialize(VisualScriptDocument::starterScript())));
		ASSERT_TRUE(m_document.loadFromFile(m_file));
	}

	void TearDown() override {
		std::filesystem::remove_all(m_root);
		core::Log::invalidate();
	}

	std::filesystem::path m_root;
	std::filesystem::path m_file;
	VisualScriptDocument m_document;
};
OWL_DIAG_POP

}// namespace

TEST_F(VisualScriptDocumentTest, LoadsAndClassifiesAsGraph) {
	EXPECT_EQ(classifyAsset(m_file), AssetKind::NodeGraph);
	EXPECT_EQ(m_document.getScript().nodes.size(), 2u);
	EXPECT_FALSE(m_document.isDirty());
	EXPECT_EQ(m_document.title(), "door.owlvs");
}

TEST_F(VisualScriptDocumentTest, AddNodeIsUndoable) {
	const auto id = m_document.addNode("flow.branch", {10.f, 20.f});
	ASSERT_NE(id, 0u);
	EXPECT_EQ(m_document.getScript().nodes.size(), 3u);
	EXPECT_TRUE(m_document.isDirty());
	EXPECT_EQ(m_document.addNode("no.such.node", {0.f, 0.f}), 0u);
	m_document.performUndo();
	EXPECT_EQ(m_document.getScript().nodes.size(), 2u);
	EXPECT_FALSE(m_document.isDirty());
	m_document.performRedo();
	EXPECT_NE(m_document.getScript().findNode(id), nullptr);
}

TEST_F(VisualScriptDocumentTest, DeleteGroupRemovesNodesAndLinks) {
	auto script = m_document.getScript();
	const auto event = script.nodes.front().id;
	const auto branch = m_document.addNode("flow.branch", {0.f, 0.f});
	script = m_document.getScript();
	script.links.push_back({.fromNode = event, .fromPin = "Then", .toNode = branch, .toPin = "Exec", .waypoints = {}});
	ASSERT_TRUE(m_document.commit(script, "Add Link"));
	const std::array group{event, branch};
	m_document.deleteNodes(group);
	EXPECT_EQ(m_document.getScript().nodes.size(), 1u);
	EXPECT_TRUE(m_document.getScript().links.empty());
	m_document.performUndo();
	EXPECT_EQ(m_document.getScript().links.size(), 1u);
}

TEST_F(VisualScriptDocumentTest, CopyPasteKeepsInternalLinksWithFreshIds) {
	auto script = m_document.getScript();
	script.variables.push_back({.name = "speed",
								.type = script::ScriptPropertyType::Float,
								.defaultValue = "2",
								.min = {},
								.max = {},
								.step = {},
								.exposed = true});
	ASSERT_TRUE(m_document.commit(script, "Add Variable"));
	const auto event = m_document.getScript().nodes.front().id;
	const auto set = m_document.addNode("var.set", {0.f, 0.f});
	script = m_document.getScript();
	script.links.push_back({.fromNode = event, .fromPin = "Then", .toNode = set, .toPin = "Exec", .waypoints = {}});
	ASSERT_TRUE(m_document.commit(script, "Add Link"));
	const std::array group{event, set};
	const auto text = m_document.copyNodes(group);

	VisualScriptDocument other;
	const auto pasted = other.pasteNodes(text, {100.f, 50.f});
	ASSERT_EQ(pasted.size(), 2u);
	const auto& result = other.getScript();
	EXPECT_EQ(result.links.size(), 1u);
	EXPECT_NE(result.findVariable("speed"), nullptr);
	EXPECT_EQ(result.findNode(event), nullptr);
	EXPECT_FLOAT_EQ(result.findNode(pasted.front())->position[0], 100.f);
	EXPECT_EQ(result.findNode(pasted.back())->values.at("variable"), "speed");
}

TEST_F(VisualScriptDocumentTest, MovesMergeIntoOneUndoEntry) {
	auto script = m_document.getScript();
	script.nodes.front().position = {5.f, 5.f};
	ASSERT_TRUE(m_document.commit(script, "Move Nodes", true));
	script.nodes.front().position = {9.f, 9.f};
	ASSERT_TRUE(m_document.commit(script, "Move Nodes", true));
	m_document.performUndo();
	EXPECT_FLOAT_EQ(m_document.getScript().nodes.front().position[0], 0.f);
	EXPECT_FALSE(m_document.commit(m_document.getScript(), "Nothing"));
}

TEST_F(VisualScriptDocumentTest, SaveWritesTheGraph) {
	m_document.addNode("value.self", {0.f, 0.f});
	const auto copy = m_root / "copy.owlvs";
	ASSERT_TRUE(m_document.saveAs(copy));
	EXPECT_FALSE(m_document.isDirty());
	VisualScriptDocument reloaded;
	ASSERT_TRUE(reloaded.loadFromFile(copy));
	EXPECT_EQ(reloaded.getScript(), m_document.getScript());
	const auto snapshot = m_document.recoverySnapshot();
	ASSERT_TRUE(snapshot.has_value());
	EXPECT_TRUE(reloaded.restoreRecoverySnapshot(*snapshot));
	EXPECT_FALSE(reloaded.restoreRecoverySnapshot("not: [yaml"));
}

TEST_F(VisualScriptDocumentTest, AddNodeLinksTheDroppedPin) {
	const auto event = m_document.getScript().nodes.front().id;
	const VisualScriptPinRef from{.node = event, .pin = "Then", .output = true, .type = vs::PinType::Exec};
	const auto branch = m_document.addNode("flow.branch", {200.f, 0.f}, from);
	ASSERT_EQ(m_document.getScript().links.size(), 1u);
	EXPECT_EQ(m_document.getScript().links.front().toNode, branch);
	EXPECT_EQ(m_document.getScript().links.front().toPin, "Exec");
	EXPECT_EQ(m_document.getSelection(), std::vector{branch});
	const auto get = m_document.addNode("var.get|speed", {0.f, 0.f});
	EXPECT_EQ(m_document.getScript().findNode(get)->values.at("variable"), "speed");
}

TEST_F(VisualScriptDocumentTest, SplitPinKeepsTheValueAndDropsItsLinks) {
	OWL_REQUIRE_MODULE(SCRIPT);
	const auto set = m_document.addNode("call.transform.set_position", {0.f, 0.f});
	const auto get = m_document.addNode("call.transform.get_position", {0.f, 200.f});
	auto script = m_document.getScript();
	script.findNode(set)->values["xyz"] = "1, 2, 3";
	script.links.push_back({.fromNode = get, .fromPin = "xyz", .toNode = set, .toPin = "xyz", .waypoints = {}});
	ASSERT_TRUE(m_document.commit(script, "Setup"));
	m_document.setPinSplit(set, "xyz", true);
	const auto* node = m_document.getScript().findNode(set);
	EXPECT_EQ(node->values.at("y"), "2");
	EXPECT_FALSE(node->values.contains("xyz"));
	EXPECT_TRUE(m_document.getScript().links.empty());
	m_document.setPinSplit(set, "xyz", false);
	EXPECT_EQ(m_document.getScript().findNode(set)->values.at("xyz"), "1, 2, 3");
	m_document.performUndo();
	m_document.performUndo();
	EXPECT_EQ(m_document.getScript().links.size(), 1u);
}

TEST_F(VisualScriptDocumentTest, ViewModeStartsOnTheGraph) {
	EXPECT_EQ(m_document.getViewMode(), VisualScriptViewMode::Graph);
	m_document.setViewMode(VisualScriptViewMode::Split);
	EXPECT_EQ(m_document.getViewMode(), VisualScriptViewMode::Split);
}

TEST_F(VisualScriptDocumentTest, NodeBreakpointsBecomeLuaLineBreakpoints) {
	const auto event = m_document.getScript().nodes.front().id;
	const VisualScriptPinRef from{.node = event, .pin = "Then", .output = true, .type = vs::PinType::Exec};
	const auto branch = m_document.addNode("flow.branch", {200.f, 0.f}, from);
	m_document.toggleBreakpoint(branch);
	EXPECT_TRUE(m_document.getBreakpoints().contains(branch));
	EXPECT_TRUE(m_document.getDebugView().breakpoints.contains(branch));
	const auto lines = script::ScriptDebugger::getBreakpoints(m_file.string());
	ASSERT_FALSE(lines.empty());
	EXPECT_EQ(m_document.findNodeAtLine(lines.front()), branch);
	EXPECT_FALSE(m_document.findNodeAtLine(100000).has_value());

	m_document.setDebugState({{.name = vs::getProbeKey(event, "Then"), .type = "number", .value = "1", .entries = {}}},
							 lines.front());
	EXPECT_EQ(m_document.getDebugView().stopped, branch);
	EXPECT_EQ(m_document.getDebugView().values.size(), 1u);
	m_document.setDebugState({}, std::nullopt);
	EXPECT_FALSE(m_document.getDebugView().stopped.has_value());

	m_document.toggleBreakpoint(branch);
	EXPECT_TRUE(script::ScriptDebugger::getBreakpoints(m_file.string()).empty());
	m_document.toggleBreakpoint(branch);
	m_document.deleteNodes(std::vector{branch});
	std::ignore = m_document.findNodeAtLine(1);
	EXPECT_TRUE(m_document.getBreakpoints().empty());
	EXPECT_TRUE(script::ScriptDebugger::getBreakpoints(m_file.string()).empty());
	script::ScriptDebugger::clear();
}

TEST_F(VisualScriptDocumentTest, FunctionsAreAddedRenamedAndRemovedWithTheirNodes) {
	const auto name = m_document.addFunction(false);
	EXPECT_EQ(name, "function");
	ASSERT_NE(m_document.getScript().findFunction(name), nullptr);
	EXPECT_EQ(m_document.getScript().nodes.size(), 4u);
	EXPECT_EQ(m_document.addFunction(true), "macro");
	EXPECT_EQ(m_document.getScript().findFunction("macro")->exits.size(), 2u);

	auto signature = *m_document.getScript().findFunction(name);
	signature.inputs.push_back({.name = "speed", .type = vs::PinType::Number});
	m_document.setFunction(name, signature);
	const auto call = m_document.addNode("func.call|function", {0.f, 0.f});
	const auto get = m_document.addNode("value.number", {0.f, 100.f});
	auto script = m_document.getScript();
	script.links.push_back({.fromNode = get, .fromPin = "Result", .toNode = call, .toPin = "speed", .waypoints = {}});
	ASSERT_TRUE(m_document.commit(script, "Link"));
	signature.inputs.front().name = "pace";
	m_document.setFunction(name, signature);
	EXPECT_TRUE(std::ranges::any_of(m_document.getScript().links,
									[&](const vs::GraphLink& iLink) -> bool { return iLink.toPin == "pace"; }));
	signature.inputs.clear();
	m_document.setFunction(name, signature);
	EXPECT_TRUE(std::ranges::none_of(m_document.getScript().links,
									 [&](const vs::GraphLink& iLink) -> bool { return iLink.toNode == call; }));

	m_document.renameFunction(name, "move");
	EXPECT_EQ(m_document.getScript().findNode(call)->values.at("function"), "move");
	m_document.removeFunction("move");
	EXPECT_EQ(m_document.getScript().findFunction("move"), nullptr);
	EXPECT_EQ(m_document.getScript().findNode(call), nullptr);
	m_document.performUndo();
	EXPECT_NE(m_document.getScript().findFunction("move"), nullptr);
}

TEST_F(VisualScriptDocumentTest, CustomNodesAreEditedWithTheirCalls) {
	const auto name = m_document.addCustomNode();
	EXPECT_EQ(name, "custom");
	ASSERT_NE(m_document.getScript().findCustomNode(name), nullptr);
	ASSERT_EQ(m_document.getScript().nodes.size(), 3u);
	const auto call = m_document.getScript().nodes.back().id;
	EXPECT_EQ(m_document.getScript().findNode(call)->type, "custom.pure");
	const auto set = m_document.addNode("call.log.info", {200.f, 0.f});
	auto script = m_document.getScript();
	script.links.push_back({.fromNode = call, .fromPin = "result", .toNode = set, .toPin = "message", .waypoints = {}});
	ASSERT_TRUE(m_document.commit(script, "Link"));

	auto custom = *m_document.getScript().findCustomNode(name);
	custom.outputs.front().name = "doubled";
	custom.code = "return value * 2";
	m_document.setCustomNode(name, custom);
	EXPECT_TRUE(std::ranges::any_of(m_document.getScript().links,
									[&](const vs::GraphLink& iLink) -> bool { return iLink.fromPin == "doubled"; }));
	custom.pure = false;
	m_document.setCustomNode(name, custom);
	EXPECT_EQ(m_document.getScript().findNode(call)->type, "custom.call");
	EXPECT_EQ(vs::getNodePins(m_document.getScript(), *m_document.getScript().findNode(call)).inputs.front().type,
			  vs::PinType::Exec);
	custom.outputs.clear();
	m_document.setCustomNode(name, custom);
	EXPECT_TRUE(std::ranges::none_of(m_document.getScript().links,
									 [&](const vs::GraphLink& iLink) -> bool { return iLink.fromNode == call; }));

	m_document.renameCustomNode(name, "twice");
	EXPECT_EQ(m_document.getScript().findNode(call)->values.at("custom"), "twice");
	m_document.removeCustomNode("twice");
	EXPECT_TRUE(m_document.getScript().customNodes.empty());
	EXPECT_EQ(m_document.getScript().findNode(call), nullptr);
	m_document.performUndo();
	EXPECT_NE(m_document.getScript().findCustomNode("twice"), nullptr);
}

TEST_F(VisualScriptDocumentTest, PaletteKeysCreateCustomAndLuaNodes) {
	const auto name = m_document.addCustomNode();
	const auto custom = m_document.addNode("custom.pure|" + name, {0.f, 0.f});
	EXPECT_EQ(m_document.getScript().findNode(custom)->values.at("custom"), name);
	const auto lua = m_document.addNode("lua.call|scripts.lib.maths|clamp", {0.f, 0.f});
	const auto* node = m_document.getScript().findNode(lua);
	ASSERT_NE(node, nullptr);
	EXPECT_EQ(node->values.at("module"), "scripts.lib.maths");
	EXPECT_EQ(node->values.at("function"), "clamp");
}

TEST_F(VisualScriptDocumentTest, EnsureCustomEventAddsTheHandlerOnce) {
	const auto first = m_document.ensureCustomEvent("on_pause");
	ASSERT_NE(m_document.getScript().findNode(first), nullptr);
	EXPECT_EQ(m_document.getScript().findNode(first)->values.at("function"), "on_pause");
	EXPECT_EQ(m_document.ensureCustomEvent("on_pause"), first);
	EXPECT_EQ(m_document.getSelection(), std::vector{first});
	EXPECT_EQ(m_document.getScript().nodes.size(), 3u);
}

TEST_F(VisualScriptDocumentTest, ListItemCountDropsTheLinksOfTheLostPins) {
	const auto make = m_document.addNode("table.make_list", {0.f, 0.f});
	const auto number = m_document.addNode("value.number", {-200.f, 0.f});
	auto next = m_document.getScript();
	next.links.push_back({.fromNode = number, .fromPin = "Result", .toNode = make, .toPin = "D", .waypoints = {}});
	next.findNode(make)->values["C"] = "3";
	ASSERT_TRUE(m_document.commit(std::move(next), "Link"));
	m_document.setListItemCount(make, 2);
	const auto& script = m_document.getScript();
	EXPECT_EQ(vs::getListItemCount(*script.findNode(make)), 2u);
	EXPECT_TRUE(std::ranges::none_of(script.links,
									 [&](const vs::GraphLink& iLink) -> bool { return iLink.toNode == make; }));
	EXPECT_FALSE(script.findNode(make)->values.contains("C"));
	m_document.setListItemCount(make, 40);
	EXPECT_EQ(vs::getListItemCount(*m_document.getScript().findNode(make)), vs::g_MaxListItems);
}

TEST_F(VisualScriptDocumentTest, DebugStateKeepsTheEntriesOfTables) {
	const auto event = m_document.getScript().nodes.front().id;
	const auto key = vs::getProbeKey(event, "List");
	script::WatchValue list{.name = key, .type = "table", .value = "{1, 2}", .entries = {}};
	list.entries.push_back({.name = "1", .value = "1"});
	list.entries.push_back({.name = "2", .value = "2"});
	m_document.setDebugState({list}, std::nullopt);
	const auto& entries = m_document.getDebugView().entries;
	ASSERT_TRUE(entries.contains(key));
	EXPECT_EQ(entries.at(key).size(), 2u);
	EXPECT_EQ(entries.at(key).back().second, "2");
}

TEST_F(VisualScriptDocumentTest, GroupsFrameMoveWithAndDeleteTheirNodes) {
	const auto first = m_document.addNode("math.add", {0.f, 0.f});
	const auto second = m_document.addNode("math.add", {200.f, 100.f});
	const auto far = m_document.addNode("math.add", {2000.f, 2000.f});
	const auto group = m_document.groupNodes(std::vector{first, second}, "Sum");
	ASSERT_NE(group, 0u);
	ASSERT_EQ(m_document.getScript().groups.size(), 1u);
	const auto frame = m_document.getScript().groups.front();
	EXPECT_EQ(frame.title, "Sum");
	EXPECT_TRUE(frame.holds(*m_document.getScript().findNode(first)));
	EXPECT_TRUE(frame.holds(*m_document.getScript().findNode(second)));
	EXPECT_FALSE(frame.holds(*m_document.getScript().findNode(far)));
	auto renamed = frame;
	renamed.title = "Total";
	renamed.color = "Green";
	m_document.setGroup(renamed);
	EXPECT_EQ(m_document.getScript().groups.front().color, "Green");
	m_document.removeGroup(group, true);
	EXPECT_TRUE(m_document.getScript().groups.empty());
	EXPECT_EQ(m_document.getScript().findNode(first), nullptr);
	EXPECT_NE(m_document.getScript().findNode(far), nullptr);
	m_document.performUndo();
	EXPECT_NE(m_document.getScript().findNode(first), nullptr);
	m_document.removeGroup(group, false);
	EXPECT_NE(m_document.getScript().findNode(first), nullptr);
	EXPECT_TRUE(m_document.getScript().groups.empty());
}

TEST_F(VisualScriptDocumentTest, PureNodesBecomeAPureFunction) {
	auto next = vs::VisualScript{};
	next.variables.push_back({.name = "result",
							  .type = script::ScriptPropertyType::Float,
							  .defaultValue = "0",
							  .min = {},
							  .max = {},
							  .step = {},
							  .exposed = true});
	const vs::GraphNode create{.id = core::UUID{1}, .type = "event.on_create", .position = {0.f, 0.f}, .values = {}};
	const vs::GraphNode two{.id = core::UUID{2},
							.type = "value.number",
							.position = {0.f, 200.f},
							.values = {{"Value", "2"}}};
	const vs::GraphNode add{.id = core::UUID{3},
							.type = "math.add",
							.position = {200.f, 200.f},
							.values = {{"B", "3"}}};
	const vs::GraphNode set{.id = core::UUID{4},
							.type = "var.set",
							.position = {400.f, 0.f},
							.values = {{"variable", "result"}}};
	next.nodes = {create, two, add, set};
	next.links = {{.fromNode = create.id, .fromPin = "Then", .toNode = set.id, .toPin = "Exec", .waypoints = {}},
				  {.fromNode = two.id, .fromPin = "Result", .toNode = add.id, .toPin = "A", .waypoints = {}},
				  {.fromNode = add.id, .fromPin = "Result", .toNode = set.id, .toPin = "Value", .waypoints = {}}};
	ASSERT_TRUE(m_document.commit(std::move(next), "Graph"));
	const auto name = m_document.collapseToFunction(std::vector{add.id});
	ASSERT_FALSE(name.empty());
	const auto& script = m_document.getScript();
	const auto* function = script.findFunction(name);
	ASSERT_NE(function, nullptr);
	EXPECT_TRUE(function->pure);
	EXPECT_EQ(function->inputs.size(), 1u);
	EXPECT_EQ(function->outputs.size(), 1u);
	EXPECT_TRUE(std::ranges::any_of(script.nodes,
									[](const vs::GraphNode& iNode) -> bool { return iNode.type == "func.pure"; }));
	EXPECT_EQ(script.groups.size(), 1u);
	const auto lua = vs::compile(script);
	ASSERT_TRUE(lua.has_value()) << (lua.has_value() ? std::string{} : lua.error().front().message);
	EXPECT_NE(lua->find(name + "("), std::string::npos);
}

TEST_F(VisualScriptDocumentTest, ExecutionNodesBecomeAFunctionWithOneEntry) {
	const auto event = m_document.getScript().nodes.front().id;
	const VisualScriptPinRef from{.node = event, .pin = "Then", .output = true, .type = vs::PinType::Exec};
	const auto branch = m_document.addNode("flow.branch", {300.f, 0.f}, from);
	EXPECT_TRUE(m_document.collapseToFunction(std::vector{event}).empty());
	const auto name = m_document.collapseToFunction(std::vector{branch});
	ASSERT_FALSE(name.empty());
	const auto& script = m_document.getScript();
	EXPECT_FALSE(script.findFunction(name)->pure);
	const auto call = std::ranges::find(script.nodes, std::string{"func.call"}, &vs::GraphNode::type);
	ASSERT_NE(call, script.nodes.end());
	EXPECT_TRUE(std::ranges::any_of(script.links, [&](const vs::GraphLink& iLink) -> bool {
		return iLink.fromNode == event && iLink.toNode == call->id && iLink.toPin == "Exec";
	}));
	EXPECT_TRUE(vs::compile(script).has_value());
}

TEST_F(VisualScriptDocumentTest, FunctionsMoveToALibraryTheirCallsThenRead) {
	const auto name = m_document.addFunction(false);
	auto next = m_document.getScript();
	const vs::GraphNode call{.id = core::UUID{},
							 .type = "func.call",
							 .position = {0.f, 400.f},
							 .values = {{"function", name}}};
	next.nodes.push_back(call);
	ASSERT_TRUE(m_document.commit(std::move(next), "Call"));
	const auto bodySize = std::ranges::count_if(m_document.getScript().nodes, [&](const vs::GraphNode& iNode) -> bool {
		return iNode.type == "func.entry" || iNode.type == "func.return";
	});
	ASSERT_TRUE(m_document.moveFunctionToLibrary(name, m_root, "scripts/lib/shared.owlvs"));
	EXPECT_EQ(m_document.getScript().findFunction(name), nullptr);
	const auto* moved = m_document.getScript().findNode(call.id);
	ASSERT_NE(moved, nullptr);
	EXPECT_EQ(moved->values.at("library"), "scripts/lib/shared.owlvs");
	const auto library = vs::deserialize(platform::fileToString(m_root / "scripts/lib/shared.owlvs"), "shared.owlvs");
	ASSERT_TRUE(library.has_value());
	EXPECT_NE(library->findFunction(name), nullptr);
	EXPECT_EQ(static_cast<long>(library->nodes.size()), bodySize);
	EXPECT_FALSE(m_document.moveFunctionToLibrary("unknown", m_root, "scripts/lib/shared.owlvs"));
	m_document.performUndo();
	EXPECT_NE(m_document.getScript().findFunction(name), nullptr);
}

TEST_F(VisualScriptDocumentTest, StraightenLinesUpTheLinkedPins) {
	const auto event = m_document.getScript().nodes.front().id;
	const VisualScriptPinRef from{.node = event, .pin = "Then", .output = true, .type = vs::PinType::Exec};
	const auto branch = m_document.addNode("flow.branch", {300.f, 137.f}, from);
	const auto measure = [](const std::string_view iText) -> float { return 7.f * static_cast<float>(iText.size()); };
	const auto pinY = [&](const core::UUID iNode, const bool iOutput) -> float {
		const auto& script = m_document.getScript();
		const auto* node = script.findNode(iNode);
		const auto layout = layoutNode(script, *node, measure);
		return node->position[1] + (iOutput ? layout.outputs.front()[1] : layout.inputs.front()[1]);
	};
	EXPECT_NE(pinY(event, true), pinY(branch, false));
	EXPECT_EQ(m_document.straightenNodes(std::vector{branch}), 1u);
	EXPECT_FLOAT_EQ(pinY(event, true), pinY(branch, false));
	EXPECT_EQ(m_document.straightenNodes(std::vector{branch}), 0u);
	m_document.performUndo();
	EXPECT_FLOAT_EQ(m_document.getScript().findNode(branch)->position[1], 137.f);
}

TEST_F(VisualScriptDocumentTest, GroupEventsFramesEachEventWithItsChain) {
	const auto& nodes = m_document.getScript().nodes;
	const auto create = nodes.front().id;
	const VisualScriptPinRef from{.node = create, .pin = "Then", .output = true, .type = vs::PinType::Exec};
	const auto branch = m_document.addNode("flow.branch", {300.f, -400.f}, from);
	EXPECT_EQ(m_document.groupEvents(), 2u);
	const auto& groups = m_document.getScript().groups;
	ASSERT_EQ(groups.size(), 2u);
	EXPECT_EQ(groups.front().title, "On Create");
	EXPECT_TRUE(groups.front().holds(*m_document.getScript().findNode(branch)));
	EXPECT_EQ(groups.back().title, "On Update");
	EXPECT_NE(groups.front().color, groups.back().color);
	EXPECT_EQ(m_document.groupEvents(), 0u);
}

TEST_F(VisualScriptDocumentTest, CheckMarksNothingOnACleanGraph) {
	m_document.check();
	EXPECT_TRUE(m_document.getCheckResults().empty());
}
