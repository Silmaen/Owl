/**
 * @file lua.h
 * @author Silmaen
 * @date 09/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once
#include "core/Macros.h"

#include <cstdint>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wold-style-cast")
OWL_DIAG_DISABLE_CLANG("-Wzero-as-null-pointer-constant")
#include <lua.h>
#include <luacode.h>
#include <lualib.h>
OWL_DIAG_POP

// Luau names the C functions it pushes; the engine names none, so the two-argument forms of the reference Lua stay.
#undef lua_pushcfunction
#undef lua_pushcclosure

/**
 * @brief
 *  Push a C function.
 * @param[in] iState The state.
 * @param[in] iFunction The function.
 */
inline void lua_pushcfunction(lua_State* iState, const lua_CFunction iFunction) {
	lua_pushcclosurek(iState, iFunction, nullptr, 0, nullptr);
}

/**
 * @brief
 *  Push a C closure over the values on top of the stack.
 * @param[in] iState The state.
 * @param[in] iFunction The function.
 * @param[in] iUpvalues Number of upvalues taken from the stack.
 */
inline void lua_pushcclosure(lua_State* iState, const lua_CFunction iFunction, const int iUpvalues) {
	lua_pushcclosurek(iState, iFunction, nullptr, iUpvalues, nullptr);
}

/**
 * @brief
 *  Push the table of the globals.
 * @param[in] iState The state.
 */
inline void lua_pushglobaltable(lua_State* iState) { lua_pushvalue(iState, LUA_GLOBALSINDEX); }
