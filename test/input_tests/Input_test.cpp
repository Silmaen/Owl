#include "testHelper.h"

#include <input/Input.h>

using namespace owl::input;

TEST(input, Input_uninitialized) {
	Input::invalidate();
	EXPECT_FALSE(Input::isMouseButtonPressed(0));
	EXPECT_FALSE(Input::isKeyPressed(0));
	EXPECT_EQ(Input::getMouseX(), 0);
	EXPECT_EQ(Input::getMouseY(), 0);
	const auto pos = Input::getMousePos();
	Input::injectKey(0);
	EXPECT_FALSE(Input::isKeyPressed(0));
	Input::injectMouseButton(0);
	Input::injectMousePos(pos);
	EXPECT_EQ(pos.x(), 0);
	EXPECT_EQ(pos.y(), 0);
	Input::invalidate();
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
