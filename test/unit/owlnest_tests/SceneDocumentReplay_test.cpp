/**
 * @file SceneDocumentReplay_test.cpp
 * @author Silmaen
 * @date 04/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include "document/SceneDocument.h"
#include "nestTestHelper.h"

#include <app/Application.h>
#include <app/EngineContext.h>
#include <scene/Replay.h>
#include <scene/SceneSerializer.h>
#include <scene/SettingsManager.h>
#include <scene/UiInputSystem.h>
#include <scene/WorldHash.h>

#include <chrono>
#include <cstdint>
#include <filesystem>

using namespace owl;
using namespace owl::nest;

namespace {

class SceneDocumentReplay : public nest::test::NestTest {
protected:
	// Plays a few frames of the document's scene.
	static void play(SceneDocument& ioDoc, const int iFrames) {
		ioDoc.onScenePlay();
		core::Timestep step;
		step.forceUpdate(std::chrono::milliseconds{16});
		for (int frame = 0; frame < iFrames; ++frame) {
			scene::UiInputSystem::update(ioDoc.getActiveScene().get(), {64, 64}, {0.f, 0.f}, false, 0.016f);
			ioDoc.getActiveScene()->onUpdateRuntime(step, false);
		}
	}
};

}// namespace

TEST_F(SceneDocumentReplay, RecordsThePlayUntilStopAndHandsItOnce) {
	SceneDocument doc;
	doc.applyLoadedScene(mkShared<scene::Scene>(), "/tmp/owl_replay_doc/level.owl", {64, 64});
	EXPECT_FALSE(doc.isRecordingNextPlay());
	doc.setRecordingNextPlay(true);
	play(doc, 5);
	EXPECT_TRUE(doc.getActiveScene()->isRecordingReplay());
	EXPECT_FALSE(doc.takeRecording().has_value());
	doc.onSceneStop();
	auto recording = doc.takeRecording();
	ASSERT_TRUE(recording.has_value());
	EXPECT_EQ(recording->frames.size(), 5u);
	EXPECT_EQ(recording->scene, "/tmp/owl_replay_doc/level.owl");
	EXPECT_FALSE(doc.takeRecording().has_value());
	EXPECT_TRUE(doc.isRecordingNextPlay());
}

TEST_F(SceneDocumentReplay, ReplaysOnlyTheNextPlay) {
	SceneDocument doc;
	doc.applyLoadedScene(mkShared<scene::Scene>(), "/tmp/owl_replay_doc/level.owl", {64, 64});
	auto replay = mkShared<scene::Replay>();
	replay->frames.resize(3);
	replay->startHash = scene::computeWorldHash(*scene::Scene::copy(doc.getEditorScene()));
	doc.setReplayForNextPlay(replay);
	play(doc, 3);
	EXPECT_TRUE(doc.getActiveScene()->getReplayProgress().finished);
	EXPECT_EQ(doc.getActiveScene()->getReplayProgress().frameCount, 3u);
	doc.onSceneStop();
	play(doc, 1);
	EXPECT_EQ(doc.getActiveScene()->getReplayProgress().frameCount, 0u);
	doc.onSceneStop();
}

// The Engine Demo replay, recorded by the runner, plays in sync in a scene document (the editor's Play, a copy of the
// edited scene) once the project's actions, settings and texts are loaded, as when Owl Nest opens the project.
TEST_F(SceneDocumentReplay, TheDemoReplayPlaysInSyncInTheEditor) {
	const auto demo = owl::test::getRootPath() / "engine_demo";
	auto app = mkShared<app::Application>(app::AppParams{.args = nullptr,
														 .frameLogFrequency = 0,
														 .name = "replayTest",
														 .assetsPattern = "",
														 .icon = "",
														 .width = 0,
														 .height = 0,
														 .argCount = 0,
														 .renderer = renderer::gpu::RenderAPI::Type::Null,
														 .hasGui = false,
														 .useDebugging = false,
														 .isDummy = true});
	app->addAssetDirectory({.title = "demo", .assetsPath = demo}, true);
	auto& context = app->getEngineContext();
	context.getSettings().loadDefaults(demo / "game_settings.yml");
	EXPECT_TRUE(context.loadActionMap(demo));
	EXPECT_TRUE(context.loadLocalisation(demo));
	const auto replay = scene::Replay::loadFile(demo / "replays" / "platformer_house_tour.owlreplay");
	ASSERT_TRUE(replay.has_value()) << replay.error();
	const auto loaded = mkShared<scene::Scene>();
	ASSERT_TRUE(scene::SceneSerializer(loaded).deserialize(demo / replay->scene));
	{
		SceneDocument doc;
		doc.applyLoadedScene(loaded, demo / replay->scene, {64, 64});
		doc.setReplayForNextPlay(mkShared<const scene::Replay>(*replay));
		play(doc, static_cast<int>(replay->frames.size()));
		const auto& progress = doc.getActiveScene()->getReplayProgress();
		EXPECT_TRUE(progress.finished);
		EXPECT_GT(progress.checkedCount, 0u);
		EXPECT_FALSE(progress.divergedFrame.has_value());
		doc.onSceneStop();
	}
	app::Application::invalidate();
}
