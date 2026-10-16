/**
 * @file HeldInput.h
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <input/ActionMap.h>
#include <input/Gamepad.h>
#include <math/vectors.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace owl::nest::session {

/**
 * @brief
 *  Gamepad state held by a scenario step or a bounded play.
 */
struct HeldGamepad {
	/// Gamepad slot.
	std::size_t pad{0};
	/// Injected state: connected, buttons held, axis values.
	input::GamepadState state;
};

/**
 * @brief
 *  One action held for a number of frames.
 */
struct ActionHold {
	/// Action name.
	std::string name;
	/// Held value: `x` for a button (non-zero = held) or an axis, both for a vector.
	math::vec2 value{0.f, 0.f};
};

/**
 * @brief
 *  The raw inputs held while frames play: what the input injection is given.
 */
struct HeldInput {
	/// Keys held.
	std::vector<uint16_t> keys{};// NOLINT(readability-redundant-member-init) callers may omit it
	/// Mouse buttons held.
	std::vector<uint8_t> mouseButtons{};// NOLINT(readability-redundant-member-init) callers may omit it
	/// Gamepad held, if any.
	std::optional<HeldGamepad> gamepad{};// NOLINT(readability-redundant-member-init) callers may omit it
};

/**
 * @brief
 *  Turn held actions into the raw inputs that produce them, through the first matching binding of each action.
 * @param[in] iMap The action map (the bindings).
 * @param[in] iHolds The actions to hold.
 * @param[in,out] ioHeld The inputs held; the ones needed are added.
 * @return Empty on success, else why an action cannot be held (unknown, no binding for the value, two gamepads).
 */
[[nodiscard]] auto resolveActionHolds(const input::ActionMap& iMap, const std::vector<ActionHold>& iHolds,
									  HeldInput& ioHeld) -> std::string;

/**
 * @brief
 *  Give the held inputs to the input injection (after `Input::resetInjection`).
 * @param[in] iHeld The inputs.
 */
void injectHeldInput(const HeldInput& iHeld);

}// namespace owl::nest::session
