/**
 * @file Input_test.cpp
 * @author Silmaen
 * @date 03/01/2024
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <input/Input.h>
#include <window/Window.h>

using namespace owl::input;

TEST(input, Input_uninitialized) {
	Input::invalidate();
	EXPECT_FALSE(Input::isMouseButtonPressed(0));
	EXPECT_FALSE(Input::isKeyPressed(0));
	EXPECT_EQ(Input::getMouseX(), 0);
	EXPECT_EQ(Input::getMouseY(), 0);
	const auto pos = Input::getMousePos();
	EXPECT_EQ(pos.x(), 0);
	EXPECT_EQ(pos.y(), 0);
	// The injection belongs to the input layer: it works without a back-end and is cleared with the input.
	Input::injectKey(0);
	EXPECT_TRUE(Input::isKeyPressed(0));
	Input::injectMouseButton(0);
	EXPECT_TRUE(Input::isMouseButtonPressed(0));
	Input::injectMousePos({5.f, 6.f});
	EXPECT_EQ(Input::getMouseY(), 6.f);
	Input::invalidate();
	EXPECT_FALSE(Input::isKeyPressed(0));
	EXPECT_FALSE(Input::isMouseButtonPressed(0));
	EXPECT_EQ(Input::getMouseX(), 0);
}

TEST(input, Input_doubleinit) {
	Input::init(owl::window::Type::Glfw);
	Input::init(owl::window::Type::Null);
	Input::injectKey(3);
	EXPECT_TRUE(Input::isKeyPressed(3));
	Input::injectKey(3);
	EXPECT_FALSE(Input::isKeyPressed(3));
	Input::invalidate();
}
