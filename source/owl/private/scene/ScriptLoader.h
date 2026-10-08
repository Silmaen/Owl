/**
 * @file ScriptLoader.h
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include "scene/component/LuaScript.h"

#include <cstdint>

namespace owl::scene {

/**
 * @brief
 *  Load the script of a component into a new instance (open pack first, then the asset directories, then the raw
 *  path) and give it the component's serialized properties. `on_create` is not called.
 * @param[in] iScript The component.
 * @param[in] iEntityId UUID of the entity owning the component.
 * @return The instance, or null when the script cannot be found or does not compile (logged).
 */
auto loadScriptInstance(const component::LuaScript& iScript, uint64_t iEntityId) -> uniq<script::ScriptInstance>;

}// namespace owl::scene
