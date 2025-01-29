
#include "testHelper.h"

#include <gui/component/render.h>
#include <scene/component/components.h>

using namespace owl::core;
using namespace owl::gui;
using namespace owl::gui::component;
using namespace owl::scene::component;

TEST(Component, render) {
	Log::init(owl::core::Log::Level::Off);
	UiLayer layer;
	layer.disableApp();
	layer.enableDocking();
	layer.onAttach();
	layer.begin();
	Transform tran{};
	renderProps(tran);
	Camera cam;
	cam.camera.setProjectionType(owl::scene::SceneCamera::ProjectionType::Orthographic);
	renderProps(cam);
	Camera cam2;
	cam2.camera.setProjectionType(owl::scene::SceneCamera::ProjectionType::Perspective);
	renderProps(cam2);
	SpriteRenderer spr{};
	renderProps(spr);
	CircleRenderer circ;
	renderProps(circ);
	Text text{};
	renderProps(text);
	PhysicBody pbody;
	renderProps(pbody);
	Player play;
	renderProps(play);
	Trigger trig;
	renderProps(trig);
	EntityLink elink;
	renderProps(elink);

	layer.end();
	layer.onDetach();
	Log::invalidate();
	EXPECT_TRUE(true);
}
