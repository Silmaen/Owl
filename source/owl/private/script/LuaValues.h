/**
 * @file LuaValues.h
 * @author Silmaen
 * @date 08/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/external/lua.h"

#include <cmath>
#include <cstdint>

/**
 * @brief
 *  Whole numbers between the engine and Luau.
 *
 * Luau numbers are doubles: a whole number crosses as one, exact up to 2^53. These helpers keep the 64-bit integers of
 * the engine (handles, counts, cells, saved values) and refuse a fraction where the engine wants a whole number.
 */
namespace owl::script {

/// Bound past which a double no longer converts to a 64-bit integer.
inline constexpr double g_IntegerBound = 9223372036854775808.0;

/**
 * @brief
 *  Push a whole number.
 * @param[in] iState The state.
 * @param[in] iValue The number.
 */
inline void pushInteger(lua_State* iState, const int64_t iValue) {
	lua_pushnumber(iState, static_cast<lua_Number>(iValue));
}

/**
 * @brief
 *  Whether a value is a whole number a 64-bit integer holds.
 * @param[in] iState The state.
 * @param[in] iIndex Stack index.
 * @return True for such a number.
 */
[[nodiscard]] inline auto isInteger(lua_State* iState, const int iIndex) -> bool {
	if (lua_type(iState, iIndex) != LUA_TNUMBER)
		return false;
	const double value = lua_tonumber(iState, iIndex);
	return std::isfinite(value) && value == std::floor(value) && value >= -g_IntegerBound && value < g_IntegerBound;
}

/**
 * @brief
 *  A value as a whole number.
 * @param[in] iState The state.
 * @param[in] iIndex Stack index.
 * @return The number, 0 when the value is no whole number.
 */
[[nodiscard]] inline auto toInteger(lua_State* iState, const int iIndex) -> int64_t {
	return isInteger(iState, iIndex) ? static_cast<int64_t>(lua_tonumber(iState, iIndex)) : 0;
}

/**
 * @brief
 *  A whole-number argument, raising a Lua error for anything else.
 * @param[in] iState The state.
 * @param[in] iIndex Argument index.
 * @return The number.
 */
[[nodiscard]] inline auto checkInteger(lua_State* iState, const int iIndex) -> int64_t {
	static_cast<void>(luaL_checknumber(iState, iIndex));
	if (!isInteger(iState, iIndex))
		luaL_argerror(iState, iIndex, "number has no integer representation");
	return static_cast<int64_t>(lua_tonumber(iState, iIndex));
}

/**
 * @brief
 *  An optional whole-number argument.
 * @param[in] iState The state.
 * @param[in] iIndex Argument index.
 * @param[in] iDefault Value of an absent or nil argument.
 * @return The number.
 */
[[nodiscard]] inline auto optInteger(lua_State* iState, const int iIndex, const int64_t iDefault) -> int64_t {
	return lua_isnoneornil(iState, iIndex) ? iDefault : checkInteger(iState, iIndex);
}

}// namespace owl::script
