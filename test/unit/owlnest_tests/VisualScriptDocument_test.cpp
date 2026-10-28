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
#include <script/visual/VisualScript.h>

#include <gtest/gtest.h>

#include <array>
#include <filesystem>
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
	script.links.push_back({.fromNode = event, .fromPin = "Then", .toNode = branch, .toPin = "Exec"});
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
	script.variables.push_back({.name = "speed", .type = script::ScriptPropertyType::Float, .defaultValue = "2"});
	ASSERT_TRUE(m_document.commit(script, "Add Variable"));
	const auto event = m_document.getScript().nodes.front().id;
	const auto set = m_document.addNode("var.set", {0.f, 0.f});
	script = m_document.getScript();
	script.links.push_back({.fromNode = event, .fromPin = "Then", .toNode = set, .toPin = "Exec"});
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
	script.links.push_back({.fromNode = get, .fromPin = "xyz", .toNode = set, .toPin = "xyz"});
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
