/**
 * @file Input.cpp
 * @author Silmaen
 * @date 18/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "input/android/Input.h"

#include <android/keycodes.h>

namespace owl::input::android {

auto toKeyCode(const int32_t iAndroidKey) -> std::optional<KeyCode> {
	if (iAndroidKey >= AKEYCODE_A && iAndroidKey <= AKEYCODE_Z)
		return static_cast<KeyCode>(key::A + (iAndroidKey - AKEYCODE_A));
	if (iAndroidKey >= AKEYCODE_0 && iAndroidKey <= AKEYCODE_9)
		return static_cast<KeyCode>(key::D0 + (iAndroidKey - AKEYCODE_0));
	switch (iAndroidKey) {
		case AKEYCODE_SPACE:
			return key::Space;
		case AKEYCODE_ENTER:
			return key::Enter;
		case AKEYCODE_TAB:
			return key::Tab;
		case AKEYCODE_DEL:
			return key::Backspace;
		// The system Back button leaves menus and pauses, as Escape does on the desktop.
		case AKEYCODE_BACK:
		case AKEYCODE_ESCAPE:
			return key::Escape;
		case AKEYCODE_DPAD_UP:
			return key::Up;
		case AKEYCODE_DPAD_DOWN:
			return key::Down;
		case AKEYCODE_DPAD_LEFT:
			return key::Left;
		case AKEYCODE_DPAD_RIGHT:
			return key::Right;
		case AKEYCODE_SHIFT_LEFT:
			return key::LeftShift;
		case AKEYCODE_CTRL_LEFT:
			return key::LeftControl;
		case AKEYCODE_MENU:
			return key::Menu;
		default:
			return std::nullopt;
	}
}

auto Input::deviceState() -> InputState& {
	static InputState sState;
	return sState;
}

auto Input::isKeyPressed_impl(const KeyCode iKeyCode) -> bool { return deviceState().isKeyPressed(iKeyCode); }

auto Input::isMouseButtonPressed_impl(const MouseCode iMouseCode) -> bool {
	return deviceState().isMouseButtonPressed(iMouseCode);
}

auto Input::getMousePos_impl() -> math::vec2 { return deviceState().cursor; }

void Input::readDevices_impl(InputState& oState) {
	const auto& state = deviceState();
	oState.keys = state.keys;
	oState.mouseButtons = state.mouseButtons;
	oState.cursor = state.cursor;
}

auto Input::getGamepadState_impl([[maybe_unused]] const std::size_t iPad) -> GamepadState { return {}; }

auto Input::getGamepadName_impl([[maybe_unused]] const std::size_t iPad) -> std::string { return {}; }

auto Input::addGamepadMappings_impl([[maybe_unused]] const std::string_view iMappings) -> bool { return true; }

}// namespace owl::input::android
