/**
 * @file gui_main.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include <owlgui.h>

#include <print>

// The signature must match the one app/Application.h declares.
auto main(int /*iArgc*/, char* /*iArgv*/[]) -> int {
	const ImVec2 size = owl::gui::vec(owl::math::vec2{2.f, 3.f});
	std::println("OwlEngine {} found and linked with Owl::Gui (imgui {}).", owl::getVersionString(), IMGUI_VERSION);
	return size.x == 2.f && size.y == 3.f ? 0 : 1;
}
