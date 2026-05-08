/**
 * @file FontPreviewCache_test.cpp
 * @author Silmaen
 * @date 07/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <gui/FontPreviewCache.h>

using namespace owl;

TEST(FontPreviewCache, RequestNullFontReturnsNullFramebuffer) {
	core::Log::init(core::Log::Level::Off);
	auto& cache = gui::FontPreviewCache::get();
	cache.clear();
	EXPECT_EQ(cache.request(nullptr), nullptr);
	cache.clear();
	core::Log::invalidate();
}

TEST(FontPreviewCache, ClearIsIdempotent) {
	core::Log::init(core::Log::Level::Off);
	auto& cache = gui::FontPreviewCache::get();
	cache.clear();
	cache.clear();// safe to call twice
	core::Log::invalidate();
}

TEST(FontPreviewCache, PumpPendingNoOpWhenEmpty) {
	core::Log::init(core::Log::Level::Off);
	auto& cache = gui::FontPreviewCache::get();
	cache.clear();
	cache.pumpPending();// must not crash with nothing pending
	core::Log::invalidate();
}
