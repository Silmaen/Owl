/**
 * @file Layer_test.cpp
 * @author Silmaen
 * @date 05/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/layer/Layer.h>
#include <event/AppEvent.h>

using namespace owl::core;
using namespace owl::event;
using namespace owl::core::layer;

TEST(Layer, base) {
	Layer layer("test");
	Timestep ts;
	ts.update();
	layer.onUpdate(ts);
	layer.onImGuiRender(ts);
	layer.onAttach();
	layer.onDetach();
	AppTickEvent evt;
	layer.onEvent(evt);
	EXPECT_STREQ(layer.getName().c_str(), "test");
}
