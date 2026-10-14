/**
 * @file Input.cpp
 * @author Silmaen
 * @date 02/08/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "input/null/Input.h"

namespace owl::input::null {

auto Input::isKeyPressed_impl([[maybe_unused]] const KeyCode iKeyCode) -> bool { return false; }

auto Input::isMouseButtonPressed_impl([[maybe_unused]] const MouseCode iMouseCode) -> bool { return false; }

auto Input::getMousePos_impl() -> math::vec2 { return {0.f, 0.f}; }

void Input::readDevices_impl([[maybe_unused]] InputState& oState) {}

}// namespace owl::input::null
