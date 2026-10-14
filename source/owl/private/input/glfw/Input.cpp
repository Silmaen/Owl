/**
 * @file Input.cpp
 * @author Silmaen
 * @date 06/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "Input.h"
#include "app/Application.h"
#include "core/external/glfw3.h"
#include "window/Window.h"

#include <cstddef>
#include <string>
#include <string_view>

namespace owl::input::glfw {

namespace {

auto getApplicationWindow() -> GLFWwindow* {
	if (!app::Application::instanced())
		return nullptr;
	const auto& window = app::Application::get().getWindow();
	if (window.getType() != window::Type::Glfw)
		return nullptr;
	return static_cast<GLFWwindow*>(window.getNativeWindow());
}

static_assert(g_GamepadButtonCount == GLFW_GAMEPAD_BUTTON_LAST + 1);
static_assert(g_GamepadAxisCount == GLFW_GAMEPAD_AXIS_LAST + 1);
static_assert(g_GamepadCount == GLFW_JOYSTICK_LAST + 1);

auto readGamepad(const int iJoystick) -> GamepadState {
	GLFWgamepadstate glfwState{};
	if (glfwJoystickIsGamepad(iJoystick) != GLFW_TRUE || glfwGetGamepadState(iJoystick, &glfwState) != GLFW_TRUE)
		return {};
	GamepadState state;
	state.connected = true;
	for (std::size_t button = 0; button < g_GamepadButtonCount; ++button)
		state.buttons[button] = glfwState.buttons[button] == GLFW_PRESS;
	for (std::size_t axis = 0; axis < g_GamepadAxisCount; ++axis) state.axes[axis] = glfwState.axes[axis];
	for (const auto trigger: {gamepad::LeftTrigger, gamepad::RightTrigger})
		state.axes[trigger] = (state.axes[trigger] + 1.f) * 0.5f;
	return state;
}

}// namespace

auto Input::isKeyPressed_impl(const KeyCode iKeycode) -> bool {
	auto* window = getApplicationWindow();
	if (window == nullptr || iKeycode < key::Space || iKeycode > key::Menu)
		return false;
	const auto state = glfwGetKey(window, iKeycode);
	return state == GLFW_PRESS || state == GLFW_REPEAT;
}

auto Input::isMouseButtonPressed_impl(const MouseCode iMouseCode) -> bool {
	auto* window = getApplicationWindow();
	if (window == nullptr || iMouseCode > mouse::ButtonLast)
		return false;
	return glfwGetMouseButton(window, iMouseCode) == GLFW_PRESS;
}

auto Input::getMousePos_impl() -> math::vec2 {
	auto* window = getApplicationWindow();
	if (window == nullptr)
		return {0.f, 0.f};
	double xPos{0};
	double yPos{0};
	glfwGetCursorPos(window, &xPos, &yPos);
	return {static_cast<float>(xPos), static_cast<float>(yPos)};
}

void Input::readDevices_impl(InputState& oState) {
	auto* window = getApplicationWindow();
	if (window == nullptr)
		return;
	for (int keyCode = key::Space; keyCode <= key::Menu; ++keyCode) {
		const auto state = glfwGetKey(window, keyCode);
		oState.keys[static_cast<std::size_t>(keyCode)] = state == GLFW_PRESS || state == GLFW_REPEAT;
	}
	for (int button = 0; button <= mouse::ButtonLast; ++button)
		oState.mouseButtons[static_cast<std::size_t>(button)] = glfwGetMouseButton(window, button) == GLFW_PRESS;
	double xPos{0};
	double yPos{0};
	glfwGetCursorPos(window, &xPos, &yPos);
	oState.cursor = {static_cast<float>(xPos), static_cast<float>(yPos)};
	for (std::size_t pad = 0; pad < g_GamepadCount; ++pad) oState.gamepads[pad] = readGamepad(static_cast<int>(pad));
}

auto Input::getGamepadState_impl(const std::size_t iPad) -> GamepadState {
	if (getApplicationWindow() == nullptr)
		return {};
	return readGamepad(static_cast<int>(iPad));
}

auto Input::getGamepadName_impl(const std::size_t iPad) -> std::string {
	if (getApplicationWindow() == nullptr)
		return {};
	const char* name = glfwGetGamepadName(static_cast<int>(iPad));
	return name != nullptr ? std::string{name} : std::string{};
}

auto Input::addGamepadMappings_impl(const std::string_view iMappings) -> bool {
	if (getApplicationWindow() == nullptr) {
		OWL_CORE_WARN("GLFW: No window, GLFW not initialised: gamepad mappings ignored.")
		return false;
	}
	const std::string text{iMappings};
	return glfwUpdateGamepadMappings(text.c_str()) == GLFW_TRUE;
}

}// namespace owl::input::glfw
