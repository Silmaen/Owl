/**
 * @file Gamepad_test.cpp
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <event/GamepadEvent.h>
#include <input/Gamepad.h>
#include <input/Input.h>
#include <input/InputFrame.h>
#include <window/Window.h>

#include <cmath>
#include <cstddef>
#include <filesystem>
#include <format>
#include <fstream>
#include <limits>

using namespace owl;
using namespace owl::input;

namespace {

constexpr auto g_mapping = "03000000de2800000112000001000000,Steam Controller,a:b0,b:b1,x:b2,y:b3,platform:Linux,";

auto withPadButton(const std::size_t iPad, const GamepadButton iButton) -> InputState {
	InputState state;
	state.gamepads[iPad].connected = true;
	state.gamepads[iPad].setButton(iButton, true);
	return state;
}

}// namespace

TEST(Gamepad, StateIgnoresCodesOutOfRange) {
	GamepadState pad;
	pad.setButton(gamepad::ButtonLast, true);
	pad.setButton(static_cast<GamepadButton>(g_GamepadButtonCount), true);
	pad.setAxis(gamepad::RightTrigger, 0.5f);
	pad.setAxis(static_cast<GamepadAxis>(g_GamepadAxisCount), 1.f);
	EXPECT_TRUE(pad.isButtonPressed(gamepad::DpadLeft));
	EXPECT_FALSE(pad.isButtonPressed(static_cast<GamepadButton>(g_GamepadButtonCount)));
	EXPECT_EQ(pad.buttons.count(), 1u);
	EXPECT_FLOAT_EQ(pad.getAxis(gamepad::RightTrigger), 0.5f);
	EXPECT_FLOAT_EQ(pad.getAxis(static_cast<GamepadAxis>(g_GamepadAxisCount)), 0.f);
	pad.setAxis(gamepad::RightX, 0.25f);
	pad.setAxis(gamepad::RightY, -0.75f);
	EXPECT_EQ(pad.getStick(gamepad::Stick::Right), (math::vec2{0.25f, -0.75f}));
	EXPECT_EQ(pad.getStick(gamepad::Stick::Left), (math::vec2{0.f, 0.f}));

	const InputState state;
	EXPECT_FALSE(state.isGamepadConnected(g_GamepadCount));
	EXPECT_FALSE(state.isGamepadButtonPressed(g_GamepadCount, gamepad::A));
}

TEST(Gamepad, ResponseCurveExponents) {
	EXPECT_FLOAT_EQ(AxisResponse{.curve = ResponseCurve::Linear}.getExponent(), 1.f);
	EXPECT_FLOAT_EQ(AxisResponse{.curve = ResponseCurve::Quadratic}.getExponent(), 2.f);
	EXPECT_FLOAT_EQ(AxisResponse{.curve = ResponseCurve::Cubic}.getExponent(), 3.f);
	EXPECT_FLOAT_EQ((AxisResponse{.curve = ResponseCurve::Custom, .exponent = 1.5f}.getExponent()), 1.5f);
	EXPECT_FLOAT_EQ((AxisResponse{.curve = ResponseCurve::Custom, .exponent = 0.f}.getExponent()), 0.1f);
	EXPECT_FLOAT_EQ((AxisResponse{.curve = ResponseCurve::Custom, .exponent = 50.f}.getExponent()), 10.f);
	EXPECT_FLOAT_EQ((AxisResponse{.curve = ResponseCurve::Custom, .exponent = std::numeric_limits<float>::quiet_NaN()}
							 .getExponent()),
					1.f);
}

TEST(Gamepad, AxisDeadZoneRescalesAndKeepsTheSign) {
	const AxisResponse linear{};
	EXPECT_FLOAT_EQ(linear.deadZone, 0.15f);
	EXPECT_FLOAT_EQ(applyAxisResponse(0.1f, linear), 0.f);
	EXPECT_FLOAT_EQ(applyAxisResponse(-0.15f, linear), 0.f);
	EXPECT_FLOAT_EQ(applyAxisResponse(1.f, linear), 1.f);
	EXPECT_FLOAT_EQ(applyAxisResponse(-1.f, linear), -1.f);
	EXPECT_NEAR(applyAxisResponse(0.575f, linear), 0.5f, 1e-5f);
	EXPECT_NEAR(applyAxisResponse(-0.575f, linear), -0.5f, 1e-5f);
	EXPECT_FLOAT_EQ(applyAxisResponse(2.f, linear), 1.f);
	EXPECT_FLOAT_EQ(applyAxisResponse(std::numeric_limits<float>::quiet_NaN(), linear), 0.f);

	const AxisResponse quadratic{.curve = ResponseCurve::Quadratic};
	EXPECT_NEAR(applyAxisResponse(0.575f, quadratic), 0.25f, 1e-5f);
	const AxisResponse cubic{.deadZone = 0.f, .curve = ResponseCurve::Cubic};
	EXPECT_NEAR(applyAxisResponse(-0.5f, cubic), -0.125f, 1e-5f);
	const AxisResponse noDeadZone{.deadZone = -1.f};
	EXPECT_NEAR(applyAxisResponse(0.01f, noDeadZone), 0.01f, 1e-6f);
	const AxisResponse fullDeadZone{.deadZone = 5.f};
	EXPECT_FLOAT_EQ(applyAxisResponse(0.98f, fullDeadZone), 0.f);
	EXPECT_FLOAT_EQ(applyAxisResponse(1.f, fullDeadZone), 1.f);
}

TEST(Gamepad, StickDeadZoneIsRadial) {
	const AxisResponse linear{};
	EXPECT_EQ(applyStickResponse({0.1f, 0.1f}, linear), (math::vec2{0.f, 0.f}));
	// Both axes below the dead zone, but the diagonal is outside: an axial dead zone would drop it.
	const auto diagonal = applyStickResponse({0.14f, 0.14f}, linear);
	EXPECT_GT(diagonal.x(), 0.f);
	EXPECT_FLOAT_EQ(diagonal.x(), diagonal.y());
	const auto right = applyStickResponse({1.f, 0.f}, linear);
	EXPECT_FLOAT_EQ(right.x(), 1.f);
	EXPECT_FLOAT_EQ(right.y(), 0.f);
	const auto corner = applyStickResponse({1.f, 1.f}, linear);
	EXPECT_NEAR(corner.norm(), 1.f, 1e-5f);
	EXPECT_NEAR(corner.x(), corner.y(), 1e-6f);
	const auto half = applyStickResponse({0.f, -0.575f}, AxisResponse{.curve = ResponseCurve::Quadratic});
	EXPECT_NEAR(half.y(), -0.25f, 1e-5f);
	EXPECT_FLOAT_EQ(half.x(), 0.f);
	EXPECT_EQ(applyStickResponse({std::numeric_limits<float>::infinity(), 0.f}, linear), (math::vec2{0.f, 0.f}));
}

TEST(Gamepad, MappingLinesAreCounted) {
	EXPECT_EQ(countGamepadMappings(""), 0u);
	EXPECT_EQ(countGamepadMappings("# comment only\n\n"), 0u);
	EXPECT_EQ(countGamepadMappings(g_mapping), 1u);
	EXPECT_EQ(countGamepadMappings(std::format("# header\r\n{}\r\n  {}\n", g_mapping, g_mapping)), 2u);
	EXPECT_EQ(countGamepadMappings("not a mapping\n"), 0u);
	EXPECT_EQ(countGamepadMappings("03000000de2800000112000001000000,,a:b0\n"), 0u);
	EXPECT_EQ(countGamepadMappings("03000000de28000001120000010000zz,Pad,a:b0\n"), 0u);
	EXPECT_EQ(countGamepadMappings("0300,Pad,a:b0\n"), 0u);
}

TEST(Gamepad, FrameEdgesForPadButtons) {
	InputFrame frame;
	frame.reset({});
	frame.advance(withPadButton(1, gamepad::A));
	EXPECT_TRUE(frame.isGamepadConnected(1));
	EXPECT_FALSE(frame.isGamepadConnected(0));
	EXPECT_TRUE(frame.isGamepadButtonPressed(1, gamepad::A));
	EXPECT_TRUE(frame.isGamepadButtonJustPressed(1, gamepad::A));
	EXPECT_FALSE(frame.isGamepadButtonJustPressed(0, gamepad::A));
	EXPECT_FALSE(frame.isGamepadButtonJustReleased(1, gamepad::A));
	frame.advance(withPadButton(1, gamepad::A));
	EXPECT_TRUE(frame.isGamepadButtonPressed(1, gamepad::A));
	EXPECT_FALSE(frame.isGamepadButtonJustPressed(1, gamepad::A));
	frame.advance({});
	EXPECT_TRUE(frame.isGamepadButtonJustReleased(1, gamepad::A));
	EXPECT_FALSE(frame.isGamepadButtonPressed(1, gamepad::A));

	auto moved = withPadButton(0, gamepad::Start);
	moved.gamepads[0].setAxis(gamepad::LeftX, 0.1f);
	moved.gamepads[0].setAxis(gamepad::RightTrigger, 0.75f);
	frame.advance(moved);
	EXPECT_FLOAT_EQ(frame.getGamepadAxis(0, gamepad::LeftX), 0.1f);
	EXPECT_FLOAT_EQ(frame.getGamepadAxis(0, gamepad::RightTrigger), 0.75f);
	EXPECT_EQ(frame.getGamepadStick(0, gamepad::Stick::Left), (math::vec2{0.f, 0.f}));
	EXPECT_NEAR(frame.getGamepadStick(0, gamepad::Stick::Left, {.deadZone = 0.f}).x(), 0.1f, 1e-6f);
}

TEST(Gamepad, SamplerKeepsShortPressesAndTakesTheLastAxes) {
	InputSampler sampler;
	sampler.reset({});
	auto pressed = withPadButton(2, gamepad::B);
	pressed.gamepads[2].setAxis(gamepad::LeftY, -1.f);
	sampler.observe(pressed);
	auto released = InputState{};
	released.gamepads[2].connected = true;
	released.gamepads[2].setAxis(gamepad::LeftY, 0.5f);
	sampler.observe(released);
	const auto tick = sampler.take();
	EXPECT_TRUE(tick.isGamepadButtonPressed(2, gamepad::B));
	EXPECT_TRUE(tick.isGamepadConnected(2));
	EXPECT_FLOAT_EQ(tick.getGamepad(2).getAxis(gamepad::LeftY), 0.5f);
	const auto next = sampler.take();
	EXPECT_FALSE(next.isGamepadButtonPressed(2, gamepad::B));
	EXPECT_FLOAT_EQ(next.getGamepad(2).getAxis(gamepad::LeftY), 0.5f);
}

TEST(Gamepad, NullBackendInjection) {
	core::Log::init(core::Log::Level::Off);
	Input::init(window::Type::Null);
	EXPECT_FALSE(Input::isGamepadConnected(0));
	EXPECT_TRUE(Input::getGamepadName(0).empty());
	EXPECT_FALSE(Input::getGamepadState(g_GamepadCount).connected);
	EXPECT_TRUE(Input::getGamepadName(g_GamepadCount).empty());

	Input::injectGamepadButton(3, gamepad::Y);
	Input::injectGamepadAxis(3, gamepad::LeftX, -0.5f);
	Input::injectGamepadButton(g_GamepadCount, gamepad::Y);
	Input::injectGamepadAxis(g_GamepadCount, gamepad::LeftX, 1.f);
	EXPECT_TRUE(Input::isGamepadConnected(3));
	EXPECT_EQ(Input::getGamepadName(3), "Injected gamepad");
	EXPECT_TRUE(Input::getGamepadState(3).isButtonPressed(gamepad::Y));
	EXPECT_FLOAT_EQ(Input::getGamepadState(3).getAxis(gamepad::LeftX), -0.5f);
	const auto state = Input::readState();
	EXPECT_TRUE(state.isGamepadButtonPressed(3, gamepad::Y));
	EXPECT_FALSE(state.isGamepadConnected(0));

	Input::injectGamepadButton(3, gamepad::Y);
	EXPECT_FALSE(Input::getGamepadState(3).isButtonPressed(gamepad::Y));
	EXPECT_TRUE(Input::isGamepadConnected(3));

	GamepadState unplugged;
	unplugged.setButton(gamepad::A, true);
	Input::injectGamepad(3, unplugged);
	EXPECT_FALSE(Input::isGamepadConnected(3));
	EXPECT_TRUE(Input::getGamepadName(3).empty());
	Input::injectGamepad(g_GamepadCount, unplugged);

	Input::resetInjection();
	EXPECT_FALSE(Input::getGamepadState(3).isButtonPressed(gamepad::A));
	EXPECT_FALSE(Input::isGamepadConnected(3));
	Input::invalidate();
	EXPECT_FALSE(Input::isGamepadConnected(0));
	EXPECT_TRUE(Input::getGamepadName(0).empty());
	core::Log::invalidate();
}

TEST(Gamepad, MappingsAreCheckedThenHandedToTheBackend) {
	core::Log::init(core::Log::Level::Off);
	Input::invalidate();
	EXPECT_FALSE(Input::addGamepadMappings(g_mapping));
	Input::init(window::Type::Null);
	EXPECT_FALSE(Input::addGamepadMappings("# nothing\n"));
	EXPECT_TRUE(Input::addGamepadMappings(g_mapping));

	const auto dir = std::filesystem::temp_directory_path() / "owl_gamepad_test_mappings";
	std::filesystem::remove_all(dir);
	std::filesystem::create_directories(dir);
	{
		std::ofstream file(dir / "gamecontrollerdb.txt");
		file << "# Test mappings\n" << g_mapping << "\n";
	}
	EXPECT_TRUE(Input::loadGamepadMappings(dir / "gamecontrollerdb.txt"));
	EXPECT_FALSE(Input::loadGamepadMappings(dir / "missing.txt"));
	EXPECT_FALSE(Input::loadGamepadMappings(dir));
	std::filesystem::remove_all(dir);
	Input::invalidate();
	core::Log::invalidate();
}

TEST(Gamepad, GlfwBackendWithoutWindowReadsNoPad) {
	core::Log::init(core::Log::Level::Off);
	Input::init(window::Type::Glfw);
	EXPECT_FALSE(Input::isGamepadConnected(0));
	EXPECT_TRUE(Input::getGamepadName(0).empty());
	EXPECT_FALSE(Input::readState().isGamepadConnected(0));
#if OWL_WITH_RENDER
	EXPECT_FALSE(Input::addGamepadMappings(g_mapping));
#endif
	Input::injectGamepadButton(0, gamepad::A);
	EXPECT_TRUE(Input::getGamepadState(0).isButtonPressed(gamepad::A));
	Input::invalidate();
	core::Log::invalidate();
}

TEST(Gamepad, ConnectionEvents) {
	const event::GamepadConnectedEvent connected(2, "Pad");
	EXPECT_EQ(connected.getType(), event::Type::GamepadConnected);
	EXPECT_EQ(event::GamepadConnectedEvent::getStaticType(), event::Type::GamepadConnected);
	EXPECT_EQ(connected.getPad(), 2u);
	EXPECT_EQ(connected.getGamepadName(), "Pad");
	EXPECT_EQ(connected.getName(), "GamepadConnectedEvent");
	EXPECT_EQ(connected.toString(), "GamepadConnectedEvent: 2 (Pad)");
	EXPECT_TRUE(connected.isInCategory(event::Category::Input));
	EXPECT_TRUE(connected.isInCategory(event::Category::Gamepad));
	EXPECT_FALSE(connected.isInCategory(event::Category::Keyboard));

	const event::GamepadDisconnectedEvent disconnected(5);
	EXPECT_EQ(disconnected.getType(), event::Type::GamepadDisconnected);
	EXPECT_EQ(event::GamepadDisconnectedEvent::getStaticType(), event::Type::GamepadDisconnected);
	EXPECT_EQ(disconnected.getPad(), 5u);
	EXPECT_EQ(disconnected.getName(), "GamepadDisconnectedEvent");
	EXPECT_EQ(disconnected.toString(), "GamepadDisconnectedEvent: 5");
	EXPECT_TRUE(disconnected.isInCategory(event::Category::Gamepad));
}
