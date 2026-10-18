/**
 * @file ImGuiLayer_test.cpp
 * @author Silmaen
 * @date 03/08/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <event/KeyEvent.h>
#include <gui/UiLayer.h>

using namespace owl::gui;

TEST(ImGuiLayer, creation) {
	UiLayer layer;
	EXPECT_STREQ(layer.getName().c_str(), "ImGuiLayer");
	layer.blockEvents(false);
	layer.enableDocking();
	layer.disableDocking();
	layer.disableApp();
	layer.onAttach();
	UiLayer::setTheme();
	owl::event::KeyPressedEvent evt(owl::input::key::A, 1);
	layer.onEvent(evt);
	layer.blockEvents(true);
	layer.onEvent(evt);
	layer.onDetach();
}
