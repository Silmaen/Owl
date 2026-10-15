/**
 * @file HeldInput.cpp
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "HeldInput.h"

#include <input/Input.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <utility>
#include <vector>

namespace owl::nest::session {

namespace {

auto inverseResponse(const float iShaped, const input::AxisResponse& iResponse) -> float {
	if (iShaped <= 0.f)
		return 0.f;
	const float deadZone = std::clamp(iResponse.deadZone, 0.f, 0.99f);
	return deadZone + std::pow(std::min(iShaped, 1.f), 1.f / iResponse.getExponent()) * (1.f - deadZone);
}

auto holdPadState(HeldInput& ioHeld, const int32_t iPad) -> input::GamepadState* {
	const auto slot = iPad < 0 ? std::size_t{0} : static_cast<std::size_t>(iPad);
	if (!ioHeld.gamepad) {
		ioHeld.gamepad = HeldGamepad{.pad = slot, .state = {}};
		ioHeld.gamepad->state.connected = true;
	}
	return ioHeld.gamepad->pad == slot ? &ioHeld.gamepad->state : nullptr;
}

auto holdStick(HeldInput& ioHeld, const input::ActionBinding& iBinding, const math::vec2& iTarget) -> bool {
	auto* state = holdPadState(ioHeld, iBinding.pad);
	if (state == nullptr)
		return false;
	const math::vec2 shaped = iTarget * (1.f / iBinding.scale);
	const float length = shaped.norm();
	const math::vec2 raw = length > 0.f ? shaped * (inverseResponse(length, iBinding.response) / length) : shaped;
	const auto first = iBinding.code == 0 ? input::gamepad::LeftX : input::gamepad::RightX;
	state->setAxis(first, raw.x());
	state->setAxis(static_cast<input::GamepadAxis>(first + 1), raw.y());
	return true;
}

auto holdScalar(HeldInput& ioHeld, const input::ActionBinding& iBinding, const float iValue) -> bool {
	switch (iBinding.source) {
		case input::BindingSource::Key:
			ioHeld.keys.push_back(iBinding.code);
			return true;
		case input::BindingSource::MouseButton:
			ioHeld.mouseButtons.push_back(static_cast<uint8_t>(iBinding.code));
			return true;
		case input::BindingSource::GamepadButton:
			{
				auto* state = holdPadState(ioHeld, iBinding.pad);
				if (state != nullptr)
					state->setButton(static_cast<input::GamepadButton>(iBinding.code), true);
				return state != nullptr;
			}
		case input::BindingSource::GamepadAxis:
			{
				auto* state = holdPadState(ioHeld, iBinding.pad);
				if (state != nullptr) {
					const float target = std::clamp(iValue / iBinding.scale, -1.f, 1.f);
					state->setAxis(static_cast<input::GamepadAxis>(iBinding.code),
								   std::copysign(inverseResponse(std::abs(target), iBinding.response), target));
				}
				return state != nullptr;
			}
		case input::BindingSource::GamepadStick:
			return holdStick(ioHeld, iBinding, {iValue, 0.f});
	}
	return false;
}

auto feeds(const input::ActionBinding& iBinding, const bool iYComponent, const float iValue) -> bool {
	if (!(std::abs(iBinding.scale) > 0.f))
		return false;
	const bool analog = iBinding.source == input::BindingSource::GamepadAxis ||
						iBinding.source == input::BindingSource::GamepadStick;
	if (!analog && (iBinding.scale < 0.f) != (iValue < 0.f))
		return false;
	if (iBinding.source == input::BindingSource::GamepadStick)
		return iBinding.component == input::BindingComponent::Both ||
			   (iBinding.component == input::BindingComponent::Y) == iYComponent;
	return (iBinding.component == input::BindingComponent::Y) == iYComponent;
}

auto holdVector(HeldInput& ioHeld, const ActionHold& iHold, const input::ActionDefinition& iAction) -> bool {
	std::vector<std::pair<const input::ActionBinding*, math::vec2>> sticks;
	for (const bool yComponent: {false, true}) {
		const float value = yComponent ? iHold.value.y() : iHold.value.x();
		if (!(std::abs(value) > 0.f))
			continue;
		const auto found = std::ranges::find_if(iAction.bindings, [&](const input::ActionBinding& iBinding) -> bool {
			return feeds(iBinding, yComponent, value);
		});
		if (found == iAction.bindings.end())
			return false;
		if (found->source != input::BindingSource::GamepadStick) {
			if (!holdScalar(ioHeld, *found, value))
				return false;
			continue;
		}
		auto stick =
				std::ranges::find_if(sticks, [&found](const auto& iEntry) -> bool { return iEntry.first == &*found; });
		if (stick == sticks.end())
			stick = sticks.insert(sticks.end(), {&*found, math::vec2{0.f, 0.f}});
		(yComponent ? stick->second.y() : stick->second.x()) = value;
	}
	return std::ranges::all_of(
			sticks, [&ioHeld](const auto& iEntry) -> bool { return holdStick(ioHeld, *iEntry.first, iEntry.second); });
}

auto holdAction(HeldInput& ioHeld, const ActionHold& iHold, const input::ActionDefinition& iAction) -> std::string {
	const auto fail = [&iHold]() -> std::string {
		return std::format("action `{}` has no binding to hold it at that value (or two gamepads are needed)",
						   iHold.name);
	};
	if (iAction.kind == input::ActionKind::Vector)
		return holdVector(ioHeld, iHold, iAction) ? std::string{} : fail();
	if (!(std::abs(iHold.value.x()) > 0.f))
		return {};
	const float value = iAction.kind == input::ActionKind::Button ? 1.f : iHold.value.x();
	const auto found = std::ranges::find_if(iAction.bindings, [value](const input::ActionBinding& iBinding) -> bool {
		return feeds(iBinding, false, value);
	});
	return found != iAction.bindings.end() && holdScalar(ioHeld, *found, value) ? std::string{} : fail();
}


}// namespace

auto resolveActionHolds(const input::ActionMap& iMap, const std::vector<ActionHold>& iHolds, HeldInput& ioHeld)
		-> std::string {
	for (const auto& hold: iHolds) {
		const auto* action = iMap.find(hold.name);
		if (action == nullptr)
			return std::format("unknown action `{}`", hold.name);
		if (auto error = holdAction(ioHeld, hold, *action); !error.empty())
			return error;
	}
	return {};
}

void injectHeldInput(const HeldInput& iHeld) {
	for (const auto key: iHeld.keys) input::Input::injectKey(key);
	for (const auto button: iHeld.mouseButtons) input::Input::injectMouseButton(button);
	if (iHeld.gamepad)
		input::Input::injectGamepad(iHeld.gamepad->pad, iHeld.gamepad->state);
}

}// namespace owl::nest::session
