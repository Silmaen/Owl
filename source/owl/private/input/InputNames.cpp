/**
 * @file InputNames.cpp
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "input/InputNames.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace owl::input::names {

namespace {

template<typename Code>
struct NamedCode {
	std::string_view name;
	Code code;
};

#define OWL_KEY(iName)                                                                                                 \
	NamedCode<KeyCode> { #iName, key::iName }

constexpr std::array g_keys{
		OWL_KEY(Space),
		OWL_KEY(Apostrophe),
		OWL_KEY(Comma),
		OWL_KEY(Minus),
		OWL_KEY(Period),
		OWL_KEY(Slash),
		OWL_KEY(D0),
		OWL_KEY(D1),
		OWL_KEY(D2),
		OWL_KEY(D3),
		OWL_KEY(D4),
		OWL_KEY(D5),
		OWL_KEY(D6),
		OWL_KEY(D7),
		OWL_KEY(D8),
		OWL_KEY(D9),
		OWL_KEY(Semicolon),
		OWL_KEY(Equal),
		OWL_KEY(A),
		OWL_KEY(B),
		OWL_KEY(C),
		OWL_KEY(D),
		OWL_KEY(E),
		OWL_KEY(F),
		OWL_KEY(G),
		OWL_KEY(H),
		OWL_KEY(I),
		OWL_KEY(J),
		OWL_KEY(K),
		OWL_KEY(L),
		OWL_KEY(M),
		OWL_KEY(N),
		OWL_KEY(O),
		OWL_KEY(P),
		OWL_KEY(Q),
		OWL_KEY(R),
		OWL_KEY(S),
		OWL_KEY(T),
		OWL_KEY(U),
		OWL_KEY(V),
		OWL_KEY(W),
		OWL_KEY(X),
		OWL_KEY(Y),
		OWL_KEY(Z),
		OWL_KEY(LeftBracket),
		OWL_KEY(Backslash),
		OWL_KEY(RightBracket),
		OWL_KEY(GraveAccent),
		OWL_KEY(Escape),
		OWL_KEY(Enter),
		OWL_KEY(Tab),
		OWL_KEY(Backspace),
		OWL_KEY(Insert),
		OWL_KEY(Delete),
		OWL_KEY(Right),
		OWL_KEY(Left),
		OWL_KEY(Down),
		OWL_KEY(Up),
		OWL_KEY(PageUp),
		OWL_KEY(PageDown),
		OWL_KEY(Home),
		OWL_KEY(End),
		OWL_KEY(CapsLock),
		OWL_KEY(ScrollLock),
		OWL_KEY(NumLock),
		OWL_KEY(PrintScreen),
		OWL_KEY(Pause),
		OWL_KEY(F1),
		OWL_KEY(F2),
		OWL_KEY(F3),
		OWL_KEY(F4),
		OWL_KEY(F5),
		OWL_KEY(F6),
		OWL_KEY(F7),
		OWL_KEY(F8),
		OWL_KEY(F9),
		OWL_KEY(F10),
		OWL_KEY(F11),
		OWL_KEY(F12),
		OWL_KEY(F13),
		OWL_KEY(F14),
		OWL_KEY(F15),
		OWL_KEY(F16),
		OWL_KEY(F17),
		OWL_KEY(F18),
		OWL_KEY(F19),
		OWL_KEY(F20),
		OWL_KEY(F21),
		OWL_KEY(F22),
		OWL_KEY(F23),
		OWL_KEY(F24),
		OWL_KEY(F25),
		OWL_KEY(Kp0),
		OWL_KEY(Kp1),
		OWL_KEY(Kp2),
		OWL_KEY(Kp3),
		OWL_KEY(Kp4),
		OWL_KEY(Kp5),
		OWL_KEY(Kp6),
		OWL_KEY(Kp7),
		OWL_KEY(Kp8),
		OWL_KEY(Kp9),
		OWL_KEY(KpDecimal),
		OWL_KEY(KpDivide),
		OWL_KEY(KpMultiply),
		OWL_KEY(KpSubtract),
		OWL_KEY(KpAdd),
		OWL_KEY(KpEnter),
		OWL_KEY(KpEqual),
		OWL_KEY(LeftShift),
		OWL_KEY(LeftControl),
		OWL_KEY(LeftAlt),
		OWL_KEY(LeftSuper),
		OWL_KEY(RightShift),
		OWL_KEY(RightControl),
		OWL_KEY(RightAlt),
		OWL_KEY(RightSuper),
		OWL_KEY(Menu),
};

#undef OWL_KEY

constexpr std::array g_mouseButtons{
		NamedCode<MouseCode>{"Left", mouse::ButtonLeft},     NamedCode<MouseCode>{"Right", mouse::ButtonRight},
		NamedCode<MouseCode>{"Middle", mouse::ButtonMiddle}, NamedCode<MouseCode>{"Button3", mouse::Button3},
		NamedCode<MouseCode>{"Button4", mouse::Button4},     NamedCode<MouseCode>{"Button5", mouse::Button5},
		NamedCode<MouseCode>{"Button6", mouse::Button6},     NamedCode<MouseCode>{"Button7", mouse::Button7},
};

constexpr std::array g_gamepadButtons{
		NamedCode<GamepadButton>{"A", gamepad::A},
		NamedCode<GamepadButton>{"B", gamepad::B},
		NamedCode<GamepadButton>{"X", gamepad::X},
		NamedCode<GamepadButton>{"Y", gamepad::Y},
		NamedCode<GamepadButton>{"LeftBumper", gamepad::LeftBumper},
		NamedCode<GamepadButton>{"RightBumper", gamepad::RightBumper},
		NamedCode<GamepadButton>{"Back", gamepad::Back},
		NamedCode<GamepadButton>{"Start", gamepad::Start},
		NamedCode<GamepadButton>{"Guide", gamepad::Guide},
		NamedCode<GamepadButton>{"LeftThumb", gamepad::LeftThumb},
		NamedCode<GamepadButton>{"RightThumb", gamepad::RightThumb},
		NamedCode<GamepadButton>{"DpadUp", gamepad::DpadUp},
		NamedCode<GamepadButton>{"DpadRight", gamepad::DpadRight},
		NamedCode<GamepadButton>{"DpadDown", gamepad::DpadDown},
		NamedCode<GamepadButton>{"DpadLeft", gamepad::DpadLeft},
};

constexpr std::array g_gamepadAxes{
		NamedCode<GamepadAxis>{"left_x", gamepad::LeftX},
		NamedCode<GamepadAxis>{"left_y", gamepad::LeftY},
		NamedCode<GamepadAxis>{"right_x", gamepad::RightX},
		NamedCode<GamepadAxis>{"right_y", gamepad::RightY},
		NamedCode<GamepadAxis>{"left_trigger", gamepad::LeftTrigger},
		NamedCode<GamepadAxis>{"right_trigger", gamepad::RightTrigger},
};

template<typename Code, std::size_t Size>
auto nameOf(const std::array<NamedCode<Code>, Size>& iTable, const Code iCode) -> std::string {
	const auto found = std::ranges::find(iTable, iCode, &NamedCode<Code>::code);
	return found != iTable.end() ? std::string{found->name} : std::to_string(iCode);
}

template<typename Code, std::size_t Size>
auto codeOf(const std::array<NamedCode<Code>, Size>& iTable, const std::string_view iName, const std::size_t iCount)
		-> std::optional<Code> {
	if (const auto found = std::ranges::find(iTable, iName, &NamedCode<Code>::name); found != iTable.end())
		return found->code;
	if (iName.empty() || iName.size() > 5 ||
		!std::ranges::all_of(iName, [](const char iChar) -> bool { return iChar >= '0' && iChar <= '9'; }))
		return std::nullopt;
	std::size_t number = 0;
	for (const char digit: iName) number = number * 10 + static_cast<std::size_t>(digit - '0');
	if (number < iCount)
		return static_cast<Code>(number);
	return std::nullopt;
}

}// namespace

auto keyName(const KeyCode iKey) -> std::string { return nameOf(g_keys, iKey); }

auto parseKey(const std::string_view iName) -> std::optional<KeyCode> {
	return codeOf(g_keys, iName, std::size_t{key::Menu} + 1);
}

auto mouseButtonName(const MouseCode iButton) -> std::string { return nameOf(g_mouseButtons, iButton); }

auto parseMouseButton(const std::string_view iName) -> std::optional<MouseCode> {
	return codeOf(g_mouseButtons, iName, std::size_t{mouse::ButtonLast} + 1);
}

auto gamepadButtonName(const GamepadButton iButton) -> std::string { return nameOf(g_gamepadButtons, iButton); }

auto parseGamepadButton(const std::string_view iName) -> std::optional<GamepadButton> {
	return codeOf(g_gamepadButtons, iName, g_GamepadButtonCount);
}

auto gamepadAxisName(const GamepadAxis iAxis) -> std::string { return nameOf(g_gamepadAxes, iAxis); }

auto parseGamepadAxis(const std::string_view iName) -> std::optional<GamepadAxis> {
	return codeOf(g_gamepadAxes, iName, g_GamepadAxisCount);
}

}// namespace owl::input::names
