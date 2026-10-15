/**
 * @file EntityLinkMigration.h
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/FormatVersion.h"
#include "core/Serializer.h"
#include "core/YamlNode.h"

namespace owl::scene {

/**
 * @brief
 *  Migration step of the scene and prefab formats (1 to 2): give every `EntityLink` that only names its target
 *  the UUID of the first entity of the document with that tag.
 *
 * Links whose target is not in the document keep their name only and bind by name at runtime.
 * @param[in] ioDocument The document root (with an `Entities` sequence).
 * @return Always true: a document without entities or links has nothing to convert.
 */
auto bindEntityLinksByName(const core::Serializer& ioDocument) -> bool;

/**
 * @brief
 *  Check whether a scene or prefab document reads as is, without going through its yaml-cpp migration chain: it is
 *  at the current format version, or the steps from its version have nothing to convert (`bindEntityLinksByName`:
 *  no `EntityLink` naming its target without an id; `centreLegacyUiText`: no `UiText` without `horizontalAlignment`;
 *  `convertLegacyHudLayout`: no `UiRect` without `anchorMin` nor `Canvas` without `scaleMode`).
 * @param[in] iRoot The document root.
 * @param[in] iFormat The format of the document (scene or prefab, current version 4).
 * @return True when the document can be read directly.
 */
[[nodiscard]] OWL_API auto canReadWithoutMigration(const core::YamlNode& iRoot, const core::DocumentFormat& iFormat)
		-> bool;

}// namespace owl::scene
