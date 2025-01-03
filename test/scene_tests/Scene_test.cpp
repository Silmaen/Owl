
#include "testHelper.h"

#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/component/Camera.h>
#include <scene/component/CircleRenderer.h>
#include <scene/component/NativeScript.h>
#include <scene/component/SpriteRenderer.h>
#include <scene/component/Text.h>
#include <scene/component/Transform.h>

using namespace owl::scene;

TEST(Scene, creation) {
	Scene sc;
	const auto cam = sc.getPrimaryCamera();
	EXPECT_FALSE(cam);
	auto ent = sc.createEntity("bob");
	EXPECT_TRUE(ent);
	sc.destroyEntity(ent);
	EXPECT_FALSE(ent);
}

TEST(Scene, camera) {
	Scene sc;
	auto cam = sc.getPrimaryCamera();
	EXPECT_FALSE(cam);
	sc.onViewportResize({0, 1});
	auto camFix = sc.createEntity("CamFixPrim");
	sc.onViewportResize({4, 0});
	auto camFloat = sc.createEntity("CamFloat");
	camFix.addOrReplaceComponent<component::Camera>(true, true);
	camFloat.addOrReplaceComponent<component::Camera>(false, false);
	cam = sc.getPrimaryCamera();
	EXPECT_TRUE(camFix == cam);
	sc.onViewportResize({60, 60});
}

TEST(Scene, Copy) {
	const owl::shared<Scene> sc = owl::mkShared<Scene>();
	auto ent = sc->createEntityWithUUID(0, "Camera");
	ent.addOrReplaceComponent<component::Camera>();
	auto ent2 = sc->duplicateEntity(ent);
	EXPECT_NE(ent.getUUID(), ent2.getUUID());
	EXPECT_STREQ(ent.getName().c_str(), ent2.getName().c_str());
	ent2.getComponent<component::Tag>().tag = "Sprite1";
	ent2.addOrReplaceComponent<component::Transform>();
	ent2.addOrReplaceComponent<component::NativeScript>();
	ent2.addOrReplaceComponent<component::SpriteRenderer>();
	ent2.addOrReplaceComponent<component::Text>();
	auto ent3 = sc->createEntityWithUUID(88, "circle");
	ent3.addOrReplaceComponent<component::Transform>();
	ent3.addOrReplaceComponent<component::CircleRenderer>();
	owl::shared<Scene> sc2 = owl::mkShared<Scene>();
	sc2->onViewportResize({800, 600});
	sc2 = Scene::copy(sc);
	EXPECT_TRUE(sc2->registry.storage<owl::scene::Entity>().size() ==
				sc->registry.storage<owl::scene::Entity>().size());
}

TEST(Scene, RenderEmpty) {
	owl::core::Log::init(spdlog::level::off);
	const owl::shared<Scene> sc = owl::mkShared<Scene>();
	sc->onViewportResize({800, 600});
	sc->onStartRuntime();
	owl::core::Timestep ts;
	ts.update();
	sc->onUpdateRuntime(ts);
	ts.update();
	sc->onEndRuntime();
	EXPECT_TRUE(true);
	owl::core::Log::invalidate();
}
