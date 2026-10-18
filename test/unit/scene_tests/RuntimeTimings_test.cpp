/**
 * @file RuntimeTimings_test.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <scene/Scene.h>
#include <scene/component/components.h>

#include <chrono>

using namespace owl::scene;

TEST(SceneRuntimeTimings, OffByDefaultAndConsistentWhenOn) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	const auto sc = owl::mkShared<Scene>();
	auto camera = sc->createEntity("camera");
	camera.addOrReplaceComponent<component::Camera>().primary = true;
	sc->onViewportResize({320, 200});
	sc->onStartRuntime();
	owl::core::Timestep ts;
	ts.forceUpdate(std::chrono::milliseconds(16));
	sc->onUpdateRuntime(ts, /*iRender=*/false);
	EXPECT_DOUBLE_EQ(sc->getLastRuntimeTimings().totalMs, 0.0);

	sc->setRuntimeTimingsEnabled(true);
	ts.forceUpdate(std::chrono::milliseconds(16));
	sc->onUpdateRuntime(ts, /*iRender=*/false);
	const auto& timings = sc->getLastRuntimeTimings();
	EXPECT_GT(timings.totalMs, 0.0);
	EXPECT_GE(timings.scriptsMs, 0.0);
	EXPECT_GE(timings.physicsMs, 0.0);
	EXPECT_GE(timings.renderMs, 0.0);
	EXPECT_LE(timings.scriptsMs + timings.physicsMs + timings.renderMs, timings.totalMs);

	sc->setRuntimeTimingsEnabled(false);
	ts.forceUpdate(std::chrono::milliseconds(16));
	sc->onUpdateRuntime(ts, /*iRender=*/false);
	EXPECT_DOUBLE_EQ(sc->getLastRuntimeTimings().totalMs, 0.0);
	sc->onEndRuntime();
	owl::core::Log::invalidate();
}
