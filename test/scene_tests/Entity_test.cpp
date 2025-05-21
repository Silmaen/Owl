
#include "testHelper.h"

#include <scene/Entity.h>
#include <scene/component/Camera.h>

using namespace owl::scene;

TEST(Entity, creation) {
	const Entity ent;
	EXPECT_FALSE(ent);
	EXPECT_TRUE(static_cast<entt::entity>(ent) == entt::null);
	EXPECT_EQ(static_cast<uint32_t>(ent), 4294967295);
	const Entity ent2(entt::null, nullptr);
	EXPECT_EQ(ent, ent2);
	EXPECT_FALSE(ent != ent2);
}

TEST(Entity, attributes) {
	Scene sc;
	const Entity ent = sc.createEntityWithUUID(0, "boby");
	EXPECT_EQ(ent.getUUID(), 0);
	EXPECT_STREQ(ent.getName().c_str(), "boby");
	Entity ent2{ent};
	EXPECT_TRUE(ent2 == ent);
	ent2.addOrReplaceComponent<component::ID>(7);
	EXPECT_TRUE(ent2.getUUID() == 7);
}

TEST(Entity, cameraOrtho) {
	Scene sc;
	Entity ent = sc.createEntityWithUUID(0, "boby");
	auto cam = ent.addComponent<component::Camera>();
	cam.camera.setViewportSize({50, 50});
	EXPECT_EQ(cam.camera.getProjectionType(), SceneCamera::ProjectionType::Orthographic);
	EXPECT_EQ(cam.camera.getOrthographicFarClip(), 1);
	EXPECT_EQ(cam.camera.getOrthographicNearClip(), -1);
	EXPECT_EQ(cam.camera.getOrthographicSize(), 10);
	auto proj = cam.camera.getProjection();
	EXPECT_NEAR(proj(0, 0), 0.2, 0.001);
	cam.camera.setOrthographic(4, 0.1f, 14.0);
	EXPECT_EQ(cam.camera.getOrthographicFarClip(), 14.0);
	EXPECT_NEAR(cam.camera.getOrthographicNearClip(), 0.1, 0.001);
	EXPECT_EQ(cam.camera.getOrthographicSize(), 4);
	proj = cam.camera.getProjection();
	EXPECT_NEAR(proj(0, 0), 0.5, 0.001);
	cam.camera.setOrthographicFarClip(1);
	cam.camera.setOrthographicNearClip(-1);
	cam.camera.setOrthographicSize(10);
	proj = cam.camera.getProjection();
	EXPECT_NEAR(proj(0, 0), 0.2, 0.001);
}

TEST(Entity, cameraPerspective) {
	Scene sc;
	sc.onViewportResize({50, 50});
	Entity ent = sc.createEntityWithUUID(0, "boby");
	auto cam = ent.addComponent<component::Camera>();
	cam.camera.setPerspective(45, -0.25, 125.0);
	EXPECT_EQ(cam.camera.getProjectionType(), SceneCamera::ProjectionType::Perspective);
	EXPECT_EQ(cam.camera.getPerspectiveFarClip(), 125.0);
	EXPECT_EQ(cam.camera.getPerspectiveNearClip(), -0.25);
	EXPECT_EQ(cam.camera.getPerspectiveVerticalFov(), 45);
	auto proj = cam.camera.getProjection();
	EXPECT_NEAR(proj(0, 0), 1.7925, 0.001);
	cam.camera.setPerspectiveFarClip(1);
	cam.camera.setPerspectiveNearClip(-1);
	cam.camera.setPerspectiveVerticalFov(85);
	EXPECT_NEAR(proj(0, 0), 1.7925, 0.001);
}
