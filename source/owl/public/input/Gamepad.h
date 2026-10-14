/**
 * @file Gamepad.h
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "math/vectors.h"

#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace owl::input {
/// Wrap to gamepad button code.
using GamepadButton = uint8_t;
/// Wrap to gamepad axis code.
using GamepadAxis = uint8_t;

/// Number of gamepad slots (GLFW joystick ids 0 to 15).
inline constexpr std::size_t g_GamepadCount = 16;
/// Number of buttons of a gamepad (standard layout).
inline constexpr std::size_t g_GamepadButtonCount = 15;
/// Number of axes of a gamepad (two sticks and two triggers).
inline constexpr std::size_t g_GamepadAxisCount = 6;
}// namespace owl::input

/**
 * @brief
 *  Gamepad button and axis codes (standard layout, Xbox names, same values as GLFW).
 */
namespace owl::input::gamepad {
enum : GamepadButton {
	A = 0,///< Bottom face button (Cross).
	B = 1,///< Right face button (Circle).
	X = 2,///< Left face button (Square).
	Y = 3,///< Top face button (Triangle).
	LeftBumper = 4,///< Left shoulder button.
	RightBumper = 5,///< Right shoulder button.
	Back = 6,///< Back / Select / Share.
	Start = 7,///< Start / Options.
	Guide = 8,///< Guide / Home.
	LeftThumb = 9,///< Left stick click.
	RightThumb = 10,///< Right stick click.
	DpadUp = 11,///< Directional pad up.
	DpadRight = 12,///< Directional pad right.
	DpadDown = 13,///< Directional pad down.
	DpadLeft = 14,///< Directional pad left.
	ButtonLast = DpadLeft///< Last button.
};

// Gamepad axis codes (same values as GLFW).
enum : GamepadAxis {
	LeftX = 0,///< Left stick, horizontal (-1 left, +1 right).
	LeftY = 1,///< Left stick, vertical (-1 up, +1 down).
	RightX = 2,///< Right stick, horizontal.
	RightY = 3,///< Right stick, vertical.
	LeftTrigger = 4,///< Left trigger, 0 released to 1 fully pressed.
	RightTrigger = 5,///< Right trigger, 0 released to 1 fully pressed.
	AxisLast = RightTrigger///< Last axis.
};

/**
 * @brief
 *  Sticks of a gamepad.
 */
enum struct Stick : uint8_t {
	Left = 0,///< Left stick (`LeftX`, `LeftY`).
	Right = 1///< Right stick (`RightX`, `RightY`).
};
}// namespace owl::input::gamepad

namespace owl::input {
/**
 * @brief
 *  Plain-data snapshot of one gamepad: connection, buttons and raw axes (no dead zone applied).
 */
struct OWL_API GamepadState {
	/// Whether a gamepad is connected in this slot (a joystick without a gamepad mapping is not).
	bool connected{false};
	/// Buttons held down, indexed by button code.
	std::bitset<g_GamepadButtonCount> buttons;
	/// Axis values, indexed by axis code: sticks in [-1, 1], triggers in [0, 1].
	std::array<float, g_GamepadAxisCount> axes{};

	/**
	 * @brief
	 *  Whether a button is held down; false for a code out of range.
	 * @param[in] iButton The button code.
	 * @return True if held.
	 */
	[[nodiscard]] auto isButtonPressed(const GamepadButton iButton) const -> bool {
		return iButton < g_GamepadButtonCount && buttons[iButton];
	}

	/**
	 * @brief
	 *  Value of an axis; 0 for a code out of range.
	 * @param[in] iAxis The axis code.
	 * @return The raw value.
	 */
	[[nodiscard]] auto getAxis(const GamepadAxis iAxis) const -> float {
		return iAxis < g_GamepadAxisCount ? axes[iAxis] : 0.f;
	}

	/**
	 * @brief
	 *  Raw position of a stick.
	 * @param[in] iStick The stick.
	 * @return The position (x right, y down), each in [-1, 1].
	 */
	[[nodiscard]] auto getStick(const gamepad::Stick iStick) const -> math::vec2 {
		const auto first = static_cast<std::size_t>(iStick == gamepad::Stick::Left ? gamepad::LeftX : gamepad::RightX);
		return {axes[first], axes[first + 1]};
	}

	/**
	 * @brief
	 *  Set the state of a button; a code out of range is ignored.
	 * @param[in] iButton The button code.
	 * @param[in] iPressed Whether the button is held.
	 */
	void setButton(const GamepadButton iButton, const bool iPressed) {
		if (iButton < g_GamepadButtonCount)
			buttons[iButton] = iPressed;
	}

	/**
	 * @brief
	 *  Set the value of an axis; a code out of range is ignored.
	 * @param[in] iAxis The axis code.
	 * @param[in] iValue The value.
	 */
	void setAxis(const GamepadAxis iAxis, const float iValue) {
		if (iAxis < g_GamepadAxisCount)
			axes[iAxis] = iValue;
	}

	/**
	 * @brief
	 *  Equality comparison.
	 * @param[in] iOther The other state.
	 * @return True if equal.
	 */
	auto operator==(const GamepadState& iOther) const -> bool = default;
};

/**
 * @brief
 *  Shape of the response of an axis past its dead zone.
 */
enum struct ResponseCurve : uint8_t {
	Linear,///< Output proportional to the input (exponent 1).
	Quadratic,///< Exponent 2: finer control near the centre.
	Cubic,///< Exponent 3.
	Custom///< Exponent of `AxisResponse::exponent`.
};

/**
 * @brief
 *  Dead zone and response curve of one axis or stick binding.
 *
 * The dead zone is radial for a stick (on the length of the vector, so diagonals are not clipped) and on the absolute
 * value for a single axis. Past it the input is rescaled to [0, 1] then raised to the curve exponent, so the output is
 * continuous at the dead zone edge and reaches 1 at full deflection.
 */
struct OWL_API AxisResponse {
	/// Default radial dead zone.
	static constexpr float defaultDeadZone = 0.15f;
	/// Inputs whose length is at most this read as zero, in [0, 1).
	float deadZone{defaultDeadZone};
	/// Response curve.
	ResponseCurve curve{ResponseCurve::Linear};
	/// Exponent of the `Custom` curve (clamped to [0.1, 10]).
	float exponent{1.f};

	/**
	 * @brief
	 *  Exponent of the curve.
	 * @return 1, 2, 3, or the clamped custom exponent.
	 */
	[[nodiscard]] auto getExponent() const -> float;

	/**
	 * @brief
	 *  Equality comparison.
	 * @param[in] iOther The other response.
	 * @return True if equal.
	 */
	auto operator==(const AxisResponse& iOther) const -> bool = default;
};

/**
 * @brief
 *  Apply a dead zone and a response curve to one axis value.
 * @param[in] iValue The raw value, in [-1, 1].
 * @param[in] iResponse The dead zone and curve.
 * @return The shaped value, in [-1, 1], with the sign of the input.
 */
OWL_API auto applyAxisResponse(float iValue, const AxisResponse& iResponse) -> float;

/**
 * @brief
 *  Apply a radial dead zone and a response curve to a stick position.
 * @param[in] iStick The raw position.
 * @param[in] iResponse The dead zone and curve.
 * @return The shaped position: same direction, length shaped and at most 1.
 */
OWL_API auto applyStickResponse(const math::vec2& iStick, const AxisResponse& iResponse) -> math::vec2;

/**
 * @brief
 *  Count the mapping lines of an SDL_GameControllerDB text: a 32-digit hexadecimal GUID, a name, then mappings.
 *
 * Empty lines and `#` comments are skipped; any other line is not a mapping.
 * @param[in] iMappings The mapping text.
 * @return The number of mapping lines.
 */
OWL_API auto countGamepadMappings(std::string_view iMappings) -> std::size_t;

}// namespace owl::input
