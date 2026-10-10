/**
 * @file WindowPlatform_test.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <window/Window.h>
#include <window/glfw/WaylandDecorations.h>

#include <cstdlib>
#include <string>

#include <optional>

using namespace owl::window;

TEST(WindowPlatform, parse) {
	EXPECT_EQ(parsePlatform("auto"), Platform::Auto);
	EXPECT_EQ(parsePlatform("Wayland"), Platform::Wayland);
	EXPECT_EQ(parsePlatform("X11"), Platform::X11);
	EXPECT_EQ(parsePlatform("xwayland"), Platform::X11);
	EXPECT_EQ(parsePlatform("WIN32"), Platform::Win32);
	EXPECT_EQ(parsePlatform("null"), Platform::None);
	EXPECT_FALSE(parsePlatform("").has_value());
	EXPECT_FALSE(parsePlatform("mir").has_value());
}

TEST(WindowPlatform, nameRoundTrip) {
	for (const auto platform: {Platform::Auto, Platform::Wayland, Platform::X11, Platform::Win32, Platform::None})
		EXPECT_EQ(parsePlatform(platformName(platform)), platform);
}

TEST(WindowPlatform, resolve) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	EXPECT_EQ(resolvePlatform(Platform::Wayland, std::nullopt), Platform::Wayland);
	EXPECT_EQ(resolvePlatform(Platform::Wayland, ""), Platform::Wayland);
	EXPECT_EQ(resolvePlatform(Platform::Wayland, "x11"), Platform::X11);
	EXPECT_EQ(resolvePlatform(Platform::X11, "AUTO"), Platform::Auto);
	EXPECT_EQ(resolvePlatform(Platform::X11, "bogus"), Platform::X11);
	owl::core::Log::invalidate();
}

TEST(WindowPlatform, appId) {
	EXPECT_EQ(makeAppId("Owl Nest"), "owl-nest");
	EXPECT_EQ(makeAppId("OwlFeatureDemo"), "owlfeaturedemo");
	EXPECT_EQ(makeAppId("My  Game: v1.2!"), "my-game-v1.2");
	EXPECT_EQ(makeAppId("org.owl.Runner"), "org.owl.runner");
	EXPECT_EQ(makeAppId(""), "owl-engine");
	EXPECT_EQ(makeAppId("???"), "owl-engine");
}

TEST(WindowPlatform, nullWindow) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	Properties props;
	props.winType = Type::Null;
	props.platform = Platform::Wayland;
	const auto wnd = Window::create(props);
	ASSERT_TRUE(wnd);
	EXPECT_EQ(wnd->getPlatform(), Platform::None);
	EXPECT_EQ(wnd->getContentScale(), owl::math::vec2(1.f, 1.f));
	EXPECT_EQ(wnd->getPresentedFrames(), 0u);
	wnd->onUpdate();
	wnd->onUpdate();
	EXPECT_EQ(wnd->getPresentedFrames(), 2u);
	owl::core::Log::invalidate();
}

#ifdef OWL_PLATFORM_LINUX
TEST(WindowPlatform, noCompositorNoServerDecorations) {
	const char* previous = std::getenv("WAYLAND_DISPLAY");
	const std::string saved = previous != nullptr ? previous : "";
	setenv("WAYLAND_DISPLAY", "owl-test-no-such-compositor", 1);
	EXPECT_FALSE(owl::window::glfw::compositorDrawsDecorations());
	if (previous != nullptr)
		setenv("WAYLAND_DISPLAY", saved.c_str(), 1);
	else
		unsetenv("WAYLAND_DISPLAY");
}
#endif
