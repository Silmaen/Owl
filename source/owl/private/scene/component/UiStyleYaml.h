/**
 * @file UiStyleYaml.h
 * @author Silmaen
 * @date 01/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once
#include "core/SerializerImpl.h"
#include "scene/component/UiStyle.h"

namespace owl::scene::component::utils {

/**
 * @brief
 *  Write the `Box` map of a widget (nothing for a plain box).
 * @param[in,out] ioEmitter The YAML emitter, inside the map of the widget.
 * @param[in] iBox The box.
 */
void emitBox(YAML::Emitter& ioEmitter, const UiBox& iBox);

/**
 * @brief
 *  Write the fields of a box into the current map (`cornerRadius`, `bottomTint`...), the non-default ones only.
 * @param[in,out] ioEmitter The YAML emitter, inside a map.
 * @param[in] iBox The box.
 */
void emitBoxFields(YAML::Emitter& ioEmitter, const UiBox& iBox);

/**
 * @brief
 *  Read the `Box` map of a widget written by `emitBox` (the box is left as it is without one).
 * @param[in] iNode The node of the widget.
 * @param[in,out] ioBox The box.
 */
void readBox(const core::YamlNode& iNode, UiBox& ioBox);

/**
 * @brief
 *  Read the fields written by `emitBoxFields` from a map.
 * @param[in] iNode The map.
 * @param[in,out] ioBox The box, keeping the fields the map does not hold.
 */
void readBoxFields(const core::YamlNode& iNode, UiBox& ioBox);

/**
 * @brief
 *  Write the `style` and `styleOverrides` keys of a widget (nothing without a style).
 * @param[in,out] ioEmitter The YAML emitter, inside the map of the widget.
 * @param[in] iStyle The reference.
 */
void emitStyleRef(YAML::Emitter& ioEmitter, const UiStyleRef& iStyle);

/**
 * @brief
 *  Read the keys written by `emitStyleRef`.
 * @param[in] iNode The node of the widget.
 * @param[in,out] ioStyle The reference.
 */
void readStyleRef(const core::YamlNode& iNode, UiStyleRef& ioStyle);

/**
 * @brief
 *  Write a style mask as a sequence of property names under a key.
 * @param[in,out] ioEmitter The YAML emitter, inside a map.
 * @param[in] iKey The key.
 * @param[in] iMask The mask.
 */
void emitStyleMask(YAML::Emitter& ioEmitter, const char* iKey, uint32_t iMask);

/**
 * @brief
 *  Read a sequence of property names written by `emitStyleMask` (unknown names are skipped).
 * @param[in] iNode The sequence.
 * @return The mask.
 */
[[nodiscard]] auto readStyleMask(const core::YamlNode& iNode) -> uint32_t;

}// namespace owl::scene::component::utils
