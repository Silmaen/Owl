/**
 * @file Replay_test.cpp
 * @author Silmaen
 * @date 04/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <input/Input.h>
#include <scene/Entity.h>
#include <scene/Replay.h>
#include <scene/Scene.h>
#include <scene/UiInputSystem.h>
#include <scene/WorldHash.h>
#include <scene/component/components.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <system_error>
#include <tuple>
#include <utility>

using namespace owl;
using namespace owl::scene;

namespace {

// Pushes its box from the jump action and a raw key, and keeps the mouse in the game state.
constexpr auto g_Script = R"(
function on_tick(dt)
	if input.is_action_pressed('jump') then physics.impulse(entity_id, 0, 3) end
	if input.is_key_pressed(68) then physics.impulse(entity_id, 0.5, 0) end
	gamestate.set('mouse_x', input.get_mouse_x())
end
)";

constexpr int g_frameCount = 180;

class ReplayTest : public testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		OWL_REQUIRE_MODULE(SCRIPT);
		input::Input::resetInjection();
		m_dir = std::filesystem::temp_directory_path() /
				("owl_replay_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
		std::filesystem::create_directories(m_dir);
		std::ofstream{m_dir / "push.lua"} << g_Script;
	}

	void TearDown() override {
		input::Input::resetInjection();
		std::error_code error;
		std::filesystem::remove_all(m_dir, error);
		core::Log::invalidate();
	}

	// Ground and one scripted box.
	void build(Scene& ioScene) const {
		ioScene.setRandomSeed(77);
		auto ground = ioScene.createEntityWithUUID(core::UUID{1}, "Ground");
		ground.getComponent<component::Transform>().transform.scale() = {40.f, 1.f, 1.f};
		ground.addComponent<component::PhysicBody>().body.type = SceneBody::BodyType::Static;
		auto box = ioScene.createEntityWithUUID(core::UUID{2}, "Box");
		box.getComponent<component::Transform>().transform.translation() = {0.f, 1.f, 0.f};
		box.addComponent<component::PhysicBody>().body.type = SceneBody::BodyType::Dynamic;
		box.addComponent<component::LuaScript>().scriptPath = (m_dir / "push.lua").generic_string();
	}

	// Frames of uneven durations: one, two or no tick each.
	[[nodiscard]] static auto frameDuration(const int iFrame) -> std::chrono::microseconds {
		constexpr std::array<int64_t, 4> durations{16'667, 33'000, 7'100, 16'667};
		return std::chrono::microseconds{durations[static_cast<std::size_t>(iFrame) % durations.size()]};
	}

	// Inputs of the recorded session, by frame.
	static void inject(const int iFrame) {
		input::Input::resetInjection();
		if ((iFrame >= 20 && iFrame < 26) || (iFrame >= 90 && iFrame < 93))
			input::Input::injectKey(input::key::Space);
		if (iFrame >= 50 && iFrame < 70)
			input::Input::injectKey(input::key::D);
		input::Input::injectMousePos({static_cast<float>(iFrame) * 1.25f, 10.f});
	}

	// Plays the scene with live inputs and records it; the hash of the last frame.
	[[nodiscard]] auto record(Replay& oReplay) const -> uint64_t {
		Scene scene;
		build(scene);
		EXPECT_TRUE(scene.setRecordingReplay(true));
		scene.onStartRuntime();
		EXPECT_FALSE(scene.setRecordingReplay(true));
		core::Timestep step;
		for (int frame = 0; frame < g_frameCount; ++frame) {
			inject(frame);
			UiInputSystem::update(&scene, {320, 200}, {static_cast<float>(frame), 5.f}, frame % 30 == 0, 0.016f);
			step.forceUpdate(frameDuration(frame));
			scene.onUpdateRuntime(step, false);
		}
		input::Input::resetInjection();
		const uint64_t hash = computeWorldHash(scene);
		auto recorded = scene.takeRecordedReplay();
		EXPECT_TRUE(recorded.has_value());
		if (recorded.has_value())
			oReplay = std::move(*recorded);
		scene.onEndRuntime();
		return hash;
	}

	// Plays the scene from a replay, with a time step and a mouse the replay must override.
	[[nodiscard]] auto replay(const Replay& iReplay, ReplayProgress& oProgress) const -> uint64_t {
		Scene scene;
		build(scene);
		EXPECT_TRUE(scene.setPlaybackReplay(mkShared<const Replay>(iReplay)));
		scene.onStartRuntime();
		core::Timestep step;
		step.forceUpdate(std::chrono::milliseconds{1});
		for (uint64_t frame = 0; frame < iReplay.frames.size(); ++frame) {
			UiInputSystem::update(&scene, {10, 10}, {99.f, 99.f}, true, 1.f);
			scene.onUpdateRuntime(step, false);
		}
		const uint64_t hash = computeWorldHash(scene);
		oProgress = scene.getReplayProgress();
		EXPECT_FALSE(scene.isPlayingReplay());
		scene.onEndRuntime();
		return hash;
	}

	std::filesystem::path m_dir;
};

}// namespace

TEST_F(ReplayTest, ReplayGivesTheRecordedWorldBitForBit) {
	Replay recorded;
	const uint64_t liveHash = record(recorded);
	ASSERT_EQ(recorded.frames.size(), static_cast<std::size_t>(g_frameCount));
	EXPECT_FALSE(recorded.checkpoints.empty());
	EXPECT_TRUE(recorded.frames.front().pointer.has_value());
	ReplayProgress progress;
	EXPECT_EQ(replay(recorded, progress), liveHash);
	EXPECT_TRUE(progress.finished);
	EXPECT_EQ(progress.frame, recorded.frames.size());
	EXPECT_EQ(progress.checkedCount, recorded.checkpoints.size());
	EXPECT_FALSE(progress.divergedFrame.has_value());
}

TEST_F(ReplayTest, ReplayThroughTheFileGivesTheSameWorld) {
	Replay recorded;
	const uint64_t liveHash = record(recorded);
	const auto file = m_dir / "session.owlreplay";
	ASSERT_TRUE(recorded.saveFile(file));
	const auto loaded = Replay::loadFile(file);
	ASSERT_TRUE(loaded.has_value()) << loaded.error();
	ReplayProgress progress;
	EXPECT_EQ(replay(*loaded, progress), liveHash);
	EXPECT_FALSE(progress.divergedFrame.has_value());
}

TEST_F(ReplayTest, ChangedInputIsReportedAtTheFirstCheckpointAfterIt) {
	Replay recorded;
	std::ignore = record(recorded);
	for (std::size_t frame = 90; frame < 93; ++frame) recorded.frames[frame].input.setKey(input::key::Space, false);
	ReplayProgress progress;
	std::ignore = replay(recorded, progress);
	ASSERT_TRUE(progress.divergedFrame.has_value());
	EXPECT_GT(*progress.divergedFrame, 90u);
	const auto first = std::ranges::find_if(
			recorded.checkpoints, [](const ReplayCheckpoint& iCheckpoint) -> bool { return iCheckpoint.frame > 90; });
	ASSERT_NE(first, recorded.checkpoints.end());
	EXPECT_EQ(*progress.divergedFrame, first->frame);
}

TEST_F(ReplayTest, ADifferentStartStateIsReportedAtFrameZero) {
	Replay recorded;
	std::ignore = record(recorded);
	recorded.startHash ^= 1u;
	ReplayProgress progress;
	std::ignore = replay(recorded, progress);
	ASSERT_TRUE(progress.divergedFrame.has_value());
	EXPECT_EQ(*progress.divergedFrame, 0u);
}

TEST_F(ReplayTest, ReplayAppliesItsSeed) {
	Replay recorded;
	const uint64_t liveHash = record(recorded);
	Scene scene;
	build(scene);
	scene.setRandomSeed(5);
	ASSERT_TRUE(scene.setPlaybackReplay(mkShared<const Replay>(recorded)));
	scene.onStartRuntime();
	EXPECT_EQ(scene.getRandomSeed(), 77u);
	const core::Timestep step;
	for (std::size_t frame = 0; frame < recorded.frames.size(); ++frame) scene.onUpdateRuntime(step, false);
	EXPECT_EQ(computeWorldHash(scene), liveHash);
	scene.onEndRuntime();
}

TEST_F(ReplayTest, TheSceneReadsTheDevicesAgainAfterTheReplay) {
	Replay recorded;
	std::ignore = record(recorded);
	recorded.frames.resize(10);
	Scene scene;
	build(scene);
	ASSERT_TRUE(scene.setPlaybackReplay(mkShared<const Replay>(recorded)));
	scene.onStartRuntime();
	core::Timestep step;
	step.forceUpdate(std::chrono::milliseconds{16});
	for (int frame = 0; frame < 10; ++frame) scene.onUpdateRuntime(step, false);
	EXPECT_TRUE(scene.getReplayProgress().finished);
	input::Input::injectMousePos({321.f, 4.f});
	scene.onUpdateRuntime(step, false);
	EXPECT_EQ(scene.getInputFrame().getCursor(), (math::vec2{321.f, 4.f}));
	scene.onEndRuntime();
}

TEST_F(ReplayTest, RequestsAreRefusedWhilePlaying) {
	Scene scene;
	build(scene);
	scene.onStartRuntime();
	EXPECT_FALSE(scene.setRecordingReplay(true));
	EXPECT_FALSE(scene.setPlaybackReplay(mkShared<const Replay>()));
	EXPECT_FALSE(scene.takeRecordedReplay().has_value());
	EXPECT_TRUE(scene.setRecordingReplay(false));
	scene.onEndRuntime();
}

// Replayed halfway then taken over, the session goes on live with the same inputs: the recording that comes out is
// the live one, frame for frame, and replays to the same world.
TEST_F(ReplayTest, ATakenOverReplayRecordsTheSessionFromThere) {
	Replay recorded;
	const uint64_t liveHash = record(recorded);
	constexpr int half = g_frameCount / 2;
	Scene scene;
	build(scene);
	EXPECT_FALSE(scene.takeOverReplay());
	ASSERT_TRUE(scene.setPlaybackReplay(mkShared<const Replay>(recorded)));
	scene.onStartRuntime();
	core::Timestep step;
	for (int frame = 0; frame < g_frameCount; ++frame) {
		if (frame == half) {
			ASSERT_TRUE(scene.takeOverReplay());
			EXPECT_FALSE(scene.isPlayingReplay());
			ASSERT_TRUE(scene.getRecordedReplay().has_value());
			EXPECT_EQ(scene.getRecordedReplay()->frames.size(), static_cast<std::size_t>(half));
		}
		inject(frame);
		UiInputSystem::update(&scene, {320, 200}, {static_cast<float>(frame), 5.f}, frame % 30 == 0, 0.016f);
		step.forceUpdate(frameDuration(frame));
		scene.onUpdateRuntime(step, false);
	}
	input::Input::resetInjection();
	EXPECT_EQ(computeWorldHash(scene), liveHash);
	const auto branched = scene.takeRecordedReplay();
	scene.onEndRuntime();
	ASSERT_TRUE(branched.has_value());
	EXPECT_EQ(branched->frames.size(), recorded.frames.size());
	EXPECT_EQ(branched->checkpoints, recorded.checkpoints);
	EXPECT_EQ(branched->startHash, recorded.startHash);
	ReplayProgress progress;
	EXPECT_EQ(replay(*branched, progress), liveHash);
	EXPECT_FALSE(progress.divergedFrame.has_value());
}
