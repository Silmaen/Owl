/**
 * @file SaveManager_test.cpp
 * @author Silmaen
 * @date 13/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <scene/Entity.h>
#include <scene/SaveManager.h>
#include <scene/Scene.h>
#include <scene/component/SpriteRenderer.h>
#include <scene/component/Transform.h>

using namespace owl;
using namespace owl::scene;

namespace {

/// Use a temp directory as save location for tests.
struct TestSaveGuard {
	TestSaveGuard() {
		SaveManager::setGameName("OwlTestGame_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
	}
	~TestSaveGuard() {
		// Cleanup save directory.
		const auto dir = SaveManager::getSaveDirectory();
		if (exists(dir))
			std::filesystem::remove_all(dir.parent_path());
	}
};

}// namespace

TEST(SaveManager, saveDirectory) {
	core::Log::init(core::Log::Level::Off);
	TestSaveGuard guard;
	const auto dir = SaveManager::getSaveDirectory();
	EXPECT_TRUE(exists(dir));
	EXPECT_TRUE(dir.string().find("saves") != std::string::npos);
	core::Log::invalidate();
}

TEST(SaveManager, hasSaveEmpty) {
	core::Log::init(core::Log::Level::Off);
	TestSaveGuard guard;
	EXPECT_FALSE(SaveManager::hasSave(1));
	core::Log::invalidate();
}

TEST(SaveManager, saveAndLoad) {
	core::Log::init(core::Log::Level::Off);
	TestSaveGuard guard;

	// Create scene with entities and game state.
	auto scn = mkShared<Scene>();
	auto entity = scn->createEntity("TestEntity");
	entity.addComponent<component::SpriteRenderer>().color = {1.f, 0.f, 0.f, 1.f};
	entity.getComponent<component::Transform>().transform.translation() = {5.f, 3.f, 0.f};
	scn->getGameState().set("coins", int64_t{42});
	scn->getGameState().set("level", std::string("forest"));

	// Save.
	EXPECT_TRUE(SaveManager::save(1, scn, "scenes/test.owl"));
	EXPECT_TRUE(SaveManager::hasSave(1));

	// Load into new scene.
	auto loaded = mkShared<Scene>();
	EXPECT_TRUE(SaveManager::load(1, loaded).success);

	// Verify GameState.
	EXPECT_EQ(std::get<int64_t>(loaded->getGameState().get("coins").value()), 42);
	EXPECT_EQ(std::get<std::string>(loaded->getGameState().get("level").value()), "forest");

	// Verify entities.
	const auto entities = loaded->getAllEntities();
	EXPECT_EQ(entities.size(), 1);
	EXPECT_EQ(entities[0].getName(), "TestEntity");
	EXPECT_TRUE(entities[0].hasComponent<component::SpriteRenderer>());
	EXPECT_NEAR(entities[0].getComponent<component::Transform>().transform.translation().x(), 5.f, 0.01f);

	core::Log::invalidate();
}

TEST(SaveManager, listSaves) {
	core::Log::init(core::Log::Level::Off);
	TestSaveGuard guard;

	auto scn = mkShared<Scene>();
	scn->createEntity("E1");
	EXPECT_TRUE(SaveManager::save(1, scn, "scene1.owl"));
	EXPECT_TRUE(SaveManager::save(3, scn, "scene3.owl"));

	const auto saves = SaveManager::listSaves();
	EXPECT_EQ(saves.size(), 2);
	EXPECT_EQ(saves[0].slot, 1);
	EXPECT_EQ(saves[0].scenePath, "scene1.owl");
	EXPECT_EQ(saves[1].slot, 3);
	EXPECT_EQ(saves[1].scenePath, "scene3.owl");
	EXPECT_FALSE(saves[0].timestamp.empty());

	core::Log::invalidate();
}

TEST(SaveManager, deleteSave) {
	core::Log::init(core::Log::Level::Off);
	TestSaveGuard guard;

	auto scn = mkShared<Scene>();
	scn->createEntity("E1");
	EXPECT_TRUE(SaveManager::save(1, scn, "test.owl"));
	EXPECT_TRUE(SaveManager::hasSave(1));
	SaveManager::deleteSave(1);
	EXPECT_FALSE(SaveManager::hasSave(1));

	core::Log::invalidate();
}

TEST(SaveManager, getScenePath) {
	core::Log::init(core::Log::Level::Off);
	TestSaveGuard guard;

	auto scn = mkShared<Scene>();
	scn->createEntity("E1");
	EXPECT_TRUE(SaveManager::save(2, scn, "scenes/level2.owl"));
	EXPECT_EQ(SaveManager::getScenePath(2), "scenes/level2.owl");
	EXPECT_TRUE(SaveManager::getScenePath(99).empty());

	core::Log::invalidate();
}
