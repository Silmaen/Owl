/**
 * @file ScreenTransition_test.cpp
 * @author Silmaen
 * @date 06/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "testHelper.h"

#include "scene/ScreenTransition.h"

#include <gtest/gtest.h>

using owl::scene::ScreenTransition;

namespace {

class ScreenTransitionTest : public ::testing::Test {
protected:
	void SetUp() override { ScreenTransition::reset(); }
	void TearDown() override { ScreenTransition::reset(); }
};

}// namespace

TEST_F(ScreenTransitionTest, idleByDefault) {
	EXPECT_FALSE(ScreenTransition::isActive());
	EXPECT_EQ(ScreenTransition::getType(), ScreenTransition::Type::None);
	EXPECT_FLOAT_EQ(ScreenTransition::getProgress(), 1.f);
}

TEST_F(ScreenTransitionTest, startSetsFadeStateBlackByDefault) {
	ScreenTransition::start(ScreenTransition::Type::FadeOut, 0.5f);
	EXPECT_TRUE(ScreenTransition::isActive());
	EXPECT_EQ(ScreenTransition::getType(), ScreenTransition::Type::FadeOut);
	const auto& color = ScreenTransition::getColor();
	EXPECT_FLOAT_EQ(color.r(), 0.f);
	EXPECT_FLOAT_EQ(color.g(), 0.f);
	EXPECT_FLOAT_EQ(color.b(), 0.f);
	EXPECT_FLOAT_EQ(color.a(), 1.f);
}

TEST_F(ScreenTransitionTest, playKeepsCustomColor) {
	ScreenTransition::play(ScreenTransition::Type::FadeOut, 0.5f, {1.f, 0.f, 0.5f, 0.8f});
	const auto& color = ScreenTransition::getColor();
	EXPECT_FLOAT_EQ(color.r(), 1.f);
	EXPECT_FLOAT_EQ(color.g(), 0.f);
	EXPECT_FLOAT_EQ(color.b(), 0.5f);
	EXPECT_FLOAT_EQ(color.a(), 0.8f);
}

TEST_F(ScreenTransitionTest, progressAdvancesWithUpdate) {
	ScreenTransition::play(ScreenTransition::Type::FadeIn, 1.f, {0.f, 0.f, 0.f, 1.f});
	EXPECT_FLOAT_EQ(ScreenTransition::getProgress(), 0.f);
	ScreenTransition::update(0.25f);
	EXPECT_FLOAT_EQ(ScreenTransition::getProgress(), 0.25f);
	ScreenTransition::update(0.5f);
	EXPECT_FLOAT_EQ(ScreenTransition::getProgress(), 0.75f);
}

TEST_F(ScreenTransitionTest, becomesInactiveAfterDuration) {
	ScreenTransition::play(ScreenTransition::Type::WipeLeft, 0.2f, {0.f, 0.f, 0.f, 1.f});
	EXPECT_TRUE(ScreenTransition::isActive());
	ScreenTransition::update(0.21f);
	EXPECT_FALSE(ScreenTransition::isActive());
	EXPECT_EQ(ScreenTransition::getType(), ScreenTransition::Type::None);
}

TEST_F(ScreenTransitionTest, eachWipeVariantIsRecorded) {
	for (const auto type:
		 {ScreenTransition::Type::WipeLeft, ScreenTransition::Type::WipeRight, ScreenTransition::Type::WipeUp,
		  ScreenTransition::Type::WipeDown}) {
		ScreenTransition::play(type, 1.f, {0.2f, 0.4f, 0.6f, 1.f});
		EXPECT_EQ(ScreenTransition::getType(), type);
		ScreenTransition::reset();
	}
}

TEST_F(ScreenTransitionTest, resetDropsInFlightTransition) {
	ScreenTransition::play(ScreenTransition::Type::WipeRight, 1.f, {1.f, 1.f, 1.f, 1.f});
	ScreenTransition::update(0.3f);
	ScreenTransition::reset();
	EXPECT_FALSE(ScreenTransition::isActive());
	EXPECT_EQ(ScreenTransition::getType(), ScreenTransition::Type::None);
	EXPECT_FLOAT_EQ(ScreenTransition::getProgress(), 1.f);
}

TEST_F(ScreenTransitionTest, durationClampedAboveZero) {
	// A zero / negative duration would otherwise divide-by-zero in `getProgress`;
	// `play` clamps to a 1ms floor so the transition still completes on the
	// first `update` instead of staying stuck.
	ScreenTransition::play(ScreenTransition::Type::FadeIn, 0.f, {0.f, 0.f, 0.f, 1.f});
	EXPECT_TRUE(ScreenTransition::isActive());
	ScreenTransition::update(0.005f);
	EXPECT_FALSE(ScreenTransition::isActive());
}
