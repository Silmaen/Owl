/**
 * @file input.cpp
 * @author Silmaen
 * @date 02/08/2023
 * Copyright © 2023 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "input/Input.h"
#include "input/glfw/Input.h"
#include "input/null/Input.h"

namespace owl::input {

Type Input::m_type = Type::Glfw;
uniq<Input> Input::m_instance = nullptr;

Input::~Input() = default;


void Input::init(const Type& iType) {
	if (m_instance)
		m_instance.reset();
	m_type = iType;
	switch (m_type) {
		case Type::Glfw:
			m_instance = mkUniq<glfw::Input>();
			return;
		case Type::Null:
			m_instance = mkUniq<null::Input>();
	}
}

void Input::invalidate() {
	if (m_instance)
		m_instance.reset();
}

auto Input::isKeyPressed(const KeyCode iKeycode) -> bool {
	if (m_instance)
		return m_instance->isKeyPressed_impl(iKeycode);
	return false;
}

auto Input::isMouseButtonPressed(const MouseCode iMouseCode) -> bool {
	if (m_instance)
		return m_instance->isMouseButtonPressed_impl(iMouseCode);
	return false;
}

auto Input::getMouseX() -> float { return getMousePos().x(); }

auto Input::getMouseY() -> float { return getMousePos().y(); }

auto Input::getMousePos() -> math::vec2 {
	if (m_instance)
		return m_instance->getMousePos_impl();
	return {};
}

void Input::injectKey(const KeyCode iKeycode) {
	if (m_instance)
		m_instance->injectKey_impl(iKeycode);
}

void Input::injectMouseButton(const MouseCode iMouseCode) {
	if (m_instance)
		m_instance->injectMouseButton_impl(iMouseCode);
}

void Input::injectMousePos(const math::vec2& iMousePos) {
	if (m_instance)
		m_instance->injectMousePos_impl(iMousePos);
}

void Input::resetInjection() {
	if (m_instance)
		m_instance->resetInjection_impl();
}

}// namespace owl::input
