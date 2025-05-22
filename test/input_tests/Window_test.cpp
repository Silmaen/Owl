
#include "testHelper.h"

#include <input/Window.h>

using namespace owl::input;

TEST(Window, creation) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	Properties props;
	props.winType = static_cast<Type>(-1);
	auto wnd = Window::create(props);
	EXPECT_FALSE(wnd);
	props.winType = Type::Null;
	wnd = Window::create(props);
	ASSERT_TRUE(wnd);
	EXPECT_EQ(wnd->getType(), Type::Null);
	EXPECT_EQ(wnd->getHeight(), 900);
	EXPECT_EQ(wnd->getWidth(), 1600);
	EXPECT_EQ(wnd->getSize(), owl::math::vec2ui(1600, 900));
	EXPECT_EQ(wnd->getNativeWindow(), nullptr);
	EXPECT_TRUE(wnd->isVSync());
	wnd->onUpdate();
	wnd->setVSync(false);
	EXPECT_FALSE(wnd->isVSync());
	owl::core::Log::invalidate();
}
