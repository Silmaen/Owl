/**
 * @file MouseEvent_test.cpp
 * @author Silmaen
 * @date 05/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <event/MouseEvent.h>

using namespace owl::event;

TEST(MouseEvent, ButtonPressed) {
	const MouseButtonPressedEvent event(12);
	EXPECT_STREQ(event.getName().c_str(), "MouseButtonPressedEvent");
	EXPECT_EQ(event.getType(), Type::MouseButtonPressed);
	EXPECT_EQ(event.getStaticType(), Type::MouseButtonPressed);
	EXPECT_EQ(event.getCategoryFlags(), Category::Input | Category::Mouse | Category::MouseButton);
	EXPECT_STREQ(event.toString().c_str(), "MouseButtonPressedEvent: 12");
	EXPECT_TRUE(event.isInCategory(Category::Input));
	EXPECT_FALSE(event.isInCategory(Category::Keyboard));
	EXPECT_FALSE(event.isInCategory(Category::Application));
	EXPECT_TRUE(event.isInCategory(Category::Mouse));
	EXPECT_TRUE(event.isInCategory(Category::MouseButton));
	EXPECT_FALSE(event.isInCategory(Category::None));
	EXPECT_EQ(event.getMouseButton(), 12);
}

TEST(MouseEvent, ButtonReleased) {
	const MouseButtonReleasedEvent event(12);
	EXPECT_STREQ(event.getName().c_str(), "MouseButtonReleasedEvent");
	EXPECT_EQ(event.getType(), Type::MouseButtonReleased);
	EXPECT_EQ(event.getStaticType(), Type::MouseButtonReleased);
	EXPECT_EQ(event.getCategoryFlags(), Category::Input | Category::Mouse | Category::MouseButton);
	EXPECT_STREQ(event.toString().c_str(), "MouseButtonReleasedEvent: 12");
	EXPECT_TRUE(event.isInCategory(Category::Input));
	EXPECT_FALSE(event.isInCategory(Category::Keyboard));
	EXPECT_FALSE(event.isInCategory(Category::Application));
	EXPECT_TRUE(event.isInCategory(Category::Mouse));
	EXPECT_TRUE(event.isInCategory(Category::MouseButton));
	EXPECT_FALSE(event.isInCategory(Category::None));
}

TEST(MouseEvent, Moved) {
	const MouseMovedEvent event(144, 155);
	EXPECT_STREQ(event.getName().c_str(), "MouseMovedEvent");
	EXPECT_EQ(event.getType(), Type::MouseMoved);
	EXPECT_EQ(event.getStaticType(), Type::MouseMoved);
	EXPECT_EQ(event.getCategoryFlags(), Category::Input | Category::Mouse);
	EXPECT_EQ(event.getX(), 144);
	EXPECT_EQ(event.getY(), 155);
	EXPECT_STREQ(event.toString().c_str(), "MouseMovedEvent: 144.0, 155.0");
	EXPECT_TRUE(event.isInCategory(Category::Input));
	EXPECT_FALSE(event.isInCategory(Category::Keyboard));
	EXPECT_FALSE(event.isInCategory(Category::Application));
	EXPECT_TRUE(event.isInCategory(Category::Mouse));
	EXPECT_FALSE(event.isInCategory(Category::MouseButton));
	EXPECT_FALSE(event.isInCategory(Category::None));
}

TEST(MouseEvent, Scrolled) {
	const MouseScrolledEvent event(1, 1);
	EXPECT_STREQ(event.getName().c_str(), "MouseScrolledEvent");
	EXPECT_EQ(event.getType(), Type::MouseScrolled);
	EXPECT_EQ(event.getStaticType(), Type::MouseScrolled);
	EXPECT_EQ(event.getCategoryFlags(), Category::Input | Category::Mouse);
	EXPECT_EQ(event.getXOff(), 1);
	EXPECT_EQ(event.getYOff(), 1);
	EXPECT_STREQ(event.toString().c_str(), "MouseScrolledEvent: 1, 1");
	EXPECT_TRUE(event.isInCategory(Category::Input));
	EXPECT_FALSE(event.isInCategory(Category::Keyboard));
	EXPECT_FALSE(event.isInCategory(Category::Application));
	EXPECT_TRUE(event.isInCategory(Category::Mouse));
	EXPECT_FALSE(event.isInCategory(Category::MouseButton));
	EXPECT_FALSE(event.isInCategory(Category::None));
}
