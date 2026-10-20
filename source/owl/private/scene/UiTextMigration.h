/**
 * @file UiTextMigration.h
 * @author Silmaen
 * @date 14/10/2026
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
 *  Migration step of the scene and prefab formats (2 to 3): every `UiText` without `horizontalAlignment` is centred on both
 *  axes, in place of its `alignment` key, which format 2 does not apply.
 * @param[in] ioDocument The document root (with an `Entities` sequence).
 * @return Always true: a document without `UiText` has nothing to convert.
 */
auto centreLegacyUiText(const core::Serializer& ioDocument) -> bool;

/**
 * @brief
 *  Check whether an entity node holds a `UiText` written before the text layout (no `horizontalAlignment` key).
 * @param[in] iEntity The entity node.
 * @return True when `centreLegacyUiText` would rewrite it.
 */
[[nodiscard]] OWL_API auto hasLegacyUiText(const core::YamlNode& iEntity) -> bool;

}// namespace owl::scene
