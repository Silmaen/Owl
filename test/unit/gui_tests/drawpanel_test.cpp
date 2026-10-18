/**
 * @file drawpanel_test.cpp
 * @author Silmaen
 * @date 08/01/2025
 * Copyright (c) 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <gui/BaseDrawPanel.h>
#include <gui/UiLayer.h>

using namespace owl::gui;
using namespace owl::core;

TEST(BaseDrawPanel, basic) {
	Log::init(Log::Level::Off);
	UiLayer layer;
	layer.disableApp();
	layer.enableDocking();
	layer.onAttach();
	BaseDrawPanel drawPanel("superpanel");
	EXPECT_STREQ(drawPanel.getName().c_str(), "superpanel");
	const Timestep ts;
	drawPanel.onUpdate(ts);
	layer.begin();
	drawPanel.onRender();
	layer.end();
	layer.onDetach();
	Log::invalidate();
}
