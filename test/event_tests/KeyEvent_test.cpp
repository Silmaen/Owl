/**
 * @file KeyEvent_test.cpp
 * @author Silmaen
 * @date 05/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <event/KeyEvent.h>

using namespace owl::event;

TEST(KeyEvent, Pressed) {
	const KeyPressedEvent event(owl::input::key::A, 12);
	EXPECT_EQ(event.getKeyCode(), owl::input::key::A);
	EXPECT_STREQ(event.getName().c_str(), "KeyPressedEvent");
	EXPECT_EQ(event.getType(), Type::KeyPressed);
	EXPECT_EQ(event.getStaticType(), Type::KeyPressed);
	EXPECT_TRUE(event.getRepeatCount());
	EXPECT_EQ(event.getCategoryFlags(), Category::Input | Category::Keyboard);
	EXPECT_STREQ(event.toString().c_str(), "KeyPressedEvent: 65 (repeat = 12)");
	EXPECT_TRUE(event.isInCategory(Category::Input));
	EXPECT_TRUE(event.isInCategory(Category::Keyboard));
	EXPECT_FALSE(event.isInCategory(Category::Application));
	EXPECT_FALSE(event.isInCategory(Category::Mouse));
	EXPECT_FALSE(event.isInCategory(Category::MouseButton));
	EXPECT_FALSE(event.isInCategory(Category::None));
}

TEST(KeyEvent, Typed) {
	const KeyTypedEvent event(12);
	EXPECT_STREQ(event.getName().c_str(), "KeyTypedEvent");
	EXPECT_EQ(event.getType(), Type::KeyTyped);
	EXPECT_EQ(event.getStaticType(), Type::KeyTyped);
	EXPECT_EQ(event.getCategoryFlags(), Category::Input | Category::Keyboard);
	EXPECT_STREQ(event.toString().c_str(), "KeyTypedEvent: 12");
	EXPECT_TRUE(event.isInCategory(Category::Input));
	EXPECT_TRUE(event.isInCategory(Category::Keyboard));
	EXPECT_FALSE(event.isInCategory(Category::Application));
	EXPECT_FALSE(event.isInCategory(Category::Mouse));
	EXPECT_FALSE(event.isInCategory(Category::MouseButton));
	EXPECT_FALSE(event.isInCategory(Category::None));
}

TEST(KeyEvent, Released) {
	const KeyReleasedEvent event(12);
	EXPECT_STREQ(event.getName().c_str(), "KeyReleasedEvent");
	EXPECT_EQ(event.getType(), Type::KeyReleased);
	EXPECT_EQ(event.getStaticType(), Type::KeyReleased);
	EXPECT_EQ(event.getCategoryFlags(), Category::Input | Category::Keyboard);
	EXPECT_STREQ(event.toString().c_str(), "KeyReleasedEvent: 12");
	EXPECT_TRUE(event.isInCategory(Category::Input));
	EXPECT_TRUE(event.isInCategory(Category::Keyboard));
	EXPECT_FALSE(event.isInCategory(Category::Application));
	EXPECT_FALSE(event.isInCategory(Category::Mouse));
	EXPECT_FALSE(event.isInCategory(Category::MouseButton));
	EXPECT_FALSE(event.isInCategory(Category::None));
}
