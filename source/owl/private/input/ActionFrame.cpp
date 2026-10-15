/**
 * @file ActionFrame.cpp
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "input/ActionFrame.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace owl::input {

namespace {

auto firstPad(const ActionBinding& iBinding) -> std::size_t {
	return iBinding.pad < 0 ? 0 : static_cast<std::size_t>(iBinding.pad);
}

auto endPad(const ActionBinding& iBinding) -> std::size_t {
	return iBinding.pad < 0 ? g_GamepadCount : static_cast<std::size_t>(iBinding.pad) + 1;
}

auto readStick(const ActionBinding& iBinding, const InputState& iState) -> math::vec2 {
	const auto stick = iBinding.code == 0 ? gamepad::Stick::Left : gamepad::Stick::Right;
	math::vec2 strongest{0.f, 0.f};
	for (std::size_t pad = firstPad(iBinding); pad < endPad(iBinding); ++pad) {
		const auto state = iState.getGamepad(pad);
		if (!state.connected)
			continue;
		if (const auto value = applyStickResponse(state.getStick(stick), iBinding.response);
			value.normSq() > strongest.normSq())
			strongest = value;
	}
	return strongest;
}

auto readScalar(const ActionBinding& iBinding, const InputState& iState) -> float {
	switch (iBinding.source) {
		case BindingSource::Key:
			return iState.isKeyPressed(iBinding.code) ? 1.f : 0.f;
		case BindingSource::MouseButton:
			return iState.isMouseButtonPressed(static_cast<MouseCode>(iBinding.code)) ? 1.f : 0.f;
		case BindingSource::GamepadButton:
			{
				for (std::size_t pad = firstPad(iBinding); pad < endPad(iBinding); ++pad)
					if (iState.isGamepadButtonPressed(pad, static_cast<GamepadButton>(iBinding.code)))
						return 1.f;
				return 0.f;
			}
		case BindingSource::GamepadAxis:
			{
				float strongest = 0.f;
				for (std::size_t pad = firstPad(iBinding); pad < endPad(iBinding); ++pad) {
					const auto state = iState.getGamepad(pad);
					if (!state.connected)
						continue;
					const float value = applyAxisResponse(state.getAxis(static_cast<GamepadAxis>(iBinding.code)),
														  iBinding.response);
					if (std::abs(value) > std::abs(strongest))
						strongest = value;
				}
				return strongest;
			}
		case BindingSource::GamepadStick:
			return readStick(iBinding, iState).x();
	}
	return 0.f;
}

}// namespace

auto sampleAction(const ActionDefinition& iAction, const InputState& iState) -> ActionState {
	ActionState state;
	math::vec2 sum{0.f, 0.f};
	bool anyDown = false;
	for (const auto& binding: iAction.bindings) {
		const bool stick = binding.source == BindingSource::GamepadStick;
		const math::vec2 stickValue = stick ? readStick(binding, iState) * binding.scale : math::vec2{0.f, 0.f};
		const float scalar = stick ? 0.f : readScalar(binding, iState) * binding.scale;
		switch (iAction.kind) {
			case ActionKind::Button:
				anyDown = anyDown || (stick ? stickValue.norm() : scalar) >= g_actionThreshold;
				break;
			case ActionKind::Axis:
				sum.x() += stick ? stickValue.x() : scalar;
				break;
			case ActionKind::Vector:
				if (stick) {
					if (binding.component != BindingComponent::Y)
						sum.x() += stickValue.x();
					if (binding.component != BindingComponent::X)
						sum.y() += stickValue.y();
				} else if (binding.component == BindingComponent::Y) {
					sum.y() += scalar;
				} else {
					sum.x() += scalar;
				}
				break;
		}
	}
	switch (iAction.kind) {
		case ActionKind::Button:
			state.held = anyDown;
			state.value = anyDown ? 1.f : 0.f;
			break;
		case ActionKind::Axis:
			state.value = std::clamp(sum.x(), -1.f, 1.f);
			state.held = std::abs(state.value) >= g_actionThreshold;
			break;
		case ActionKind::Vector:
			if (sum.norm() > 1.f)
				sum.normalize();
			state.vector = sum;
			state.value = sum.norm();
			state.held = state.value >= g_actionThreshold;
			break;
	}
	return state;
}

auto evaluateAction(const ActionDefinition& iAction, const InputFrame& iInput) -> ActionState {
	auto state = sampleAction(iAction, iInput.current);
	const bool wasHeld = sampleAction(iAction, iInput.previous).held;
	state.pressed = state.held && !wasHeld;
	state.released = !state.held && wasHeld;
	return state;
}

void ActionFrame::update(const shared<const ActionMap>& iMap, const InputFrame& iInput) {
	m_map = iMap;
	if (!m_map) {
		m_states.clear();
		return;
	}
	const auto& actions = m_map->getActions();
	m_states.resize(actions.size());
	for (std::size_t index = 0; index < actions.size(); ++index)
		m_states[index] = evaluateAction(actions[index], iInput);
}

auto ActionFrame::has(const std::string_view iName) const -> bool { return m_map && m_map->indexOf(iName).has_value(); }

auto ActionFrame::get(const std::string_view iName) const -> const ActionState& {
	static const ActionState noAction;
	if (!m_map)
		return noAction;
	const auto index = m_map->indexOf(iName);
	return index && *index < m_states.size() ? m_states[*index] : noAction;
}

}// namespace owl::input
