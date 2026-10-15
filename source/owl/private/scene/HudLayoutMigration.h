/**
 * @file HudLayoutMigration.h
 * @author Silmaen
 * @date 15/10/2026
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
 *  Migration step of the scene and prefab formats (3 to 4): every `UiRect` written in the Y-up layout (origin at the
 *  bottom left, `anchor` preset, `pivotX`, `sizeX`, `offsetX` keys) moves to the Y-down layout with min / max
 *  anchors, and every `Canvas` written before the reference resolution keeps one screen pixel per pixel.
 *
 * The widget keeps its place on screen: the anchors take the preset point, the vertical pivot and offset change
 * sign (`pivot.y` becomes `1 - pivotY`, `offset.y` becomes `-offsetY`); the canvas gets `ScaleMode::ConstantPixels`.
 * @param[in] ioDocument The document root (with an `Entities` sequence).
 * @return Always true: a document without HUD has nothing to convert.
 */
auto convertLegacyHudLayout(const core::Serializer& ioDocument) -> bool;

/**
 * @brief
 *  Check whether an entity node holds a `UiRect` or a `Canvas` written before the Y-down HUD layout.
 * @param[in] iEntity The entity node.
 * @return True when `convertLegacyHudLayout` would rewrite it.
 */
[[nodiscard]] OWL_API auto hasLegacyHudLayout(const core::YamlNode& iEntity) -> bool;

}// namespace owl::scene
