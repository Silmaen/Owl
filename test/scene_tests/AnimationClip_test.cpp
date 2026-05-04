/**
 * @file AnimationClip_test.cpp
 * @author Silmaen
 * @date 27/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <scene/AnimationClip.h>

using namespace owl;
using namespace owl::scene;

namespace {
class AnimationClipFixture : public testing::Test {
protected:
	static void SetUpTestSuite() { core::Log::init(core::Log::Level::Off); }
};
}// namespace

TEST_F(AnimationClipFixture, RoundTripDefault) {
	AnimationClip clip;
	const auto yaml = clip.serializeToString("default");

	AnimationClip restored;
	ASSERT_TRUE(restored.deserializeFromString(yaml));
	EXPECT_EQ(restored.columns, clip.columns);
	EXPECT_EQ(restored.rows, clip.rows);
	EXPECT_EQ(restored.firstFrame, clip.firstFrame);
	EXPECT_EQ(restored.lastFrame, clip.lastFrame);
	EXPECT_FLOAT_EQ(restored.frameDuration, clip.frameDuration);
	EXPECT_EQ(restored.loop, clip.loop);
	EXPECT_TRUE(restored.speedCurve.empty());
}

TEST_F(AnimationClipFixture, RoundTripCustomFields) {
	AnimationClip clip;
	clip.columns = 4;
	clip.rows = 2;
	clip.firstFrame = 1;
	clip.lastFrame = 6;
	clip.frameDuration = 0.05f;
	clip.loop = false;
	clip.speedCurve.setInterpolation(math::CurveInterpolation::Smooth);
	clip.speedCurve.addKey({0.0f, 1.0f});
	clip.speedCurve.addKey({0.5f, 0.25f});
	clip.speedCurve.addKey({1.0f, 1.0f});

	const auto yaml = clip.serializeToString("walk");

	AnimationClip restored;
	ASSERT_TRUE(restored.deserializeFromString(yaml));
	EXPECT_EQ(restored.columns, 4u);
	EXPECT_EQ(restored.rows, 2u);
	EXPECT_EQ(restored.firstFrame, 1u);
	EXPECT_EQ(restored.lastFrame, 6u);
	EXPECT_FLOAT_EQ(restored.frameDuration, 0.05f);
	EXPECT_FALSE(restored.loop);
	ASSERT_EQ(restored.speedCurve.keyCount(), 3u);
	EXPECT_EQ(restored.speedCurve.getInterpolation(), math::CurveInterpolation::Smooth);
	EXPECT_FLOAT_EQ(restored.speedCurve.key(1).time, 0.5f);
	EXPECT_FLOAT_EQ(restored.speedCurve.key(1).value, 0.25f);
}

TEST_F(AnimationClipFixture, EmptySpeedCurveIsOmitted) {
	AnimationClip clip;
	const auto yaml = clip.serializeToString();
	EXPECT_EQ(yaml.find("speedCurve"), std::string::npos);
}

TEST_F(AnimationClipFixture, NonEmptySpeedCurveIsEmitted) {
	AnimationClip clip;
	clip.speedCurve.addKey({0.0f, 0.5f});
	const auto yaml = clip.serializeToString();
	EXPECT_NE(yaml.find("speedCurve"), std::string::npos);
	EXPECT_NE(yaml.find("interpolation"), std::string::npos);
	EXPECT_NE(yaml.find("keys"), std::string::npos);
}

TEST_F(AnimationClipFixture, MalformedYamlIsRejected) {
	AnimationClip clip;
	clip.columns = 7;
	EXPECT_FALSE(clip.deserializeFromString("not a clip: \n  foo: bar"));
	// Original state preserved on failure.
	EXPECT_EQ(clip.columns, 7u);
}

TEST_F(AnimationClipFixture, GarbageInputDoesNotCrash) {
	AnimationClip clip;
	EXPECT_FALSE(clip.deserializeFromString("\t\nthis is :: not :: yaml ["));
}

TEST_F(AnimationClipFixture, FileRoundTripInTempDir) {
	const auto tmp = std::filesystem::temp_directory_path() / "owl_animation_clip_test.owlanim";
	std::filesystem::remove(tmp);

	AnimationClip clip;
	clip.columns = 8;
	clip.rows = 4;
	clip.firstFrame = 2;
	clip.lastFrame = 30;
	clip.frameDuration = 0.04f;
	clip.loop = false;
	clip.speedCurve.addKey({0.0f, 1.0f});
	clip.speedCurve.addKey({1.0f, 2.0f});

	ASSERT_TRUE(clip.saveToFile(tmp));
	ASSERT_TRUE(std::filesystem::exists(tmp));

	AnimationClip loaded;
	ASSERT_TRUE(loaded.loadFromFile(tmp));
	EXPECT_EQ(loaded.columns, 8u);
	EXPECT_EQ(loaded.rows, 4u);
	EXPECT_EQ(loaded.firstFrame, 2u);
	EXPECT_EQ(loaded.lastFrame, 30u);
	EXPECT_FLOAT_EQ(loaded.frameDuration, 0.04f);
	EXPECT_FALSE(loaded.loop);
	ASSERT_EQ(loaded.speedCurve.keyCount(), 2u);

	std::filesystem::remove(tmp);
}

TEST_F(AnimationClipFixture, BundledCoinClipLoads) {
	// Fixture lives under engine_assets/ — tests must not depend on sample_project/.
	const auto path = owl::test::getRootPath() / "engine_assets" / "animations" / "coin.owlanim";
	if (!std::filesystem::exists(path))
		GTEST_SKIP() << "Engine asset missing: " << path;

	AnimationClip clip;
	ASSERT_TRUE(clip.loadFromFile(path));
	EXPECT_EQ(clip.columns, 6u);
	EXPECT_EQ(clip.rows, 3u);
	EXPECT_EQ(clip.firstFrame, 0u);
	EXPECT_EQ(clip.lastFrame, 17u);
	EXPECT_FLOAT_EQ(clip.frameDuration, 0.08f);
	EXPECT_TRUE(clip.loop);
	ASSERT_EQ(clip.speedCurve.keyCount(), 3u);
	EXPECT_EQ(clip.speedCurve.getInterpolation(), math::CurveInterpolation::Smooth);
}

TEST_F(AnimationClipFixture, ColumnsAndRowsClampedToOne) {
	AnimationClip clip;
	clip.columns = 0;
	clip.rows = 0;
	const auto yaml = clip.serializeToString();

	AnimationClip restored;
	ASSERT_TRUE(restored.deserializeFromString(yaml));
	EXPECT_GE(restored.columns, 1u);
	EXPECT_GE(restored.rows, 1u);
}
