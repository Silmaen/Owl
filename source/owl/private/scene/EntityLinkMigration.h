/**
 * @file EntityLinkMigration.h
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Serializer.h"

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

}// namespace owl::scene
