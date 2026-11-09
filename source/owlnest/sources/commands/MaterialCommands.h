/**
 * @file MaterialCommands.h
 * @author Silmaen
 * @date 09/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <core/UUID.h>
#include <renderer/Material.h>
#include <renderer/MaterialGraph.h>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace owl::nest::commands {

class CommandRegistry;

/**
 * @brief
 *  A `.owlmat` material open in an editor tab, edited by the `material.*` commands through its undo history.
 */
class MaterialEditor {
public:
	MaterialEditor() = default;

	MaterialEditor(const MaterialEditor&) = default;

	MaterialEditor(MaterialEditor&&) = default;

	auto operator=(const MaterialEditor&) -> MaterialEditor& = default;

	auto operator=(MaterialEditor&&) -> MaterialEditor& = default;

	/**
	 * @brief
	 *  Destructor.
	 */
	virtual ~MaterialEditor();

	/**
	 * @brief
	 *  The material as it is edited.
	 * @return The asset.
	 */
	[[nodiscard]] virtual auto material() const -> const renderer::MaterialAsset& = 0;

	/**
	 * @brief
	 *  Replace the material as one undoable step.
	 * @param[in] iAsset The material after the edit.
	 * @param[in] iDescription Name of the step in the history.
	 * @param[in] iMerge Whether the step joins the previous one of the same name (a slider drag).
	 * @return False when nothing changes.
	 */
	virtual auto setMaterial(const renderer::MaterialAsset& iAsset, const std::string& iDescription, bool iMerge)
			-> bool = 0;

	/**
	 * @brief
	 *  Choose the parent material, or none.
	 * @param[in] iParent Asset path of the parent `.owlmat`, empty for a root material.
	 * @return True when it changed.
	 */
	virtual auto setMaterialParent(const std::filesystem::path& iParent) -> bool = 0;

	/**
	 * @brief
	 *  Replace the values and the overridden fields.
	 * @param[in] iValues The new values.
	 * @param[in] iOverrides Mask of the fields the material sets (used with a parent).
	 * @param[in] iMerge True for a drag of a slider.
	 * @return True when it changed.
	 */
	virtual auto setMaterialValues(const renderer::MaterialParameters& iValues, uint32_t iOverrides, bool iMerge)
			-> bool = 0;
};

/**
 * @brief
 *  Name of a pin type (`Float`, `Float2`, `Float3`, `Float4`, `Any`).
 * @param[in] iType The type.
 * @return The name.
 */
[[nodiscard]] auto getMaterialPinTypeName(renderer::MaterialPinType iType) -> std::string_view;

/**
 * @brief
 *  Name of a parameter type (`Float`, `Vector`, `Color`, `Texture`).
 * @param[in] iType The type.
 * @return The name.
 */
[[nodiscard]] auto getMaterialParameterTypeName(renderer::MaterialParameterType iType) -> std::string_view;

/**
 * @brief
 *  Write typed pins as the `inputs` / `outputs` argument of the commands: `name:Type=default`, separated by commas.
 * @param[in] iPins The pins.
 * @return The text.
 */
[[nodiscard]] auto formatMaterialPins(std::span<const renderer::MaterialPinSpec> iPins) -> std::string;

/**
 * @brief
 *  Read typed pins written by `formatMaterialPins` (the default is optional, the type is `Float` when absent).
 * @param[in] iText The text.
 * @return The pins, nothing when a name is not a Slang identifier, is repeated, or a type is unknown.
 */
[[nodiscard]] auto parseMaterialPins(std::string_view iText) -> std::optional<std::vector<renderer::MaterialPinSpec>>;

/**
 * @brief
 *  Write node identifiers as the `nodes` argument of the commands (decimal, separated by spaces).
 * @param[in] iNodes The nodes.
 * @return The text.
 */
[[nodiscard]] auto formatMaterialNodes(std::span<const core::UUID> iNodes) -> std::string;

/**
 * @brief
 *  The text the clipboard holds for some nodes of a graph or of a function body: those nodes, the links between
 *  them and the parameters, custom nodes and functions they name.
 * @param[in] iGraph The graph.
 * @param[in] iFunction The function whose body holds the nodes, empty for the graph itself.
 * @param[in] iNodes The nodes copied.
 * @return The text, empty when no node is copied.
 */
[[nodiscard]] auto copyMaterialNodes(const renderer::MaterialGraph& iGraph, const std::string& iFunction,
									 std::span<const core::UUID> iNodes) -> std::string;

/**
 * @brief
 *  Find the nodes of a graph or of a function body.
 * @param[in] iGraph The graph.
 * @param[in] iFunction The function, empty for the graph itself.
 * @return The nodes, null for an unknown function.
 */
[[nodiscard]] auto getMaterialScopeNodes(const renderer::MaterialGraph& iGraph, const std::string& iFunction)
		-> const std::vector<renderer::MaterialGraphNode>*;

/**
 * @brief
 *  Find the links of a graph or of a function body.
 * @param[in] iGraph The graph.
 * @param[in] iFunction The function, empty for the graph itself.
 * @return The links, null for an unknown function.
 */
[[nodiscard]] auto getMaterialScopeLinks(const renderer::MaterialGraph& iGraph, const std::string& iFunction)
		-> const std::vector<renderer::MaterialGraphLink>*;

/**
 * @brief
 *  Register `material.convert_to_graph`, `material.convert_to_parameters`, `material.set_parent`,
 *  `material.set_values`, the node commands (`material.add_node`, `remove_nodes`, `move_nodes`, `link`, `unlink`,
 *  `set_node_value`, `paste_nodes`), the parameter, custom node and function commands, and `material.set_graph_value`.
 * @param[in,out] ioRegistry The registry.
 */
void addMaterialCommands(CommandRegistry& ioRegistry);

}// namespace owl::nest::commands
