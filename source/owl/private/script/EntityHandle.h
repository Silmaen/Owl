/**
 * @file EntityHandle.h
 * @author Silmaen
 * @date 08/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/UUID.h"
#include "scene/Entity.h"

#include <cstdint>
#include <optional>

/**
 * @brief
 *  Entities as scripts see them.
 *
 * A script holds a handle, not a UUID: the registry id of the entity plus one, 0 standing for no entity, with bit 32
 * set for a widget of the scene's HUD. A handle fits a Lua number exactly, compares and keys tables like one, and stays
 * valid while the entity lives in this Play session; a destroyed entity's handle resolves to nothing, even once its
 * slot is reused.
 */
namespace owl::script {

/// Bit marking a handle of the HUD registry.
inline constexpr uint64_t g_HudEntityHandleBit = uint64_t{1} << 32U;

/**
 * @brief
 *  Handle of an entity of a scene or of its HUD.
 * @param[in] iScene The scene scripts are bound to.
 * @param[in] iEntity The entity, of the scene or of its HUD.
 * @return Its handle, 0 for an invalid entity.
 */
[[nodiscard]] OWL_API auto getEntityHandle(const scene::Scene& iScene, const scene::Entity& iEntity) -> uint64_t;

/**
 * @brief
 *  Handle of the entity of a UUID, looked up in the scene then in its HUD.
 * @param[in] iScene The scene scripts are bound to.
 * @param[in] iUuid The UUID.
 * @return Its handle, 0 when no entity has it.
 */
[[nodiscard]] OWL_API auto getEntityHandle(const scene::Scene& iScene, core::UUID iUuid) -> uint64_t;

/**
 * @brief
 *  Entity of a handle.
 * @param[in] iScene The scene scripts are bound to.
 * @param[in] iHandle The handle.
 * @return The living entity, or nothing.
 */
[[nodiscard]] OWL_API auto resolveEntityHandle(scene::Scene& iScene, uint64_t iHandle) -> std::optional<scene::Entity>;

/**
 * @brief
 *  UUID of the entity of a handle.
 * @param[in] iScene The scene scripts are bound to.
 * @param[in] iHandle The handle.
 * @return Its UUID, 0 when the handle names no living entity.
 */
[[nodiscard]] OWL_API auto getEntityHandleUuid(scene::Scene& iScene, uint64_t iHandle) -> core::UUID;

}// namespace owl::script
