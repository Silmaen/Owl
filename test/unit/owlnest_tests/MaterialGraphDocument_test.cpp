/**
 * @file MaterialGraphDocument_test.cpp
 * @author Silmaen
 * @date 09/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "commands/CommandRegistry.h"
#include "commands/DocumentCommands.h"
#include "commands/MaterialCommands.h"
#include "document/MaterialDocument.h"
#include "testHelper.h"

#include <core/Log.h>
#include <renderer/Material.h>
#include <renderer/MaterialGraph.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

using namespace owl;
using namespace owl::nest;
using namespace owl::nest::commands;

namespace {

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wweak-vtables")
// Hands the commands the two material documents of the test.
class MaterialHost final : public DocumentHost {
public:
	auto openSoundCue(const std::filesystem::path& /*iPath*/) -> SoundCueEditor* override { return nullptr; }

	auto openLocalisation(const std::filesystem::path& /*iPath*/) -> LocalisationEditor* override { return nullptr; }

	auto openDataTable(const std::filesystem::path& /*iPath*/) -> DataTableEditor* override { return nullptr; }

	auto openDialogue(const std::filesystem::path& /*iPath*/) -> DialogueEditor* override { return nullptr; }

	auto openSequence(const std::filesystem::path& /*iPath*/) -> SequenceEditor* override { return nullptr; }

	auto openHudTheme(const std::filesystem::path& /*iPath*/) -> HudThemeEditor* override { return nullptr; }

	auto openMaterial(const std::filesystem::path& iPath) -> MaterialEditor* override {
		if (iPath == root.filePath())
			return &root;
		return iPath == child.filePath() ? &child : nullptr;
	}

	MaterialDocument root;
	MaterialDocument child;
};

class MaterialGraphDocumentTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_dir = std::filesystem::temp_directory_path() / "owl_material_graph_document_test";
		std::filesystem::remove_all(m_dir);
		std::filesystem::create_directories(m_dir);
		m_rootFile = m_dir / "lava.owlmat";
		ASSERT_TRUE(renderer::MaterialAsset{}.save(m_rootFile));
		ASSERT_TRUE(m_host.root.loadFromFile(m_rootFile));
		m_childFile = m_dir / "lava_dark.owlmat";
		renderer::MaterialAsset child;
		child.parent = "lava.owlmat";
		child.overrides = 0;
		ASSERT_TRUE(child.save(m_childFile));
		ASSERT_TRUE(m_host.child.loadFromFile(m_childFile));
	}

	void TearDown() override {
		m_host.root.onDetach();
		m_host.child.onDetach();
		std::filesystem::remove_all(m_dir);
		core::Log::invalidate();
	}

	auto run(const std::string& iName, CommandArgs iArgs, const std::filesystem::path& iPath = {})
			-> expected<CommandResult, CommandFailure> {
		iArgs.set("path", (iPath.empty() ? m_rootFile : iPath).generic_string());
		CommandContext context;
		context.documents = &m_host;
		return m_registry.execute(iName, iArgs, context);
	}

	static auto text(const CommandResult& iResult, const std::string& iField) -> std::string {
		const auto found = iResult.data.find(iField);
		if (found == iResult.data.end())
			return {};
		const auto* value = std::get_if<std::string>(&found->second);
		return value != nullptr ? *value : std::string{};
	}

	[[nodiscard]] auto graph() const -> const renderer::MaterialGraph& { return *m_host.root.asset().graph; }

	auto addNode(const std::string& iType, const std::string& iItem = {}, const std::string& iFunction = {})
			-> std::string {
		CommandArgs args{{"type", iType}, {"x", 10.0}, {"y", 20.0}};
		if (!iItem.empty())
			args.set("item", iItem);
		if (!iFunction.empty())
			args.set("function", iFunction);
		const auto result = run("material.add_node", args);
		EXPECT_TRUE(result.has_value()) << iType;
		return result ? text(*result, "node") : std::string{};
	}

	std::filesystem::path m_dir;
	std::filesystem::path m_rootFile;
	std::filesystem::path m_childFile;
	MaterialHost m_host;
	CommandRegistry m_registry;
};
OWL_DIAG_POP

auto idOf(const std::string& iText) -> core::UUID { return core::UUID{std::stoull(iText)}; }

}// namespace

TEST_F(MaterialGraphDocumentTest, ConvertsAParameterMaterialAndBack) {
	EXPECT_FALSE(m_host.root.isGraph());
	EXPECT_FALSE(run("material.add_node", {{"type", std::string{"math.add"}}}).has_value()) << "no graph yet";
	ASSERT_TRUE(run("material.convert_to_graph", {}).has_value());
	EXPECT_TRUE(m_host.root.isGraph());
	EXPECT_EQ(graph(), renderer::getParameterMaterialGraph());
	EXPECT_TRUE(m_host.root.getProgram().isValid());
	EXPECT_FALSE(run("material.convert_to_graph", {}).has_value()) << "already a graph";
	EXPECT_FALSE(run("material.convert_to_graph", {}, m_childFile).has_value()) << "an instance has no graph";
	m_host.root.performUndo();
	EXPECT_FALSE(m_host.root.isGraph());
	m_host.root.performRedo();
	ASSERT_TRUE(m_host.root.isGraph());
	ASSERT_TRUE(run("material.convert_to_parameters", {}).has_value());
	EXPECT_FALSE(m_host.root.isGraph());
}

TEST_F(MaterialGraphDocumentTest, AddsLinksAndRemovesNodes) {
	ASSERT_TRUE(run("material.convert_to_graph", {}).has_value());
	const auto color = addNode("const.color");
	const auto multiply = addNode("math.multiply");
	ASSERT_FALSE(color.empty());
	EXPECT_EQ(graph().nodes.size(), 4u);
	EXPECT_FALSE(run("material.add_node", {{"type", std::string{"math.nothing"}}}).has_value());
	EXPECT_FALSE(run("material.add_node", {{"type", std::string{"function.input"}}}).has_value())
			<< "only in a function body";

	ASSERT_TRUE(run("material.link", {{"from_node", color},
									  {"from_pin", std::string{"RGB"}},
									  {"to_node", multiply},
									  {"to_pin", std::string{"A"}}})
						.has_value());
	const auto surface = std::to_string(static_cast<uint64_t>(graph().nodes[1].id));
	ASSERT_TRUE(run("material.link", {{"from_node", multiply},
									  {"from_pin", std::string{"Result"}},
									  {"to_node", surface},
									  {"to_pin", std::string{"Base Color"}}})
						.has_value());
	EXPECT_EQ(
			std::ranges::count_if(graph().links, [](const auto& iLink) -> bool { return iLink.toPin == "Base Color"; }),
			1)
			<< "an input takes one link";
	EXPECT_FALSE(run("material.link", {{"from_node", multiply},
									   {"from_pin", std::string{"Result"}},
									   {"to_node", color},
									   {"to_pin", std::string{"Nothing"}}})
						 .has_value());
	EXPECT_FALSE(run("material.link", {{"from_node", multiply},
									   {"from_pin", std::string{"Result"}},
									   {"to_node", multiply},
									   {"to_pin", std::string{"B"}}})
						 .has_value())
			<< "no loop";
	EXPECT_TRUE(m_host.root.getProgram().isValid());

	ASSERT_TRUE(run("material.move_nodes", {{"layout", color + " 100 -50"}}).has_value());
	EXPECT_FLOAT_EQ(graph().nodes[2].position[0], 100.f);
	EXPECT_FLOAT_EQ(graph().nodes[2].position[1], -50.f);

	ASSERT_TRUE(run("material.unlink", {{"to_node", multiply}, {"to_pin", std::string{"A"}}}).has_value());
	EXPECT_FALSE(run("material.unlink", {{"to_node", multiply}, {"to_pin", std::string{"A"}}}).has_value());
	const auto linksBefore = graph().links.size();
	ASSERT_TRUE(run("material.remove_nodes", {{"nodes", color + " " + multiply}}).has_value());
	EXPECT_EQ(graph().nodes.size(), 2u);
	EXPECT_EQ(graph().links.size(), linksBefore - 1) << "the links of the removed nodes go with them";
	m_host.root.performUndo();
	EXPECT_EQ(graph().nodes.size(), 4u);

	m_host.root.syncCanvas();
	EXPECT_EQ(m_host.root.canvas().nodes().size(), 4u);
}

TEST_F(MaterialGraphDocumentTest, NodeValuesAndDiagnostics) {
	ASSERT_TRUE(run("material.convert_to_graph", {}).has_value());
	const auto number = addNode("const.float");
	ASSERT_TRUE(run("material.set_node_value",
					{{"node", number}, {"name", std::string{"value"}}, {"value", std::string{"0.25"}}})
						.has_value());
	const auto& node = *std::ranges::find(graph().nodes, idOf(number), &renderer::MaterialGraphNode::id);
	EXPECT_EQ(renderer::getMaterialNodeValue(node, "value"), "0.25");
	EXPECT_FALSE(run("material.set_node_value",
					 {{"node", number}, {"name", std::string{"bogus"}}, {"value", std::string{"1"}}})
						 .has_value());
	ASSERT_TRUE(run("material.set_node_value", {{"node", number}, {"name", std::string{"value"}}, {"reset", true}})
						.has_value());
	EXPECT_TRUE(graph().nodes.back().values.empty());

	const auto sample = addNode("texture.sample");
	const auto surface = std::to_string(static_cast<uint64_t>(graph().nodes[1].id));
	ASSERT_TRUE(run("material.link", {{"from_node", sample},
									  {"from_pin", std::string{"RGB"}},
									  {"to_node", surface},
									  {"to_pin", std::string{"Emissive"}}})
						.has_value());
	const auto& program = m_host.root.getProgram();
	EXPECT_FALSE(program.isValid()) << "the sample names no texture";
	EXPECT_TRUE(std::ranges::any_of(program.diagnostics,
									[&](const auto& iDiagnostic) -> bool { return iDiagnostic.node == idOf(sample); }));
	m_host.root.syncCanvas();
	const auto* canvasNode = m_host.root.canvas().findNode(idOf(sample));
	ASSERT_NE(canvasNode, nullptr);
	EXPECT_TRUE(canvasNode->faulty);
	EXPECT_FALSE(canvasNode->tooltip.empty());
	EXPECT_GT(canvasNode->footerHeight, 0.f) << "a node with outputs shows its thumbnail";
	m_host.root.setThumbnails(false);
	m_host.root.syncCanvas();
	EXPECT_FLOAT_EQ(m_host.root.canvas().findNode(idOf(sample))->footerHeight, 0.f);
}

TEST_F(MaterialGraphDocumentTest, ParametersRenameTheirNodes) {
	ASSERT_TRUE(run("material.convert_to_graph", {}).has_value());
	const auto added = run("material.add_parameter", {{"name", std::string{"glow"}}, {"type", std::string{"Color"}}});
	ASSERT_TRUE(added.has_value());
	EXPECT_EQ(text(*added, "name"), "glow");
	const auto again = run("material.add_parameter", {{"name", std::string{"glow"}}});
	ASSERT_TRUE(again.has_value());
	EXPECT_EQ(text(*again, "name"), "glow_2");
	EXPECT_FALSE(run("material.add_parameter", {{"name", std::string{"2bad"}}}).has_value());
	const auto get = addNode("param.get", "glow");
	ASSERT_TRUE(run("material.set_parameter", {{"name", std::string{"glow"}},
											   {"new_name", std::string{"heat"}},
											   {"default", std::string{"1 0.5 0 1"}}})
						.has_value());
	ASSERT_NE(graph().findParameter("heat"), nullptr);
	EXPECT_EQ(graph().findParameter("heat")->defaultValue, "1 0.5 0 1");
	const auto& node = *std::ranges::find(graph().nodes, idOf(get), &renderer::MaterialGraphNode::id);
	EXPECT_EQ(renderer::getMaterialNodeValue(node, "parameter"), "heat");
	ASSERT_TRUE(run("material.set_parameter", {{"name", std::string{"heat"}}, {"type", std::string{"Texture"}}})
						.has_value());
	EXPECT_TRUE(graph().findParameter("heat")->defaultValue.empty()) << "a new kind resets the default";
	ASSERT_TRUE(run("material.remove_parameter", {{"name", std::string{"heat"}}}).has_value());
	EXPECT_EQ(graph().findParameter("heat"), nullptr);
	EXPECT_FALSE(run("material.remove_parameter", {{"name", std::string{"heat"}}}).has_value());
}

TEST_F(MaterialGraphDocumentTest, CustomNodeCompilesWithItsPins) {
	ASSERT_TRUE(run("material.convert_to_graph", {}).has_value());
	const auto added = run("material.add_custom_node", {{"name", std::string{"stripes"}}});
	ASSERT_TRUE(added.has_value());
	ASSERT_TRUE(run("material.set_custom_node", {{"name", std::string{"stripes"}},
												 {"title", std::string{"Stripes"}},
												 {"inputs", std::string{"uv:Float2, count:Float=8"}},
												 {"outputs", std::string{"mask:Float"}},
												 {"code", std::string{"mask = step(0.5, frac(uv.x * count));"}}})
						.has_value());
	const auto* custom = graph().findCustomNode("stripes");
	ASSERT_NE(custom, nullptr);
	ASSERT_EQ(custom->inputs.size(), 2u);
	EXPECT_EQ(custom->inputs[1].defaultValue, "8");
	EXPECT_EQ(custom->outputs.front().name, "mask");
	EXPECT_FALSE(
			run("material.set_custom_node", {{"name", std::string{"stripes"}}, {"inputs", std::string{"a b:Float"}}})
					.has_value());
	const auto node = addNode("custom", "stripes");
	const auto uv = addNode("input.uv");
	const auto surface = std::to_string(static_cast<uint64_t>(graph().nodes[1].id));
	ASSERT_TRUE(
			run("material.link",
				{{"from_node", uv}, {"from_pin", std::string{"UV"}}, {"to_node", node}, {"to_pin", std::string{"uv"}}})
					.has_value());
	ASSERT_TRUE(run("material.link", {{"from_node", node},
									  {"from_pin", std::string{"mask"}},
									  {"to_node", surface},
									  {"to_pin", std::string{"Roughness"}}})
						.has_value());
	EXPECT_TRUE(m_host.root.getProgram().isValid());
	ASSERT_TRUE(run("material.set_custom_node", {{"name", std::string{"stripes"}}, {"new_name", std::string{"bands"}}})
						.has_value());
	const auto& renamed = *std::ranges::find(graph().nodes, idOf(node), &renderer::MaterialGraphNode::id);
	EXPECT_EQ(renderer::getMaterialNodeValue(renamed, "custom"), "bands");
	ASSERT_TRUE(
			run("material.set_custom_node", {{"name", std::string{"bands"}}, {"outputs", std::string{"value:Float"}}})
					.has_value());
	EXPECT_FALSE(std::ranges::any_of(graph().links, [&](const auto& iLink) -> bool { return iLink.fromPin == "mask"; }))
			<< "the links of a removed pin go";
	ASSERT_TRUE(run("material.remove_custom_node", {{"name", std::string{"bands"}}}).has_value());
	EXPECT_TRUE(graph().customNodes.empty());
}

TEST_F(MaterialGraphDocumentTest, FunctionBodiesAreEditedInTheirScope) {
	ASSERT_TRUE(run("material.convert_to_graph", {}).has_value());
	const auto added = run("material.add_function", {{"name", std::string{"tint"}}});
	ASSERT_TRUE(added.has_value());
	const auto* function = graph().findFunction("tint");
	ASSERT_NE(function, nullptr);
	ASSERT_EQ(function->nodes.size(), 2u);
	const auto input = std::to_string(static_cast<uint64_t>(function->nodes[0].id));
	const auto output = std::to_string(static_cast<uint64_t>(function->nodes[1].id));
	const auto multiply = addNode("math.multiply", {}, "tint");
	EXPECT_EQ(graph().findFunction("tint")->nodes.size(), 3u);
	EXPECT_EQ(graph().nodes.size(), 2u) << "the graph itself is untouched";
	ASSERT_TRUE(run("material.link", {{"from_node", input},
									  {"from_pin", std::string{"value"}},
									  {"to_node", multiply},
									  {"to_pin", std::string{"A"}},
									  {"function", std::string{"tint"}}})
						.has_value());
	ASSERT_TRUE(run("material.link", {{"from_node", multiply},
									  {"from_pin", std::string{"Result"}},
									  {"to_node", output},
									  {"to_pin", std::string{"result"}},
									  {"function", std::string{"tint"}}})
						.has_value());
	ASSERT_TRUE(run("material.set_function", {{"name", std::string{"tint"}},
											  {"inputs", std::string{"value:Float3"}},
											  {"outputs", std::string{"result:Float3"}}})
						.has_value());
	EXPECT_EQ(graph().findFunction("tint")->inputs.front().type, renderer::MaterialPinType::Float3);

	EXPECT_TRUE(m_host.root.setScope("tint"));
	EXPECT_FALSE(m_host.root.setScope("missing"));
	m_host.root.syncCanvas();
	EXPECT_EQ(m_host.root.canvas().nodes().size(), 3u);
	const auto call = addNode("function.call", "tint");
	EXPECT_EQ(graph().nodes.size(), 3u);
	EXPECT_FALSE(call.empty());
	ASSERT_TRUE(run("material.set_function", {{"name", std::string{"tint"}}, {"new_name", std::string{"shade"}}})
						.has_value());
	EXPECT_TRUE(m_host.root.getScope().empty()) << "the shown function is gone under that name";
	const auto& renamed = *std::ranges::find(graph().nodes, idOf(call), &renderer::MaterialGraphNode::id);
	EXPECT_EQ(renderer::getMaterialNodeValue(renamed, "function"), "shade");
	ASSERT_TRUE(run("material.remove_function", {{"name", std::string{"shade"}}}).has_value());
	EXPECT_TRUE(graph().functions.empty());
}

TEST_F(MaterialGraphDocumentTest, CopyPasteKeepsLinksAndDefinitions) {
	ASSERT_TRUE(run("material.convert_to_graph", {}).has_value());
	ASSERT_TRUE(run("material.add_parameter", {{"name", std::string{"speed"}}}).has_value());
	const auto get = addNode("param.get", "speed");
	const auto multiply = addNode("math.multiply");
	ASSERT_TRUE(run("material.link", {{"from_node", get},
									  {"from_pin", std::string{"Value"}},
									  {"to_node", multiply},
									  {"to_pin", std::string{"A"}}})
						.has_value());
	const std::vector nodes{idOf(get), idOf(multiply)};
	const auto clip = copyMaterialNodes(graph(), {}, nodes);
	ASSERT_FALSE(clip.empty());
	const auto pasted = run("material.paste_nodes", {{"nodes", clip}, {"x", 500.0}, {"y", 0.0}});
	ASSERT_TRUE(pasted.has_value());
	EXPECT_EQ(graph().nodes.size(), 6u);
	EXPECT_EQ(std::ranges::count_if(graph().links, [](const auto& iLink) -> bool { return iLink.toPin == "A"; }), 2);
	EXPECT_EQ(graph().parameters.size(), 1u) << "the parameter exists already";

	ASSERT_TRUE(run("material.add_function", {{"name", std::string{"f"}}}).has_value());
	ASSERT_TRUE(run("material.paste_nodes", {{"nodes", clip}, {"function", std::string{"f"}}}).has_value());
	EXPECT_EQ(graph().findFunction("f")->nodes.size(), 4u);
	EXPECT_FALSE(run("material.paste_nodes", {{"nodes", std::string{"nonsense"}}}).has_value());
}

TEST_F(MaterialGraphDocumentTest, ChildOverridesParentParameters) {
	ASSERT_TRUE(
			run("material.set_graph_value", {{"name", std::string{"speed"}}, {"value", std::string{"3"}}}, m_childFile)
					.has_value());
	EXPECT_EQ(m_host.child.asset().graphValues.at("speed"), "3");
	ASSERT_TRUE(run("material.set_graph_value", {{"name", std::string{"speed"}}, {"reset", true}}, m_childFile)
						.has_value());
	EXPECT_TRUE(m_host.child.asset().graphValues.empty());
	EXPECT_FALSE(run("material.set_graph_value", {{"name", std::string{"speed"}}, {"reset", true}}, m_childFile)
						 .has_value());
	m_host.child.performUndo();
	EXPECT_EQ(m_host.child.asset().graphValues.at("speed"), "3");
	ASSERT_TRUE(m_host.child.save());
	const auto saved = renderer::MaterialAsset::load(m_childFile);
	ASSERT_TRUE(saved.has_value());
	EXPECT_EQ(saved->graphValues.at("speed"), "3");
}

TEST_F(MaterialGraphDocumentTest, ParentAndValuesGoThroughCommands) {
	ASSERT_TRUE(run("material.convert_to_graph", {}).has_value());
	renderer::MaterialOverrides values{.fields = renderer::g_allMaterialFields, .values = m_host.root.asset().values};
	values.values.shadingModel = renderer::ShadingModel::Toon;
	values.values.toonBands = 5;
	ASSERT_TRUE(run("material.set_values", {{"values", values.toYaml()}}).has_value());
	EXPECT_EQ(m_host.root.asset().values.shadingModel, renderer::ShadingModel::Toon);
	EXPECT_EQ(m_host.root.asset().values.toonBands, 5u);
	EXPECT_FALSE(run("material.set_values", {{"values", values.toYaml()}}).has_value()) << "nothing changes";
	ASSERT_TRUE(run("material.set_parent", {{"parent", std::string{"other.owlmat"}}}, m_childFile).has_value());
	EXPECT_EQ(m_host.child.asset().parent.generic_string(), "other.owlmat");
	EXPECT_FALSE(run("material.set_parent", {{"parent", std::string{"x.png"}}}, m_childFile).has_value());
}

TEST_F(MaterialGraphDocumentTest, SaveReloadRoundTripsTheGraph) {
	ASSERT_TRUE(run("material.convert_to_graph", {}).has_value());
	ASSERT_TRUE(run("material.add_parameter", {{"name", std::string{"albedo"}},
											   {"type", std::string{"Texture"}},
											   {"default", std::string{"textures/rock.png"}}})
						.has_value());
	ASSERT_TRUE(run("material.add_custom_node", {}).has_value());
	ASSERT_TRUE(run("material.add_function", {}).has_value());
	std::ignore = addNode("texture.sample", "albedo");
	EXPECT_TRUE(m_host.root.isDirty());
	ASSERT_TRUE(m_host.root.save());
	EXPECT_FALSE(m_host.root.isDirty());
	MaterialDocument reloaded;
	ASSERT_TRUE(reloaded.loadFromFile(m_rootFile));
	ASSERT_TRUE(reloaded.isGraph());
	EXPECT_EQ(*reloaded.asset().graph, graph());
	const auto snapshot = m_host.root.recoverySnapshot();
	ASSERT_TRUE(snapshot.has_value());
	ASSERT_TRUE(reloaded.restoreRecoverySnapshot(*snapshot));
	EXPECT_EQ(*reloaded.asset().graph, graph());
	reloaded.onDetach();

	size_t steps = 0;
	for (; m_host.root.canUndo(); ++steps) m_host.root.performUndo();
	EXPECT_EQ(steps, 5u);
	EXPECT_FALSE(m_host.root.isGraph());
	while (m_host.root.canRedo()) m_host.root.performRedo();
	EXPECT_EQ(*m_host.root.asset().graph, *reloaded.asset().graph);
}

TEST(MaterialCommands, PinsRoundTripThroughText) {
	const std::vector<renderer::MaterialPinSpec> pins{
			{.name = "uv", .type = renderer::MaterialPinType::Float2, .defaultValue = "0 0"},
			{.name = "scale", .type = renderer::MaterialPinType::Float, .defaultValue = "1"}};
	const auto text = formatMaterialPins(pins);
	EXPECT_EQ(text, "uv:Float2=0 0, scale:Float=1");
	const auto parsed = parseMaterialPins(text);
	ASSERT_TRUE(parsed.has_value());
	EXPECT_EQ(*parsed, pins);
	EXPECT_FALSE(parseMaterialPins("a:Float, a:Float2").has_value()) << "a name once";
	EXPECT_FALSE(parseMaterialPins("a:Matrix").has_value());
	EXPECT_FALSE(parseMaterialPins("a:Any").has_value());
	ASSERT_TRUE(parseMaterialPins("plain").has_value());
	EXPECT_EQ(parseMaterialPins("plain")->front().type, renderer::MaterialPinType::Float);
	EXPECT_EQ(getMaterialParameterTypeName(renderer::MaterialParameterType::Texture), "Texture");
	EXPECT_EQ(getMaterialPinTypeName(renderer::MaterialPinType::Any), "Any");
	EXPECT_EQ(formatMaterialNodes(std::vector{core::UUID{3}, core::UUID{7}}), "3 7");
	EXPECT_TRUE(copyMaterialNodes(renderer::getParameterMaterialGraph(), {}, {}).empty());
	EXPECT_EQ(getMaterialScopeNodes(renderer::getParameterMaterialGraph(), "missing"), nullptr);
}
