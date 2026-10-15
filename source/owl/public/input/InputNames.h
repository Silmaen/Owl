/**
 * @file InputNames.h
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "input/Gamepad.h"
#include "input/KeyCodes.h"
#include "input/MouseCode.h"

#include <optional>
#include <string>
#include <string_view>

/**
 * @brief
 *  Text names of the keys, mouse buttons, gamepad buttons and axes: the vocabulary of the action map file, the
 *  scenarios and the editor.
 *
 * Names are case-sensitive. A parser also accepts a plain number (the raw code); a writer gives the name when there
 * is one and the number otherwise.
 */
namespace owl::input::names {

/**
 * @brief
 *  Name of a key (`A`..`Z`, `D0`..`D9`, `Space`, `Enter`, `Escape`, `Left`, `F1`..`F25`, `LeftShift`, ...).
 * @param[in] iKey The key code.
 * @return The name, or the code as a decimal number for a key without a name.
 */
[[nodiscard]] OWL_API auto keyName(KeyCode iKey) -> std::string;

/**
 * @brief
 *  Key code of a name or of a decimal number.
 * @param[in] iName The name.
 * @return The key code, or nothing for an unknown name.
 */
[[nodiscard]] OWL_API auto parseKey(std::string_view iName) -> std::optional<KeyCode>;

/**
 * @brief
 *  Name of a mouse button (`Left`, `Right`, `Middle`, `Button3`..`Button7`).
 * @param[in] iButton The mouse button.
 * @return The name, or the code as a decimal number for a button out of range.
 */
[[nodiscard]] OWL_API auto mouseButtonName(MouseCode iButton) -> std::string;

/**
 * @brief
 *  Mouse button of a name or of a decimal number.
 * @param[in] iName The name.
 * @return The mouse button, or nothing for an unknown name.
 */
[[nodiscard]] OWL_API auto parseMouseButton(std::string_view iName) -> std::optional<MouseCode>;

/**
 * @brief
 *  Name of a gamepad button (`A`, `B`, `X`, `Y`, `LeftBumper`, `Start`, `DpadUp`, ...).
 * @param[in] iButton The button code.
 * @return The name, or the code as a decimal number for a button out of range.
 */
[[nodiscard]] OWL_API auto gamepadButtonName(GamepadButton iButton) -> std::string;

/**
 * @brief
 *  Gamepad button of a name or of a decimal number.
 * @param[in] iName The name.
 * @return The button code, or nothing for an unknown name.
 */
[[nodiscard]] OWL_API auto parseGamepadButton(std::string_view iName) -> std::optional<GamepadButton>;

/**
 * @brief
 *  Name of a gamepad axis (`left_x`, `left_y`, `right_x`, `right_y`, `left_trigger`, `right_trigger`).
 * @param[in] iAxis The axis code.
 * @return The name, or the code as a decimal number for an axis out of range.
 */
[[nodiscard]] OWL_API auto gamepadAxisName(GamepadAxis iAxis) -> std::string;

/**
 * @brief
 *  Gamepad axis of a name or of a decimal number.
 * @param[in] iName The name.
 * @return The axis code, or nothing for an unknown name.
 */
[[nodiscard]] OWL_API auto parseGamepadAxis(std::string_view iName) -> std::optional<GamepadAxis>;

}// namespace owl::input::names
