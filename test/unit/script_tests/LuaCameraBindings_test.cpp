/**
 * @file LuaCameraBindings_test.cpp
 * @author Silmaen
 * @date 24/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneCamera.h>
#include <scene/component/components.h>
#include <script/EntityHandle.h>
#include <script/ScriptInstance.h>

#include <cstdint>
#include <format>
#include <string>
#include <tuple>
#include <vector>

using namespace owl;
using namespace owl::script;

namespace {

/// A scene with a camera entity and a bare entity, and a script running `iBody` on creation.
class LuaCamera : public testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_scene = mkShared<scene::Scene>();
		m_camera = m_scene->createEntity("Camera");
		m_camera.addComponent<scene::component::Camera>().primary = true;
		m_bare = m_scene->createEntity("Bare");
	}

	void TearDown() override { core::Log::invalidate(); }

	/// Run a script on creation and return the instance holding its globals.
	auto run(const std::string& iBody) -> shared<ScriptInstance> {
		const std::string text = std::format("cam = {}\nbare = {}\nfunction on_create()\n{}\nend\n",
											 static_cast<int64_t>(getEntityHandle(*m_scene, m_camera)),
											 static_cast<int64_t>(getEntityHandle(*m_scene, m_bare)), iBody);
		const std::vector<uint8_t> data(text.begin(), text.end());
		auto instance = mkShared<ScriptInstance>();
		instance->setScene(m_scene.get());
		EXPECT_TRUE(instance->createFromBuffer(data, "camera_test", static_cast<uint64_t>(m_camera.getUUID())));
		instance->onCreate();
		return instance;
	}

	auto camera() -> scene::SceneCamera& { return m_camera.getComponent<scene::component::Camera>().camera; }

	shared<scene::Scene> m_scene;
	scene::Entity m_camera;
	scene::Entity m_bare;
};

}// namespace

TEST_F(LuaCamera, theOrthographicSizeIsReadAndSet) {
	camera().setOrthographic(8.f, -1.f, 10.f);
	const auto instance = run("before = entity.get_camera_size(cam)\n"
							  "done = entity.set_camera_size(cam, 12.5)\n"
							  "after = entity.get_camera_size(cam)");
	EXPECT_NEAR(instance->getPropertyFloat("before").value_or(0.f), 8.f, 1e-5f);
	EXPECT_TRUE(instance->getPropertyBool("done").value_or(false));
	EXPECT_NEAR(instance->getPropertyFloat("after").value_or(0.f), 12.5f, 1e-5f);
	EXPECT_NEAR(camera().getOrthographicSize(), 12.5f, 1e-5f);
}

TEST_F(LuaCamera, aSizeBelowTheSmallestIsRaised) {
	std::ignore = run("entity.set_camera_size(cam, -4)");
	EXPECT_NEAR(camera().getOrthographicSize(), 0.01f, 1e-6f);
}

TEST_F(LuaCamera, theFieldOfViewIsReadAndSet) {
	camera().setPerspective(1.f, 0.1f, 100.f);
	const auto instance = run("before = entity.get_camera_fov(cam)\n"
							  "done = entity.set_camera_fov(cam, 0.5)\n"
							  "after = entity.get_camera_fov(cam)\n"
							  "entity.set_camera_fov(cam, 9)\n"
							  "wide = entity.get_camera_fov(cam)");
	EXPECT_NEAR(instance->getPropertyFloat("before").value_or(0.f), 1.f, 1e-5f);
	EXPECT_TRUE(instance->getPropertyBool("done").value_or(false));
	EXPECT_NEAR(instance->getPropertyFloat("after").value_or(0.f), 0.5f, 1e-5f);
	EXPECT_NEAR(instance->getPropertyFloat("wide").value_or(0.f), 3.f, 1e-5f);
	EXPECT_NEAR(camera().getPerspectiveVerticalFov(), 3.f, 1e-5f);
}

TEST_F(LuaCamera, anEntityWithoutCameraIsRefused) {
	const auto instance = run("size = entity.set_camera_size(bare, 4)\n"
							  "fov = entity.set_camera_fov(bare, 1)\n"
							  "none = entity.get_camera_size(bare) == nil and entity.get_camera_fov(bare) == nil");
	EXPECT_FALSE(instance->getPropertyBool("size").value_or(true));
	EXPECT_FALSE(instance->getPropertyBool("fov").value_or(true));
	EXPECT_TRUE(instance->getPropertyBool("none").value_or(false));
}
