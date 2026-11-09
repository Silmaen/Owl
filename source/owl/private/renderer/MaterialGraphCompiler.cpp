/**
 * @file MaterialGraphCompiler.cpp
 * @author Silmaen
 * @date 09/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "renderer/MaterialGraph.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <format>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace owl::renderer {

namespace {

using enum MaterialPinType;

enum struct Stage : uint8_t { Fragment, Vertex };

struct Value {
	std::string expr;
	MaterialPinType type{Float};
};

auto widthOf(const MaterialPinType iType) -> size_t {
	switch (iType) {
		case Float:
			return 1;
		case Float2:
			return 2;
		case Float3:
			return 3;
		case Float4:
			return 4;
		case Any:
			return 0;
	}
	return 0;
}

auto typeOfWidth(const size_t iWidth) -> MaterialPinType {
	switch (iWidth) {
		case 2:
			return Float2;
		case 3:
			return Float3;
		case 4:
			return Float4;
		default:
			return Float;
	}
}

auto slangType(const MaterialPinType iType) -> std::string_view {
	switch (iType) {
		case Float2:
			return "float2";
		case Float3:
			return "float3";
		case Float4:
			return "float4";
		case Float:
		case Any:
			return "float";
	}
	return "float";
}

// A scalar fills every component, a longer vector is truncated, a shorter one gains zeros (and an alpha of 1).
auto convert(const Value& iValue, const MaterialPinType iTo) -> std::string {
	const size_t from = widthOf(iValue.type);
	const size_t to = widthOf(iTo);
	if (from == to || to == 0)
		return iValue.expr;
	if (from == 1)
		return std::format("{}({})", slangType(iTo), iValue.expr);
	if (to == 1)
		return std::format("({}).x", iValue.expr);
	if (to < from)
		return std::format("({}).{}", iValue.expr, std::string_view{"xyzw"}.substr(0, to));
	if (from == 2 && to == 3)
		return std::format("float3({}, 0.0f)", iValue.expr);
	if (from == 2)
		return std::format("float4({}, 0.0f, 1.0f)", iValue.expr);
	return std::format("float4({}, 1.0f)", iValue.expr);
}

auto parseNumbers(const std::string_view iText) -> std::vector<float> {
	std::vector<float> numbers;
	size_t index = 0;
	while (index < iText.size()) {
		while (index < iText.size() &&
			   (std::isspace(static_cast<unsigned char>(iText[index])) != 0 || iText[index] == ','))
			++index;
		if (index >= iText.size())
			break;
		float number = 0.f;
		const auto [end, error] = std::from_chars(iText.data() + index, iText.data() + iText.size(), number);
		if (error != std::errc{})
			break;
		numbers.push_back(number);
		index = static_cast<size_t>(end - iText.data());
	}
	return numbers;
}

auto floatLiteral(const float iValue) -> std::string {
	auto text = std::format("{:.9g}", iValue);
	if (text.find_first_of(".en") == std::string::npos)
		text += ".0";
	return text + "f";
}

// Numbers as a value of `iType` (Any takes as many components as there are numbers).
auto literal(const std::string_view iText, const MaterialPinType iType) -> Value {
	auto numbers = parseNumbers(iText);
	if (numbers.empty())
		numbers.push_back(0.f);
	const size_t width = iType == Any ? std::min<size_t>(numbers.size(), 4) : widthOf(iType);
	if (numbers.size() == 1 && width > 1)
		numbers.resize(width, numbers.front());
	numbers.resize(width, 0.f);
	if (width == 1)
		return {.expr = floatLiteral(numbers.front()), .type = Float};
	std::string expr{slangType(typeOfWidth(width))};
	expr += "(";
	for (size_t index = 0; index < width; ++index) {
		if (index > 0)
			expr += ", ";
		expr += floatLiteral(numbers[index]);
	}
	return {.expr = expr + ")", .type = typeOfWidth(width)};
}

auto identifier(const std::string_view iName) -> std::string {
	std::string out;
	for (const char character: iName)
		out.push_back(std::isalnum(static_cast<unsigned char>(character)) != 0 ? character : '_');
	if (out.empty() || std::isdigit(static_cast<unsigned char>(out.front())) != 0)
		out.insert(out.begin(), '_');
	return out;
}

auto isIdentifier(const std::string_view iName) -> bool {
	return !iName.empty() && identifier(iName) == iName && iName.front() != '_';
}

auto fnv1a(const std::string_view iText) -> uint64_t {
	uint64_t hash = 14695981039346656037ULL;
	for (const char character: iText) {
		hash ^= static_cast<uint8_t>(character);
		hash *= 1099511628211ULL;
	}
	return hash;
}

auto builtin(const std::string_view iName, const Stage iStage) -> std::optional<Value> {
	if (iName == "@uv")
		return Value{.expr = "input.uv", .type = Float2};
	if (iName == "@position")
		return Value{.expr = "(input.position + gScene.origin.xyz)", .type = Float3};
	if (iName == "@normal")
		return Value{.expr = "input.normal", .type = Float3};
	if (iName == "@view")
		return Value{.expr = "normalize(-input.position)", .type = Float3};
	if (iName == "@time")
		return Value{.expr = "gMaterial.graphTime.x", .type = Float};
	if (iName == "@ao")
		return Value{.expr = iStage == Stage::Fragment ? "input.ao" : "1.0f", .type = Float};
	return std::nullopt;
}

// The nodes and links compiled into one function: the graph, or the body of a material function.
struct Scope {
	const std::vector<MaterialGraphNode>* nodes = nullptr;
	const std::vector<MaterialGraphLink>* links = nullptr;
	const MaterialFunction* function = nullptr;
	Stage stage{Stage::Fragment};
	std::string body;
	std::map<std::pair<uint64_t, std::string>, Value> outputs;
	std::set<uint64_t> done;
	std::set<uint64_t> visiting;
};

using Outputs = std::vector<std::pair<std::string, Value>>;

auto makeScope(const std::vector<MaterialGraphNode>& iNodes, const std::vector<MaterialGraphLink>& iLinks,
			   const MaterialFunction* iFunction, const Stage iStage) -> Scope {
	Scope scope;
	scope.nodes = &iNodes;
	scope.links = &iLinks;
	scope.function = iFunction;
	scope.stage = iStage;
	return scope;
}

class Compiler final {
public:
	explicit Compiler(const MaterialGraph& iGraph) : m_graph{iGraph} {
		for (const auto& parameter: m_graph.parameters) {
			if (!isIdentifier(parameter.name)) {
				error(0, std::format("Parameter '{}' needs a Slang name.", parameter.name));
				continue;
			}
			auto& list = parameter.type == MaterialParameterType::Texture ? m_program.textures : m_program.values;
			list.push_back(parameter.name);
		}
		if (m_program.values.size() > g_materialGraphValueCount)
			error(0, std::format("The graph exposes {} number parameters, {} at most.", m_program.values.size(),
								 g_materialGraphValueCount));
		if (m_program.textures.size() > g_materialGraphTextureCount)
			error(0, std::format("The graph exposes {} textures, {} at most.", m_program.textures.size(),
								 g_materialGraphTextureCount));
	}

	auto compile(const std::optional<MaterialPreviewTarget>& iPreview) -> MaterialProgram {
		const MaterialGraphNode* surface = nullptr;
		for (const auto& node: m_graph.nodes) {
			if (node.type != "output.surface")
				continue;
			if (surface != nullptr)
				error(node.id, "The graph has two Surface nodes.");
			surface = &node;
		}
		if (surface == nullptr && !iPreview.has_value())
			error(0, "The graph has no Surface node.");
		std::string fragment;
		std::string vertex;
		if (surface != nullptr || iPreview.has_value()) {
			fragment = iPreview.has_value() ? compilePreview(*iPreview) : compileSurface(*surface);
			vertex = surface != nullptr && !iPreview.has_value() ? compileVertex(*surface) : zeroVertex();
		}
		if (!m_program.diagnostics.empty())
			return std::move(m_program);
		m_program.code =
				std::format("// owl:material-begin\n{}{}{}// owl:material-end\n", m_definitions, fragment, vertex);
		m_program.key = std::format("{:016x}", fnv1a(m_program.code));
		return std::move(m_program);
	}

private:
	void error(const core::UUID iNode, std::string iMessage) {
		m_program.diagnostics.push_back({.node = iNode, .message = std::move(iMessage)});
	}

	[[nodiscard]] auto graphScope(const Stage iStage) const -> Scope {
		return makeScope(m_graph.nodes, m_graph.links, nullptr, iStage);
	}

	auto compileSurface(const MaterialGraphNode& iSurface) -> std::string {
		auto scope = graphScope(Stage::Fragment);
		const auto pins = getMaterialNodePins(m_graph, iSurface);
		auto in = [&](const std::string_view iName, const MaterialPinType iType) -> std::string {
			const auto spec = std::ranges::find(pins.inputs, iName, &MaterialPinSpec::name);
			return convert(input(scope, iSurface, *spec), iType);
		};
		std::string assign;
		assign += std::format("\tm.base = float4({}, {});\n", in("Base Color", Float3), in("Alpha", Float));
		assign += isLinked(scope, iSurface.id, "Normal")
						  ? std::format("\tm.normal = normalize(mul({}, input.frame));\n", in("Normal", Float3))
						  : "\tm.normal = input.normal;\n";
		assign += std::format("\tm.metallic = {};\n", in("Metallic", Float));
		assign += std::format("\tm.roughness = {};\n", in("Roughness", Float));
		assign += std::format("\tm.occlusion = {} * input.ao;\n", in("Occlusion", Float));
		assign += std::format("\tm.emissive = {} * gScene.ambient.w;\n", in("Emissive", Float3));
		assign += std::format("\tm.clearcoat = {};\n", in("Clearcoat", Float));
		assign += std::format("\tm.clearcoatRoughness = {};\n", in("Clearcoat Roughness", Float));
		assign += std::format("\tm.sheenColor = {};\n", in("Sheen Color", Float3));
		assign += std::format("\tm.sheenRoughness = {};\n", in("Sheen Roughness", Float));
		assign += std::format("\tm.anisotropy = {};\n", in("Anisotropy", Float));
		assign += std::format("\tm.anisotropyRotation = {};\n", in("Anisotropy Rotation", Float));
		return std::format("MaterialSurface evaluateSurface(SurfaceInput input) {{\n{}\tMaterialSurface m;\n{}"
						   "\treturn m;\n}}\n\n",
						   scope.body, assign);
	}

	auto compilePreview(const MaterialPreviewTarget& iTarget) -> std::string {
		auto scope = graphScope(Stage::Fragment);
		const auto* node = findNode(scope, iTarget.node);
		std::string color = "float3(0.0f, 0.0f, 0.0f)";
		if (node == nullptr) {
			error(iTarget.node, "The previewed node is not in the graph.");
		} else {
			const auto pins = getMaterialNodePins(m_graph, *node);
			const auto& pinName =
					iTarget.pin.empty() && !pins.outputs.empty() ? pins.outputs.front().name : iTarget.pin;
			if (const auto value = output(scope, *node, pinName); value.has_value())
				color = convert(*value, Float3);
		}
		return std::format("MaterialSurface evaluateSurface(SurfaceInput input) {{\n{}\tMaterialSurface m;\n"
						   "\tm.base = float4({}, 1.0f);\n\tm.normal = input.normal;\n\tm.metallic = 0.0f;\n"
						   "\tm.roughness = 1.0f;\n\tm.occlusion = 1.0f;\n\tm.emissive = float3(0.0f, 0.0f, 0.0f);\n"
						   "\tm.clearcoat = 0.0f;\n\tm.clearcoatRoughness = 0.0f;\n"
						   "\tm.sheenColor = float3(0.0f, 0.0f, 0.0f);\n\tm.sheenRoughness = 0.0f;\n"
						   "\tm.anisotropy = 0.0f;\n\tm.anisotropyRotation = 0.0f;\n\treturn m;\n}}\n\n",
						   scope.body, color);
	}

	auto compileVertex(const MaterialGraphNode& iSurface) -> std::string {
		if (!isLinked(graphScope(Stage::Vertex), iSurface.id, "World Position Offset"))
			return zeroVertex();
		auto scope = graphScope(Stage::Vertex);
		const auto pins = getMaterialNodePins(m_graph, iSurface);
		const auto spec = std::ranges::find(pins.inputs, "World Position Offset", &MaterialPinSpec::name);
		const auto offset = convert(input(scope, iSurface, *spec), Float3);
		m_program.vertexOffset = true;
		return std::format("float3 evaluateVertexOffset(VertexSurfaceInput input) {{\n{}\treturn {};\n}}\n", scope.body,
						   offset);
	}

	static auto zeroVertex() -> std::string {
		return "float3 evaluateVertexOffset(VertexSurfaceInput input) {\n\treturn float3(0.0f, 0.0f, 0.0f);\n}\n";
	}

	static auto findNode(const Scope& iScope, const core::UUID iId) -> const MaterialGraphNode* {
		const auto found = std::ranges::find(*iScope.nodes, iId, &MaterialGraphNode::id);
		return found == iScope.nodes->end() ? nullptr : &*found;
	}

	static auto findLink(const Scope& iScope, const core::UUID iNode, const std::string_view iPin)
			-> const MaterialGraphLink* {
		const auto found = std::ranges::find_if(*iScope.links, [&](const MaterialGraphLink& iLink) -> bool {
			return iLink.toNode == iNode && iLink.toPin == iPin;
		});
		return found == iScope.links->end() ? nullptr : &*found;
	}

	static auto isLinked(const Scope& iScope, const core::UUID iNode, const std::string_view iPin) -> bool {
		return findLink(iScope, iNode, iPin) != nullptr;
	}

	// NOLINTBEGIN(misc-no-recursion): bounded, a node being emitted and a function being called are refused again.
	auto input(Scope& ioScope, const MaterialGraphNode& iNode, const MaterialPinSpec& iPin) -> Value {
		if (const auto* link = findLink(ioScope, iNode.id, iPin.name); link != nullptr) {
			if (const auto* source = findNode(ioScope, link->fromNode); source != nullptr) {
				if (auto value = output(ioScope, *source, link->fromPin); value.has_value())
					return *value;
			} else {
				error(iNode.id, std::format("Input '{}' is linked to a missing node.", iPin.name));
			}
			return literal("0", iPin.type);
		}
		const auto found = iNode.values.find(iPin.name);
		const auto& text = found != iNode.values.end() ? found->second : iPin.defaultValue;
		if (auto value = builtin(text, ioScope.stage); value.has_value())
			return *value;
		return literal(text, iPin.type);
	}

	auto output(Scope& ioScope, const MaterialGraphNode& iNode, const std::string& iPin) -> std::optional<Value> {
		if (!emitNode(ioScope, iNode))
			return std::nullopt;
		const auto found = ioScope.outputs.find({static_cast<uint64_t>(iNode.id), iPin});
		if (found == ioScope.outputs.end()) {
			error(iNode.id, std::format("The node has no output '{}'.", iPin));
			return std::nullopt;
		}
		return found->second;
	}

	auto emitNode(Scope& ioScope, const MaterialGraphNode& iNode) -> bool {
		const auto id = static_cast<uint64_t>(iNode.id);
		if (ioScope.done.contains(id))
			return true;
		if (ioScope.visiting.contains(id)) {
			error(iNode.id, "The node takes its own result: the graph has a loop.");
			return false;
		}
		ioScope.visiting.insert(id);
		const auto pins = getMaterialNodePins(m_graph, iNode, ioScope.function);
		if (findMaterialNodeType(iNode.type) == nullptr)
			error(iNode.id, std::format("Unknown node type '{}'.", iNode.type));
		std::map<std::string, Value> inputs;
		size_t width = 1;
		for (const auto& pin: pins.inputs) {
			auto value = input(ioScope, iNode, pin);
			if (pin.type == Any)
				width = std::max(width, widthOf(value.type));
			inputs.emplace(pin.name, std::move(value));
		}
		const auto resolved = typeOfWidth(width);
		for (const auto& pin: pins.inputs) {
			auto& value = inputs.at(pin.name);
			const auto type = pin.type == Any ? resolved : pin.type;
			value = {.expr = convert(value, type), .type = type};
		}
		const auto outputs = generate(ioScope, iNode, inputs, resolved);
		for (const auto& [name, value]: outputs) {
			const auto local = std::format("v{}", m_counter++);
			ioScope.body += std::format("\t{} {} = {};\n", slangType(value.type), local, value.expr);
			ioScope.outputs[{id, name}] = {.expr = local, .type = value.type};
		}
		ioScope.visiting.erase(id);
		ioScope.done.insert(id);
		return true;
	}

	auto fragmentOnly(const Scope& iScope, const MaterialGraphNode& iNode) -> bool {
		if (iScope.stage == Stage::Fragment)
			return true;
		error(iNode.id, "The node runs in the fragment stage only, not in the world position offset.");
		return false;
	}

	auto textureSlot(const MaterialGraphNode& iNode) -> std::optional<size_t> {
		const auto name = getMaterialNodeValue(iNode, "texture");
		const auto found = std::ranges::find(m_program.textures, name);
		if (found == m_program.textures.end()) {
			error(iNode.id, name.empty() ? std::string{"The node samples no texture parameter."}
										 : std::format("'{}' is not a texture parameter of the graph.", name));
			return std::nullopt;
		}
		return 1 + static_cast<size_t>(found - m_program.textures.begin());
	}

	static auto decode(const std::string& iTexel, const std::string& iSpace) -> std::string {
		if (iSpace == "Color")
			return std::format("float4(authoredColor(({0}).rgb), ({0}).a)", iTexel);
		if (iSpace == "Normal")
			return std::format("float4(({0}).xyz * 2.0f - 1.0f, ({0}).a)", iTexel);
		return iTexel;
	}

	auto temporary(Scope& ioScope, const MaterialPinType iType, const std::string& iExpr) -> std::string {
		const auto local = std::format("v{}", m_counter++);
		ioScope.body += std::format("\t{} {} = {};\n", slangType(iType), local, iExpr);
		return local;
	}

	static auto sampled(const std::string& iTexel) -> Outputs {
		return {{"RGBA", {.expr = iTexel, .type = Float4}},    {"RGB", {.expr = iTexel + ".rgb", .type = Float3}},
				{"R", {.expr = iTexel + ".r", .type = Float}}, {"G", {.expr = iTexel + ".g", .type = Float}},
				{"B", {.expr = iTexel + ".b", .type = Float}}, {"A", {.expr = iTexel + ".a", .type = Float}}};
	}

	// NOLINTNEXTLINE(readability-function-cognitive-complexity): one flat dispatch over the node types.
	auto generate(Scope& ioScope, const MaterialGraphNode& iNode, const std::map<std::string, Value>& iIn,
				  const MaterialPinType iResolved) -> Outputs {
		const auto& type = iNode.type;
		auto in = [&iIn](const std::string& iName) -> const std::string& { return iIn.at(iName).expr; };
		auto one = [](const std::string& iName, std::string iExpr, const MaterialPinType iType) -> Outputs {
			return {{iName, {.expr = std::move(iExpr), .type = iType}}};
		};
		auto result = [&](std::string iExpr) -> Outputs { return one("Result", std::move(iExpr), iResolved); };
		if (type == "input.uv")
			return one("UV", "input.uv", Float2);
		if (type == "input.position")
			return one("Position", builtin("@position", ioScope.stage)->expr, Float3);
		if (type == "input.normal")
			return one("Normal", "input.normal", Float3);
		if (type == "input.view")
			return one("View", builtin("@view", ioScope.stage)->expr, Float3);
		if (type == "input.time")
			return one("Time", "gMaterial.graphTime.x", Float);
		if (type == "input.vertex_occlusion")
			return one("Occlusion", builtin("@ao", ioScope.stage)->expr, Float);
		if (type == "const.float")
			return one("Value", literal(getMaterialNodeValue(iNode, "value"), Float).expr, Float);
		if (type == "const.vector") {
			const auto vector = literal(getMaterialNodeValue(iNode, "value"), Float4).expr;
			return {{"Vector", {.expr = vector, .type = Float4}},
					{"XYZ", {.expr = std::format("({}).xyz", vector), .type = Float3}},
					{"XY", {.expr = std::format("({}).xy", vector), .type = Float2}}};
		}
		if (type == "const.color") {
			auto numbers = parseNumbers(getMaterialNodeValue(iNode, "value"));
			numbers.resize(4, 1.f);
			const auto color = temporary(ioScope, Float4,
										 std::format("float4(authoredColor(float3({}, {}, {})), {})",
													 floatLiteral(numbers[0]), floatLiteral(numbers[1]),
													 floatLiteral(numbers[2]), floatLiteral(numbers[3])));
			return {{"RGB", {.expr = color + ".rgb", .type = Float3}},
					{"RGBA", {.expr = color, .type = Float4}},
					{"Alpha", {.expr = color + ".a", .type = Float}}};
		}
		if (type == "param.get")
			return parameter(iNode);
		if (type == "math.add")
			return result(std::format("({} + {})", in("A"), in("B")));
		if (type == "math.subtract")
			return result(std::format("({} - {})", in("A"), in("B")));
		if (type == "math.multiply")
			return result(std::format("({} * {})", in("A"), in("B")));
		if (type == "math.divide")
			return result(std::format("({} / {})", in("A"), in("B")));
		if (type == "math.min" || type == "math.max" || type == "math.step")
			return result(std::format("{}({}, {})", type.substr(5), in("A"), in("B")));
		if (type == "math.power")
			return result(std::format("pow({}, {})", in("A"), in("B")));
		if (type == "math.lerp")
			return result(std::format("lerp({}, {}, {})", in("A"), in("B"), in("T")));
		if (type == "math.clamp")
			return result(std::format("clamp({}, {}, {})", in("Value"), in("Min"), in("Max")));
		if (type == "math.smoothstep")
			return result(std::format("smoothstep({}, {}, {})", in("Min"), in("Max"), in("Value")));
		if (type == "math.remap")
			return result(std::format("({} + ({} - {}) / ({} - {}) * ({} - {}))", in("Out Min"), in("Value"),
									  in("In Min"), in("In Max"), in("In Min"), in("Out Max"), in("Out Min")));
		if (type == "math.one_minus")
			return result(std::format("(1.0f - {})", in("Value")));
		if (type == "math.negate")
			return result(std::format("(-{})", in("Value")));
		if (type == "math.saturate" || type == "math.abs" || type == "math.sqrt" || type == "math.floor" ||
			type == "math.ceil" || type == "math.frac" || type == "math.sin" || type == "math.cos")
			return result(std::format("{}({})", type.substr(5), in("Value")));
		if (type == "math.normalize")
			return result(iResolved == Float ? std::format("sign({})", in("Value"))
											 : std::format("normalize({})", in("Value")));
		if (type == "math.length")
			return one("Length",
					   iResolved == Float ? std::format("abs({})", in("Value"))
										  : std::format("length({})", in("Value")),
					   Float);
		if (type == "math.distance")
			return one("Distance", std::format("length({} - {})", in("A"), in("B")), Float);
		if (type == "math.dot")
			return one("Dot", std::format("dot({}, {})", in("A"), in("B")), Float);
		if (type == "math.cross")
			return one("Cross", std::format("cross({}, {})", in("A"), in("B")), Float3);
		if (type == "vector.split")
			return {{"X", {.expr = in("Vector") + ".x", .type = Float}},
					{"Y", {.expr = in("Vector") + ".y", .type = Float}},
					{"Z", {.expr = in("Vector") + ".z", .type = Float}},
					{"W", {.expr = in("Vector") + ".w", .type = Float}}};
		if (type == "vector.combine") {
			const auto vector = temporary(ioScope, Float4,
										  std::format("float4({}, {}, {}, {})", in("X"), in("Y"), in("Z"), in("W")));
			return {{"XYZW", {.expr = vector, .type = Float4}},
					{"XYZ", {.expr = vector + ".xyz", .type = Float3}},
					{"XY", {.expr = vector + ".xy", .type = Float2}}};
		}
		if (type == "uv.tiling")
			return one("UV", std::format("({} * {} + {})", in("UV"), in("Tiling"), in("Offset")), Float2);
		if (type == "uv.panner")
			return one("UV", std::format("({} + {} * gMaterial.graphTime.x)", in("UV"), in("Speed")), Float2);
		if (type == "uv.rotate")
			return one("UV", std::format("owlRotateUv({}, {}, {})", in("UV"), in("Center"), in("Angle")), Float2);
		if (type == "texture.sample") {
			const auto slot = textureSlot(iNode);
			if (!slot.has_value())
				return {};
			const auto read = ioScope.stage == Stage::Fragment
									  ? std::format("gTextures[{}].Sample({})", *slot, in("UV"))
									  : std::format("gTextures[{}].SampleLevel({}, 0.0f)", *slot, in("UV"));
			return sampled(temporary(ioScope, Float4, decode(read, getMaterialNodeValue(iNode, "space"))));
		}
		if (type == "texture.triplanar") {
			const auto slot = textureSlot(iNode);
			if (!slot.has_value() || !fragmentOnly(ioScope, iNode))
				return {};
			const auto read = std::format("owlTriplanar({}, {}, {}, {}, {})", *slot, in("Position"), in("Normal"),
										  in("Scale"), in("Sharpness"));
			const auto texel = temporary(ioScope, Float4, decode(read, getMaterialNodeValue(iNode, "space")));
			return {{"RGBA", {.expr = texel, .type = Float4}}, {"RGB", {.expr = texel + ".rgb", .type = Float3}}};
		}
		if (type == "texture.parallax") {
			const auto slot = textureSlot(iNode);
			if (!slot.has_value() || !fragmentOnly(ioScope, iNode))
				return {};
			return one("UV",
					   std::format("owlParallax({}, {}, mul(input.frame, normalize(-input.position)), {})", *slot,
								   in("UV"), in("Height")),
					   Float2);
		}
		if (type == "noise.value")
			return one("Value", std::format("owlValueNoise({} * {})", in("Coordinates"), in("Scale")), Float);
		if (type == "noise.gradient")
			return one("Value", std::format("owlGradientNoise({} * {})", in("Coordinates"), in("Scale")), Float);
		if (type == "noise.fbm")
			return one("Value",
					   std::format("owlFbm({} * {}, {}, {})", in("Coordinates"), in("Scale"),
								   literal(getMaterialNodeValue(iNode, "octaves"), Float).expr, in("Roughness")),
					   Float);
		if (type == "noise.voronoi") {
			const auto cell =
					temporary(ioScope, Float2, std::format("owlVoronoi({} * {})", in("Coordinates"), in("Scale")));
			return {{"Distance", {.expr = cell + ".x", .type = Float}}, {"Cell", {.expr = cell + ".y", .type = Float}}};
		}
		if (type == "effect.fresnel")
			return one("Fresnel",
					   std::format("pow(1.0f - saturate(dot(input.normal, {})), {})",
								   builtin("@view", ioScope.stage)->expr, in("Power")),
					   Float);
		if (type == "effect.height_blend") {
			const auto weight = temporary(ioScope, Float,
										  std::format("owlHeightBlend({}, {}, {}, {})", in("Height A"), in("Height B"),
													  in("Blend"), in("Contrast")));
			return {{"Result", {.expr = std::format("lerp({}, {}, {})", in("A"), in("B"), weight), .type = iResolved}},
					{"Weight", {.expr = weight, .type = Float}}};
		}
		if (type == "effect.detail")
			return one(
					"Color",
					std::format("lerp({0}, saturate({0} * {1} * 2.0f), {2})", in("Base"), in("Detail"), in("Strength")),
					Float3);
		if (type == "normal.blend")
			return one("Normal", std::format("owlBlendNormals({}, {})", in("A"), in("B")), Float3);
		if (type == "normal.strength")
			return one("Normal", std::format("owlNormalStrength({}, {})", in("Normal"), in("Strength")), Float3);
		if (type == "surface.parameters")
			return parameterSurface(ioScope, iNode);
		if (type == "custom")
			return custom(ioScope, iNode, iIn);
		if (type == "function.call")
			return call(ioScope, iNode, iIn);
		if (type == "function.input")
			return functionInputs(ioScope);
		if (type == "output.surface" || type == "function.output")
			error(iNode.id, "The node takes the result and has no output.");
		return {};
	}

	auto parameter(const MaterialGraphNode& iNode) -> Outputs {
		const auto name = getMaterialNodeValue(iNode, "parameter");
		const auto* parameter = m_graph.findParameter(name);
		const auto slot = std::ranges::find(m_program.values, name);
		if (parameter == nullptr || slot == m_program.values.end()) {
			error(iNode.id, name.empty() ? std::string{"The node reads no parameter."}
										 : std::format("'{}' is not a number, vector or colour parameter.", name));
			return {};
		}
		const auto value = std::format("gMaterial.graph[{}]", slot - m_program.values.begin());
		switch (parameter->type) {
			case MaterialParameterType::Float:
				return {{"Value", {.expr = value + ".x", .type = Float}}};
			case MaterialParameterType::Vector:
				return {{"Value", {.expr = value, .type = Float4}}};
			case MaterialParameterType::Color:
				return {{"Color",
						 {.expr = std::format("float4(authoredColor({0}.rgb), {0}.a)", value), .type = Float4}}};
			case MaterialParameterType::Texture:
				break;
		}
		return {};
	}

	auto parameterSurface(Scope& ioScope, const MaterialGraphNode& iNode) -> Outputs {
		if (!fragmentOnly(ioScope, iNode))
			return {};
		const auto base = temporary(ioScope, Float4, "owlParameterBase(input.uv, input.texIndex)");
		auto value = [](std::string iExpr, const MaterialPinType iType) -> Value {
			return {.expr = std::move(iExpr), .type = iType};
		};
		return {{"Base Color", value(base + ".rgb", Float3)},
				{"Alpha", value(base + ".a", Float)},
				{"Normal", value("owlParameterNormal(input.uv)", Float3)},
				{"Metallic", value("owlParameterMetallic(input.uv)", Float)},
				{"Roughness", value("owlParameterRoughness(input.uv)", Float)},
				{"Occlusion", value("owlParameterOcclusion(input.uv)", Float)},
				{"Emissive", value("owlParameterEmissive(input.uv)", Float3)},
				{"Clearcoat", value("gMaterial.layers.x", Float)},
				{"Clearcoat Roughness", value("gMaterial.layers.y", Float)},
				{"Sheen Color", value("authoredColor(gMaterial.sheen.rgb)", Float3)},
				{"Sheen Roughness", value("gMaterial.sheen.w", Float)},
				{"Anisotropy", value("gMaterial.layers.z", Float)},
				{"Anisotropy Rotation", value("gMaterial.layers.w", Float)}};
	}

	// Locals for the outputs of a call, filled by a `void f(in ..., out ...)` function.
	auto callOutputs(Scope& ioScope, const std::vector<MaterialPinSpec>& iOutputs, std::string& oArguments) -> Outputs {
		Outputs outputs;
		for (const auto& output: iOutputs) {
			const auto type = output.type == Any ? Float4 : output.type;
			const auto local = std::format("v{}", m_counter++);
			ioScope.body += std::format("\t{} {};\n", slangType(type), local);
			oArguments += std::format("{}{}", oArguments.empty() ? "" : ", ", local);
			outputs.emplace_back(output.name, Value{.expr = local, .type = type});
		}
		return outputs;
	}

	static auto parameters(const std::vector<MaterialPinSpec>& iInputs, const std::vector<MaterialPinSpec>& iOutputs,
						   std::string_view iPrefix) -> std::string {
		std::string list;
		for (const auto& pin: iInputs)
			list += std::format(", in {} {}", slangType(pin.type == Any ? Float4 : pin.type), identifier(pin.name));
		for (const auto& pin: iOutputs)
			list += std::format(", out {} {}{}", slangType(pin.type == Any ? Float4 : pin.type), iPrefix,
								identifier(pin.name));
		return list.empty() ? list : list.substr(2);
	}

	auto custom(Scope& ioScope, const MaterialGraphNode& iNode, const std::map<std::string, Value>& iIn) -> Outputs {
		const auto* custom = m_graph.findCustomNode(getMaterialNodeValue(iNode, "custom"));
		if (custom == nullptr) {
			error(iNode.id, "The node names no custom node of the graph.");
			return {};
		}
		if (!isIdentifier(custom->name)) {
			error(iNode.id, std::format("Custom node '{}' needs a Slang name.", custom->name));
			return {};
		}
		const auto name = std::format("owlCustom_{}", custom->name);
		if (m_customs.insert(custom->name).second)
			m_definitions += std::format("void {}({}) {{\n{}\n}}\n\n", name,
										 parameters(custom->inputs, custom->outputs, ""), custom->code);
		std::string arguments;
		for (const auto& pin: custom->inputs)
			arguments += std::format("{}{}", arguments.empty() ? "" : ", ", iIn.at(pin.name).expr);
		auto outputs = callOutputs(ioScope, custom->outputs, arguments);
		ioScope.body += std::format("\t{}({});\n", name, arguments);
		return outputs;
	}

	auto call(Scope& ioScope, const MaterialGraphNode& iNode, const std::map<std::string, Value>& iIn) -> Outputs {
		const auto* function = m_graph.findFunction(getMaterialNodeValue(iNode, "function"));
		if (function == nullptr) {
			error(iNode.id, "The node names no function of the graph.");
			return {};
		}
		const auto name = defineFunction(*function, ioScope.stage, iNode.id);
		if (name.empty())
			return {};
		std::string arguments = "input";
		for (const auto& pin: function->inputs) arguments += std::format(", {}", iIn.at(pin.name).expr);
		std::string outs;
		auto outputs = callOutputs(ioScope, function->outputs, outs);
		if (!outs.empty())
			arguments += ", " + outs;
		ioScope.body += std::format("\t{}({});\n", name, arguments);
		return outputs;
	}

	static auto functionInputs(const Scope& iScope) -> Outputs {
		Outputs outputs;
		if (iScope.function == nullptr)
			return outputs;
		for (const auto& pin: iScope.function->inputs)
			outputs.emplace_back(pin.name,
								 Value{.expr = identifier(pin.name), .type = pin.type == Any ? Float4 : pin.type});
		return outputs;
	}

	auto defineFunction(const MaterialFunction& iFunction, const Stage iStage, const core::UUID iCaller)
			-> std::string {
		if (!isIdentifier(iFunction.name)) {
			error(iCaller, std::format("Function '{}' needs a Slang name.", iFunction.name));
			return {};
		}
		auto name = std::format("owlFunction_{}_{}", iFunction.name, iStage == Stage::Fragment ? "fragment" : "vertex");
		if (m_functions.contains(name))
			return name;
		if (std::ranges::find(m_callStack, iFunction.name) != m_callStack.end()) {
			error(iCaller, std::format("Function '{}' calls itself.", iFunction.name));
			return {};
		}
		const auto output =
				std::ranges::find(iFunction.nodes, std::string{"function.output"}, &MaterialGraphNode::type);
		if (output == iFunction.nodes.end()) {
			error(iCaller, std::format("Function '{}' has no Function Outputs node.", iFunction.name));
			return {};
		}
		m_callStack.push_back(iFunction.name);
		auto scope = makeScope(iFunction.nodes, iFunction.links, &iFunction, iStage);
		std::string assign;
		for (const auto& pin: iFunction.outputs) {
			const auto type = pin.type == Any ? Float4 : pin.type;
			assign += std::format("\to_{} = {};\n", identifier(pin.name), convert(input(scope, *output, pin), type));
		}
		m_callStack.pop_back();
		const auto* const inputType = iStage == Stage::Fragment ? "SurfaceInput" : "VertexSurfaceInput";
		auto list = parameters(iFunction.inputs, iFunction.outputs, "o_");
		m_definitions += std::format("void {}({} input{}{}) {{\n{}{}}}\n\n", name, inputType, list.empty() ? "" : ", ",
									 list, scope.body, assign);
		m_functions.insert(name);
		return name;
	}
	// NOLINTEND(misc-no-recursion)

	const MaterialGraph& m_graph;
	MaterialProgram m_program;
	std::string m_definitions;
	std::set<std::string> m_customs;
	std::set<std::string> m_functions;
	std::vector<std::string> m_callStack;
	size_t m_counter = 0;
};

}// namespace

auto compileMaterialGraph(const MaterialGraph& iGraph, const std::optional<MaterialPreviewTarget>& iPreview)
		-> MaterialProgram {
	Compiler compiler{iGraph};
	return compiler.compile(iPreview);
}

}// namespace owl::renderer
