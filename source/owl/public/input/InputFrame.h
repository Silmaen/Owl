/**
 * @file InputFrame.h
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "input/Gamepad.h"
#include "input/KeyCodes.h"
#include "input/MouseCode.h"
#include "math/vectors.h"

#include <array>
#include <bitset>
#include <cstddef>

namespace owl::input {

/// Number of key slots of an input state (every GLFW key code up to `key::Menu`).
inline constexpr std::size_t g_KeyCount = key::Menu + 1;
/// Number of mouse button slots of an input state.
inline constexpr std::size_t g_MouseButtonCount = mouse::ButtonLast + 1;

/**
 * @brief
 *  Plain-data snapshot of the keyboard, the mouse and the gamepads at one sampling point.
 */
struct OWL_API InputState {
	/// Keys held down, indexed by key code.
	std::bitset<g_KeyCount> keys;
	/// Mouse buttons held down, indexed by mouse code.
	std::bitset<g_MouseButtonCount> mouseButtons;
	/// Cursor position in the window, in pixels.
	math::vec2 cursor{0.f, 0.f};
	/// Wheel offset scrolled during the sampled interval (x horizontal, y vertical).
	math::vec2 wheel{0.f, 0.f};
	/// Gamepads, indexed by slot (GLFW joystick id).
	std::array<GamepadState, g_GamepadCount> gamepads{};

	/**
	 * @brief
	 *  State of a gamepad slot; a disconnected empty state for a slot out of range.
	 * @param[in] iPad The gamepad slot.
	 * @return The gamepad state.
	 */
	[[nodiscard]] auto getGamepad(const std::size_t iPad) const -> GamepadState {
		return iPad < g_GamepadCount ? gamepads[iPad] : GamepadState{};
	}

	/**
	 * @brief
	 *  Whether a gamepad is connected in a slot.
	 * @param[in] iPad The gamepad slot.
	 * @return True if connected.
	 */
	[[nodiscard]] auto isGamepadConnected(const std::size_t iPad) const -> bool { return getGamepad(iPad).connected; }

	/**
	 * @brief
	 *  Whether a gamepad button is held down.
	 * @param[in] iPad The gamepad slot.
	 * @param[in] iButton The button code.
	 * @return True if held.
	 */
	[[nodiscard]] auto isGamepadButtonPressed(const std::size_t iPad, const GamepadButton iButton) const -> bool {
		return getGamepad(iPad).isButtonPressed(iButton);
	}

	/**
	 * @brief
	 *  Whether a key is held down; false for a code out of range.
	 * @param[in] iKey The key code.
	 * @return True if held.
	 */
	[[nodiscard]] auto isKeyPressed(KeyCode iKey) const -> bool { return iKey < g_KeyCount && keys[iKey]; }

	/**
	 * @brief
	 *  Whether a mouse button is held down; false for a code out of range.
	 * @param[in] iButton The mouse button.
	 * @return True if held.
	 */
	[[nodiscard]] auto isMouseButtonPressed(MouseCode iButton) const -> bool {
		return iButton < g_MouseButtonCount && mouseButtons[iButton];
	}

	/**
	 * @brief
	 *  Set the state of a key; a code out of range is ignored.
	 * @param[in] iKey The key code.
	 * @param[in] iPressed Whether the key is held.
	 */
	void setKey(KeyCode iKey, bool iPressed) {
		if (iKey < g_KeyCount)
			keys[iKey] = iPressed;
	}

	/**
	 * @brief
	 *  Set the state of a mouse button; a code out of range is ignored.
	 * @param[in] iButton The mouse button.
	 * @param[in] iPressed Whether the button is held.
	 */
	void setMouseButton(MouseCode iButton, bool iPressed) {
		if (iButton < g_MouseButtonCount)
			mouseButtons[iButton] = iPressed;
	}
};

/**
 * @brief
 *  Two consecutive input samples (previous and current) giving the held state and the edges between them.
 *
 * A scene keeps one per simulation tick (read in `tick` functions and `on_tick`) and one per frame (read in `update`
 * functions and `on_update`): an edge is seen exactly once by the code running at the matching rate.
 */
struct OWL_API InputFrame {
	/// State at the previous sample.
	InputState previous;
	/// State at the current sample.
	InputState current;

	/**
	 * @brief
	 *  Shift the samples: the current state becomes the previous one.
	 * @param[in] iNext The new current state.
	 */
	void advance(const InputState& iNext) {
		previous = current;
		current = iNext;
	}

	/**
	 * @brief
	 *  Start from a state without any edge (both samples equal, no wheel).
	 * @param[in] iState The state held at the start.
	 */
	void reset(const InputState& iState) {
		current = iState;
		current.wheel = {0.f, 0.f};
		previous = current;
	}

	/**
	 * @brief
	 *  Whether a key is held at the current sample.
	 * @param[in] iKey The key code.
	 * @return True if held.
	 */
	[[nodiscard]] auto isKeyPressed(KeyCode iKey) const -> bool { return current.isKeyPressed(iKey); }

	/**
	 * @brief
	 *  Whether a key went down between the previous and the current sample.
	 * @param[in] iKey The key code.
	 * @return True on the press edge.
	 */
	[[nodiscard]] auto isKeyJustPressed(KeyCode iKey) const -> bool {
		return current.isKeyPressed(iKey) && !previous.isKeyPressed(iKey);
	}

	/**
	 * @brief
	 *  Whether a key went up between the previous and the current sample.
	 * @param[in] iKey The key code.
	 * @return True on the release edge.
	 */
	[[nodiscard]] auto isKeyJustReleased(KeyCode iKey) const -> bool {
		return !current.isKeyPressed(iKey) && previous.isKeyPressed(iKey);
	}

	/**
	 * @brief
	 *  Whether a mouse button is held at the current sample.
	 * @param[in] iButton The mouse button.
	 * @return True if held.
	 */
	[[nodiscard]] auto isMouseButtonPressed(MouseCode iButton) const -> bool {
		return current.isMouseButtonPressed(iButton);
	}

	/**
	 * @brief
	 *  Whether a mouse button went down between the previous and the current sample.
	 * @param[in] iButton The mouse button.
	 * @return True on the press edge.
	 */
	[[nodiscard]] auto isMouseButtonJustPressed(MouseCode iButton) const -> bool {
		return current.isMouseButtonPressed(iButton) && !previous.isMouseButtonPressed(iButton);
	}

	/**
	 * @brief
	 *  Whether a mouse button went up between the previous and the current sample.
	 * @param[in] iButton The mouse button.
	 * @return True on the release edge.
	 */
	[[nodiscard]] auto isMouseButtonJustReleased(MouseCode iButton) const -> bool {
		return !current.isMouseButtonPressed(iButton) && previous.isMouseButtonPressed(iButton);
	}

	/**
	 * @brief
	 *  Whether a gamepad is connected at the current sample.
	 * @param[in] iPad The gamepad slot.
	 * @return True if connected.
	 */
	[[nodiscard]] auto isGamepadConnected(const std::size_t iPad) const -> bool {
		return current.isGamepadConnected(iPad);
	}

	/**
	 * @brief
	 *  Whether a gamepad button is held at the current sample.
	 * @param[in] iPad The gamepad slot.
	 * @param[in] iButton The button code.
	 * @return True if held.
	 */
	[[nodiscard]] auto isGamepadButtonPressed(const std::size_t iPad, const GamepadButton iButton) const -> bool {
		return current.isGamepadButtonPressed(iPad, iButton);
	}

	/**
	 * @brief
	 *  Whether a gamepad button went down between the previous and the current sample.
	 * @param[in] iPad The gamepad slot.
	 * @param[in] iButton The button code.
	 * @return True on the press edge.
	 */
	[[nodiscard]] auto isGamepadButtonJustPressed(const std::size_t iPad, const GamepadButton iButton) const -> bool {
		return current.isGamepadButtonPressed(iPad, iButton) && !previous.isGamepadButtonPressed(iPad, iButton);
	}

	/**
	 * @brief
	 *  Whether a gamepad button went up between the previous and the current sample.
	 * @param[in] iPad The gamepad slot.
	 * @param[in] iButton The button code.
	 * @return True on the release edge.
	 */
	[[nodiscard]] auto isGamepadButtonJustReleased(const std::size_t iPad, const GamepadButton iButton) const -> bool {
		return !current.isGamepadButtonPressed(iPad, iButton) && previous.isGamepadButtonPressed(iPad, iButton);
	}

	/**
	 * @brief
	 *  Raw value of a gamepad axis at the current sample.
	 * @param[in] iPad The gamepad slot.
	 * @param[in] iAxis The axis code.
	 * @return The value (sticks in [-1, 1], triggers in [0, 1]).
	 */
	[[nodiscard]] auto getGamepadAxis(const std::size_t iPad, const GamepadAxis iAxis) const -> float {
		return current.getGamepad(iPad).getAxis(iAxis);
	}

	/**
	 * @brief
	 *  Position of a gamepad stick at the current sample, through a dead zone and a response curve.
	 * @param[in] iPad The gamepad slot.
	 * @param[in] iStick The stick.
	 * @param[in] iResponse The dead zone and curve.
	 * @return The shaped position (x right, y down).
	 */
	[[nodiscard]] auto getGamepadStick(const std::size_t iPad, const gamepad::Stick iStick,
									   const AxisResponse& iResponse = {}) const -> math::vec2 {
		return applyStickResponse(current.getGamepad(iPad).getStick(iStick), iResponse);
	}

	/**
	 * @brief
	 *  Cursor position at the current sample.
	 * @return The position in pixels.
	 */
	[[nodiscard]] auto getCursor() const -> math::vec2 { return current.cursor; }

	/**
	 * @brief
	 *  Cursor motion between the previous and the current sample.
	 * @return The motion in pixels.
	 */
	[[nodiscard]] auto getCursorDelta() const -> math::vec2 { return current.cursor - previous.cursor; }

	/**
	 * @brief
	 *  Wheel offset scrolled since the previous sample.
	 * @return The offset (x horizontal, y vertical).
	 */
	[[nodiscard]] auto getWheel() const -> math::vec2 { return current.wheel; }
};

/**
 * @brief
 *  Builds the per-tick input states from the per-frame ones, without losing nor doubling an edge.
 *
 * `observe()` takes the state of every frame, `take()` gives the state of every tick. A key held in any frame since
 * the previous tick counts as held at the next tick, so a press shorter than a tick still gives a press edge then a
 * release edge; several ticks in one frame see the frame state once (the edges and the wheel go to the first one).
 * A key released then pressed again between two ticks is seen as held throughout. Gamepad buttons follow the keys;
 * gamepad axes and connections take the last observed frame.
 */
class OWL_API InputSampler final {
public:
	/**
	 * @brief
	 *  Start from a state: the next tick sees it without any wheel.
	 * @param[in] iState The state held at the start.
	 */
	void reset(const InputState& iState);

	/**
	 * @brief
	 *  Record the state of a frame.
	 * @param[in] iFrame The frame state (its wheel is the offset of that frame).
	 */
	void observe(const InputState& iFrame);

	/**
	 * @brief
	 *  Build the state of a tick and start the next tick interval from the last observed frame.
	 * @return The tick state: last cursor and axes, keys and buttons held during the interval, wheel scrolled during it.
	 */
	auto take() -> InputState;

private:
	/// Last observed frame state.
	InputState m_latest;
	/// Keys and buttons held in any frame since the previous tick, wheel scrolled since then.
	InputState m_pending;
	/// Whether a frame was observed since the previous tick (else the next tick repeats the last frame).
	bool m_observedSinceTick = false;
};

}// namespace owl::input
