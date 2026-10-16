/**
 * @file UiRendererTagMigration.h
 * @author Silmaen
 * @date 16/10/2026
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
 *  Migration step of the scene and prefab formats (4 to 5): the `RendererTag` naming the `ui` layer is removed from
 *  every canvas and widget, since the HUD pass draws them whatever their tag.
 * @param[in] ioDocument The document root (with an `Entities` sequence).
 * @return Always true: a document without such a tag has nothing to convert.
 */
auto dropUiRendererTag(const core::Serializer& ioDocument) -> bool;

/**
 * @brief
 *  Check whether an entity node is a canvas or a widget tagged for the `ui` layer.
 * @param[in] iEntity The entity node.
 * @return True when `dropUiRendererTag` would rewrite it.
 */
[[nodiscard]] OWL_API auto hasUiRendererTag(const core::YamlNode& iEntity) -> bool;

}// namespace owl::scene
