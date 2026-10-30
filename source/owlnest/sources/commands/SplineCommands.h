/**
 * @file SplineCommands.h
 * @author Silmaen
 * @date 30/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

namespace owl::nest::commands {

class CommandRegistry;

/**
 * @brief
 *  Register the `spline.*` commands that edit the points of the `Spline` of an entity (add, insert, delete, move,
 *  set), each one undo entry.
 * @param[in,out] ioRegistry The registry that receives them.
 */
void addSplineCommands(CommandRegistry& ioRegistry);

}// namespace owl::nest::commands
