/**
 * @file BindingCapture.h
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <input/ActionMap.h>
#include <input/Gamepad.h>
#include <input/InputFrame.h>

#include <array>
#include <bitset>
#include <cstddef>
#include <optional>

namespace owl::nest {

/**
 * @brief
 *  The "press a key" capture of an action binding: waits for the next input the user gives on a keyboard, a mouse or a
 *  gamepad and turns it into an `ActionBinding`.
 *
 * Pure state machine fed with device states, so it needs neither a window nor a device. Whatever is held when the
 * capture starts (the click on the button that started it) is ignored until it is released. A key, a mouse button or
 * a gamepad button counts when it goes down; a gamepad axis counts when it crosses `axisThreshold`: a stick of a
 * `Vector` action yields the whole stick, any other axis yields that axis.
 */
class BindingCapture final {
public:
	/// Deflection from which a gamepad axis counts as pressed.
	static constexpr float axisThreshold = 0.6f;
	/// Deflection under which a gamepad axis held at the start counts as released.
	static constexpr float axisRelease = 0.3f;

	BindingCapture(const BindingCapture&) = default;

	BindingCapture(BindingCapture&&) = default;

	auto operator=(const BindingCapture&) -> BindingCapture& = default;

	auto operator=(BindingCapture&&) -> BindingCapture& = default;

	BindingCapture() = default;

	~BindingCapture() = default;

	/**
	 * @brief
	 *  Start waiting for an input.
	 * @param[in] iBaseline Device state at this moment: what is held is ignored until released.
	 * @param[in] iKind Kind of the action the binding is for: a gamepad stick is captured whole for a `Vector`.
	 */
	void begin(const input::InputState& iBaseline, input::ActionKind iKind);

	/**
	 * @brief
	 *  Stop waiting.
	 */
	void cancel() { m_armed = false; }

	/**
	 * @brief
	 *  Whether an input is awaited.
	 * @return True between `begin` and the captured input or `cancel`.
	 */
	[[nodiscard]] auto isArmed() const -> bool { return m_armed; }

	/**
	 * @brief
	 *  Look at the devices.
	 * @param[in] iState Device state of this frame.
	 * @return The binding of the first input given (the capture then stops), nothing while none came or when not armed.
	 */
	[[nodiscard]] auto feed(const input::InputState& iState) -> std::optional<input::ActionBinding>;

private:
	/// Whether an input is awaited.
	bool m_armed{false};
	/// Kind of the action the binding is for.
	input::ActionKind m_kind{input::ActionKind::Button};
	/// Keys held at the start (or since), ignored until released.
	std::bitset<input::g_KeyCount> m_ignoredKeys;
	/// Mouse buttons held at the start, ignored until released.
	std::bitset<input::g_MouseButtonCount> m_ignoredMouse;
	/// Gamepad buttons held at the start, per slot.
	std::array<std::bitset<input::g_GamepadButtonCount>, input::g_GamepadCount> m_ignoredPadButtons;
	/// Gamepad axes deflected at the start, per slot.
	std::array<std::bitset<input::g_GamepadAxisCount>, input::g_GamepadCount> m_ignoredPadAxes;
};

}// namespace owl::nest
