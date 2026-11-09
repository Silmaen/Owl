/**
 * @file MaterialGraph.h
 * @author Silmaen
 * @date 09/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/UUID.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/**
 * @brief
 *  Material graphs: the surface of a 3D material written as a graph of Slang nodes.
 *
 * A graph computes the surface of a fragment (base colour, normal, metallic, roughness, emissive, the clearcoat, sheen
 * and anisotropy layers) and the world position offset of a vertex; the lights, the shadows and the shading models stay
 * those of the parameter materials. `compileMaterialGraph` turns it into the two Slang functions of the material block
 * of `common/mesh_surface.slangh`.
 */
namespace owl::renderer {

/// Number of number parameters a graph exposes (one `float4` each in the material block).
inline constexpr size_t g_materialGraphValueCount = 8;

/// Number of texture parameters a graph exposes (the material map slots).
inline constexpr size_t g_materialGraphTextureCount = 6;

/// Renderer folder of the shaders a material graph makes variants of.
inline constexpr std::string_view g_materialGraphRenderer = "renderer3D";

/// The shaders a material graph makes variants of.
inline constexpr std::array<std::string_view, 2> g_materialGraphShaders{"mesh3d", "skinned3d"};

/**
 * @brief
 *  Data carried by a pin.
 */
enum struct MaterialPinType : uint8_t {
	Float,///< One number.
	Float2,///< Two numbers (a uv).
	Float3,///< Three numbers (a colour, a direction, a position).
	Float4,///< Four numbers (a colour and its alpha).
	Any///< As wide as the widest input of the node (the maths nodes).
};

/**
 * @brief
 *  Kind of a node setting.
 */
enum struct MaterialValueType : uint8_t {
	Float,///< A number.
	Vector,///< Four numbers.
	Color,///< An sRGB colour and its alpha, as authored.
	Choice,///< One of the `choices`.
	Name///< The name of a parameter, a custom node or a function of the graph.
};

/**
 * @brief
 *  A pin of a node.
 */
struct OWL_API MaterialPinSpec {
	/// Name, unique on its side of the node.
	std::string name;
	/// Carried data.
	MaterialPinType type{MaterialPinType::Float};
	/// Value of an unlinked input, numbers separated by spaces (a node value of the same name replaces it).
	std::string defaultValue;

	/**
	 * @brief
	 *  Comparison operator.
	 * @param[in] iOther The other pin.
	 * @return True when both pins are the same.
	 */
	auto operator==(const MaterialPinSpec& iOther) const -> bool = default;
};

/**
 * @brief
 *  A setting of a node type, edited in the inspector and saved in the node values.
 */
struct OWL_API MaterialSettingSpec {
	/// Name, the key of the node value.
	std::string name;
	/// Kind of value.
	MaterialValueType type{MaterialValueType::Float};
	/// Value of a node that does not set it.
	std::string defaultValue;
	/// Values of a choice.
	std::vector<std::string> choices;
	/// One-sentence description, shown as the tooltip.
	std::string description;
};

/**
 * @brief
 *  A kind of node: its pins and settings.
 */
struct OWL_API MaterialNodeType {
	/// Stable identifier saved in the graph (`math.multiply`).
	std::string id;
	/// Title shown on the node.
	std::string title;
	/// Palette group (`Inputs`, `Maths`, `Vectors`, `Textures`, `Effects`, `Surface`, `Graph`).
	std::string category;
	/// One-sentence description.
	std::string description;
	/// Input pins, in order.
	std::vector<MaterialPinSpec> inputs;
	/// Output pins, in order.
	std::vector<MaterialPinSpec> outputs;
	/// Settings, in order.
	std::vector<MaterialSettingSpec> settings;
};

/**
 * @brief
 *  A node of a graph.
 */
struct OWL_API MaterialGraphNode {
	/// Node identifier.
	core::UUID id;
	/// Identifier of its `MaterialNodeType`.
	std::string type;
	/// Top-left corner on the canvas.
	std::array<float, 2> position{0.f, 0.f};
	/// Settings and unlinked input values by name (defaults when absent).
	std::map<std::string, std::string> values;

	/**
	 * @brief
	 *  Comparison operator.
	 * @param[in] iOther The other node.
	 * @return True when both nodes are the same.
	 */
	auto operator==(const MaterialGraphNode& iOther) const -> bool = default;
};

/**
 * @brief
 *  A link from an output pin to an input pin.
 */
struct OWL_API MaterialGraphLink {
	/// Source node.
	core::UUID fromNode{0};
	/// Output pin name on the source node.
	std::string fromPin;
	/// Destination node.
	core::UUID toNode{0};
	/// Input pin name on the destination node.
	std::string toPin;

	/**
	 * @brief
	 *  Comparison operator.
	 * @param[in] iOther The other link.
	 * @return True when both links are the same.
	 */
	auto operator==(const MaterialGraphLink& iOther) const -> bool = default;
};

/**
 * @brief
 *  Kind of value a graph exposes.
 */
enum struct MaterialParameterType : uint8_t {
	Float,///< A number.
	Vector,///< Four numbers.
	Color,///< An sRGB colour and its alpha.
	Texture///< A texture, sampled by the texture nodes.
};

/**
 * @brief
 *  A value the graph exposes: a material whose parent is the graph overrides it without a recompile.
 */
struct OWL_API MaterialGraphParameter {
	/// Name, a Slang identifier.
	std::string name;
	/// Kind of value.
	MaterialParameterType type{MaterialParameterType::Float};
	/// Value: numbers separated by spaces, or the asset path of a texture.
	std::string defaultValue;

	/**
	 * @brief
	 *  Comparison operator.
	 * @param[in] iOther The other parameter.
	 * @return True when both parameters are the same.
	 */
	auto operator==(const MaterialGraphParameter& iOther) const -> bool = default;
};

/**
 * @brief
 *  A node its author writes in Slang: typed pins and a body assigning the outputs from the inputs.
 */
struct OWL_API MaterialCustomNode {
	/// Name, a Slang identifier, unique in the graph.
	std::string name;
	/// Title shown on the node, the name when empty.
	std::string title;
	/// Inputs, Slang variables of the body.
	std::vector<MaterialPinSpec> inputs;
	/// Outputs, Slang `out` variables the body assigns.
	std::vector<MaterialPinSpec> outputs;
	/// Slang body.
	std::string code;

	/**
	 * @brief
	 *  Comparison operator.
	 * @param[in] iOther The other node.
	 * @return True when both nodes are the same.
	 */
	auto operator==(const MaterialCustomNode& iOther) const -> bool = default;
};

/**
 * @brief
 *  A reusable sub-graph: its `function.input` node gives the inputs, its `function.output` node takes the outputs.
 */
struct OWL_API MaterialFunction {
	/// Name, a Slang identifier, unique in the graph.
	std::string name;
	/// Inputs of a call.
	std::vector<MaterialPinSpec> inputs;
	/// Outputs of a call.
	std::vector<MaterialPinSpec> outputs;
	/// Nodes of the body.
	std::vector<MaterialGraphNode> nodes;
	/// Links of the body.
	std::vector<MaterialGraphLink> links;

	/**
	 * @brief
	 *  Comparison operator.
	 * @param[in] iOther The other function.
	 * @return True when both functions are the same.
	 */
	auto operator==(const MaterialFunction& iOther) const -> bool = default;
};

/**
 * @brief
 *  A material graph, saved in the `graph` section of a `.owlmat` file.
 */
struct OWL_API MaterialGraph {
	/// Nodes; one `output.surface` takes the result.
	std::vector<MaterialGraphNode> nodes;
	/// Links; an input takes at most one.
	std::vector<MaterialGraphLink> links;
	/// Exposed values.
	std::vector<MaterialGraphParameter> parameters;
	/// Slang nodes of the graph.
	std::vector<MaterialCustomNode> customNodes;
	/// Sub-graphs of the graph.
	std::vector<MaterialFunction> functions;

	/**
	 * @brief
	 *  Find a parameter.
	 * @param[in] iName The parameter.
	 * @return The parameter, null when absent.
	 */
	[[nodiscard]] auto findParameter(std::string_view iName) const -> const MaterialGraphParameter*;

	/**
	 * @brief
	 *  Find a custom node.
	 * @param[in] iName The custom node.
	 * @return The custom node, null when absent.
	 */
	[[nodiscard]] auto findCustomNode(std::string_view iName) const -> const MaterialCustomNode*;

	/**
	 * @brief
	 *  Find a function.
	 * @param[in] iName The function.
	 * @return The function, null when absent.
	 */
	[[nodiscard]] auto findFunction(std::string_view iName) const -> const MaterialFunction*;

	/**
	 * @brief
	 *  Comparison operator.
	 * @param[in] iOther The other graph.
	 * @return True when both graphs are the same.
	 */
	auto operator==(const MaterialGraph& iOther) const -> bool = default;
};

/**
 * @brief
 *  The pins of a node as the graph defines them (a custom node, a function call or a parameter has the pins of its
 *  definition).
 */
struct OWL_API MaterialNodePins {
	/// Inputs, in order.
	std::vector<MaterialPinSpec> inputs;
	/// Outputs, in order.
	std::vector<MaterialPinSpec> outputs;
};

/**
 * @brief
 *  A problem of a graph the compiler found.
 */
struct OWL_API MaterialGraphDiagnostic {
	/// The node, 0 for the graph.
	core::UUID node{0};
	/// What is wrong, one sentence.
	std::string message;
};

/**
 * @brief
 *  What the compiler shows instead of the surface: one output of one node, unlit (the preview of a node).
 */
struct OWL_API MaterialPreviewTarget {
	/// The node.
	core::UUID node{0};
	/// Its output pin, the first when empty.
	std::string pin;
};

/**
 * @brief
 *  A compiled graph: the Slang of the material block and where its parameters go.
 */
struct OWL_API MaterialProgram {
	/// Hash of the code, part of the name of its shaders.
	std::string key;
	/// Slang of the material block (`evaluateSurface`, `evaluateVertexOffset` and their helpers).
	std::string code;
	/// Number parameters, by slot of the material block.
	std::vector<std::string> values;
	/// Texture parameters, by material map slot.
	std::vector<std::string> textures;
	/// Whether the vertex stage moves the vertices.
	bool vertexOffset = false;
	/// Problems found; the code is empty when there is one.
	std::vector<MaterialGraphDiagnostic> diagnostics;

	/**
	 * @brief
	 *  Check whether the graph compiled.
	 * @return True when there is code.
	 */
	[[nodiscard]] auto isValid() const -> bool { return !code.empty(); }
};

/**
 * @brief
 *  The node types of the palette.
 * @return The node types.
 */
[[nodiscard]] OWL_API auto getMaterialNodeTypes() -> const std::vector<MaterialNodeType>&;

/**
 * @brief
 *  Find a node type.
 * @param[in] iId The node type identifier.
 * @return The node type, null when unknown.
 */
[[nodiscard]] OWL_API auto findMaterialNodeType(std::string_view iId) -> const MaterialNodeType*;

/**
 * @brief
 *  The pins of a node.
 * @param[in] iGraph The graph.
 * @param[in] iNode The node.
 * @param[in] iFunction Function whose body holds the node, null for the graph itself.
 * @return The pins, empty for an unknown type.
 */
[[nodiscard]] OWL_API auto getMaterialNodePins(const MaterialGraph& iGraph, const MaterialGraphNode& iNode,
											   const MaterialFunction* iFunction = nullptr) -> MaterialNodePins;

/**
 * @brief
 *  The value of a node setting or unlinked input, its default when the node does not set it.
 * @param[in] iNode The node.
 * @param[in] iName The setting or input.
 * @return The value, empty for an unknown name.
 */
[[nodiscard]] OWL_API auto getMaterialNodeValue(const MaterialGraphNode& iNode, std::string_view iName) -> std::string;

/**
 * @brief
 *  The pre-coded graph of a parameter material: its `surface.parameters` node linked to the surface.
 * @return The graph.
 */
[[nodiscard]] OWL_API auto getParameterMaterialGraph() -> MaterialGraph;

/**
 * @brief
 *  Compile a graph into the Slang of the material block.
 * @param[in] iGraph The graph.
 * @param[in] iPreview The node output shown instead of the surface, none for the material itself.
 * @return The program; its diagnostics say what is wrong when it has no code.
 */
[[nodiscard]] OWL_API auto compileMaterialGraph(const MaterialGraph& iGraph,
												const std::optional<MaterialPreviewTarget>& iPreview = std::nullopt)
		-> MaterialProgram;

/**
 * @brief
 *  Name of the variant of a mesh shader that draws a program, the same in the game and in the export that bakes it.
 * @param[in] iBaseShader The mesh shader (`mesh3d`, `skinned3d`).
 * @param[in] iProgram The program.
 * @return The shader name.
 */
[[nodiscard]] OWL_API auto getMaterialShaderName(std::string_view iBaseShader, const MaterialProgram& iProgram)
		-> std::string;

/**
 * @brief
 *  Put the material block of a program in the source of a mesh shader, between its `owl:material` markers.
 * @param[in] iShaderSource The expanded source of the mesh shader (`mesh3d`, `skinned3d`).
 * @param[in] iProgram The program.
 * @return The source, nothing when the shader has no material block.
 */
[[nodiscard]] OWL_API auto insertMaterialProgram(std::string_view iShaderSource, const MaterialProgram& iProgram)
		-> std::optional<std::string>;

}// namespace owl::renderer
