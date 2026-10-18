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
	/// The transition under test (fresh for every test).
	ScreenTransition m_transition;
};

}// namespace

TEST_F(ScreenTransitionTest, idleByDefault) {
	EXPECT_FALSE(m_transition.isActive());
	EXPECT_EQ(m_transition.getType(), ScreenTransition::Type::None);
	EXPECT_FLOAT_EQ(m_transition.getProgress(), 1.f);
}

TEST_F(ScreenTransitionTest, startSetsFadeStateBlackByDefault) {
	m_transition.start(ScreenTransition::Type::FadeOut, 0.5f);
	EXPECT_TRUE(m_transition.isActive());
	EXPECT_EQ(m_transition.getType(), ScreenTransition::Type::FadeOut);
	const auto& color = m_transition.getColor();
	EXPECT_FLOAT_EQ(color.r(), 0.f);
	EXPECT_FLOAT_EQ(color.g(), 0.f);
	EXPECT_FLOAT_EQ(color.b(), 0.f);
	EXPECT_FLOAT_EQ(color.a(), 1.f);
}

TEST_F(ScreenTransitionTest, playKeepsCustomColor) {
	m_transition.play(ScreenTransition::Type::FadeOut, 0.5f, {1.f, 0.f, 0.5f, 0.8f});
	const auto& color = m_transition.getColor();
	EXPECT_FLOAT_EQ(color.r(), 1.f);
	EXPECT_FLOAT_EQ(color.g(), 0.f);
	EXPECT_FLOAT_EQ(color.b(), 0.5f);
	EXPECT_FLOAT_EQ(color.a(), 0.8f);
}

TEST_F(ScreenTransitionTest, progressAdvancesWithUpdate) {
	m_transition.play(ScreenTransition::Type::FadeIn, 1.f, {0.f, 0.f, 0.f, 1.f});
	EXPECT_FLOAT_EQ(m_transition.getProgress(), 0.f);
	m_transition.update(0.25f);
	EXPECT_FLOAT_EQ(m_transition.getProgress(), 0.25f);
	m_transition.update(0.5f);
	EXPECT_FLOAT_EQ(m_transition.getProgress(), 0.75f);
}

TEST_F(ScreenTransitionTest, becomesInactiveAfterDuration) {
	m_transition.play(ScreenTransition::Type::WipeLeft, 0.2f, {0.f, 0.f, 0.f, 1.f});
	EXPECT_TRUE(m_transition.isActive());
	m_transition.update(0.21f);
	EXPECT_FALSE(m_transition.isActive());
	EXPECT_EQ(m_transition.getType(), ScreenTransition::Type::None);
}

TEST_F(ScreenTransitionTest, eachWipeVariantIsRecorded) {
	for (const auto type: {ScreenTransition::Type::WipeLeft, ScreenTransition::Type::WipeRight,
						   ScreenTransition::Type::WipeUp, ScreenTransition::Type::WipeDown}) {
		m_transition.play(type, 1.f, {0.2f, 0.4f, 0.6f, 1.f});
		EXPECT_EQ(m_transition.getType(), type);
		m_transition.reset();
	}
}

TEST_F(ScreenTransitionTest, resetDropsInFlightTransition) {
	m_transition.play(ScreenTransition::Type::WipeRight, 1.f, {1.f, 1.f, 1.f, 1.f});
	m_transition.update(0.3f);
	m_transition.reset();
	EXPECT_FALSE(m_transition.isActive());
	EXPECT_EQ(m_transition.getType(), ScreenTransition::Type::None);
	EXPECT_FLOAT_EQ(m_transition.getProgress(), 1.f);
}

TEST_F(ScreenTransitionTest, durationClampedAboveZero) {
	// A zero / negative duration would otherwise divide-by-zero in `getProgress`;
	// `play` clamps to a 1ms floor so the transition still completes on the
	// first `update` instead of staying stuck.
	m_transition.play(ScreenTransition::Type::FadeIn, 0.f, {0.f, 0.f, 0.f, 1.f});
	EXPECT_TRUE(m_transition.isActive());
	m_transition.update(0.005f);
	EXPECT_FALSE(m_transition.isActive());
}

TEST_F(ScreenTransitionTest, playWithTypeNoneStaysIdle) {
	// `play(None, ...)` should not flip the phase to OutAnim when starting
	// from Idle — there is nothing to render.
	m_transition.play(ScreenTransition::Type::None, 0.5f, {0.f, 0.f, 0.f, 1.f});
	EXPECT_FALSE(m_transition.isActive());
}

TEST_F(ScreenTransitionTest, sceneLoadFlowOutLoadingIn) {
	// requestSceneLoad → OutAnim, finishes → Loading, host calls
	// pendingLoadPath → Loading runs minHoldDuration → InAnim → Idle.
	ScreenTransition::SceneLoadRequest req;
	req.scenePath = "level/foo.scene";
	req.outType = ScreenTransition::Type::FadeOut;
	req.inType = ScreenTransition::Type::FadeIn;
	req.outDuration = 0.1f;
	req.inDuration = 0.1f;
	req.minHoldDuration = 0.05f;
	req.color = {0.1f, 0.2f, 0.3f, 1.f};
	m_transition.requestSceneLoad(req);
	EXPECT_EQ(m_transition.getPhase(), ScreenTransition::Phase::OutAnim);

	// Finish out-anim.
	m_transition.update(0.2f);
	EXPECT_EQ(m_transition.getPhase(), ScreenTransition::Phase::Loading);

	// First call returns the path; subsequent calls return nullopt.
	const auto path = m_transition.pendingLoadPath();
	ASSERT_TRUE(path.has_value());
	EXPECT_EQ(path.value(), "level/foo.scene");
	EXPECT_FALSE(m_transition.pendingLoadPath().has_value());

	// Hold long enough to flip to InAnim.
	m_transition.update(0.1f);
	EXPECT_EQ(m_transition.getPhase(), ScreenTransition::Phase::InAnim);
	EXPECT_EQ(m_transition.getType(), ScreenTransition::Type::FadeIn);

	// Finish in-anim → Idle.
	m_transition.update(0.2f);
	EXPECT_EQ(m_transition.getPhase(), ScreenTransition::Phase::Idle);
	EXPECT_FLOAT_EQ(m_transition.getProgress(), 1.f);
}

TEST_F(ScreenTransitionTest, loadingProgressReflectsHold) {
	ScreenTransition::SceneLoadRequest req;
	req.scenePath = "x";
	req.outDuration = 0.01f;
	req.inDuration = 0.01f;
	req.minHoldDuration = 1.f;
	m_transition.requestSceneLoad(req);
	m_transition.update(0.05f);// finish out
	ASSERT_EQ(m_transition.getPhase(), ScreenTransition::Phase::Loading);
	// 0.5s held / 1s minimum → 0.5 progress
	m_transition.update(0.5f);
	EXPECT_NEAR(m_transition.getProgress(), 0.5f, 0.05f);
}

TEST_F(ScreenTransitionTest, pendingLoadPathNoneOutsideLoading) {
	// Idle — no pending path.
	EXPECT_FALSE(m_transition.pendingLoadPath().has_value());
	// Active OutAnim without scene-load request — no pending path either.
	m_transition.play(ScreenTransition::Type::FadeOut, 0.5f, {});
	EXPECT_FALSE(m_transition.pendingLoadPath().has_value());
}
