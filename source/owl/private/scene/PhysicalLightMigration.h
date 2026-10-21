/**
 * @file PhysicalLightMigration.h
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/Serializer.h"
#include "core/YamlNode.h"

namespace owl::scene {

/**
 * @brief
 *  Migration step of the scene and prefab formats (5 to 6): the light intensities become physical values that look
 *  the same through a camera at the default EV100 (15): a directional `Light` intensity factor becomes lux, a point or
 *  spot one lumens (the same light at one metre), an `Environment` intensity luminance in cd/m².
 * @param[in] ioDocument The document root (with an `Entities` sequence).
 * @return Always true: a document without lights has nothing to convert.
 */
auto convertToPhysicalLights(const core::Serializer& ioDocument) -> bool;

/**
 * @brief
 *  Check whether an entity node carries a light value `convertToPhysicalLights` rewrites.
 * @param[in] iEntity The entity node.
 * @return True when it has a `Light` or an `Environment`.
 */
[[nodiscard]] OWL_API auto hasLegacyLight(const core::YamlNode& iEntity) -> bool;

}// namespace owl::scene
