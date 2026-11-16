/**
 * @file LuaNetBindings.h
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "LuaBindings.h"

#include <vector>

namespace owl::script {

/**
 * @brief
 *  Append the bindings of the `net` table (sessions, lobby, remote procedure calls, replication, lockstep inputs, lag
 *  compensation, figures) to the registry.
 * @param[in,out] ioBindings The registry being declared.
 */
void appendNetworkBindings(std::vector<LuaBinding>& ioBindings);

}// namespace owl::script
