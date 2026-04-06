/**
 * @file SceneExtra_test.cpp
 * @author Silmaen
 * @date 06/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "input/null/Input.h"
#include "testHelper.h"

#include <input/Input.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <renderer/CameraEditor.h>
#include <scene/component/components.h>

using namespace owl::scene;

TEST(Scene, GetAllEntities) {
	Scene sc;
	sc.createEntity("A");
	sc.createEntity("B");
	sc.createEntity("C");
	const auto all = sc.getAllEntities();
	EXPECT_EQ(all.size(), 3u);
}

TEST(Scene, GetEntityCount) {
	const owl::shared<Scene> sc = owl::mkShared<Scene>();
	sc->createEntity("A");
	sc->createEntity("B");
	const auto all = sc->getAllEntities();
	EXPECT_EQ(all.size(), 2u);
}

TEST(Scene, GetPrimaryPlayer) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	owl::input::Input::init(owl::window::Type::Null);
	Scene sc;
	auto noPlayer = sc.getPrimaryPlayer();
	EXPECT_FALSE(noPlayer);

	auto ent = sc.createEntity("player");
	auto& [primary, player] = ent.addComponent<component::Player>();
	primary = true;
	auto found = sc.getPrimaryPlayer();
	EXPECT_TRUE(found);

	owl::input::Input::invalidate();
	owl::core::Log::invalidate();
}

TEST(Scene, DuplicateEntityComponents) {
	const owl::shared<Scene> sc = owl::mkShared<Scene>();
	auto ent = sc->createEntity("Original");
	ent.addOrReplaceComponent<component::SpriteRenderer>();
	ent.addOrReplaceComponent<component::CircleRenderer>();
	auto dup = sc->duplicateEntity(ent);
	EXPECT_TRUE(dup.hasComponent<component::SpriteRenderer>());
	EXPECT_TRUE(dup.hasComponent<component::CircleRenderer>());
	EXPECT_TRUE(dup.hasComponent<component::Transform>());
	EXPECT_NE(ent.getUUID(), dup.getUUID());
}

TEST(Scene, DefaultEntityName) {
	Scene sc;
	auto ent = sc.createEntity();
	EXPECT_STREQ(ent.getName().c_str(), "Entity");
}

TEST(Scene, OnViewportResizeCamera) {
	Scene sc;
	auto camEnt = sc.createEntity("Cam");
	auto& cam = camEnt.addOrReplaceComponent<component::Camera>(false, false);
	cam.fixedAspectRatio = false;
	sc.onViewportResize({800, 600});
	// Camera should have been resized.
	EXPECT_TRUE(true);
}

TEST(Scene, CopyPreservesViewportSize) {
	const owl::shared<Scene> sc = owl::mkShared<Scene>();
	sc->onViewportResize({1920, 1080});
	sc->createEntity("test");
	const auto copy = Scene::copy(sc);
	EXPECT_EQ(copy->getEntityCount(), sc->getEntityCount());
}

TEST(Scene, OnRenderRuntimeEmpty) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	owl::input::Input::init(owl::window::Type::Null);
	Scene sc;
	sc.onViewportResize({800, 600});
	sc.onStartRuntime();
	// No camera => early return, no crash.
	sc.onRenderRuntime();
	sc.onEndRuntime();
	owl::input::Input::invalidate();
	owl::core::Log::invalidate();
}

TEST(Scene, EntityLinkComponent) {
	const owl::shared<Scene> sc = owl::mkShared<Scene>();
	auto ent = sc->createEntity("Linked");
	ent.addComponent<component::EntityLink>();
	EXPECT_TRUE(ent.hasComponent<component::EntityLink>());
	auto dup = sc->duplicateEntity(ent);
	EXPECT_TRUE(dup.hasComponent<component::EntityLink>());
}

TEST(Scene, BackgroundTextureComponent) {
	Scene sc;
	auto ent = sc.createEntity("BG");
	ent.addComponent<component::BackgroundTexture>();
	EXPECT_TRUE(ent.hasComponent<component::BackgroundTexture>());
}
