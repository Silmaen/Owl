/**
 * @file Input.cpp
 * @author Silmaen
 * @date 06/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "Input.h"
#include "core/Application.h"
#include "core/external/glfw3.h"

namespace owl::input::glfw {

auto Input::isKeyPressed_impl(const KeyCode iKeycode) -> bool {
	auto* windows = static_cast<GLFWwindow*>(core::Application::get().getWindow().getNativeWindow());
	const auto state = glfwGetKey(windows, iKeycode);
	return state == GLFW_PRESS || state == GLFW_REPEAT;
}

auto Input::isMouseButtonPressed_impl(const MouseCode iMouseCode) -> bool {
	auto* windows = static_cast<GLFWwindow*>(core::Application::get().getWindow().getNativeWindow());
	const auto state = glfwGetMouseButton(windows, iMouseCode);
	return state == GLFW_PRESS;
}

auto Input::getMousePos_impl() -> math::vec2 {
	auto* windows = static_cast<GLFWwindow*>(core::Application::get().getWindow().getNativeWindow());
	double xPos{0};
	double yPos{0};
	glfwGetCursorPos(windows, &xPos, &yPos);
	return {static_cast<float>(xPos), static_cast<float>(yPos)};
}

}// namespace owl::input::glfw
