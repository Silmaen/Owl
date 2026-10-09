/**
 * @file LuaBindings.h
 * @author Silmaen
 * @date 09/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

struct lua_State;

namespace owl::scene {
class Scene;
}// namespace owl::scene

namespace owl::script {

/**
 * @brief
 *  Type of a value a binding takes or returns, as documented (and, later, as a visual-scripting pin).
 */
enum struct LuaType : uint8_t {
	Boolean,///< `true` / `false`.
	Integer,///< Lua integer.
	Number,///< Lua number (an integer is accepted).
	String,///< Lua string.
	Table,///< Lua table.
	Entity,///< Entity UUID (integer, 0 for none).
	Any///< Any of the above.
};

/**
 * @brief
 *  A named, typed parameter or return value of a binding.
 */
struct LuaValue {
	/// Name shown in the reference.
	std::string_view name;
	/// Type.
	LuaType type{LuaType::Any};
	/// Whether a parameter may be left out.
	bool optional{false};
};

/**
 * @brief
 *  A C function callable from Lua (`lua_CFunction`).
 */
using LuaFunction = int (*)(lua_State*);

/**
 * @brief
 *  Declaration of one Lua binding: the function scripts call and its documentation.
 *
 * The registry (getLuaBindings) is the single source of the registered functions and of the generated reference
 * page `doc/pages/lua-api.md`.
 */
struct LuaBinding {
	/// Lua table holding the function (`transform`).
	std::string_view table;
	/// Function name in the table (`get_position`).
	std::string_view name;
	/// The C function, under `LuaEngine::callGuarded`; it calls `luaL_check*` before creating any C++ object.
	LuaFunction function{nullptr};
	/// One-sentence description.
	std::string_view description;
	/// Parameters, in order.
	std::vector<LuaValue> params;
	/// Returned values, in order.
	std::vector<LuaValue> returns;
};

/**
 * @brief
 *  A Lua table of bindings.
 */
struct LuaTable {
	/// Table name.
	std::string_view name;
	/// One-sentence description.
	std::string_view description;
};

/**
 * @brief
 *  Every binding, in reference order.
 * @return The bindings.
 */
OWL_API auto getLuaBindings() -> const std::vector<LuaBinding>&;

/**
 * @brief
 *  Every bound table, in reference order.
 * @return The tables.
 */
OWL_API auto getLuaTables() -> const std::vector<LuaTable>&;

/**
 * @brief
 *  Generate the Markdown reference page of the bindings (`doc/pages/lua-api.md`).
 * @return The page.
 */
OWL_API auto generateLuaReference() -> std::string;

/**
 * @brief
 *  Register every binding of getLuaBindings into a Lua state, one global table per LuaTable, each function
 *  under the exception trampoline of `LuaEngine::callGuarded`.
 * @param[in] iState The Lua state to register bindings into.
 */
OWL_API void registerBindings(lua_State* iState);

/**
 * @brief
 *  Bind a Lua state to the scene its bindings act on (host pointer of its LuaEngine).
 * @param[in] iState The Lua state.
 * @param[in] iScene The scene, or nullptr to unbind.
 */
OWL_API void setBoundScene(lua_State* iState, scene::Scene* iScene);

/**
 * @brief
 *  Scene a Lua state is bound to.
 * @param[in] iState The Lua state.
 * @return The scene given to `setBoundScene`, or nullptr.
 */
OWL_API auto getBoundScene(lua_State* iState) -> scene::Scene*;

}// namespace owl::script
