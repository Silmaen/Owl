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

TEST(SaveManager, loadNonExistentSlot) {
	core::Log::init(core::Log::Level::Off);
	TestSaveGuard guard;

	auto scn = mkShared<Scene>();
	const auto result = SaveManager::load(999, scn);
	EXPECT_FALSE(result.success);
	EXPECT_TRUE(result.physicsSnapshots.empty());

	core::Log::invalidate();
}

TEST(SaveManager, loadCorruptSaveFile) {
	core::Log::init(core::Log::Level::Off);
	TestSaveGuard guard;

	// Create a corrupt save file manually.
	const auto savePath = SaveManager::getSaveDirectory() / "save_50.owl_save";
	{
		std::ofstream file(savePath);
		file << "this is not valid YAML: [[[";
		file.close();
	}
	EXPECT_TRUE(SaveManager::hasSave(50));

	auto scn = mkShared<Scene>();
	const auto result = SaveManager::load(50, scn);
	EXPECT_FALSE(result.success);

	// getScenePath on corrupt file should return empty.
	EXPECT_TRUE(SaveManager::getScenePath(50).empty());

	SaveManager::deleteSave(50);
	core::Log::invalidate();
}

TEST(SaveManager, loadSaveWithoutSceneData) {
	core::Log::init(core::Log::Level::Off);
	TestSaveGuard guard;

	// Write a valid YAML save file that is missing SceneData.
	const auto savePath = SaveManager::getSaveDirectory() / "save_51.owl_save";
	{
		std::ofstream file(savePath);
		file << "OwlSave:\n";
		file << "  version: 1\n";
		file << "  timestamp: '2026-04-14T00:00:00'\n";
		file << "  scenePath: test.owl\n";
		file << "GameState:\n";
		file << "  Entries: []\n";
		file.close();
	}
	EXPECT_TRUE(SaveManager::hasSave(51));

	auto scn = mkShared<Scene>();
	const auto result = SaveManager::load(51, scn);
	// Load succeeds even without SceneData (just no entities loaded).
	EXPECT_TRUE(result.success);

	SaveManager::deleteSave(51);
	core::Log::invalidate();
}

TEST(SaveManager, loadSaveWithPhysicsSnapshots) {
	core::Log::init(core::Log::Level::Off);
	TestSaveGuard guard;

	// Write a save file with explicit PhysicsSnapshots section.
	const auto savePath = SaveManager::getSaveDirectory() / "save_52.owl_save";
	{
		std::ofstream file(savePath);
		file << "OwlSave:\n";
		file << "  version: 1\n";
		file << "  timestamp: '2026-04-14T12:00:00'\n";
		file << "  scenePath: scenes/physics_test.owl\n";
		file << "PhysicsSnapshots:\n";
		file << "  - uuid: 100\n";
		file << "    vx: 1.5\n";
		file << "    vy: -2.5\n";
		file << "    av: 0.3\n";
		file << "    awake: true\n";
		file << "  - uuid: 200\n";
		file << "    vx: 0.0\n";
		file << "    vy: 0.0\n";
		file << "    av: 0.0\n";
		file << "    awake: false\n";
		file.close();
	}

	auto scn = mkShared<Scene>();
	const auto result = SaveManager::load(52, scn);
	EXPECT_TRUE(result.success);
	EXPECT_EQ(result.physicsSnapshots.size(), 2);

	// Verify first snapshot.
	ASSERT_TRUE(result.physicsSnapshots.contains(100));
	const auto& snap1 = result.physicsSnapshots.at(100);
	EXPECT_NEAR(snap1.linearVelocity.x(), 1.5f, 0.01f);
	EXPECT_NEAR(snap1.linearVelocity.y(), -2.5f, 0.01f);
	EXPECT_NEAR(snap1.angularVelocity, 0.3f, 0.01f);
	EXPECT_TRUE(snap1.awake);

	// Verify second snapshot.
	ASSERT_TRUE(result.physicsSnapshots.contains(200));
	const auto& snap2 = result.physicsSnapshots.at(200);
	EXPECT_NEAR(snap2.linearVelocity.x(), 0.0f, 0.01f);
	EXPECT_NEAR(snap2.linearVelocity.y(), 0.0f, 0.01f);
	EXPECT_NEAR(snap2.angularVelocity, 0.0f, 0.01f);
	EXPECT_FALSE(snap2.awake);

	SaveManager::deleteSave(52);
	core::Log::invalidate();
}

TEST(SaveManager, loadSaveWithPartialPhysicsSnapshot) {
	core::Log::init(core::Log::Level::Off);
	TestSaveGuard guard;

	// A snapshot entry missing some fields should use defaults.
	const auto savePath = SaveManager::getSaveDirectory() / "save_53.owl_save";
	{
		std::ofstream file(savePath);
		file << "OwlSave:\n";
		file << "  version: 1\n";
		file << "  timestamp: '2026-04-14T12:00:00'\n";
		file << "  scenePath: test.owl\n";
		file << "PhysicsSnapshots:\n";
		file << "  - uuid: 300\n";
		file << "    vx: 5.0\n";
		// Missing vy, av, awake — should be defaults.
		file.close();
	}

	auto scn = mkShared<Scene>();
	const auto result = SaveManager::load(53, scn);
	EXPECT_TRUE(result.success);
	ASSERT_EQ(result.physicsSnapshots.size(), 1);
	ASSERT_TRUE(result.physicsSnapshots.contains(300));
	const auto& snap = result.physicsSnapshots.at(300);
	EXPECT_NEAR(snap.linearVelocity.x(), 5.0f, 0.01f);
	// Default values for missing fields.
	EXPECT_NEAR(snap.linearVelocity.y(), 0.0f, 0.01f);
	EXPECT_NEAR(snap.angularVelocity, 0.0f, 0.01f);
	EXPECT_TRUE(snap.awake);// PhysicsSnapshot default is true.

	SaveManager::deleteSave(53);
	core::Log::invalidate();
}

TEST(SaveManager, loadSavePhysicsSnapshotMissingUuid) {
	core::Log::init(core::Log::Level::Off);
	TestSaveGuard guard;

	// A snapshot entry without uuid should be skipped.
	const auto savePath = SaveManager::getSaveDirectory() / "save_54.owl_save";
	{
		std::ofstream file(savePath);
		file << "OwlSave:\n";
		file << "  version: 1\n";
		file << "  timestamp: '2026-04-14T12:00:00'\n";
		file << "  scenePath: test.owl\n";
		file << "PhysicsSnapshots:\n";
		file << "  - vx: 1.0\n";
		file << "    vy: 2.0\n";
		file << "  - uuid: 400\n";
		file << "    vx: 3.0\n";
		file.close();
	}

	auto scn = mkShared<Scene>();
	const auto result = SaveManager::load(54, scn);
	EXPECT_TRUE(result.success);
	// Only the entry with uuid should be present.
	EXPECT_EQ(result.physicsSnapshots.size(), 1);
	EXPECT_TRUE(result.physicsSnapshots.contains(400));

	SaveManager::deleteSave(54);
	core::Log::invalidate();
}

TEST(SaveManager, listSavesWithCorruptFile) {
	core::Log::init(core::Log::Level::Off);
	TestSaveGuard guard;

	// Create one valid save.
	auto scn = mkShared<Scene>();
	scn->createEntity("E1");
	EXPECT_TRUE(SaveManager::save(1, scn, "scene1.owl"));

	// Create a corrupt save file.
	const auto corruptPath = SaveManager::getSaveDirectory() / "save_2.owl_save";
	{
		std::ofstream file(corruptPath);
		file << "not valid yaml: [[[{{{";
		file.close();
	}

	// listSaves should skip the corrupt file and still return the valid one.
	const auto saves = SaveManager::listSaves();
	EXPECT_EQ(saves.size(), 1);
	EXPECT_EQ(saves[0].slot, 1);

	SaveManager::deleteSave(2);
	core::Log::invalidate();
}

TEST(SaveManager, listSavesIgnoresNonSaveFiles) {
	core::Log::init(core::Log::Level::Off);
	TestSaveGuard guard;

	auto scn = mkShared<Scene>();
	scn->createEntity("E1");
	EXPECT_TRUE(SaveManager::save(1, scn, "scene1.owl"));

	// Create a non .owl_save file in the save directory.
	const auto otherPath = SaveManager::getSaveDirectory() / "notes.txt";
	{
		std::ofstream file(otherPath);
		file << "some notes";
		file.close();
	}

	const auto saves = SaveManager::listSaves();
	EXPECT_EQ(saves.size(), 1);
	EXPECT_EQ(saves[0].slot, 1);

	std::filesystem::remove(otherPath);
	core::Log::invalidate();
}

TEST(SaveManager, deleteSaveNonExistent) {
	core::Log::init(core::Log::Level::Off);
	TestSaveGuard guard;

	// Deleting a non-existent save should not crash.
	SaveManager::deleteSave(999);
	EXPECT_FALSE(SaveManager::hasSave(999));

	core::Log::invalidate();
}

TEST(SaveManager, defaultGameName) {
	core::Log::init(core::Log::Level::Off);
	// Set empty game name — should use "OwlGame" as default.
	SaveManager::setGameName("");
	const auto dir = SaveManager::getSaveDirectory();
	EXPECT_TRUE(dir.string().find("OwlGame") != std::string::npos);

	// Cleanup.
	if (exists(dir))
		std::filesystem::remove_all(dir.parent_path());
	core::Log::invalidate();
}

TEST(SaveManager, loadSetsLoadedFromSave) {
	core::Log::init(core::Log::Level::Off);
	TestSaveGuard guard;

	auto scn = mkShared<Scene>();
	scn->createEntity("E1");
	EXPECT_TRUE(SaveManager::save(1, scn, "test.owl"));

	auto loaded = mkShared<Scene>();
	const auto result = SaveManager::load(1, loaded);
	EXPECT_TRUE(result.success);

	// Verify the loaded_from_save flag is set.
	const auto val = loaded->getGameState().get("loaded_from_save");
	ASSERT_TRUE(val.has_value());
	EXPECT_TRUE(std::get<bool>(val.value()));

	core::Log::invalidate();
}
