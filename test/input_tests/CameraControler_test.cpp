
#include "testHelper.h"

#include <input/CameraOrthoController.h>
#include <input/Input.h>

using namespace owl::input;

TEST(Input, baseCameraControler) {
	CameraOrthoController coc(1);
	EXPECT_EQ(coc.getZoomLevel(), 1.0f);
	coc.setZoomLevel(2.0f);
	EXPECT_EQ(coc.getZoomLevel(), 2.0f);
}

TEST(Input, ResizeCameraControler) {
	CameraOrthoController coc(1);
	coc.onResize({10, 5});
	EXPECT_EQ(coc.getZoomLevel(), 1.0f);
}

TEST(Input, EventCameraControler) {
	CameraOrthoController coc(1);
	owl::event::AppRenderEvent e;
	coc.onEvent(e);// nothing
	owl::event::WindowResizeEvent re({10, 5});
	coc.onEvent(re);// resize
	owl::event::MouseScrolledEvent me(0, 1);
	coc.onEvent(me);// resize
	EXPECT_EQ(coc.getZoomLevel(), 0.75f);
}

TEST(Input, KeyOrthoControler) {
	Input::init(owl::window::Type::Null);
	owl::core::Timestep ts;
	const owl::core::Timestep::duration delta = std::chrono::milliseconds(3);
	ts.forceUpdate(delta);
	CameraOrthoController coc(1);
	coc.onUpdate(ts);
	coc.getCamera().setPosition({0, 0, 0});
	{
		Input::injectKey(key::A);
		ts.forceUpdate(delta);
		coc.onUpdate(ts);
		EXPECT_NEAR(coc.getCamera().getPosition().x(), -0.003, 0.001);
		Input::resetInjection();
	}
	coc.getCamera().setPosition({0, 0, 0});
	{
		Input::injectKey(key::D);
		ts.forceUpdate(delta);
		coc.onUpdate(ts);
		EXPECT_NEAR(coc.getCamera().getPosition().x(), 0.0, 0.001);
		Input::resetInjection();
	}
	{
		Input::injectKey(key::W);
		ts.forceUpdate(delta);
		coc.onUpdate(ts);
		EXPECT_NEAR(coc.getCamera().getPosition().y(), 0.003, 0.001);
		Input::resetInjection();
	}
	{
		Input::injectKey(key::S);
		ts.forceUpdate(delta);
		coc.onUpdate(ts);
		EXPECT_NEAR(coc.getCamera().getPosition().y(), -0.0, 0.001);
		Input::resetInjection();
	}
	{
		Input::injectKey(key::Q);
		ts.forceUpdate(delta);
		coc.onUpdate(ts);
		EXPECT_NEAR(coc.getCamera().getRotation(), 0.00, 0.001);
		Input::resetInjection();
	}
	{
		Input::injectKey(key::E);
		ts.forceUpdate(delta);
		coc.onUpdate(ts);
		EXPECT_NEAR(coc.getCamera().getRotation(), 0, 0.001);
		Input::resetInjection();
	}
}

TEST(Input, KeyOrthoControlerRotation) {
	Input::init(owl::window::Type::Null);
	owl::core::Timestep ts;
	const owl::core::Timestep::duration delta = std::chrono::milliseconds(3);
	ts.forceUpdate(delta);
	CameraOrthoController coc(1, true);
	coc.onUpdate(ts);

	{
		Input::injectKey(key::Q);
		ts.forceUpdate(delta);
		coc.onUpdate(ts);
		EXPECT_NEAR(coc.getCamera().getRotation(), 0.54, 0.001);
		Input::resetInjection();
	}
	{
		Input::injectKey(key::E);
		ts.forceUpdate(delta);
		coc.onUpdate(ts);
		EXPECT_NEAR(coc.getCamera().getRotation(), 0, 0.001);
		Input::resetInjection();
	}
	{
		Input::injectKey(key::Q);
		ts.forceUpdate(std::chrono::milliseconds(500));
		coc.onUpdate(ts);
		EXPECT_NEAR(coc.getCamera().getRotation(), 90, 0.001);
		Input::resetInjection();
	}
	coc.getCamera().setPosition({0, 0, 0});
	{
		Input::injectKey(key::A);
		ts.forceUpdate(delta);
		coc.onUpdate(ts);
		EXPECT_NEAR(coc.getCamera().getPosition().y(), -0.003, 0.001);
		Input::resetInjection();
	}
	coc.getCamera().setPosition({0, 0, 0});
	{
		Input::injectKey(key::D);
		ts.forceUpdate(delta);
		coc.onUpdate(ts);
		EXPECT_NEAR(coc.getCamera().getPosition().y(), 0.0, 0.001);
		Input::resetInjection();
	}
	{
		Input::injectKey(key::W);
		ts.forceUpdate(delta);
		coc.onUpdate(ts);
		EXPECT_NEAR(coc.getCamera().getPosition().x(), 0.003, 0.001);
		Input::resetInjection();
	}
	{
		Input::injectKey(key::S);
		ts.forceUpdate(delta);
		coc.onUpdate(ts);
		EXPECT_NEAR(coc.getCamera().getPosition().x(), -0.0, 0.001);
		Input::resetInjection();
	}
}
