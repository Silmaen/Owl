/**
 * @file SceneDocumentStop_test.cpp
 * @author Silmaen
 * @date 08/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include "document/SceneDocument.h"
#include "nestTestHelper.h"

#include <app/Application.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>

#include <filesystem>
#include <tuple>

using namespace owl;
using namespace owl::nest;

namespace {

// The far orbit of the Engine Demo, played in a scene document, leaves for the hub as Escape does it.
class SceneDocumentStop : public nest::test::NestTest {
protected:
	void SetUp() override {
		NestTest::SetUp();
		const auto demo = owl::test::getRootPath() / "engine_demo";
		mp_app = mkShared<app::Application>(app::AppParams{.args = nullptr,
														   .frameLogFrequency = 0,
														   .name = "stopTest",
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
		const auto scenePath = demo / "scenes" / "far_orbit.owl";
		const auto loaded = mkShared<scene::Scene>();
		ASSERT_TRUE(scene::SceneSerializer(loaded).deserialize(scenePath));
		m_doc.applyLoadedScene(loaded, scenePath, {64, 64});
		std::ignore = m_doc.consumeSceneSwapped();
	}

	void TearDown() override {
		if (m_doc.state() != SceneDocument::State::Edit)
			m_doc.onSceneStop();
		app::Application::invalidate();
		mp_app.reset();
		NestTest::TearDown();
	}

	shared<app::Application> mp_app;
	SceneDocument m_doc;
};

}// namespace

TEST_F(SceneDocumentStop, StoppingInATeleportedLevelGivesBackTheRenderersOfTheEditedScene) {
	const auto edited = m_doc.getActiveScene()->getEnabledRenderers().toYaml();
	m_doc.onScenePlay();
	std::ignore = m_doc.consumeSceneSwapped();
	auto& request = m_doc.getActiveScene()->teleportRequest;
	request.pending = true;
	request.levelName = "scenes/world_map.owl";
	m_doc.handleTeleportRequest({64, 64});
	ASSERT_TRUE(m_doc.consumeSceneSwapped());
	ASSERT_NE(m_doc.getActiveScene()->getEnabledRenderers().toYaml(), edited);

	m_doc.onSceneStop();
	EXPECT_TRUE(m_doc.consumeSceneSwapped());
	EXPECT_EQ(m_doc.getActiveScene()->getEnabledRenderers().toYaml(), edited);
}
