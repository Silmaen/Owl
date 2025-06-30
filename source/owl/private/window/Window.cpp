/**
 * @file Window.cpp
 * @author Silmaen
 * @date 04/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "../../public/window/Window.h"
#include "glfw/Window.h"
#include "null/Window.h"

namespace owl::window {

auto Window::create(const Properties& iProps) -> uniq<Window> {
	switch (iProps.winType) {
		case Type::Glfw:
			return mkUniq<glfw::Window>(iProps);
		case Type::Null:
			return mkUniq<null::Window>(iProps);
	}
	return nullptr;
}

Window::~Window() = default;

}// namespace owl::input
