/**
 * @file WalkAreaCommands.h
 * @author Silmaen
 * @date 31/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <gui/component/render.h>

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace owl::nest::commands {

class CommandRegistry;

/**
 * @brief
 *  Register the `walk.*` commands that edit the floors of the `WalkArea` of an entity (add a floor or a hole, insert,
 *  move, set and delete vertices, delete a ring), each one undo entry.
 * @param[in,out] ioRegistry The registry that receives them.
 */
void addWalkAreaCommands(CommandRegistry& ioRegistry);

/**
 * @brief
 *  Format walk area vertices as `polygon:ring:index` items separated by commas (`0:0:2,0:1:0`).
 * @param[in] iVertices The vertices.
 * @return The text.
 */
[[nodiscard]] auto formatWalkVertices(std::span<const gui::component::WalkAreaSelection::Vertex> iVertices)
		-> std::string;

/**
 * @brief
 *  Read walk area vertices as `formatWalkVertices` writes them.
 * @param[in] iText The text.
 * @return The vertices, nothing when the text is not such a list.
 */
[[nodiscard]] auto parseWalkVertices(std::string_view iText)
		-> std::optional<std::vector<gui::component::WalkAreaSelection::Vertex>>;

}// namespace owl::nest::commands
