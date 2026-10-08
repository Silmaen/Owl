/**
 * @file SceneRobustness_test.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <core/Timestep.h>
#include <debug/LogSink.h>
#include <physics/PhysicCommand.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/component/components.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <tuple>
#include <vector>

using namespace owl;
using namespace owl::scene;

namespace {

class SceneRobustnessTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_dir = std::filesystem::temp_directory_path() / "owl_scene_robustness_test";
		std::filesystem::remove_all(m_dir);
		std::filesystem::create_directories(m_dir);
	}

	void TearDown() override {
		if (physics::PhysicCommand::isInitialized())
			physics::PhysicCommand::destroy();
		std::filesystem::remove_all(m_dir);
		core::Log::invalidate();
	}

	[[nodiscard]] auto writeFile(const std::string& iName, const std::string& iContent) const -> std::filesystem::path {
		const auto path = m_dir / iName;
		std::ofstream file(path);
		file << iContent;
		return path;
	}

	std::filesystem::path m_dir;
};

auto makeStep(const int iMs) -> core::Timestep {
	core::Timestep ts;
	ts.forceUpdate(std::chrono::milliseconds(iMs));
	return ts;
}

auto load(const shared<Scene>& ioScene, const std::string& iYaml) -> SceneLoadResult {
	const SceneSerializer serializer(ioScene);
	const std::vector<uint8_t> bytes(iYaml.begin(), iYaml.end());
	return serializer.deserializeFromBuffer(bytes, "<test>");
}

}// namespace

// C-04: a link to a missing entity is ignored instead of crashing the frame.
TEST_F(SceneRobustnessTest, EntityLinkToMissingTargetIsIgnored) {
	Scene scn;
	auto follower = scn.createEntity("Follower");
	follower.getComponent<component::Transform>().transform.translation() = {2.f, 3.f, 0.f};
	follower.addComponent<component::EntityLink>().linkedEntityName = "DoesNotExist";
	scn.onStartRuntime();
	scn.onUpdateRuntime(makeStep(16), false);
	scn.onUpdateRuntime(makeStep(16), false);
	EXPECT_FALSE(follower.getComponent<component::EntityLink>().linkedEntity);
	EXPECT_TRUE(follower.getComponent<component::EntityLink>().wasUnresolvedReported);
	EXPECT_FLOAT_EQ(follower.getComponent<component::Transform>().transform.translation().x(), 2.f);

	auto target = scn.createEntity("DoesNotExist");
	target.getComponent<component::Transform>().transform.translation() = {7.f, 0.f, 0.f};
	scn.onUpdateRuntime(makeStep(16), false);
	EXPECT_FALSE(follower.getComponent<component::EntityLink>().wasUnresolvedReported);
	EXPECT_FLOAT_EQ(follower.getComponent<component::Transform>().transform.translation().x(), 7.f);

	scn.destroyEntity(target);
	scn.onUpdateRuntime(makeStep(16), false);
	EXPECT_FALSE(follower.getComponent<component::EntityLink>().linkedEntity);
	EXPECT_FLOAT_EQ(follower.getComponent<component::Transform>().transform.translation().x(), 7.f);
	scn.onEndRuntime();
}

// C-04: an empty link name is a no-op.
TEST_F(SceneRobustnessTest, EntityLinkWithEmptyNameIsIgnored) {
	Scene scn;
	auto follower = scn.createEntity("Follower");
	follower.addComponent<component::EntityLink>();
	scn.onStartRuntime();
	scn.onUpdateRuntime(makeStep(16), false);
	EXPECT_FALSE(follower.getComponent<component::EntityLink>().linkedEntity);
	scn.onEndRuntime();
}

// C-03: blocks edited in the Play copy do not leak into the editor scene.
TEST_F(SceneRobustnessTest, PlayCopyDoesNotShareVoxelChunks) {
	const auto editor = mkShared<Scene>();
	auto world = editor->createEntity("World");
	auto& voxel = world.addComponent<component::VoxelWorld>();
	voxel.world.setBlock({1, 2, 3}, 1);
	voxel.pendingChunks.insert(42);
	const auto play = Scene::copy(editor);
	auto& playVoxel = play->findEntityByUUID(world.getUUID()).getComponent<component::VoxelWorld>();
	EXPECT_TRUE(playVoxel.pendingChunks.empty());
	playVoxel.world.setBlock({1, 2, 3}, 0);
	playVoxel.world.setBlock({100, 0, 0}, 1);
	EXPECT_EQ(world.getComponent<component::VoxelWorld>().world.getBlock({1, 2, 3}), 1u);
	EXPECT_EQ(world.getComponent<component::VoxelWorld>().world.chunkCount(), 1u);
}

// C-06: a two-entity parent cycle is cut at load, so subtree walks terminate.
TEST_F(SceneRobustnessTest, LoadBreaksHierarchyCycle) {
	const auto scn = mkShared<Scene>();
	ASSERT_TRUE(load(scn, "Scene: x\nEntities:\n"
						  "  - Entity: 11\n    Tag: {tag: A}\n    Hierarchy: {parentId: 22}\n"
						  "  - Entity: 22\n    Tag: {tag: B}\n    Hierarchy: {parentId: 11}\n"
						  "  - Entity: 33\n    Tag: {tag: C}\n    Hierarchy: {parentId: 33}\n"));
	EXPECT_EQ(scn->getRootEntities().size(), 2u);
	const auto a = scn->findEntityByUUID(core::UUID{11});
	const auto b = scn->findEntityByUUID(core::UUID{22});
	ASSERT_TRUE(a);
	ASSERT_TRUE(b);
	const bool aIsRoot = a.getComponent<component::Hierarchy>().parentId == core::UUID{0};
	const bool bIsRoot = b.getComponent<component::Hierarchy>().parentId == core::UUID{0};
	EXPECT_NE(aIsRoot, bIsRoot);
	EXPECT_EQ(scn->findEntityByUUID(core::UUID{33}).getComponent<component::Hierarchy>().parentId, core::UUID{0});
	std::ignore = scn->duplicateSubtree(aIsRoot ? a : b);
	EXPECT_EQ(scn->getAllEntities().size(), 5u);
}

// C-06: a duplicated UUID gets a fresh one instead of shadowing the first entity.
TEST_F(SceneRobustnessTest, LoadRenamesDuplicatedUuid) {
	const auto scn = mkShared<Scene>();
	ASSERT_TRUE(load(scn, "Scene: x\nEntities:\n"
						  "  - Entity: 1\n    Tag: {tag: First}\n"
						  "  - Entity: 1\n    Tag: {tag: Second}\n"
						  "  - Entity: 0\n    Tag: {tag: Null}\n"));
	EXPECT_EQ(scn->getAllEntities().size(), 3u);
	EXPECT_EQ(scn->findEntityByUUID(core::UUID{1}).getName(), "First");
	for (const auto& entity: scn->getAllEntities()) EXPECT_NE(entity.getUUID(), core::UUID{0});
}

// C-06: a dangling parent reference moves the entity to the root.
TEST_F(SceneRobustnessTest, LoadOrphansMissingParent) {
	const auto scn = mkShared<Scene>();
	ASSERT_TRUE(load(scn, "Scene: x\nEntities:\n  - Entity: 5\n    Tag: {tag: A}\n    Hierarchy: {parentId: 99}\n"));
	EXPECT_EQ(scn->findEntityByUUID(core::UUID{5}).getComponent<component::Hierarchy>().parentId, core::UUID{0});
}

// C-06: corrupted data fails with a typed error and leaves the scene as it was.
TEST_F(SceneRobustnessTest, CorruptedSceneFailsWithTypedErrorAndRollsBack) {
	const auto scn = mkShared<Scene>();
	scn->createEntityWithUUID(core::UUID{1000}, "Existing");
	const auto expectError = [&scn](const std::string& iYaml, const SceneLoadError iError) -> void {
		const auto result = load(scn, iYaml);
		ASSERT_FALSE(result);
		EXPECT_EQ(result.error(), iError);
		EXPECT_FALSE(describe(result.error()).empty());
		ASSERT_EQ(scn->getAllEntities().size(), 1u);
		EXPECT_TRUE(scn->findEntityByUUID(core::UUID{1000}));
	};
	expectError("Scene: [unterminated", SceneLoadError::InvalidYaml);
	expectError("Bob: toto\n", SceneLoadError::NotAScene);
	expectError("- just\n- a list\n", SceneLoadError::NotAScene);
	expectError("Scene: x\nEntities:\n  A: 1\n", SceneLoadError::NotAScene);
	expectError("Scene: x\nEntities:\n  - Entity: 1\n  - Tag: {tag: NoId}\n", SceneLoadError::InvalidEntity);
	expectError("Scene: x\nEntities:\n  - Entity: 1\n  - Entity: [2]\n", SceneLoadError::InvalidEntity);
	expectError("Scene: x\nEntities:\n  - Entity: 1\n    Tag: {tag: A}\n  - Entity: 2\n    Hierarchy: {parentId: 1}\n"
				"    Transform: {translation: notanumber}\n",
				SceneLoadError::InvalidEntity);
	expectError("Scene: x\nEntities:\n  - Entity: notanumber\n", SceneLoadError::InvalidEntity);
}

// Phase D: a load error names the file, the entity and the fix.
TEST_F(SceneRobustnessTest, LoadErrorNamesFileEntityAndFix) {
	core::Log::setVerbosityLevel(core::Log::Level::Error);
	core::Log::getLogBuffer().clear();
	const auto scn = mkShared<Scene>();
	const SceneSerializer serializer(scn);
	const std::string yaml = "Scene: x\nEntities:\n  - Entity: 1\n    Tag: {tag: Fine}\n  - Entity: 42\n"
							 "    Tag: {tag: Broken}\n    Transform: {translation: notanumber}\n";
	const std::vector<uint8_t> bytes(yaml.begin(), yaml.end());
	ASSERT_FALSE(serializer.deserializeFromBuffer(bytes, "scenes/level.owl"));
	bool found = false;
	for (const auto& entry: core::Log::getLogBuffer().getEntries()) {
		if (entry.message.find("scenes/level.owl") == std::string::npos)
			continue;
		found = true;
		EXPECT_NE(entry.message.find("42"), std::string::npos) << entry.message;
		EXPECT_NE(entry.message.find("'Broken'"), std::string::npos) << entry.message;
		EXPECT_NE(entry.message.find("line 5"), std::string::npos) << entry.message;
		EXPECT_NE(entry.message.find("Fix: "), std::string::npos) << entry.message;
	}
	EXPECT_TRUE(found);
}

// Phase D: every load error has a fix hint.
TEST(SceneLoadErrorHint, EveryErrorHasAFix) {
	for (const auto error: {SceneLoadError::FileUnreadable, SceneLoadError::InvalidYaml, SceneLoadError::NotAScene,
							SceneLoadError::InvalidEntity, SceneLoadError::InvalidFormatVersion,
							SceneLoadError::NewerFormatVersion, SceneLoadError::MigrationFailed}) {
		EXPECT_FALSE(fixHint(error).empty());
		EXPECT_NE(fixHint(error), describe(error));
	}
}

// C-06: a missing file reports FileUnreadable.
TEST_F(SceneRobustnessTest, MissingFileIsUnreadable) {
	const auto scn = mkShared<Scene>();
	const SceneSerializer serializer(scn);
	const auto result = serializer.deserialize(m_dir / "missing.owl");
	ASSERT_FALSE(result);
	EXPECT_EQ(result.error(), SceneLoadError::FileUnreadable);
	const auto good = writeFile("good.owl", "Scene: x\nEntities:\n  - Entity: 7\n    Tag: {tag: Good}\n");
	EXPECT_TRUE(serializer.deserialize(good));
	EXPECT_EQ(scn->findEntityByUUID(core::UUID{7}).getName(), "Good");
}

// C-17: a script hiding a trigger in on_update stops it in the same frame (the visibility cache is armed later).
TEST_F(SceneRobustnessTest, TriggerHiddenByScriptDoesNotFireSameFrame) {
	OWL_REQUIRE_MODULE(SCRIPT);
	const auto script = writeFile("hide.lua", "function on_update(dt)\n    ui.set_visible(entity_id, false)\nend\n");
	Scene scn;
	scn.createEntity("Player").addComponent<component::Player>().primary = true;
	auto goal = scn.createEntity("Goal");
	goal.addComponent<component::Trigger>().trigger.type = SceneTrigger::TriggerType::Victory;
	goal.addComponent<component::LuaScript>().scriptPath = script.string();
	scn.onStartRuntime();
	scn.onUpdateRuntime(makeStep(16), false);
	EXPECT_FALSE(goal.getComponent<component::Visibility>().gameVisible);
	EXPECT_EQ(scn.status, Scene::Status::Playing);
	scn.onEndRuntime();
}
