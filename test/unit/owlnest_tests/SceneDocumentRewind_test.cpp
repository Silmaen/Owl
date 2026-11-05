/**
 * @file SceneDocumentRewind_test.cpp
 * @author Silmaen
 * @date 05/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include "document/SceneDocument.h"
#include "nestTestHelper.h"

#include <app/Application.h>
#include <app/EngineContext.h>
#include <input/Input.h>
#include <scene/Replay.h>
#include <scene/SceneSerializer.h>
#include <scene/SettingsManager.h>
#include <scene/UiInputSystem.h>
#include <scene/WorldHash.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>

using namespace owl;
using namespace owl::nest;

namespace {

constexpr std::chrono::hours g_NoBudget{1};
// Frames whose world is compared after a seek (hashing every frame would cost more than the test).
constexpr std::array<uint64_t, 6> g_CheckedFrames{10, 37, 150, 157, 160, 220};

// The platformer house of the Engine Demo, played in a scene document as Owl Nest opens the project.
class SceneDocumentRewind : public nest::test::NestTest {
protected:
	void SetUp() override {
		NestTest::SetUp();
		input::Input::resetInjection();
		const auto demo = owl::test::getRootPath() / "engine_demo";
		mp_app = mkShared<app::Application>(app::AppParams{.args = nullptr,
														   .frameLogFrequency = 0,
														   .name = "rewindTest",
														   .assetsPattern = "",
														   .icon = "",
														   .width = 0,
														   .height = 0,
														   .argCount = 0,
														   .renderer = renderer::gpu::RenderAPI::Type::Null,
														   .hasGui = false,
														   .useDebugging = false,
														   .isDummy = true});
		mp_app->addAssetDirectory({.title = "demo", .assetsPath = demo}, true);
		auto& context = mp_app->getEngineContext();
		context.getSettings().loadDefaults(demo / "game_settings.yml");
		ASSERT_TRUE(context.loadActionMap(demo));
		ASSERT_TRUE(context.loadLocalisation(demo));
		m_scenePath = demo / "scenes" / "platformer_house.owl";
		const auto loaded = mkShared<scene::Scene>();
		ASSERT_TRUE(scene::SceneSerializer(loaded).deserialize(m_scenePath));
		m_doc.applyLoadedScene(loaded, m_scenePath, {64, 64});
	}

	void TearDown() override {
		if (m_doc.state() != SceneDocument::State::Edit)
			m_doc.onSceneStop();
		input::Input::resetInjection();
		app::Application::invalidate();
		mp_app.reset();
		NestTest::TearDown();
	}

	// The player walks right, jumps now and then, walks back: inputs by frame.
	static void inject(const uint64_t iFrame) {
		input::Input::resetInjection();
		if (iFrame >= 10 && iFrame < 140)
			input::Input::injectKey(input::key::D);
		if (iFrame >= 160 && iFrame < 220)
			input::Input::injectKey(input::key::A);
		if (iFrame % 70 >= 30 && iFrame % 70 < 36)
			input::Input::injectKey(input::key::Space);
	}

	// Plays live frames from the frame the document reached, keeping the world hash of the checked frames.
	void playLive(const uint64_t iFrames) {
		core::Timestep step;
		step.forceUpdate(std::chrono::milliseconds{16});
		for (uint64_t i = 0; i < iFrames; ++i) {
			const uint64_t frame = m_doc.getTimelineFrame();
			inject(frame);
			scene::UiInputSystem::update(m_doc.getActiveScene().get(), {64, 64}, {0.f, 0.f}, false, 0.016f);
			m_doc.getActiveScene()->onUpdateRuntime(step, false);
			if (std::ranges::contains(g_CheckedFrames, frame + 1))
				m_liveHashes[frame + 1] = scene::computeWorldHash(*m_doc.getActiveScene());
		}
		input::Input::resetInjection();
	}

	// Seeks and runs the seek to its end.
	auto seekTo(const uint64_t iFrame) -> uint64_t {
		EXPECT_TRUE(m_doc.seek(iFrame));
		EXPECT_TRUE(m_doc.advanceSeek(g_NoBudget));
		EXPECT_FALSE(m_doc.isSeeking());
		EXPECT_EQ(m_doc.state(), SceneDocument::State::Pause);
		EXPECT_EQ(m_doc.getTimelineFrame(), iFrame);
		return scene::computeWorldHash(*m_doc.getActiveScene());
	}

	shared<app::Application> mp_app;
	std::filesystem::path m_scenePath;
	SceneDocument m_doc;
	std::map<uint64_t, uint64_t> m_liveHashes;
};

}// namespace

TEST_F(SceneDocumentRewind, SeekingBackAndForthGivesTheWorldOfThatFrame) {
	EXPECT_EQ(m_doc.getTimelineFrameCount(), 0u);
	EXPECT_FALSE(m_doc.seek(0));
	m_doc.onScenePlay();
	playLive(240);
	EXPECT_EQ(m_doc.getTimelineFrameCount(), 240u);
	EXPECT_GT(m_doc.getTimelineSeconds(240), 3.8);
	EXPECT_EQ(seekTo(160), m_liveHashes[160]);
	EXPECT_TRUE(m_doc.isOnTimeline());
	EXPECT_EQ(m_doc.getTimelineFrameCount(), 240u);
	EXPECT_EQ(seekTo(220), m_liveHashes[220]);
	EXPECT_TRUE(m_doc.rewind(1.0));
	EXPECT_TRUE(m_doc.advanceSeek(g_NoBudget));
	EXPECT_EQ(m_doc.getTimelineFrame(), 220u - 63u);
	EXPECT_EQ(scene::computeWorldHash(*m_doc.getActiveScene()), m_liveHashes[220 - 63]);
	EXPECT_EQ(seekTo(37), m_liveHashes[37]);
}

TEST_F(SceneDocumentRewind, ASeekRunsWithinItsBudget) {
	m_doc.onScenePlay();
	playLive(200);
	ASSERT_TRUE(m_doc.seek(150));
	EXPECT_FALSE(m_doc.advanceSeek(std::chrono::microseconds{0}));
	EXPECT_TRUE(m_doc.isSeeking());
	EXPECT_EQ(m_doc.getSeekTarget(), 150u);
	EXPECT_EQ(m_doc.getTimelineFrame(), 1u);
	EXPECT_TRUE(m_doc.advanceSeek(g_NoBudget));
	EXPECT_EQ(scene::computeWorldHash(*m_doc.getActiveScene()), m_liveHashes[150]);
}

// Resumed after a rewind, the session branches: it records live from that frame, the frames after it are gone, and
// the session written on Stop replays in sync.
TEST_F(SceneDocumentRewind, ResumingBranchesTheSessionFromTheFrameReached) {
	m_doc.setRecordingNextPlay(true);
	m_doc.onScenePlay();
	playLive(240);
	std::ignore = seekTo(120);
	m_doc.onSceneResume();
	EXPECT_EQ(m_doc.state(), SceneDocument::State::Play);
	EXPECT_FALSE(m_doc.isOnTimeline());
	EXPECT_EQ(m_doc.getTimelineFrameCount(), 120u);
	m_liveHashes.clear();
	playLive(60);
	EXPECT_EQ(m_doc.getTimelineFrameCount(), 180u);
	EXPECT_EQ(seekTo(150), m_liveHashes[150]);
	m_doc.onSceneStop();
	const auto recording = m_doc.takeRecording();
	ASSERT_TRUE(recording.has_value());
	EXPECT_EQ(recording->frames.size(), 180u);
	m_doc.setReplayForNextPlay(mkShared<const scene::Replay>(*recording));
	m_doc.onScenePlay();
	EXPECT_EQ(m_doc.getTimelineFrameCount(), 0u);
	core::Timestep step;
	for (std::size_t frame = 0; frame < recording->frames.size(); ++frame) {
		scene::UiInputSystem::update(m_doc.getActiveScene().get(), {64, 64}, {0.f, 0.f}, false, 0.016f);
		m_doc.getActiveScene()->onUpdateRuntime(step, false);
	}
	const auto& progress = m_doc.getActiveScene()->getReplayProgress();
	EXPECT_TRUE(progress.finished);
	EXPECT_GT(progress.checkedCount, 1u);
	EXPECT_FALSE(progress.divergedFrame.has_value());
}

TEST_F(SceneDocumentRewind, AnUnrecordedPlayIsRewoundButNotHandedOver) {
	m_doc.onScenePlay();
	playLive(30);
	EXPECT_EQ(seekTo(10), m_liveHashes[10]);
	m_doc.onSceneStep();
	EXPECT_FALSE(m_doc.isOnTimeline());
	EXPECT_EQ(m_doc.getTimelineFrameCount(), 10u);
	m_doc.onSceneStop();
	EXPECT_FALSE(m_doc.takeRecording().has_value());
	EXPECT_EQ(m_doc.getTimelineFrameCount(), 0u);
}
