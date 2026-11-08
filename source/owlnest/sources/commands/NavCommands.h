/**
 * @file NavCommands.h
 * @author Silmaen
 * @date 08/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

namespace owl::nest::commands {

class CommandRegistry;

/**
 * @brief
 *  Register the `nav.*` commands: `nav.bake` cuts the navigation mesh of a `NavMeshVolume` again, one undo entry.
 * @param[in,out] ioRegistry The registry that receives them.
 */
void addNavCommands(CommandRegistry& ioRegistry);

}// namespace owl::nest::commands
