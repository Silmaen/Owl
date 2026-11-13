/**
 * @file MaterialGraph_test.cpp
 * @author Silmaen
 * @date 09/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <renderer/Material.h>
#include <renderer/MaterialGraph.h>
#if OWL_WITH_RENDER
#include <renderer/utils/shaderFileUtils.h>
#endif

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace owl;
using namespace owl::renderer;

namespace {

#if OWL_WITH_RENDER
auto findRoot() -> std::filesystem::path {
	auto cwd = std::filesystem::current_path();
	if (std::filesystem::exists(cwd / "CMakeLists.txt") && std::filesystem::exists(cwd / "engine_assets"))
		return cwd;
	return test::getRootPath();
}

auto meshShader(const std::string& iName) -> std::string {
	return utils::readShaderSource(findRoot() / "engine_assets" / "shaders" / "renderer3D" / (iName + ".slang"));
}

#endif

auto addNode(MaterialGraph& ioGraph, const uint64_t iId, std::string iType,
			 std::map<std::string, std::string> iValues = {}) -> core::UUID {
	ioGraph.nodes.push_back(
			{.id = core::UUID{iId}, .type = std::move(iType), .position = {0.f, 0.f}, .values = std::move(iValues)});
	return core::UUID{iId};
}

void link(MaterialGraph& ioGraph, const core::UUID iFrom, std::string iFromPin, const core::UUID iTo,
		  std::string iToPin) {
	ioGraph.links.push_back(
			{.fromNode = iFrom, .fromPin = std::move(iFromPin), .toNode = iTo, .toPin = std::move(iToPin)});
}

// Moss over a tinted texture by height, a panning noise in the emissive, a swaying vertex stage, a custom node and a
// function: most node kinds in one graph.
auto richGraph() -> MaterialGraph {
	MaterialGraph graph;
	graph.parameters = {{.name = "tint", .type = MaterialParameterType::Color, .defaultValue = "0.8 0.6 0.4 1"},
						{.name = "sway", .type = MaterialParameterType::Float, .defaultValue = "0.1"},
						{.name = "albedo", .type = MaterialParameterType::Texture, .defaultValue = "textures/a.png"},
						{.name = "height", .type = MaterialParameterType::Texture, .defaultValue = "textures/h.png"}};
	graph.customNodes = {{.name = "glow",
						  .title = "Glow",
						  .inputs = {{.name = "Value", .type = MaterialPinType::Float, .defaultValue = "0"}},
						  .outputs = {{.name = "Color", .type = MaterialPinType::Float3, .defaultValue = {}}},
						  .code = "Color = float3(Value, Value * 0.5f, 0.0f);"}};
	MaterialFunction scale{.name = "scaled",
						   .inputs = {{.name = "In", .type = MaterialPinType::Float3, .defaultValue = "0"}},
						   .outputs = {{.name = "Out", .type = MaterialPinType::Float3, .defaultValue = {}}},
						   .nodes = {},
						   .links = {}};
	scale.nodes = {{.id = core::UUID{1}, .type = "function.input", .position = {}, .values = {}},
				   {.id = core::UUID{2}, .type = "math.multiply", .position = {}, .values = {{"B", "0.5"}}},
				   {.id = core::UUID{3}, .type = "function.output", .position = {}, .values = {}}};
	scale.links = {{.fromNode = core::UUID{1}, .fromPin = "In", .toNode = core::UUID{2}, .toPin = "A"},
				   {.fromNode = core::UUID{2}, .fromPin = "Result", .toNode = core::UUID{3}, .toPin = "Out"}};
	graph.functions = {scale};

	const auto surface = addNode(graph, 100, "output.surface");
	const auto texture = addNode(graph, 101, "texture.sample", {{"texture", "albedo"}});
	const auto tint = addNode(graph, 102, "param.get", {{"parameter", "tint"}});
	const auto multiply = addNode(graph, 103, "math.multiply");
	link(graph, texture, "RGB", multiply, "A");
	link(graph, tint, "Color", multiply, "B");
	const auto moss = addNode(graph, 104, "const.color", {{"value", "0.2 0.5 0.1 1"}});
	const auto noise = addNode(graph, 105, "noise.fbm", {{"Scale", "4"}});
	const auto blend = addNode(graph, 106, "effect.height_blend");
	link(graph, multiply, "Result", blend, "A");
	link(graph, moss, "RGB", blend, "B");
	link(graph, noise, "Value", blend, "Height B");
	const auto parallax = addNode(graph, 107, "texture.parallax", {{"texture", "height"}});
	link(graph, parallax, "UV", texture, "UV");
	link(graph, blend, "Result", surface, "Base Color");
	const auto panner = addNode(graph, 108, "uv.panner");
	const auto voronoi = addNode(graph, 109, "noise.voronoi", {{"Scale", "8"}});
	const auto combine = addNode(graph, 110, "vector.combine");
	link(graph, panner, "UV", combine, "X");
	link(graph, combine, "XYZ", voronoi, "Coordinates");
	const auto glow = addNode(graph, 111, "custom", {{"custom", "glow"}});
	link(graph, voronoi, "Distance", glow, "Value");
	const auto scaled = addNode(graph, 112, "function.call", {{"function", "scaled"}});
	link(graph, glow, "Color", scaled, "In");
	link(graph, scaled, "Out", surface, "Emissive");
	const auto normal = addNode(graph, 113, "normal.strength", {{"Strength", "0.5"}});
	link(graph, normal, "Normal", surface, "Normal");
	const auto fresnel = addNode(graph, 114, "effect.fresnel");
	link(graph, fresnel, "Fresnel", surface, "Sheen Color");
	const auto time = addNode(graph, 115, "input.time");
	const auto sine = addNode(graph, 116, "math.sin");
	const auto sway = addNode(graph, 117, "param.get", {{"parameter", "sway"}});
	const auto offset = addNode(graph, 118, "math.multiply");
	link(graph, time, "Time", sine, "Value");
	link(graph, sine, "Result", offset, "A");
	link(graph, sway, "Value", offset, "B");
	const auto split = addNode(graph, 119, "vector.combine");
	link(graph, offset, "Result", split, "X");
	link(graph, split, "XYZ", surface, "World Position Offset");
	return graph;
}

auto hasDiagnostic(const MaterialProgram& iProgram, const std::string_view iText) -> bool {
	return std::ranges::any_of(iProgram.diagnostics, [iText](const MaterialGraphDiagnostic& iDiagnostic) -> bool {
		return iDiagnostic.message.find(iText) != std::string::npos;
	});
}

}// namespace

TEST(MaterialGraph, NodeTypesHaveUniqueIdsAndKnownPins) {
	std::set<std::string> ids;
	for (const auto& type: getMaterialNodeTypes()) {
		EXPECT_TRUE(ids.insert(type.id).second) << type.id;
		EXPECT_FALSE(type.title.empty()) << type.id;
		EXPECT_EQ(findMaterialNodeType(type.id), &type);
	}
	EXPECT_EQ(findMaterialNodeType("math.nothing"), nullptr);
	ASSERT_NE(findMaterialNodeType("output.surface"), nullptr);
	EXPECT_EQ(findMaterialNodeType("output.surface")->inputs.size(), 14u);
}

TEST(MaterialGraph, PinsOfGraphDefinedNodesComeFromTheGraph) {
	const auto graph = richGraph();
	const auto custom =
			getMaterialNodePins(graph, {.id = core::UUID{1}, .type = "custom", .values = {{"custom", "glow"}}});
	ASSERT_EQ(custom.outputs.size(), 1u);
	EXPECT_EQ(custom.outputs.front().name, "Color");
	const auto parameter =
			getMaterialNodePins(graph, {.id = core::UUID{1}, .type = "param.get", .values = {{"parameter", "tint"}}});
	ASSERT_EQ(parameter.outputs.size(), 1u);
	EXPECT_EQ(parameter.outputs.front().type, MaterialPinType::Float4);
	EXPECT_TRUE(
			getMaterialNodePins(graph, {.id = core::UUID{1}, .type = "param.get", .values = {{"parameter", "albedo"}}})
					.outputs.empty());
	const auto& function = graph.functions.front();
	EXPECT_EQ(getMaterialNodePins(graph, function.nodes.front(), &function).outputs, function.inputs);
	EXPECT_EQ(getMaterialNodeValue({.id = core::UUID{1}, .type = "math.multiply", .values = {}}, "B"), "1");
}

TEST(MaterialGraph, ParameterGraphCompilesIntoTheMaterialBlock) {
	const auto program = compileMaterialGraph(getParameterMaterialGraph());
	ASSERT_TRUE(program.isValid()) << (program.diagnostics.empty() ? "" : program.diagnostics.front().message);
	EXPECT_NE(program.code.find("MaterialSurface evaluateSurface(SurfaceInput input)"), std::string::npos);
	EXPECT_NE(program.code.find("owlParameterBase(input.uv, input.texIndex)"), std::string::npos);
	EXPECT_FALSE(program.vertexOffset);
	EXPECT_EQ(program.key.size(), 16u);
	EXPECT_EQ(compileMaterialGraph(getParameterMaterialGraph()).key, program.key);
}

TEST(MaterialGraph, RichGraphPlacesParametersAndTheVertexStage) {
	const auto program = compileMaterialGraph(richGraph());
	ASSERT_TRUE(program.isValid()) << (program.diagnostics.empty() ? "" : program.diagnostics.front().message);
	EXPECT_EQ(program.values, (std::vector<std::string>{"tint", "sway"}));
	EXPECT_EQ(program.textures, (std::vector<std::string>{"albedo", "height"}));
	EXPECT_TRUE(program.vertexOffset);
	EXPECT_NE(program.code.find("gTextures[1].Sample("), std::string::npos);
	EXPECT_NE(program.code.find("owlParallax(2,"), std::string::npos);
	EXPECT_NE(program.code.find("void owlCustom_glow(in float Value, out float3 Color)"), std::string::npos);
	EXPECT_NE(program.code.find("owlFunction_scaled_fragment"), std::string::npos);
	EXPECT_NE(program.code.find("gMaterial.graph[1].x"), std::string::npos);
	EXPECT_NE(program.code.find("gMaterial.graphTime.x"), std::string::npos);
}

TEST(MaterialGraph, ProblemsAreReportedOnTheirNode) {
	const MaterialGraph empty;
	EXPECT_TRUE(hasDiagnostic(compileMaterialGraph(empty), "no Surface node"));

	auto graph = richGraph();
	const auto loopA = addNode(graph, 200, "math.add");
	const auto loopB = addNode(graph, 201, "math.add");
	link(graph, loopA, "Result", loopB, "A");
	link(graph, loopB, "Result", loopA, "A");
	link(graph, loopA, "Result", core::UUID{100}, "Metallic");
	const auto looped = compileMaterialGraph(graph);
	EXPECT_FALSE(looped.isValid());
	EXPECT_TRUE(hasDiagnostic(looped, "loop"));

	auto vertex = richGraph();
	std::erase_if(vertex.links,
				  [](const MaterialGraphLink& iLink) -> bool { return iLink.toPin == "World Position Offset"; });
	const auto triplanar = addNode(vertex, 300, "texture.triplanar", {{"texture", "albedo"}});
	link(vertex, triplanar, "RGB", core::UUID{100}, "World Position Offset");
	EXPECT_TRUE(hasDiagnostic(compileMaterialGraph(vertex), "fragment stage only"));

	auto unknown = richGraph();
	unknown.nodes[1].values["texture"] = "missing";
	EXPECT_TRUE(hasDiagnostic(compileMaterialGraph(unknown), "'missing' is not a texture parameter"));

	auto many = richGraph();
	for (int index = 0; index < 8; ++index)
		many.parameters.push_back(
				{.name = std::format("p{}", index), .type = MaterialParameterType::Float, .defaultValue = "0"});
	EXPECT_TRUE(hasDiagnostic(compileMaterialGraph(many), "number parameters"));

	auto recursive = richGraph();
	recursive.functions.front().nodes.push_back(
			{.id = core::UUID{4}, .type = "function.call", .position = {}, .values = {{"function", "scaled"}}});
	recursive.functions.front().links.push_back(
			{.fromNode = core::UUID{4}, .fromPin = "Out", .toNode = core::UUID{2}, .toPin = "B"});
	EXPECT_TRUE(hasDiagnostic(compileMaterialGraph(recursive), "calls itself"));
}

TEST(MaterialGraph, PreviewShowsOneNodeOutput) {
	const auto program = compileMaterialGraph(richGraph(), MaterialPreviewTarget{.node = core::UUID{105}, .pin = {}});
	ASSERT_TRUE(program.isValid()) << (program.diagnostics.empty() ? "" : program.diagnostics.front().message);
	EXPECT_NE(program.code.find("owlFbm("), std::string::npos);
	EXPECT_EQ(program.code.find("owlVoronoi("), std::string::npos);
	EXPECT_FALSE(program.vertexOffset);
}

// Slang comes with the render module.
#if OWL_WITH_RENDER
TEST(MaterialGraph, InsertedProgramsCompileWithSlang) {
	core::Log::init(core::Log::Level::Off);
	for (const auto& shader: {std::string{"mesh3d"}, std::string{"skinned3d"}}) {
		const auto source = meshShader(shader);
		ASSERT_FALSE(source.empty()) << shader;
		for (const auto& program:
			 {compileMaterialGraph(getParameterMaterialGraph()), compileMaterialGraph(richGraph()),
			  compileMaterialGraph(richGraph(), MaterialPreviewTarget{.node = core::UUID{106}, .pin = "Weight"})}) {
			ASSERT_TRUE(program.isValid());
			const auto inserted = insertMaterialProgram(source, program);
			ASSERT_TRUE(inserted.has_value());
			const auto result = utils::compileSlangToSpirv(*inserted, shader + "_material_test", /*iForVulkan=*/true);
			EXPECT_TRUE(result.success) << shader;
			EXPECT_EQ(result.spirvData.size(), 2u) << shader;
		}
	}
	EXPECT_FALSE(
			insertMaterialProgram("float4 main() {}", compileMaterialGraph(getParameterMaterialGraph())).has_value());
	core::Log::invalidate();
}

TEST(MaterialGraph, EngineDemoGraphsCompile) {
	core::Log::init(core::Log::Level::Off);
	const auto source = meshShader("mesh3d");
	size_t graphs = 0;
	for (const auto& entry: std::filesystem::recursive_directory_iterator(findRoot() / "engine_demo")) {
		if (entry.path().extension() != ".owlmat")
			continue;
		const auto asset = MaterialAsset::load(entry.path());
		ASSERT_TRUE(asset.has_value()) << entry.path();
		if (!asset->graph.has_value())
			continue;
		++graphs;
		const auto program = compileMaterialGraph(*asset->graph);
		for (const auto& diagnostic: program.diagnostics) ADD_FAILURE() << entry.path() << ": " << diagnostic.message;
		ASSERT_TRUE(program.isValid()) << entry.path();
		const auto inserted = insertMaterialProgram(source, program);
		ASSERT_TRUE(inserted.has_value());
		EXPECT_TRUE(utils::compileSlangToSpirv(*inserted, "demo_material", /*iForVulkan=*/true).success)
				<< entry.path();
	}
	EXPECT_GE(graphs, 2u);
	core::Log::invalidate();
}

TEST(MaterialGraph, SampleGameGraphsCompile) {
	core::Log::init(core::Log::Level::Off);
	const auto source = meshShader("mesh3d");
	size_t graphs = 0;
	for (const auto& entry: std::filesystem::recursive_directory_iterator(findRoot() / "samples")) {
		if (entry.path().extension() != ".owlmat")
			continue;
		const auto asset = MaterialAsset::load(entry.path());
		ASSERT_TRUE(asset.has_value()) << entry.path();
		if (!asset->graph.has_value())
			continue;
		++graphs;
		const auto program = compileMaterialGraph(*asset->graph);
		for (const auto& diagnostic: program.diagnostics) ADD_FAILURE() << entry.path() << ": " << diagnostic.message;
		ASSERT_TRUE(program.isValid()) << entry.path();
		const auto inserted = insertMaterialProgram(source, program);
		ASSERT_TRUE(inserted.has_value());
		EXPECT_TRUE(utils::compileSlangToSpirv(*inserted, "sample_material", /*iForVulkan=*/true).success)
				<< entry.path();
	}
	EXPECT_GE(graphs, 3u);
	core::Log::invalidate();
}
#endif

TEST(MaterialGraph, AssetKeepsTheGraphThroughYaml) {
	MaterialAsset asset;
	asset.graph = richGraph();
	asset.graphValues = {{"sway", "0.3"}};
	const auto read = MaterialAsset::fromYaml(asset.toYaml());
	ASSERT_TRUE(read.has_value());
	ASSERT_TRUE(read->graph.has_value());
	EXPECT_EQ(*read->graph, *asset.graph);
	EXPECT_EQ(read->graphValues, asset.graphValues);

	MaterialAsset child;
	child.parent = "materials/moss.owlmat";
	child.overrides = 0;
	child.graphValues = {{"tint", "1 0 0 1"}};
	const auto readChild = MaterialAsset::fromYaml(child.toYaml());
	ASSERT_TRUE(readChild.has_value());
	EXPECT_FALSE(readChild->graph.has_value());
	EXPECT_EQ(readChild->graphValues, child.graphValues);
}

TEST(MaterialGraph, MaterialPacksTheParameterValues) {
	core::Log::init(core::Log::Level::Off);
	const auto graph = mkShared<const MaterialGraph>(richGraph());
	const auto program = mkShared<const MaterialProgram>(compileMaterialGraph(*graph));
	const auto material = Material{}.withGraph(graph, program, {{"sway", "0.25"}});
	EXPECT_EQ(material.getProgram(), program);
	EXPECT_EQ(material.getGraphValues().at("tint"), "0.8 0.6 0.4 1");
	EXPECT_FLOAT_EQ(material.getGraphUniforms()[0].x(), 0.8f);
	EXPECT_FLOAT_EQ(material.getGraphUniforms()[0].w(), 1.f);
	EXPECT_FLOAT_EQ(material.getGraphUniforms()[1].x(), 0.25f);
	EXPECT_EQ(material.getParameters().maps[0], "textures/a.png");
	EXPECT_EQ(material.getParameters().maps[1], "textures/h.png");
	core::Log::invalidate();
}
