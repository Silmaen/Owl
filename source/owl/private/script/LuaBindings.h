/**
 * @file LuaBindings.h
 * @author Silmaen
 * @date 09/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

struct lua_State;

namespace owl::script {

/**
 * @brief Register all engine API bindings into a Lua state.
 *
 * Creates the following Lua tables: entity, transform, physics, input, sound, scene, time, log.
 *
 * @param[in] iState The Lua state to register bindings into.
 */
OWL_API void registerBindings(lua_State* iState);

}// namespace owl::script
