/**
 * @file LayerStack_test.cpp
 * @author Silmaen
 * @date 05/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/layer/LayerStack.h>

using namespace owl::core::layer;

TEST(LayerStack, base) {
	{
		LayerStack stack;
		EXPECT_EQ(stack.begin(), stack.end());
		EXPECT_EQ(stack.rbegin(), stack.rend());
	}
	{
		const LayerStack stack;
		EXPECT_EQ(stack.begin(), stack.end());
		EXPECT_EQ(stack.rbegin(), stack.rend());
	}
}

TEST(LayerStack, pushLayer) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	{
		auto stack = LayerStack();
		stack.pushLayer(owl::mkShared<Layer>("Layer 1"));
		stack.pushLayer(owl::mkShared<Layer>("Layer 2"));
		stack.pushOverlay(owl::mkShared<Layer>("Overlay 1"));
		stack.pushLayer(owl::mkShared<Layer>("Layer 3"));
	}
	{
		LayerStack stack;
		stack.pushLayer(owl::mkShared<Layer>("Layer 1"));
		stack.pushLayer(owl::mkShared<Layer>("Layer 2"));
		stack.pushOverlay(owl::mkShared<Layer>("Overlay 1"));
		stack.pushLayer(owl::mkShared<Layer>("Layer 3"));
		const auto item = *(stack.begin() + 2);
		EXPECT_STREQ(item->getName().c_str(), "Layer 3");
		const auto item2 = *(stack.begin() + 3);
		EXPECT_STREQ(item2->getName().c_str(), "Overlay 1");
	}
	{
		LayerStack stack;
		stack.pushLayer(owl::mkShared<Layer>("Layer 1"));
		stack.pushLayer(owl::mkShared<Layer>("Layer 2"));
		stack.pushOverlay(owl::mkShared<Layer>("Overlay 1"));
		stack.pushLayer(owl::mkShared<Layer>("Layer 3"));
		const auto item = *(stack.begin() + 2);
		const auto item2 = *(stack.begin() + 3);
		stack.popLayer(item);
		stack.popOverlay(item2);
		stack.popLayer(item);
		stack.popOverlay(item2);
		EXPECT_EQ(std::distance(stack.begin(), stack.end()), 2);
		stack.clear();
		EXPECT_EQ(std::distance(stack.begin(), stack.end()), 0);
	}
	owl::core::Log::invalidate();
}
