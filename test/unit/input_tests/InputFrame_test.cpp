/**
 * @file InputFrame_test.cpp
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <input/Input.h>
#include <input/InputFrame.h>
#include <window/Window.h>

using namespace owl;
using namespace owl::input;

namespace {

auto withKey(const KeyCode iKey) -> InputState {
	InputState state;
	state.setKey(iKey, true);
	return state;
}

}// namespace

TEST(InputFrame, StateIgnoresCodesOutOfRange) {
	InputState state;
	state.setKey(key::Menu, true);
	state.setKey(static_cast<KeyCode>(g_KeyCount + 5), true);
	state.setMouseButton(mouse::ButtonLast, true);
	state.setMouseButton(static_cast<MouseCode>(g_MouseButtonCount), true);
	EXPECT_TRUE(state.isKeyPressed(key::Menu));
	EXPECT_FALSE(state.isKeyPressed(static_cast<KeyCode>(g_KeyCount + 5)));
	EXPECT_TRUE(state.isMouseButtonPressed(mouse::ButtonLast));
	EXPECT_FALSE(state.isMouseButtonPressed(static_cast<MouseCode>(g_MouseButtonCount)));
	EXPECT_EQ(state.keys.count(), 1u);
	EXPECT_EQ(state.mouseButtons.count(), 1u);
}

TEST(InputFrame, EdgesComeFromThePreviousAndCurrentSamples) {
	InputFrame frame;
	InputState down = withKey(key::E);
	down.setMouseButton(mouse::ButtonLeft, true);
	down.cursor = {10.f, 4.f};
	frame.advance(down);
	EXPECT_TRUE(frame.isKeyPressed(key::E));
	EXPECT_TRUE(frame.isKeyJustPressed(key::E));
	EXPECT_FALSE(frame.isKeyJustReleased(key::E));
	EXPECT_TRUE(frame.isMouseButtonJustPressed(mouse::ButtonLeft));
	EXPECT_EQ(frame.getCursorDelta(), (math::vec2{10.f, 4.f}));
	frame.advance(down);
	EXPECT_TRUE(frame.isKeyPressed(key::E));
	EXPECT_FALSE(frame.isKeyJustPressed(key::E));
	EXPECT_TRUE(frame.isMouseButtonPressed(mouse::ButtonLeft));
	EXPECT_FALSE(frame.isMouseButtonJustPressed(mouse::ButtonLeft));
	EXPECT_EQ(frame.getCursorDelta(), (math::vec2{0.f, 0.f}));
	frame.advance({});
	EXPECT_FALSE(frame.isKeyPressed(key::E));
	EXPECT_TRUE(frame.isKeyJustReleased(key::E));
	EXPECT_TRUE(frame.isMouseButtonJustReleased(mouse::ButtonLeft));
	EXPECT_EQ(frame.getCursor(), (math::vec2{0.f, 0.f}));
}

TEST(InputFrame, ResetStartsWithoutEdgeNorWheel) {
	InputFrame frame;
	InputState held = withKey(key::Escape);
	held.wheel = {0.f, 3.f};
	frame.reset(held);
	EXPECT_TRUE(frame.isKeyPressed(key::Escape));
	EXPECT_FALSE(frame.isKeyJustPressed(key::Escape));
	EXPECT_EQ(frame.getWheel(), (math::vec2{0.f, 0.f}));
}

TEST(InputSampler, OneTickPerFrameFollowsTheFrames) {
	InputSampler sampler;
	InputFrame tick;
	sampler.reset({});
	tick.reset({});
	sampler.observe(withKey(key::Space));
	tick.advance(sampler.take());
	EXPECT_TRUE(tick.isKeyJustPressed(key::Space));
	sampler.observe(withKey(key::Space));
	tick.advance(sampler.take());
	EXPECT_FALSE(tick.isKeyJustPressed(key::Space));
	EXPECT_TRUE(tick.isKeyPressed(key::Space));
	sampler.observe({});
	tick.advance(sampler.take());
	EXPECT_TRUE(tick.isKeyJustReleased(key::Space));
}

TEST(InputSampler, SeveralTicksInOneFrameSeeTheEdgeOnce) {
	InputSampler sampler;
	InputFrame tick;
	sampler.reset({});
	tick.reset({});
	InputState frame = withKey(key::J);
	frame.wheel = {0.f, 2.f};
	sampler.observe(frame);
	int presses = 0;
	float wheel = 0.f;
	for (int i = 0; i < 3; ++i) {
		tick.advance(sampler.take());
		presses += tick.isKeyJustPressed(key::J) ? 1 : 0;
		wheel += tick.getWheel().y();
		EXPECT_TRUE(tick.isKeyPressed(key::J));
	}
	EXPECT_EQ(presses, 1);
	EXPECT_FLOAT_EQ(wheel, 2.f);
}

TEST(InputSampler, FramesWithoutTickKeepAShortPress) {
	InputSampler sampler;
	InputFrame tick;
	sampler.reset({});
	tick.reset({});
	// Pressed then released by two frames that run no tick.
	InputState pressed = withKey(key::E);
	pressed.wheel = {1.f, 0.f};
	sampler.observe(pressed);
	sampler.observe({});
	tick.advance(sampler.take());
	EXPECT_TRUE(tick.isKeyJustPressed(key::E));
	EXPECT_FLOAT_EQ(tick.getWheel().x(), 1.f);
	tick.advance(sampler.take());
	EXPECT_TRUE(tick.isKeyJustReleased(key::E));
	EXPECT_FLOAT_EQ(tick.getWheel().x(), 0.f);
	tick.advance(sampler.take());
	EXPECT_FALSE(tick.isKeyJustPressed(key::E));
	EXPECT_FALSE(tick.isKeyJustReleased(key::E));
}

TEST(InputSampler, ResetDropsThePendingInput) {
	InputSampler sampler;
	sampler.observe(withKey(key::A));
	InputState held = withKey(key::D);
	held.wheel = {0.f, 5.f};
	sampler.reset(held);
	const InputState tick = sampler.take();
	EXPECT_FALSE(tick.isKeyPressed(key::A));
	EXPECT_TRUE(tick.isKeyPressed(key::D));
	EXPECT_EQ(tick.wheel, (math::vec2{0.f, 0.f}));
}

TEST(InputLayer, InjectionIsMergedIntoTheState) {
	Input::init(window::Type::Null);
	Input::injectKey(key::W);
	Input::injectMouseButton(mouse::ButtonRight);
	Input::injectMousePos({12.f, 34.f});
	const InputState state = Input::readState();
	EXPECT_TRUE(state.isKeyPressed(key::W));
	EXPECT_TRUE(state.isMouseButtonPressed(mouse::ButtonRight));
	EXPECT_EQ(state.cursor, (math::vec2{12.f, 34.f}));
	Input::resetInjection();
	const InputState cleared = Input::readState();
	EXPECT_FALSE(cleared.isKeyPressed(key::W));
	EXPECT_FALSE(cleared.isMouseButtonPressed(mouse::ButtonRight));
	EXPECT_EQ(cleared.cursor, (math::vec2{0.f, 0.f}));
	Input::invalidate();
}

TEST(InputLayer, InjectionWorksOnTheGlfwBackend) {
	// No application window: the GLFW back-end reports no device, the injection still drives the input.
	Input::init(window::Type::Glfw);
	EXPECT_FALSE(Input::isKeyPressed(key::E));
	Input::injectKey(key::E);
	EXPECT_TRUE(Input::isKeyPressed(key::E));
	EXPECT_TRUE(Input::readState().isKeyPressed(key::E));
	Input::injectMousePos({3.f, 4.f});
	EXPECT_EQ(Input::getMousePos(), (math::vec2{3.f, 4.f}));
	Input::invalidate();
	EXPECT_FALSE(Input::isKeyPressed(key::E));
}

TEST(InputLayer, WheelBelongsToTheFrameItWasScrolledBefore) {
	Input::init(window::Type::Null);
	Input::addWheel({0.f, 1.f});
	Input::addWheel({0.5f, 1.f});
	EXPECT_EQ(Input::readState().wheel, (math::vec2{0.f, 0.f}));
	Input::beginFrame();
	EXPECT_EQ(Input::readState().wheel, (math::vec2{0.5f, 2.f}));
	EXPECT_EQ(Input::readState().wheel, (math::vec2{0.5f, 2.f}));
	Input::beginFrame();
	EXPECT_EQ(Input::readState().wheel, (math::vec2{0.f, 0.f}));
	Input::invalidate();
}
