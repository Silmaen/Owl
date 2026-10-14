/**
 * @file HeadlessSession_test.cpp
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include "nestTestHelper.h"
#include "session/HeadlessSession.h"

#include <app/Application.h>
#include <input/KeyCodes.h>
#include <renderer/TextureDecoder.h>
#include <scene/Entity.h>
#include <scene/component/Transform.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

using namespace owl;
using namespace owl::nest;
using namespace owl::nest::session;

namespace {

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wweak-vtables")
class HeadlessSessionTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_app = mkShared<app::Application>(app::AppParams{.args = nullptr,
														  .frameLogFrequency = 0,
														  .name = "headlessSessionTest",
														  .assetsPattern = "",
														  .icon = "",
														  .width = 0,
														  .height = 0,
														  .argCount = 0,
														  .renderer = renderer::gpu::RenderAPI::Type::Null,
														  .hasGui = false,
														  .useDebugging = false,
														  .isDummy = true});
	}

	void TearDown() override {
		app::Application::invalidate();
		m_app.reset();
		core::Log::invalidate();
	}

	static auto samplePath() -> std::filesystem::path {
		return owl::test::getRootPath() / "sample_project" / "scenes" / "platformer_house.owl";
	}

	static auto playerX(const EditorSession& iSession) -> float {
		const auto scene = iSession.getSessionScene();
		return scene->getWorldTransform(scene->getPrimaryPlayer()).translation().x();
	}

	/// Dummy application on the Null renderer.
	shared<app::Application> m_app;
};
OWL_DIAG_POP

}// namespace

TEST_F(HeadlessSessionTest, LoadsASampleScene) {
	HeadlessSession session;
	EXPECT_FALSE(session.getSessionScene());
	EXPECT_EQ(session.getSessionUndo(), nullptr);
	ASSERT_TRUE(session.loadScene(samplePath()).has_value());
	ASSERT_TRUE(session.getSessionScene());
	EXPECT_EQ(session.getSessionScenePath(), std::filesystem::weakly_canonical(samplePath()));
	EXPECT_EQ(session.getProjectDirectory(),
			  std::filesystem::weakly_canonical(owl::test::getRootPath() / "sample_project"));
	EXPECT_FALSE(session.isScenePlaying());
	ASSERT_NE(session.getSessionUndo(), nullptr);
	EXPECT_FALSE(session.getSessionUndo()->canUndo());
	EXPECT_GT(session.getStatistics().entityCount, 0u);
	EXPECT_TRUE(session.getSessionScene()->getPrimaryPlayer());
	EXPECT_EQ(session.getRegistry().find("entity.create")->name, "entity.create");

	const auto missing = session.loadScene(owl::test::getRootPath() / "sample_project" / "scenes" / "missing.owl");
	ASSERT_FALSE(missing.has_value());
	EXPECT_EQ(missing.error().error, SessionError::LoadFailed);
	EXPECT_EQ(session.getSessionScenePath(), std::filesystem::weakly_canonical(samplePath()));
}

TEST_F(HeadlessSessionTest, RunsCommandsAndUndoesThem) {
	HeadlessSession session;
	ASSERT_TRUE(session.loadScene(samplePath()).has_value());
	const auto before = nest::test::sceneState(*session.getSessionScene());

	const auto created = session.execute("entity.create", {{"name", std::string{"Crate"}}});
	ASSERT_TRUE(created.has_value());
	const auto crate = session.getSessionScene()->findEntityByUUID(created->entity);
	ASSERT_TRUE(crate);
	const auto moved = session.execute("entity.set_transform",
									   {{"entity", std::string{"Crate"}}, {"translation", math::vec3{2.f, 3.f, 0.f}}});
	ASSERT_TRUE(moved.has_value());
	EXPECT_FLOAT_EQ(nest::test::localX(crate), 2.f);
	EXPECT_TRUE(session.getSessionUndo()->isDirty());

	ASSERT_TRUE(session.execute("edit.undo", {}).has_value());
	ASSERT_TRUE(session.execute("edit.undo", {}).has_value());
	EXPECT_EQ(nest::test::sceneState(*session.getSessionScene()), before);
	EXPECT_FALSE(session.execute("edit.undo", {}).has_value());
	ASSERT_TRUE(session.execute("edit.redo", {}).has_value());
	EXPECT_TRUE(session.getSessionScene()->findEntityByUUID(created->entity));

	const auto unknown = session.execute("entity.teleport", {});
	ASSERT_FALSE(unknown.has_value());
	EXPECT_EQ(unknown.error().error, commands::CommandError::UnknownCommand);
}

TEST_F(HeadlessSessionTest, SavesThroughTheSceneCommand) {
	HeadlessSession session;
	session.newScene();
	EXPECT_TRUE(session.getSessionScenePath().empty());
	ASSERT_TRUE(session.execute("entity.create", {{"name", std::string{"Lonely"}}}).has_value());
	EXPECT_FALSE(session.execute("scene.save", {}).has_value());
	const auto file = std::filesystem::temp_directory_path() / "owl_headless_session_save.owl";
	std::filesystem::remove(file);
	const auto saved = session.execute("scene.save", {{"path", file.string()}});
	ASSERT_TRUE(saved.has_value());
	EXPECT_EQ(session.getSessionScenePath(), file);
	EXPECT_FALSE(session.getSessionUndo()->isDirty());
	EXPECT_TRUE(exists(file));
	std::filesystem::remove(file);
}

TEST_F(HeadlessSessionTest, PlaysTicksDeterministically) {
	OWL_REQUIRE_MODULE(PHYSICS);
	HeadlessSession session;
	ASSERT_TRUE(session.loadScene(samplePath()).has_value());
	const float editX = playerX(session);

	ASSERT_TRUE(session.startPlay({.frames = 0, .ticks = 30, .stopAtEnd = false}).has_value());
	EXPECT_TRUE(session.isScenePlaying());
	EXPECT_FALSE(session.isPlayRunning());
	const auto report = session.takePlayReport();
	ASSERT_TRUE(report.has_value());
	EXPECT_EQ(report->frames, 30u);
	EXPECT_EQ(report->ticks, 30u);
	EXPECT_FALSE(report->interrupted);
	EXPECT_GT(report->entityCount, 0u);
	EXPECT_FALSE(report->capture.has_value());
	EXPECT_FALSE(session.takePlayReport().has_value());
	EXPECT_EQ(session.getStatistics().tickCount, 30u);
	const float firstRun = playerX(session);
	EXPECT_NEAR(firstRun, 4.5f, 0.01f);

	// Held keys apply to the frames of the play; a playing scene goes on from where it is.
	ASSERT_TRUE(session.startPlay({.frames = 20, .keys = {input::key::Right}, .stopAtEnd = false}).has_value());
	EXPECT_GT(playerX(session), 5.f);

	const auto refused = session.execute("entity.create", {});
	ASSERT_FALSE(refused.has_value());
	EXPECT_EQ(refused.error().error, commands::CommandError::RefusedDuringPlay);

	ASSERT_TRUE(session.stopPlay().has_value());
	EXPECT_FALSE(session.isScenePlaying());
	EXPECT_FLOAT_EQ(playerX(session), editX);
	EXPECT_EQ(session.getStatistics().tickCount, 0u);

	ASSERT_TRUE(session.startPlay({.ticks = 30, .stopAtEnd = false}).has_value());
	EXPECT_FLOAT_EQ(playerX(session), firstRun);
	ASSERT_TRUE(session.stopPlay().has_value());
}

TEST_F(HeadlessSessionTest, BoundsAPlayByTicksAtALargerStep) {
	HeadlessSession session;
	ASSERT_TRUE(session.loadScene(samplePath()).has_value());
	const double tickMs = 1000.0 / static_cast<double>(m_app->getEngineContext().getSimulationSettings().tickRate);
	ASSERT_TRUE(session.startPlay({.frames = 100, .ticks = 10, .timestepMs = 2.0 * tickMs}).has_value());
	const auto report = session.takePlayReport();
	ASSERT_TRUE(report.has_value());
	EXPECT_EQ(report->ticks, 10u);
	EXPECT_EQ(report->frames, 5u);
	EXPECT_NEAR(report->timestepMs, 2.0 * tickMs, 0.01);
	EXPECT_FALSE(session.isScenePlaying());
}

TEST_F(HeadlessSessionTest, RefusesInvalidRequests) {
	HeadlessSession session;
	const auto noScene = session.startPlay({.frames = 1});
	ASSERT_FALSE(noScene.has_value());
	EXPECT_EQ(noScene.error().error, SessionError::NoScene);
	EXPECT_EQ(session.captureFrame().error().error, SessionError::NoScene);
	session.newScene();
	EXPECT_EQ(session.startPlay({}).error().error, SessionError::InvalidRequest);
	EXPECT_EQ(session.startPlay({.frames = PlayRequest::maxCount + 1}).error().error, SessionError::InvalidRequest);
	EXPECT_EQ(session.startPlay({.frames = 1, .timestepMs = -1.0}).error().error, SessionError::InvalidRequest);
	EXPECT_EQ(session.stopPlay().error().error, SessionError::NotPlaying);
	EXPECT_FALSE(session.isScenePlaying());
}

TEST_F(HeadlessSessionTest, CapturesABlankFrameOnTheNullRenderer) {
	HeadlessSession session{{64, 48}};
	ASSERT_TRUE(session.loadScene(samplePath()).has_value());
	const auto frame = session.captureFrame();
	ASSERT_TRUE(frame.has_value());
	EXPECT_EQ(frame->size, (math::vec2ui{64, 48}));
	ASSERT_TRUE(frame->isValid());
	size_t unexpectedBytes = 0;
	for (size_t i = 0; i < frame->rgba.size(); ++i)
		if (frame->rgba[i] != (i % 4 == 3 ? 255 : 0))
			++unexpectedBytes;
	EXPECT_EQ(unexpectedBytes, 0u);
	const auto png = frame->encodePng();
	ASSERT_FALSE(png.empty());
	const auto decoded = renderer::decodeImageBytes(png, 4);
	ASSERT_TRUE(decoded.valid);
	EXPECT_EQ(decoded.size, (math::vec2ui{64, 48}));
	EXPECT_TRUE(FrameCapture{}.encodePng().empty());

	ASSERT_TRUE(session.startPlay({.frames = 3, .capture = true}).has_value());
	const auto report = session.takePlayReport();
	ASSERT_TRUE(report.has_value());
	ASSERT_TRUE(report->capture.has_value());
	EXPECT_TRUE(report->capture->isValid());
	EXPECT_EQ(report->capture->size, (math::vec2ui{64, 48}));
}

TEST_F(HeadlessSessionTest, MeasuresTheFrames) {
	HeadlessSession session;
	ASSERT_TRUE(session.loadScene(samplePath()).has_value());
	EXPECT_EQ(session.getStatistics().frameCount, 0u);
	ASSERT_TRUE(session.startPlay({.frames = 10}).has_value());
	const auto statistics = session.getStatistics();
	EXPECT_EQ(statistics.frameCount, 10u);
	EXPECT_GE(statistics.meanFrameMs, 0.0);
	EXPECT_GE(statistics.lastFrameMs, 0.0);
	EXPECT_GT(statistics.entityCount, 0u);
	const auto report = session.takePlayReport();
	ASSERT_TRUE(report.has_value());
	EXPECT_GE(report->totalMs, report->maxFrameMs);
	EXPECT_GE(report->getMeanFrameMs(), 0.0);
	EXPECT_GT(report->drawCalls, 0u);
}

TEST(SessionStatistics, AveragesTheFrames) {
	SessionStatistics statistics;
	statistics.addFrame(2.0, 3);
	statistics.addFrame(4.0, 5);
	EXPECT_EQ(statistics.frameCount, 2u);
	EXPECT_DOUBLE_EQ(statistics.meanFrameMs, 3.0);
	EXPECT_DOUBLE_EQ(statistics.lastFrameMs, 4.0);
	EXPECT_EQ(statistics.lastDrawCalls, 5u);
	EXPECT_DOUBLE_EQ(PlayReport{}.getMeanFrameMs(), 0.0);
}

TEST(SessionProject, FindsTheProjectOfAScene) {
	const auto root = owl::test::getRootPath();
	EXPECT_EQ(findProjectDirectory(root / "sample_project" / "scenes" / "main_menu.owl"), root / "sample_project");
	EXPECT_TRUE(findProjectDirectory(std::filesystem::path{"/no_such_dir/scene.owl"}).empty());
}
