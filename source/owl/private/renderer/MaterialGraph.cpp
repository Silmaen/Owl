/**
 * @file MaterialGraph.cpp
 * @author Silmaen
 * @date 09/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "renderer/MaterialGraph.h"

#include <algorithm>
#include <cstddef>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace owl::renderer {

namespace {

using enum MaterialPinType;

auto pin(std::string iName, const MaterialPinType iType, std::string iDefault = "0") -> MaterialPinSpec {
	return {.name = std::move(iName), .type = iType, .defaultValue = std::move(iDefault)};
}

auto setting(std::string iName, const MaterialValueType iType, std::string iDefault, std::string iDescription,
			 std::vector<std::string> iChoices = {}) -> MaterialSettingSpec {
	return {.name = std::move(iName),
			.type = iType,
			.defaultValue = std::move(iDefault),
			.choices = std::move(iChoices),
			.description = std::move(iDescription)};
}

auto node(std::string iId, std::string iTitle, std::string iCategory, std::string iDescription,
		  std::vector<MaterialPinSpec> iInputs, std::vector<MaterialPinSpec> iOutputs,
		  std::vector<MaterialSettingSpec> iSettings = {}) -> MaterialNodeType {
	return {.id = std::move(iId),
			.title = std::move(iTitle),
			.category = std::move(iCategory),
			.description = std::move(iDescription),
			.inputs = std::move(iInputs),
			.outputs = std::move(iOutputs),
			.settings = std::move(iSettings)};
}

// A maths node of one result as wide as its widest input.
auto anyNode(std::string iId, std::string iTitle, std::string iDescription, std::vector<MaterialPinSpec> iInputs)
		-> MaterialNodeType {
	return node(std::move(iId), std::move(iTitle), "Maths", std::move(iDescription), std::move(iInputs),
				{pin("Result", Any)});
}

auto unary(std::string iId, std::string iTitle, std::string iDescription) -> MaterialNodeType {
	return anyNode(std::move(iId), std::move(iTitle), std::move(iDescription), {pin("Value", Any)});
}

auto binary(std::string iId, std::string iTitle, std::string iDescription, std::string iDefaultB = "0")
		-> MaterialNodeType {
	return anyNode(std::move(iId), std::move(iTitle), std::move(iDescription),
				   {pin("A", Any), pin("B", Any, std::move(iDefaultB))});
}

auto textureSetting() -> MaterialSettingSpec {
	return setting("texture", MaterialValueType::Name, "", "Texture parameter of the graph sampled by the node.");
}

auto spaceSetting() -> MaterialSettingSpec {
	return setting("space", MaterialValueType::Choice, "Color",
				   "Color decodes an sRGB texture, Data reads the values as stored, Normal unpacks a normal map.",
				   {"Color", "Data", "Normal"});
}

auto noiseInputs() -> std::vector<MaterialPinSpec> {
	return {pin("Coordinates", Float3, "@position"), pin("Scale", Float, "1")};
}

auto buildInputs(std::vector<MaterialNodeType>& ioTypes) {
	ioTypes.push_back(node("input.uv", "UV", "Inputs", "Texture coordinates of the mesh.", {}, {pin("UV", Float2)}));
	ioTypes.push_back(node("input.position", "World Position", "Inputs", "World position of the fragment or vertex.",
						   {}, {pin("Position", Float3)}));
	ioTypes.push_back(node("input.normal", "World Normal", "Inputs", "Normal of the mesh, towards the eye.", {},
						   {pin("Normal", Float3)}));
	ioTypes.push_back(node("input.view", "View Direction", "Inputs", "Direction from the surface to the eye.", {},
						   {pin("View", Float3)}));
	ioTypes.push_back(node("input.time", "Time", "Inputs", "Seconds of the scene.", {}, {pin("Time", Float)}));
	ioTypes.push_back(node("input.vertex_occlusion", "Vertex Occlusion", "Inputs",
						   "Occlusion baked in the vertices, 1 in the vertex stage.", {}, {pin("Occlusion", Float)}));
	ioTypes.push_back(node("const.float", "Number", "Inputs", "A number.", {}, {pin("Value", Float)},
						   {setting("value", MaterialValueType::Float, "0", "The number.")}));
	ioTypes.push_back(node("const.vector", "Vector", "Inputs", "Four numbers.", {},
						   {pin("Vector", Float4), pin("XYZ", Float3), pin("XY", Float2)},
						   {setting("value", MaterialValueType::Vector, "0 0 0 0", "The numbers.")}));
	ioTypes.push_back(node("const.color", "Color", "Inputs", "An sRGB colour, decoded to linear.", {},
						   {pin("RGB", Float3), pin("RGBA", Float4), pin("Alpha", Float)},
						   {setting("value", MaterialValueType::Color, "1 1 1 1", "The colour.")}));
	ioTypes.push_back(node("param.get", "Parameter", "Inputs",
						   "A value the graph exposes; a child material overrides it without a recompile.", {}, {},
						   {setting("parameter", MaterialValueType::Name, "", "Number, vector or colour parameter.")}));
}

auto buildMaths(std::vector<MaterialNodeType>& ioTypes) {
	ioTypes.push_back(binary("math.add", "Add", "A + B."));
	ioTypes.push_back(binary("math.subtract", "Subtract", "A - B."));
	ioTypes.push_back(binary("math.multiply", "Multiply", "A × B, component by component.", "1"));
	ioTypes.push_back(binary("math.divide", "Divide", "A / B, component by component.", "1"));
	ioTypes.push_back(binary("math.min", "Min", "The smaller of A and B."));
	ioTypes.push_back(binary("math.max", "Max", "The larger of A and B."));
	ioTypes.push_back(binary("math.power", "Power", "A raised to B.", "1"));
	ioTypes.push_back(binary("math.step", "Step", "1 where B reaches A, else 0."));
	ioTypes.push_back(
			anyNode("math.lerp", "Lerp", "From A to B by T.", {pin("A", Any), pin("B", Any, "1"), pin("T", Any)}));
	ioTypes.push_back(anyNode("math.clamp", "Clamp", "Value kept between Min and Max.",
							  {pin("Value", Any), pin("Min", Any), pin("Max", Any, "1")}));
	ioTypes.push_back(anyNode("math.smoothstep", "Smoothstep", "0 below Min, 1 above Max, smooth in between.",
							  {pin("Min", Any), pin("Max", Any, "1"), pin("Value", Any)}));
	ioTypes.push_back(anyNode("math.remap", "Remap", "Value moved from the In range to the Out range.",
							  {pin("Value", Any), pin("In Min", Any), pin("In Max", Any, "1"), pin("Out Min", Any),
							   pin("Out Max", Any, "1")}));
	ioTypes.push_back(unary("math.saturate", "Saturate", "Value kept between 0 and 1."));
	ioTypes.push_back(unary("math.one_minus", "One Minus", "1 - Value."));
	ioTypes.push_back(unary("math.negate", "Negate", "-Value."));
	ioTypes.push_back(unary("math.abs", "Abs", "Absolute value."));
	ioTypes.push_back(unary("math.sqrt", "Square Root", "Square root."));
	ioTypes.push_back(unary("math.floor", "Floor", "Largest whole number not above Value."));
	ioTypes.push_back(unary("math.ceil", "Ceil", "Smallest whole number not below Value."));
	ioTypes.push_back(unary("math.frac", "Fraction", "Value minus its floor."));
	ioTypes.push_back(unary("math.sin", "Sine", "Sine of radians."));
	ioTypes.push_back(unary("math.cos", "Cosine", "Cosine of radians."));
	ioTypes.push_back(unary("math.normalize", "Normalize", "Vector of length 1."));
	ioTypes.push_back(
			node("math.length", "Length", "Maths", "Length of a vector.", {pin("Value", Any)}, {pin("Length", Float)}));
	ioTypes.push_back(node("math.distance", "Distance", "Maths", "Distance between two points.",
						   {pin("A", Any), pin("B", Any)}, {pin("Distance", Float)}));
	ioTypes.push_back(node("math.dot", "Dot", "Maths", "Dot product.", {pin("A", Float3), pin("B", Float3)},
						   {pin("Dot", Float)}));
	ioTypes.push_back(node("math.cross", "Cross", "Maths", "Cross product.", {pin("A", Float3), pin("B", Float3)},
						   {pin("Cross", Float3)}));
}

auto buildVectors(std::vector<MaterialNodeType>& ioTypes) {
	ioTypes.push_back(node("vector.split", "Split", "Vectors", "The components of a vector.", {pin("Vector", Float4)},
						   {pin("X", Float), pin("Y", Float), pin("Z", Float), pin("W", Float)}));
	ioTypes.push_back(node("vector.combine", "Combine", "Vectors", "A vector of components.",
						   {pin("X", Float), pin("Y", Float), pin("Z", Float), pin("W", Float)},
						   {pin("XYZW", Float4), pin("XYZ", Float3), pin("XY", Float2)}));
	ioTypes.push_back(node("uv.tiling", "Tiling and Offset", "Vectors", "UV × Tiling + Offset.",
						   {pin("UV", Float2, "@uv"), pin("Tiling", Float2, "1 1"), pin("Offset", Float2, "0 0")},
						   {pin("UV", Float2)}));
	ioTypes.push_back(node("uv.panner", "Panner", "Vectors", "UV scrolling with the time.",
						   {pin("UV", Float2, "@uv"), pin("Speed", Float2, "0.1 0")}, {pin("UV", Float2)}));
	ioTypes.push_back(node("uv.rotate", "Rotate UV", "Vectors", "UV turned about a centre by radians.",
						   {pin("UV", Float2, "@uv"), pin("Center", Float2, "0.5 0.5"), pin("Angle", Float)},
						   {pin("UV", Float2)}));
}

auto buildTextures(std::vector<MaterialNodeType>& ioTypes) {
	const std::vector<MaterialPinSpec> sampled{pin("RGBA", Float4), pin("RGB", Float3), pin("R", Float),
											   pin("G", Float),     pin("B", Float),    pin("A", Float)};
	ioTypes.push_back(node("texture.sample", "Sample Texture", "Textures", "A texture parameter read at a uv.",
						   {pin("UV", Float2, "@uv")}, sampled, {textureSetting(), spaceSetting()}));
	ioTypes.push_back(node("texture.triplanar", "Triplanar", "Textures",
						   "A texture projected along the world axes, without uv seams (fragment stage).",
						   {pin("Position", Float3, "@position"), pin("Normal", Float3, "@normal"),
							pin("Scale", Float, "1"), pin("Sharpness", Float, "4")},
						   {pin("RGBA", Float4), pin("RGB", Float3)}, {textureSetting(), spaceSetting()}));
	ioTypes.push_back(node("texture.parallax", "Parallax", "Textures",
						   "The uv shifted by a height texture towards the eye (fragment stage).",
						   {pin("UV", Float2, "@uv"), pin("Height", Float, "0.05")}, {pin("UV", Float2)},
						   {textureSetting()}));
}

auto buildEffects(std::vector<MaterialNodeType>& ioTypes) {
	ioTypes.push_back(node("noise.value", "Value Noise", "Effects", "Smooth random values in [0, 1].", noiseInputs(),
						   {pin("Value", Float)}));
	ioTypes.push_back(node("noise.gradient", "Gradient Noise", "Effects", "Perlin noise in [0, 1].", noiseInputs(),
						   {pin("Value", Float)}));
	auto fbmInputs = noiseInputs();
	fbmInputs.push_back(pin("Roughness", Float, "0.5"));
	ioTypes.push_back(node("noise.fbm", "Fractal Noise", "Effects", "Octaves of gradient noise in [0, 1].", fbmInputs,
						   {pin("Value", Float)},
						   {setting("octaves", MaterialValueType::Float, "4", "Number of octaves, 1 to 8.")}));
	ioTypes.push_back(node("noise.voronoi", "Voronoi", "Effects",
						   "Distance to the nearest cell point and a random value per cell.", noiseInputs(),
						   {pin("Distance", Float), pin("Cell", Float)}));
	ioTypes.push_back(node("effect.fresnel", "Fresnel", "Effects", "1 at grazing angles, 0 facing the eye.",
						   {pin("Power", Float, "5")}, {pin("Fresnel", Float)}));
	ioTypes.push_back(node("effect.height_blend", "Height Blend", "Effects",
						   "B shows where its height rises above A's: sand between stones, moss in the cracks.",
						   {pin("A", Any), pin("B", Any, "1"), pin("Height A", Float, "0.5"),
							pin("Height B", Float, "0.5"), pin("Blend", Float, "0.5"), pin("Contrast", Float, "0.1")},
						   {pin("Result", Any), pin("Weight", Float)}));
	ioTypes.push_back(
			node("effect.detail", "Detail Blend", "Effects",
				 "A detail colour over a base colour, 0.5 grey leaving it unchanged.",
				 {pin("Base", Float3, "1 1 1"), pin("Detail", Float3, "0.5 0.5 0.5"), pin("Strength", Float, "1")},
				 {pin("Color", Float3)}));
	ioTypes.push_back(node("normal.blend", "Blend Normals", "Effects", "Two tangent-space normals combined.",
						   {pin("A", Float3, "0 0 1"), pin("B", Float3, "0 0 1")}, {pin("Normal", Float3)}));
	ioTypes.push_back(node("normal.strength", "Normal Strength", "Effects",
						   "A tangent-space normal flatter below 1, deeper above.",
						   {pin("Normal", Float3, "0 0 1"), pin("Strength", Float, "1")}, {pin("Normal", Float3)}));
}

auto buildSurface(std::vector<MaterialNodeType>& ioTypes) {
	ioTypes.push_back(
			node("surface.parameters", "Parameter Surface", "Surface",
				 "The surface of a parameter material: its colours, factors and maps.", {},
				 {pin("Base Color", Float3), pin("Alpha", Float), pin("Normal", Float3), pin("Metallic", Float),
				  pin("Roughness", Float), pin("Occlusion", Float), pin("Emissive", Float3), pin("Clearcoat", Float),
				  pin("Clearcoat Roughness", Float), pin("Sheen Color", Float3), pin("Sheen Roughness", Float),
				  pin("Anisotropy", Float), pin("Anisotropy Rotation", Float)}));
	ioTypes.push_back(
			node("output.surface", "Surface", "Surface",
				 "The result: the surface of a fragment, and the world position offset of a vertex.",
				 {pin("Base Color", Float3, "0.8 0.8 0.8"), pin("Alpha", Float, "1"), pin("Normal", Float3, "0 0 1"),
				  pin("Metallic", Float), pin("Roughness", Float, "0.5"), pin("Occlusion", Float, "1"),
				  pin("Emissive", Float3, "0 0 0"), pin("Clearcoat", Float), pin("Clearcoat Roughness", Float),
				  pin("Sheen Color", Float3, "0 0 0"), pin("Sheen Roughness", Float), pin("Anisotropy", Float),
				  pin("Anisotropy Rotation", Float), pin("World Position Offset", Float3, "0 0 0")},
				 {}));
}

auto buildGraphNodes(std::vector<MaterialNodeType>& ioTypes) {
	ioTypes.push_back(node("custom", "Custom", "Graph", "A node written in Slang in the Custom tab.", {}, {},
						   {setting("custom", MaterialValueType::Name, "", "Custom node of the graph.")}));
	ioTypes.push_back(node("function.call", "Function", "Graph", "A material function of the graph.", {}, {},
						   {setting("function", MaterialValueType::Name, "", "Function of the graph.")}));
	ioTypes.push_back(
			node("function.input", "Function Inputs", "Graph", "The inputs of the function being edited.", {}, {}));
	ioTypes.push_back(
			node("function.output", "Function Outputs", "Graph", "The outputs of the function being edited.", {}, {}));
}

auto buildNodeTypes() -> std::vector<MaterialNodeType> {
	std::vector<MaterialNodeType> types;
	buildInputs(types);
	buildMaths(types);
	buildVectors(types);
	buildTextures(types);
	buildEffects(types);
	buildSurface(types);
	buildGraphNodes(types);
	return types;
}

auto parameterPin(const MaterialGraphParameter& iParameter) -> std::optional<MaterialPinSpec> {
	switch (iParameter.type) {
		case MaterialParameterType::Float:
			return pin("Value", Float);
		case MaterialParameterType::Vector:
			return pin("Value", Float4);
		case MaterialParameterType::Color:
			return pin("Color", Float4);
		case MaterialParameterType::Texture:
			return std::nullopt;
	}
	return std::nullopt;
}

template<typename T>
auto findNamed(const std::vector<T>& iItems, const std::string_view iName) -> const T* {
	const auto found = std::ranges::find(iItems, iName, &T::name);
	return found == iItems.end() ? nullptr : &*found;
}

constexpr std::string_view g_beginMarker = "// owl:material-begin";
constexpr std::string_view g_endMarker = "// owl:material-end";

}// namespace

auto MaterialGraph::findParameter(const std::string_view iName) const -> const MaterialGraphParameter* {
	return findNamed(parameters, iName);
}

auto MaterialGraph::findCustomNode(const std::string_view iName) const -> const MaterialCustomNode* {
	return findNamed(customNodes, iName);
}

auto MaterialGraph::findFunction(const std::string_view iName) const -> const MaterialFunction* {
	return findNamed(functions, iName);
}

auto getMaterialNodeTypes() -> const std::vector<MaterialNodeType>& {
	static const auto s_types = buildNodeTypes();
	return s_types;
}

auto findMaterialNodeType(const std::string_view iId) -> const MaterialNodeType* {
	const auto& types = getMaterialNodeTypes();
	const auto found = std::ranges::find(types, iId, &MaterialNodeType::id);
	return found == types.end() ? nullptr : &*found;
}

auto getMaterialNodePins(const MaterialGraph& iGraph, const MaterialGraphNode& iNode, const MaterialFunction* iFunction)
		-> MaterialNodePins {
	const auto* type = findMaterialNodeType(iNode.type);
	if (type == nullptr)
		return {};
	if (iNode.type == "param.get") {
		const auto* parameter = iGraph.findParameter(getMaterialNodeValue(iNode, "parameter"));
		if (parameter == nullptr)
			return {};
		if (const auto output = parameterPin(*parameter); output.has_value())
			return {.inputs = {}, .outputs = {*output}};
		return {};
	}
	if (iNode.type == "custom") {
		if (const auto* custom = iGraph.findCustomNode(getMaterialNodeValue(iNode, "custom")); custom != nullptr)
			return {.inputs = custom->inputs, .outputs = custom->outputs};
		return {};
	}
	if (iNode.type == "function.call") {
		if (const auto* function = iGraph.findFunction(getMaterialNodeValue(iNode, "function")); function != nullptr)
			return {.inputs = function->inputs, .outputs = function->outputs};
		return {};
	}
	if (iNode.type == "function.input")
		return iFunction == nullptr ? MaterialNodePins{} : MaterialNodePins{.inputs = {}, .outputs = iFunction->inputs};
	if (iNode.type == "function.output")
		return iFunction == nullptr ? MaterialNodePins{}
									: MaterialNodePins{.inputs = iFunction->outputs, .outputs = {}};
	return {.inputs = type->inputs, .outputs = type->outputs};
}

auto getMaterialNodeValue(const MaterialGraphNode& iNode, const std::string_view iName) -> std::string {
	if (const auto found = iNode.values.find(std::string{iName}); found != iNode.values.end())
		return found->second;
	const auto* type = findMaterialNodeType(iNode.type);
	if (type == nullptr)
		return {};
	if (const auto setting = std::ranges::find(type->settings, iName, &MaterialSettingSpec::name);
		setting != type->settings.end())
		return setting->defaultValue;
	if (const auto input = std::ranges::find(type->inputs, iName, &MaterialPinSpec::name); input != type->inputs.end())
		return input->defaultValue;
	return {};
}

auto getParameterMaterialGraph() -> MaterialGraph {
	MaterialGraph graph;
	const core::UUID surfaceId{1};
	const core::UUID outputId{2};
	graph.nodes.push_back({.id = surfaceId, .type = "surface.parameters", .position = {0.f, 0.f}, .values = {}});
	graph.nodes.push_back({.id = outputId, .type = "output.surface", .position = {360.f, 0.f}, .values = {}});
	for (const auto& output: findMaterialNodeType("surface.parameters")->outputs)
		graph.links.push_back(
				{.fromNode = surfaceId, .fromPin = output.name, .toNode = outputId, .toPin = output.name});
	return graph;
}

auto getMaterialShaderName(const std::string_view iBaseShader, const MaterialProgram& iProgram) -> std::string {
	return std::format("{}_material_{}", iBaseShader, iProgram.key);
}

auto insertMaterialProgram(const std::string_view iShaderSource, const MaterialProgram& iProgram)
		-> std::optional<std::string> {
	const auto begin = iShaderSource.find(g_beginMarker);
	const auto end = begin == std::string_view::npos ? begin : iShaderSource.find(g_endMarker, begin);
	if (end == std::string_view::npos) {
		OWL_CORE_WARN("MaterialGraph: The shader has no material block.")
		return std::nullopt;
	}
	std::string source{iShaderSource.substr(0, begin)};
	source.append(iProgram.code);
	source.append(iShaderSource.substr(end + g_endMarker.size()));
	return source;
}

}// namespace owl::renderer
