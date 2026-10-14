/**
 * @file input.cpp
 * @author Silmaen
 * @date 02/08/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "input/Input.h"
#include "input/null/Input.h"
#include "platform/FileUtils.h"
#include "window/Window.h"
#if OWL_WITH_RENDER
#include "input/glfw/Input.h"
#endif

#include <array>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

namespace owl::input {

namespace {

// Input injected by scenarios, tests and tools, merged with the devices whatever the back-end.
struct Injection {
	InputState held;
	std::optional<math::vec2> cursor;
	math::vec2 pendingWheel{0.f, 0.f};
	math::vec2 frameWheel{0.f, 0.f};
	std::array<std::optional<GamepadState>, g_GamepadCount> gamepads;
};


auto injection() -> Injection& {
	static Injection s_injection;
	return s_injection;
}

auto injectedGamepad(const std::size_t iPad) -> GamepadState& {
	auto& slot = injection().gamepads[iPad];
	if (!slot.has_value()) {
		slot.emplace();
		slot->connected = true;
	}
	return *slot;
}

}// namespace

window::Type Input::m_type = window::Type::Glfw;
uniq<Input> Input::m_instance = nullptr;

Input::~Input() = default;

void Input::init() { init(window::Type::Glfw); }

void Input::init(const window::Type& iType) {
	if (m_instance)
		m_instance.reset();
	injection() = {};
	m_type = iType;
#if !OWL_WITH_RENDER
	// GLFW not built (OWL_MODULE_RENDER=OFF): the Null window comes with the Null input.
	m_type = window::Type::Null;
#endif
	switch (m_type) {
		case window::Type::Glfw:
#if OWL_WITH_RENDER
			m_instance = mkUniq<glfw::Input>();
#endif
			return;
		case window::Type::Null:
			m_instance = mkUniq<null::Input>();
	}
}

void Input::invalidate() {
	if (m_instance)
		m_instance.reset();
	injection() = {};
}

auto Input::isKeyPressed(const KeyCode iKeycode) -> bool {
	if (injection().held.isKeyPressed(iKeycode))
		return true;
	return m_instance && m_instance->isKeyPressed_impl(iKeycode);
}

auto Input::isMouseButtonPressed(const MouseCode iMouseCode) -> bool {
	if (injection().held.isMouseButtonPressed(iMouseCode))
		return true;
	return m_instance && m_instance->isMouseButtonPressed_impl(iMouseCode);
}

auto Input::getMouseX() -> float { return getMousePos().x(); }

auto Input::getMouseY() -> float { return getMousePos().y(); }

auto Input::getMousePos() -> math::vec2 {
	if (const auto& cursor = injection().cursor; cursor.has_value())
		return *cursor;
	if (m_instance)
		return m_instance->getMousePos_impl();
	return {};
}

auto Input::readState() -> InputState {
	InputState state;
	if (m_instance)
		m_instance->readDevices_impl(state);
	const auto& inj = injection();
	state.keys |= inj.held.keys;
	state.mouseButtons |= inj.held.mouseButtons;
	if (inj.cursor.has_value())
		state.cursor = *inj.cursor;
	state.wheel = inj.frameWheel;
	for (std::size_t pad = 0; pad < g_GamepadCount; ++pad)
		if (const auto& injected = inj.gamepads[pad]; injected.has_value())
			state.gamepads[pad] = *injected;
	return state;
}

auto Input::getGamepadState(const std::size_t iPad) -> GamepadState {
	if (iPad >= g_GamepadCount)
		return {};
	if (const auto& injected = injection().gamepads[iPad]; injected.has_value())
		return *injected;
	return m_instance ? m_instance->getGamepadState_impl(iPad) : GamepadState{};
}

auto Input::isGamepadConnected(const std::size_t iPad) -> bool { return getGamepadState(iPad).connected; }

auto Input::getGamepadName(const std::size_t iPad) -> std::string {
	if (iPad >= g_GamepadCount)
		return {};
	if (const auto& injected = injection().gamepads[iPad]; injected.has_value())
		return injected->connected ? "Injected gamepad" : "";
	return m_instance ? m_instance->getGamepadName_impl(iPad) : std::string{};
}

auto Input::addGamepadMappings(const std::string_view iMappings) -> bool {
	const auto count = countGamepadMappings(iMappings);
	if (count == 0) {
		OWL_CORE_WARN("Input: No gamepad mapping found in the mapping text.")
		return false;
	}
	if (!m_instance || !m_instance->addGamepadMappings_impl(iMappings)) {
		OWL_CORE_WARN("Input: The back-end refused {} gamepad mapping(s).", count)
		return false;
	}
	OWL_CORE_INFO("Input: {} gamepad mapping(s) added.", count)
	return true;
}

auto Input::loadGamepadMappings(const std::filesystem::path& iFile) -> bool {
	if (std::error_code errorCode; !is_regular_file(iFile, errorCode)) {
		OWL_CORE_WARN("Input: Gamepad mapping file {} not found.", iFile.string())
		return false;
	}
	return addGamepadMappings(platform::fileToString(iFile));
}

void Input::beginFrame() {
	auto& inj = injection();
	inj.frameWheel = inj.pendingWheel;
	inj.pendingWheel = {0.f, 0.f};
}

void Input::addWheel(const math::vec2& iOffset) { injection().pendingWheel += iOffset; }

void Input::injectKey(const KeyCode iKeycode) {
	auto& held = injection().held;
	held.setKey(iKeycode, !held.isKeyPressed(iKeycode));
}

void Input::injectMouseButton(const MouseCode iMouseCode) {
	auto& held = injection().held;
	held.setMouseButton(iMouseCode, !held.isMouseButtonPressed(iMouseCode));
}

void Input::injectMousePos(const math::vec2& iMousePos) { injection().cursor = iMousePos; }

void Input::injectGamepadButton(const std::size_t iPad, const GamepadButton iButton) {
	if (iPad >= g_GamepadCount)
		return;
	auto& pad = injectedGamepad(iPad);
	pad.setButton(iButton, !pad.isButtonPressed(iButton));
}

void Input::injectGamepadAxis(const std::size_t iPad, const GamepadAxis iAxis, const float iValue) {
	if (iPad < g_GamepadCount)
		injectedGamepad(iPad).setAxis(iAxis, iValue);
}

void Input::injectGamepad(const std::size_t iPad, const GamepadState& iState) {
	if (iPad < g_GamepadCount)
		injection().gamepads[iPad] = iState;
}

void Input::resetInjection() {
	auto& inj = injection();
	inj.held = {};
	inj.cursor.reset();
	inj.gamepads = {};
}

}// namespace owl::input
