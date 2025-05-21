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

auto Input::isKeyPressed_impl(const KeyCode iKeyCode) -> bool { return keyPressed.contains(iKeyCode); }

auto Input::isMouseButtonPressed_impl(const MouseCode iMouseCode) -> bool {
	return mouseBtnPressed.contains(iMouseCode);
}

auto Input::getMousePos_impl() -> math::vec2 { return mousePos; }

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

void Input::injectMousePos_impl(const math::vec2& iMousePos) { mousePos = iMousePos; }

void Input::resetInjection_impl() {
	keyPressed.clear();
	mouseBtnPressed.clear();
}

}// namespace owl::input::null
