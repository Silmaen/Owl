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
}

}// namespace owl::input::glfw
