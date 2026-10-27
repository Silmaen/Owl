/**
 * @file BindingNodes.h
 * @author Silmaen
 * @date 27/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "script/visual/VisualScript.h"

#include <vector>

namespace owl::script::visual {

/**
 * @brief
 *  One node type per binding of the typed registry, empty when the engine is built without scripting.
 * @return The binding nodes, in reference order.
 */
[[nodiscard]] auto getBindingNodeTypes() -> std::vector<NodeType>;

}// namespace owl::script::visual
