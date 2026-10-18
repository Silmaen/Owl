/**
 * @file CameraEditor_test.cpp
 * @author Silmaen
 * @date 03/08/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <input/Input.h>
#include <renderer/CameraEditor.h>
#include <window/Window.h>

#include <array>
#include <cmath>
#include <utility>

using namespace owl::renderer;
using namespace owl::renderer::gpu;

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
	EXPECT_FLOAT_EQ(cam.getFov(), 45.f);
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
	// Alt + left-drag rotates the camera in place: yaw changes, but the alt scheme never zooms.
	owl::input::Input::injectKey(owl::input::key::LeftAlt);
	owl::input::Input::injectMouseButton(owl::input::mouse::ButtonLeft);
	owl::input::Input::injectMousePos({0.f, 0.f});
	ts.update();
	cam.onUpdate(ts);// anchor the drag origin
	owl::input::Input::injectMousePos({10.f, 0.f});
	ts.update();
	cam.onUpdate(ts);// drag right
	EXPECT_NE(cam.getYaw(), 0.f);
	EXPECT_NEAR(cam.getDistance(), 10.f, 0.001f);
	// A wheel scroll is what changes the distance.
	owl::event::MouseScrolledEvent scrE(0.f, 4.f);
	cam.onEvent(scrE);
	EXPECT_LT(cam.getDistance(), 10.f);

	owl::input::Input::invalidate();
}

TEST(CameraEditor, OrthographicFacesThePlaneAndKeepsItsFraming) {
	CameraEditor cam(45, 1.5f, 0.1f, 100);
	EXPECT_EQ(cam.getProjectionMode(), CameraEditor::Projection::Perspective);
	EXPECT_NEAR(cam.getProjection()(3, 3), 0.f, 1e-6f);
	owl::input::Input::init(owl::window::Type::Null);
	owl::core::Timestep ts;
	cam.setProjection(CameraEditor::Projection::Orthographic);
	EXPECT_EQ(cam.getProjectionMode(), CameraEditor::Projection::Orthographic);
	EXPECT_NEAR(cam.getProjection()(3, 3), 1.f, 1e-6f);
	// An orbit drag does not rotate an orthographic view.
	owl::input::Input::injectKey(owl::input::key::LeftControl);
	owl::input::Input::injectMouseButton(owl::input::mouse::ButtonLeft);
	owl::input::Input::injectMousePos({0.f, 0.f});
	ts.update();
	cam.onUpdate(ts);
	owl::input::Input::injectMousePos({40.f, 20.f});
	ts.update();
	cam.onUpdate(ts);
	EXPECT_FLOAT_EQ(cam.getYaw(), 0.f);
	EXPECT_FLOAT_EQ(cam.getPitch(), 0.f);
	// Zooming narrows the visible height.
	const float before = cam.getProjection()(1, 1);
	owl::event::MouseScrolledEvent scroll(0.f, 4.f);
	cam.onEvent(scroll);
	EXPECT_GT(std::abs(cam.getProjection()(1, 1)), std::abs(before));
	owl::input::Input::invalidate();
}

TEST(CameraEditor, FrameCentresAndFitsTheSphere) {
	CameraEditor cam(45, 1.5f, 0.1f, 100);
	cam.frame({4.f, -2.f, 0.f}, 3.f);
	EXPECT_FLOAT_EQ(cam.getFocalPoint().x(), 4.f);
	EXPECT_FLOAT_EQ(cam.getFocalPoint().y(), -2.f);
	EXPECT_GT(cam.getDistance(), 3.f);
	const float far = cam.getDistance();
	cam.frame({0.f, 0.f, 0.f}, 0.f);
	EXPECT_LT(cam.getDistance(), far);
	EXPECT_GE(cam.getDistance(), 1.f);
}

namespace {
auto near(const owl::math::vec3& iA, const owl::math::vec3& iB) -> bool { return (iA - iB).norm() < 1e-4f; }
}// namespace

TEST(CameraEditor, ViewAlongLooksFromEachSide) {
	CameraEditor cam(45, 1.5f, 0.1f, 100);
	cam.frame({1.f, 2.f, 3.f}, 2.f);
	const float distance = cam.getDistance();
	using Axis = CameraEditor::ViewAxis;
	const std::array<std::pair<Axis, owl::math::vec3>, 6> expected{{{Axis::Front, {0.f, 0.f, -1.f}},
																	{Axis::Back, {0.f, 0.f, 1.f}},
																	{Axis::Top, {0.f, -1.f, 0.f}},
																	{Axis::Bottom, {0.f, 1.f, 0.f}},
																	{Axis::Right, {-1.f, 0.f, 0.f}},
																	{Axis::Left, {1.f, 0.f, 0.f}}}};
	for (const auto& [axis, forward]: expected) {
		cam.viewAlong(axis);
		EXPECT_TRUE(near(cam.getForwardDirection(), forward)) << static_cast<int>(axis);
		// The reference point and the distance stay.
		EXPECT_TRUE(near(cam.getFocalPoint(), {1.f, 2.f, 3.f}));
		EXPECT_FLOAT_EQ(cam.getDistance(), distance);
		EXPECT_TRUE(near(cam.getPosition(), owl::math::vec3{1.f, 2.f, 3.f} - forward * distance));
	}
	// The top view keeps -Z at the top of the screen.
	cam.viewAlong(Axis::Top);
	EXPECT_TRUE(near(cam.getUpDirection(), {0.f, 0.f, -1.f}));
}

TEST(CameraEditor, ResetViewRestoresTheDefaultPose) {
	CameraEditor cam(45, 1.5f, 0.1f, 100);
	cam.frame({5.f, 5.f, 5.f}, 10.f);
	cam.viewAlong(CameraEditor::ViewAxis::Left);
	cam.resetView();
	EXPECT_TRUE(near(cam.getFocalPoint(), {0.f, 0.f, 0.f}));
	EXPECT_FLOAT_EQ(cam.getDistance(), 10.f);
	EXPECT_FLOAT_EQ(cam.getPitch(), 0.f);
	EXPECT_FLOAT_EQ(cam.getYaw(), 0.f);
	EXPECT_TRUE(near(cam.getPosition(), {0.f, 0.f, 10.f}));
}

TEST(CameraEditor, LookFromPlacesTheEyeAndTheFacing) {
	CameraEditor cam(45, 1.5f, 0.1f, 100);
	const owl::math::vec3 eye{2.f, 3.f, 4.f};
	for (const owl::math::vec3& direction:
		 {owl::math::vec3{1.f, -1.f, -1.f}, owl::math::vec3{0.f, 0.f, 2.f}, owl::math::vec3{-3.f, 0.5f, 1.f}}) {
		cam.lookFrom(eye, direction);
		EXPECT_TRUE(near(cam.getPosition(), eye));
		owl::math::vec3 unit = direction;
		unit.normalize();
		EXPECT_TRUE(near(cam.getForwardDirection(), unit));
	}
	// A null direction keeps the facing and only moves the eye.
	const auto forward = cam.getForwardDirection();
	cam.lookFrom({0.f, 0.f, 0.f}, {0.f, 0.f, 0.f});
	EXPECT_TRUE(near(cam.getPosition(), {0.f, 0.f, 0.f}));
	EXPECT_TRUE(near(cam.getForwardDirection(), forward));
}

TEST(CameraEditor, NavigationScalesAndFiltersTheDrags) {
	owl::input::Input::init(owl::window::Type::Null);
	owl::core::Timestep ts;
	const auto orbit = [&ts](CameraEditor& ioCam, const float iDx, const float iDy) -> void {
		owl::input::Input::injectKey(owl::input::key::LeftControl);
		owl::input::Input::injectMouseButton(owl::input::mouse::ButtonLeft);
		owl::input::Input::injectMousePos({0.f, 0.f});
		ts.update();
		ioCam.onUpdate(ts);
		owl::input::Input::injectMousePos({iDx, iDy});
		ts.update();
		ioCam.onUpdate(ts);
		// The injections toggle: release the key and the button for the next drag.
		owl::input::Input::injectKey(owl::input::key::LeftControl);
		owl::input::Input::injectMouseButton(owl::input::mouse::ButtonLeft);
		owl::input::Input::injectMousePos({0.f, 0.f});
	};
	CameraEditor reference(45, 1.5f, 0.1f, 100);
	orbit(reference, 10.f, 10.f);
	ASSERT_NE(reference.getYaw(), 0.f);
	ASSERT_NE(reference.getPitch(), 0.f);

	CameraEditor faster(45, 1.5f, 0.1f, 100);
	faster.setNavigation({.rotate = 2.f, .pan = 1.f, .zoom = 1.f, .invertY = true, .deadZone = 0.f});
	EXPECT_FLOAT_EQ(faster.getNavigation().rotate, 2.f);
	orbit(faster, 10.f, 10.f);
	EXPECT_NEAR(faster.getYaw(), 2.f * reference.getYaw(), 1e-5f);
	EXPECT_NEAR(faster.getPitch(), -2.f * reference.getPitch(), 1e-5f);

	// A drag shorter than the dead zone moves nothing.
	CameraEditor steady(45, 1.5f, 0.1f, 100);
	steady.setNavigation({.rotate = 1.f, .pan = 1.f, .zoom = 1.f, .invertY = false, .deadZone = 50.f});
	orbit(steady, 10.f, 10.f);
	EXPECT_FLOAT_EQ(steady.getYaw(), 0.f);
	EXPECT_FLOAT_EQ(steady.getPitch(), 0.f);

	// The zoom factor scales the wheel.
	CameraEditor wheel(45, 1.5f, 0.1f, 100);
	wheel.setNavigation({.rotate = 1.f, .pan = 1.f, .zoom = 0.5f, .invertY = false, .deadZone = 0.f});
	owl::event::MouseScrolledEvent scroll(0.f, 1.f);
	CameraEditor plain(45, 1.5f, 0.1f, 100);
	wheel.onEvent(scroll);
	plain.onEvent(scroll);
	EXPECT_NEAR(10.f - wheel.getDistance(), 0.5f * (10.f - plain.getDistance()), 1e-4f);
	owl::input::Input::invalidate();
}
