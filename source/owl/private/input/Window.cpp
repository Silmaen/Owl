/**
 * @file Window.cpp
 * @author Silmaen
 * @date 04/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "input/Window.h"
#include "glfw/Window.h"
#include "null/Window.h"

namespace owl::input {

auto Window::create(const Properties& iProps) -> uniq<Window> {
	switch (iProps.winType) {
		case Type::GLFW:
			return mkUniq<glfw::Window>(iProps);
		case Type::Null:
			return mkUniq<null::Window>(iProps);
	}
	return nullptr;
}

Window::~Window() = default;

}// namespace owl::input
