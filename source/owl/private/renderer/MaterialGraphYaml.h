/**
 * @file MaterialGraphYaml.h
 * @author Silmaen
 * @date 09/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/external/yaml.h"
#include "renderer/MaterialGraph.h"

/**
 * @brief
 *  The `graph` section of a `.owlmat` file.
 */
namespace owl::renderer {

/**
 * @brief
 *  Write a graph as a YAML map.
 * @param[in,out] ioOut The emitter, at a value.
 * @param[in] iGraph The graph.
 */
void emitMaterialGraph(YAML::Emitter& ioOut, const MaterialGraph& iGraph);

/**
 * @brief
 *  Read a graph from a YAML map.
 * @param[in] iNode The map.
 * @return The graph; throws a YAML exception on a malformed entry.
 */
[[nodiscard]] auto parseMaterialGraph(const YAML::Node& iNode) -> MaterialGraph;

}// namespace owl::renderer
