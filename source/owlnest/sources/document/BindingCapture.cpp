/**
 * @file BindingCapture.cpp
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "BindingCapture.h"

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace owl::nest {

namespace {

auto makeBinding(const input::BindingSource iSource, const std::size_t iCode) -> input::ActionBinding {
	input::ActionBinding binding;
	binding.source = iSource;
	binding.code = static_cast<uint16_t>(iCode);
	return binding;
}

}// namespace

void BindingCapture::begin(const input::InputState& iBaseline, const input::ActionKind iKind) {
	m_armed = true;
	m_kind = iKind;
	m_ignoredKeys = iBaseline.keys;
	m_ignoredMouse = iBaseline.mouseButtons;
	for (std::size_t pad = 0; pad < input::g_GamepadCount; ++pad) {
		const auto& state = iBaseline.gamepads[pad];
		m_ignoredPadButtons[pad] = state.buttons;
		m_ignoredPadAxes[pad].reset();
		for (std::size_t axis = 0; axis < input::g_GamepadAxisCount; ++axis)
			m_ignoredPadAxes[pad][axis] = std::abs(state.axes[axis]) >= axisRelease;
	}
}

auto BindingCapture::feed(const input::InputState& iState) -> std::optional<input::ActionBinding> {
	if (!m_armed)
		return std::nullopt;
	m_ignoredKeys &= iState.keys;
	m_ignoredMouse &= iState.mouseButtons;
	for (std::size_t pad = 0; pad < input::g_GamepadCount; ++pad) {
		const auto& state = iState.gamepads[pad];
		m_ignoredPadButtons[pad] &= state.buttons;
		for (std::size_t axis = 0; axis < input::g_GamepadAxisCount; ++axis) {
			if (std::abs(state.axes[axis]) < axisRelease)
				m_ignoredPadAxes[pad][axis] = false;
		}
	}
	for (std::size_t code = 0; code < input::g_KeyCount; ++code) {
		if (iState.keys[code] && !m_ignoredKeys[code]) {
			m_armed = false;
			return makeBinding(input::BindingSource::Key, code);
		}
	}
	for (std::size_t code = 0; code < input::g_MouseButtonCount; ++code) {
		if (iState.mouseButtons[code] && !m_ignoredMouse[code]) {
			m_armed = false;
			return makeBinding(input::BindingSource::MouseButton, code);
		}
	}
	for (std::size_t pad = 0; pad < input::g_GamepadCount; ++pad) {
		const auto& state = iState.gamepads[pad];
		if (!state.connected)
			continue;
		for (std::size_t code = 0; code < input::g_GamepadButtonCount; ++code) {
			if (state.buttons[code] && !m_ignoredPadButtons[pad][code]) {
				m_armed = false;
				return makeBinding(input::BindingSource::GamepadButton, code);
			}
		}
		for (std::size_t axis = 0; axis < input::g_GamepadAxisCount; ++axis) {
			if (std::abs(state.axes[axis]) < axisThreshold || m_ignoredPadAxes[pad][axis])
				continue;
			m_armed = false;
			const auto isStick = axis < static_cast<std::size_t>(input::gamepad::LeftTrigger);
			if (isStick && m_kind == input::ActionKind::Vector)
				return makeBinding(input::BindingSource::GamepadStick, axis < 2 ? 0 : 1);
			return makeBinding(input::BindingSource::GamepadAxis, axis);
		}
	}
	return std::nullopt;
}

}// namespace owl::nest
