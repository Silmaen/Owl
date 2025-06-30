
#include "testHelper.h"

#include <input/Input.h>
#include <renderer/CameraEditor.h>

using namespace owl::renderer;

TEST(CameraEditor, defaultCreation) {
	const CameraEditor cam;
	EXPECT_EQ(cam.getDistance(), 10.f);
	const auto proj = cam.getViewProjection();
	const auto view = cam.getView();
	EXPECT_EQ(proj(0, 0), view(0, 0));
	EXPECT_TRUE(cam.getPosition() == owl::math::vec3({0.f, 0.f, 0.f}));
	EXPECT_EQ(cam.getPitch(), 0.f);
	EXPECT_EQ(cam.getYaw(), 0.f);

	EXPECT_TRUE(cam.getForwardDirection() == owl::math::vec3({0.f, 0.f, -1.f}));
	EXPECT_TRUE(cam.getRightDirection() == owl::math::vec3({1.f, 0.f, 0.f}));
	EXPECT_TRUE(cam.getUpDirection() == owl::math::vec3({0.f, 1.f, 0.f}));
	EXPECT_LE((cam.getOrientation() - owl::math::quat(1.f, 0.f, 0.f, 0.f)).norm(), 0.01);
}

TEST(CameraEditor, fovCreation) {
	CameraEditor cam(45, 1.5f, 0.1f, 15);
	EXPECT_EQ(cam.getDistance(), 10.f);
	const auto proj = cam.getViewProjection();
	const auto view = cam.getView();
	EXPECT_EQ(view(0, 0), 1.f);
	cam.setTransform(view);
	EXPECT_NEAR(proj(0, 0), 1.6094f, 0.001);
	EXPECT_TRUE(cam.getPosition() == owl::math::vec3({0.f, 0.f, 10.f}));
	EXPECT_EQ(cam.getPitch(), 0.f);
	EXPECT_EQ(cam.getYaw(), 0.f);

	EXPECT_TRUE(cam.getForwardDirection() == owl::math::vec3({0.f, 0.f, -1.f}));
	EXPECT_TRUE(cam.getRightDirection() == owl::math::vec3({1.f, 0.f, 0.f}));
	EXPECT_TRUE(cam.getUpDirection() == owl::math::vec3({0.f, 1.f, 0.f}));
	EXPECT_LE((cam.getOrientation() - owl::math::quat(1.f, 0.f, 0.f, 0.f)).norm(), 0.01);
}

TEST(CameraEditor, ChangeViewport) {
	CameraEditor cam(45, 1.5f, 0.1f, 15);
	cam.setViewportSize({800, 600});
	const auto proj = cam.getViewProjection();
	EXPECT_NEAR(proj(0, 0), 1.8106f, 0.001);
}

TEST(CameraEditor, Events) {
	CameraEditor cam(45, 1.5f, 0.1f, 15);
	owl::event::MouseButtonPressedEvent btnE(owl::input::mouse::Button0);
	cam.onEvent(btnE);// should do nothing
	EXPECT_EQ(cam.getDistance(), 10.f);
	owl::event::MouseScrolledEvent scrE(0.f, 4.f);
	cam.onEvent(scrE);
	EXPECT_NEAR(cam.getDistance(), 8.4f, 0.01);
	owl::event::MouseScrolledEvent scrE2(0.f, 400.f);
	cam.onEvent(scrE2);
	EXPECT_NEAR(cam.getDistance(), 1.f, 0.01);
}

TEST(CameraEditor, Frames) {
	owl::core::Timestep ts;
	CameraEditor cam(45, 1.5f, 0.1f, 15);
	owl::input::Input::init(owl::window::Type::Null);
	ts.update();
	cam.onUpdate(ts);
	owl::input::Input::injectKey(owl::input::key::LeftAlt);// activate mouse ctrl
	ts.update();
	cam.onUpdate(ts);
	owl::input::Input::injectMouseButton(owl::input::mouse::ButtonRight);
	owl::input::Input::injectMousePos({0.f, 1.f});
	ts.update();
	cam.onUpdate(ts);
	owl::input::Input::injectMouseButton(owl::input::mouse::ButtonRight);// release btn
	owl::input::Input::injectMouseButton(owl::input::mouse::ButtonLeft);// press btn
	owl::input::Input::injectMousePos({1.f, 1.f});
	ts.update();
	cam.onUpdate(ts);
	owl::input::Input::injectMouseButton(owl::input::mouse::ButtonLeft);// release btn
	owl::input::Input::injectMouseButton(owl::input::mouse::ButtonMiddle);// press btn
	owl::input::Input::injectMousePos({1.f, 4.f});
	ts.update();
	cam.onUpdate(ts);
	EXPECT_NEAR(cam.getDistance(), 9.988f, 0.01);

	owl::input::Input::invalidate();
}
