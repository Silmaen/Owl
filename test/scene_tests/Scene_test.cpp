
#include "input/null/Input.h"
#include "testHelper.h"

#include <input/Input.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/component/components.h>

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
	EXPECT_TRUE(sc2->getEntityCount() == sc->getEntityCount());
}

TEST(Scene, RenderEmpty) {
	owl::core::Log::init(owl::core::Log::Level::Off);
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

namespace {
void createMinGameScene(Scene& ioScene) {
	{
		auto ground = ioScene.createEntity("ground");
		auto& [body] = ground.addComponent<component::PhysicBody>();
		auto& tran = ground.addOrReplaceComponent<component::Transform>().transform;
		tran.scale().x() = 10.f;
		tran.scale().y() = .1f;
		tran.translation().y() = -.1f;
		body.type = SceneBody::BodyType::Static;
	}
	{
		auto player = ioScene.createEntity("player");
		auto& tran = player.addOrReplaceComponent<component::Transform>().transform;
		tran.translation().y() = 0.5;
		auto& [primary, pplayer] = player.addComponent<component::Player>();
		primary = true;
		pplayer.linearImpulse = 5.f;
		pplayer.canJump = false;
		player.addComponent<component::Trigger>();
		player.removeComponent<component::Trigger>();
		auto& [body] = player.addComponent<component::PhysicBody>();
		body.type = SceneBody::BodyType::Dynamic;
	}
	{
		auto winZone = ioScene.createEntity("win");
		winZone.addOrReplaceComponent<component::Transform>().transform.translation().x() = 5;
		auto& [trigger] = winZone.addComponent<component::Trigger>();
		trigger.type = SceneTrigger::TriggerType::Victory;
		auto& [body] = winZone.addComponent<component::PhysicBody>();
		body.type = SceneBody::BodyType::Static;
	}
	{
		auto looseZone = ioScene.createEntity("looze");
		looseZone.addOrReplaceComponent<component::Transform>().transform.translation().x() = -5;
		auto& [trigger] = looseZone.addComponent<component::Trigger>();
		trigger.type = SceneTrigger::TriggerType::Death;
		auto& [body] = looseZone.addComponent<component::PhysicBody>();
		body.type = SceneBody::BodyType::Static;
	}
}
}// namespace

TEST(Scene, RenderGame_loose) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	owl::input::Input::init(owl::input::Type::Null);
	Scene sc;
	createMinGameScene(sc);
	owl::core::Timestep ts;
	ts.forceUpdate(std::chrono::milliseconds(500));

	EXPECT_EQ(sc.status, Scene::Status::Editing);
	sc.onStartRuntime();
	sc.onUpdateRuntime(ts);
	EXPECT_EQ(sc.status, Scene::Status::Playing);
	owl::input::Input::injectKey(owl::input::key::A);
	owl::input::Input::injectKey(owl::input::key::Space);

	ts.forceUpdate(std::chrono::milliseconds(500));
	sc.onUpdateRuntime(ts);
	ts.forceUpdate(std::chrono::milliseconds(500));
	sc.onUpdateRuntime(ts);

	ts.forceUpdate(std::chrono::milliseconds(500));
	sc.onUpdateRuntime(ts);
	ts.forceUpdate(std::chrono::milliseconds(500));
	sc.onUpdateRuntime(ts);

	EXPECT_EQ(sc.status, Scene::Status::Death);
	sc.onEndRuntime();

	owl::input::Input::invalidate();
	owl::core::Log::invalidate();
}


TEST(Scene, RenderGame_win) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	owl::input::Input::init(owl::input::Type::Null);
	Scene sc;
	createMinGameScene(sc);
	owl::core::Timestep ts;
	ts.forceUpdate(std::chrono::milliseconds(500));

	EXPECT_EQ(sc.status, Scene::Status::Editing);
	sc.onStartRuntime();
	sc.onUpdateRuntime(ts);
	EXPECT_EQ(sc.status, Scene::Status::Playing);
	owl::input::Input::injectKey(owl::input::key::D);

	ts.forceUpdate(std::chrono::milliseconds(500));
	sc.onUpdateRuntime(ts);
	ts.forceUpdate(std::chrono::milliseconds(500));
	sc.onUpdateRuntime(ts);

	ts.forceUpdate(std::chrono::milliseconds(500));
	sc.onUpdateRuntime(ts);
	ts.forceUpdate(std::chrono::milliseconds(500));
	sc.onUpdateRuntime(ts);

	EXPECT_EQ(sc.status, Scene::Status::Victory);
	sc.onEndRuntime();

	owl::input::Input::invalidate();
	owl::core::Log::invalidate();
}
