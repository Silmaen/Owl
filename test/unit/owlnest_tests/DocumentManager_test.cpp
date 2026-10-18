/**
 * @file DocumentManager_test.cpp
 * @author Silmaen
 * @date 12/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "document/DocumentManager.h"
#include "document/AnimationDocument.h"
#include "testHelper.h"

#include <core/Timestep.h>

#include <gtest/gtest.h>

#include <chrono>

using namespace owl;
using namespace owl::nest;

TEST(DocumentManager, UpdateScenelessPlaysTheAnimations) {
	DocumentManager documents;
	auto* animation = static_cast<AnimationDocument*>(documents.add(mkUniq<AnimationDocument>()));
	auto& clip = animation->clip();
	clip.columns = 4;
	clip.rows = 1;
	clip.firstFrame = 0;
	clip.lastFrame = 3;
	clip.frameDuration = 0.1f;
	clip.loop = true;
	animation->play();
	core::Timestep step;
	step.forceUpdate(std::chrono::milliseconds(150));
	documents.updateSceneless(step);
	EXPECT_EQ(animation->currentFrame(), 1u);
	documents.updateSceneless(step);
	EXPECT_EQ(animation->currentFrame(), 3u);
	EXPECT_TRUE(animation->isPlaying());
}
