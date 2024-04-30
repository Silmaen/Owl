/**
 * @file Input.cpp
 * @author Silmaen
 * @date 02/08/2023
 * Copyright © 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "Input.h"
#include "core/Application.h"

namespace owl::input::null {

bool Input::isKeyPressed_impl(const KeyCode iKeyCode) { return keyPressed.contains(iKeyCode); }

bool Input::isMouseButtonPressed_impl(const MouseCode iMouseCode) { return mouseBtnPressed.contains(iMouseCode); }

glm::vec2 Input::getMousePos_impl() { return mousePos; }

void Input::injectKey_impl(const KeyCode iKeyCode) {
	if (keyPressed.contains(iKeyCode)) {
		keyPressed.erase(iKeyCode);
	} else {
		keyPressed.insert(iKeyCode);
	}
}

void Input::injectMouseButton_impl(const MouseCode iMouseCode) {
	if (mouseBtnPressed.contains(iMouseCode)) {
		mouseBtnPressed.erase(iMouseCode);
	} else {
		mouseBtnPressed.insert(iMouseCode);
	}
}

void Input::injectMousePos_impl(const glm::vec2 &iMousePos) { mousePos = iMousePos; }

void Input::resetInjection_impl() {
	keyPressed.clear();
	mouseBtnPressed.clear();
}

}// namespace owl::input::null
