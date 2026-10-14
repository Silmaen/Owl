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
#include "window/Window.h"
#if OWL_WITH_RENDER
#include "input/glfw/Input.h"
#endif

#include <optional>

namespace owl::input {

namespace {

// Input injected by scenarios, tests and tools, merged with the devices whatever the back-end.
struct Injection {
	InputState held;
	std::optional<math::vec2> cursor;
	math::vec2 pendingWheel{0.f, 0.f};
	math::vec2 frameWheel{0.f, 0.f};
};

auto injection() -> Injection& {
	static Injection s_injection;
	return s_injection;
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
	return state;
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

void Input::resetInjection() {
	auto& inj = injection();
	inj.held = {};
	inj.cursor.reset();
}

}// namespace owl::input
